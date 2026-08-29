/*
Module: engine/world
File: engine/world/sources/ReliefLayer.cpp

Responsibility:
- The hand-edit layer declared in ReliefLayer.h, and its sidecar text format.

Dependencies:
- Uses: ReliefLayer.h, std::filesystem, std streams.
- Used by: Worldgen (compose_passes), ChunkManager, engine/app (EditorBrush).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE WRITER SORTS. Never write straight out of the hash map, however tempting:
  the file's whole value as a record is that a diff names the samples that
  moved, and an unordered container reshuffles them on rehash.
- THE READER IS TOTAL BUT NOT FORGIVING ABOUT NUMBERS. An unknown key is
  skipped (the format will grow); a malformed number is an error WITH ITS LINE,
  because "0 by accident" is the failure mode this project keeps paying for.
*/

#include "engine/world/sources/ReliefLayer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/geometric.hpp>
#include <sstream>

namespace dfn::world {
namespace {

/// The class names the file uses. SPELLED, not numbered: an ordinal in a
/// hand-edited file is a number nobody can read and one that silently means
/// something else the day the enum grows. The mapping is exhaustive by switch
/// so a new SurfaceClass cannot be forgotten here (Rule 32).
[[nodiscard]] const char* surface_name(math::SurfaceClass c) {
    switch (c) {
    case math::SurfaceClass::Grass:          return "grass";
    case math::SurfaceClass::GrassRockBlend: return "blend";
    case math::SurfaceClass::Rock:           return "rock";
    case math::SurfaceClass::Sand:           return "sand";
    case math::SurfaceClass::WaterBed:       return "waterbed";
    }
    return "grass";
}

[[nodiscard]] bool surface_of_name(const std::string& s, math::SurfaceClass& out) {
    if (s == "grass")    { out = math::SurfaceClass::Grass;          return true; }
    if (s == "blend")    { out = math::SurfaceClass::GrassRockBlend; return true; }
    if (s == "rock")     { out = math::SurfaceClass::Rock;           return true; }
    if (s == "sand")     { out = math::SurfaceClass::Sand;           return true; }
    if (s == "waterbed") { out = math::SurfaceClass::WaterBed;       return true; }
    return false;
}

} // namespace

int32_t relief_index_floor(float world_coord) {
    return static_cast<int32_t>(std::floor(world_coord / RELIEF_STEP_M));
}

float relief_world_of(int32_t i) {
    return static_cast<float>(i) * RELIEF_STEP_M;
}

const ReliefLayer::Cell* ReliefLayer::find(int32_t x, int32_t z) const {
    const auto it = cells_.find(key_of(x, z));
    return it == cells_.end() ? nullptr : &it->second;
}

float ReliefLayer::delta_at(int32_t x, int32_t z) const {
    const Cell* c = find(x, z);
    return c == nullptr ? 0.0f : c->height_delta;
}

float ReliefLayer::height_delta_at(glm::vec2 world_xz) const {
    if (cells_.empty()) {
        return 0.0f;
    }
    const int32_t x0 = relief_index_floor(world_xz.x);
    const int32_t z0 = relief_index_floor(world_xz.y);
    const float fx = (world_xz.x - relief_world_of(x0)) / RELIEF_STEP_M;
    const float fz = (world_xz.y - relief_world_of(z0)) / RELIEF_STEP_M;
    // Untouched neighbours read as 0, which is what makes an edit fade to
    // nothing across its own last lattice cell with nobody drawing a skirt.
    const float h00 = delta_at(x0, z0);
    const float h10 = delta_at(x0 + 1, z0);
    const float h01 = delta_at(x0, z0 + 1);
    const float h11 = delta_at(x0 + 1, z0 + 1);
    const float a = h00 + (h10 - h00) * fx;
    const float b = h01 + (h11 - h01) * fx;
    return a + (b - a) * fz;
}

bool ReliefLayer::surface_at(glm::vec2 world_xz, math::SurfaceClass& out) const {
    if (cells_.empty()) {
        return false;
    }
    // NEAREST, not interpolated: a class is an enum, and the average of grass
    // and rock is neither. Rounding rather than flooring so the painted sample
    // owns the half-step around itself in every direction, which is what makes
    // a painted patch the size the brush said it was.
    const int32_t x = static_cast<int32_t>(std::lround(world_xz.x / RELIEF_STEP_M));
    const int32_t z = static_cast<int32_t>(std::lround(world_xz.y / RELIEF_STEP_M));
    const Cell* c = find(x, z);
    if (c == nullptr || !c->has_surface) {
        return false;
    }
    out = c->surface;
    return true;
}

void ReliefLayer::set_delta(int32_t x, int32_t z, float metres) {
    const uint64_t k = key_of(x, z);
    const auto it = cells_.find(k);
    if (it == cells_.end()) {
        if (metres == 0.0f) {
            return; // nothing to record, and recording it would be a lie in a diff
        }
        Cell c;
        c.height_delta = metres;
        cells_.emplace(k, c);
        return;
    }
    it->second.height_delta = metres;
    // AN UNDONE EDIT LEAVES NO TRACE. A cell holding a zero delta and no paint
    // is a sample nobody edited, and it must not survive into the file: a
    // composer who raised a hill and lowered it back has to get his original
    // file back, or every abandoned experiment lives forever as a diff line.
    if (metres == 0.0f && !it->second.has_surface) {
        cells_.erase(it);
    }
}

void ReliefLayer::set_surface(int32_t x, int32_t z, math::SurfaceClass surface) {
    Cell& c = cells_[key_of(x, z)];
    c.surface = surface;
    c.has_surface = true;
}

void ReliefLayer::clear_surface(int32_t x, int32_t z) {
    const auto it = cells_.find(key_of(x, z));
    if (it == cells_.end()) {
        return;
    }
    it->second.has_surface = false;
    if (it->second.height_delta == 0.0f) {
        cells_.erase(it);
    }
}

std::vector<ReliefSample> ReliefLayer::samples() const {
    std::vector<ReliefSample> out;
    out.reserve(cells_.size());
    for (const auto& [key, cell] : cells_) {
        ReliefSample s;
        s.x = static_cast<int32_t>(static_cast<uint32_t>(key >> 32));
        s.z = static_cast<int32_t>(static_cast<uint32_t>(key));
        s.height_delta = cell.height_delta;
        s.surface = cell.surface;
        s.has_surface = cell.has_surface;
        out.push_back(s);
    }
    return out;
}

bool ReliefLayer::bounds_xz(glm::vec2& min_xz, glm::vec2& max_xz) const {
    if (cells_.empty()) {
        return false;
    }
    int32_t lo_x = 0;
    int32_t lo_z = 0;
    int32_t hi_x = 0;
    int32_t hi_z = 0;
    bool first = true;
    for (const auto& [key, cell] : cells_) {
        (void)cell;
        const auto x = static_cast<int32_t>(static_cast<uint32_t>(key >> 32));
        const auto z = static_cast<int32_t>(static_cast<uint32_t>(key));
        if (first) {
            lo_x = hi_x = x;
            lo_z = hi_z = z;
            first = false;
            continue;
        }
        lo_x = std::min(lo_x, x);
        hi_x = std::max(hi_x, x);
        lo_z = std::min(lo_z, z);
        hi_z = std::max(hi_z, z);
    }
    // ONE STEP OF PADDING. A delta at a sample reaches half a lattice cell
    // either way through the bilinear filter, so a chunk that only overlaps the
    // padding still has ground that moved and still has to be rebuilt.
    min_xz = {relief_world_of(lo_x) - RELIEF_STEP_M, relief_world_of(lo_z) - RELIEF_STEP_M};
    max_xz = {relief_world_of(hi_x) + RELIEF_STEP_M, relief_world_of(hi_z) + RELIEF_STEP_M};
    return true;
}

// ============================== THE PATH CURVE ==============================

namespace {

/// Distance from `p` to the segment [a, b], and the segment's own length is
/// allowed to be zero (two identical control points is a thing a hand does).
[[nodiscard]] float dist_to_segment(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    const glm::vec2 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    if (len2 <= 0.0f) {
        return glm::length(p - a);
    }
    const float t = std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
    return glm::length(p - (a + ab * t));
}

/// One centripetal Catmull-Rom span, Barry-Goldman form. `t` runs 0..1 across
/// the span p1 -> p2; p0 and p3 are the neighbours (duplicated at the ends).
///
/// CENTRIPETAL (alpha = 0.5) AND NOT UNIFORM, and the difference is visible on
/// the first afternoon: with uniform knots a point placed close to its
/// neighbour makes the curve loop back on itself and the path crosses itself
/// where the hand drew no crossing. Centripetal knots cannot self-intersect
/// within a span — that is the theorem the parameterization exists for.
[[nodiscard]] glm::vec2 catmull_rom(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2,
                                    glm::vec2 p3, float t) {
    const auto knot = [](float t_prev, glm::vec2 a, glm::vec2 b) {
        const float d = std::sqrt(glm::length(b - a));
        // A ZERO SPACING WOULD DIVIDE BY ZERO, and two identical points is
        // exactly what a double click leaves behind. One lattice step is the
        // smallest spacing that means anything on this ground anyway.
        return t_prev + std::max(d, 1.0e-3f);
    };
    const float t0 = 0.0f;
    const float t1 = knot(t0, p0, p1);
    const float t2 = knot(t1, p1, p2);
    const float t3 = knot(t2, p2, p3);
    const float tt = t1 + (t2 - t1) * std::clamp(t, 0.0f, 1.0f);
    const auto lerp_knot = [](glm::vec2 a, glm::vec2 b, float ta, float tb, float x) {
        const float d = tb - ta;
        if (std::fabs(d) < 1.0e-6f) {
            return a;
        }
        const float w = (tb - x) / d;
        return a * w + b * (1.0f - w);
    };
    const glm::vec2 a1 = lerp_knot(p0, p1, t0, t1, tt);
    const glm::vec2 a2 = lerp_knot(p1, p2, t1, t2, tt);
    const glm::vec2 a3 = lerp_knot(p2, p3, t2, t3, tt);
    const glm::vec2 b1 = lerp_knot(a1, a2, t0, t2, tt);
    const glm::vec2 b2 = lerp_knot(a2, a3, t1, t3, tt);
    return lerp_knot(b1, b2, t1, t2, tt);
}

} // namespace

std::vector<glm::vec2> relief_path_polyline(const ReliefPath& path, float max_step_m) {
    std::vector<glm::vec2> out;
    if (path.points.size() < 2) {
        // A PATH OF ONE POINT IS A PLACE, NOT A PATH. Returning the point (and
        // not an invented arc through it) is what lets the tool draw the first
        // click without pretending it already knows where the walk goes.
        return path.points;
    }
    const float step = std::max(max_step_m, 0.05f);
    const std::size_t n = path.points.size();
    const auto at = [&](std::ptrdiff_t i) {
        return path.points[static_cast<std::size_t>(
            std::clamp<std::ptrdiff_t>(i, 0, static_cast<std::ptrdiff_t>(n) - 1))];
    };
    out.push_back(path.points.front());
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const glm::vec2 p0 = at(static_cast<std::ptrdiff_t>(i) - 1);
        const glm::vec2 p1 = path.points[i];
        const glm::vec2 p2 = path.points[i + 1];
        const glm::vec2 p3 = at(static_cast<std::ptrdiff_t>(i) + 2);
        // The chord is a floor on the arc, so this many samples is at least as
        // dense as asked and never fewer than one.
        const float chord = glm::length(p2 - p1);
        const int steps = std::max(1, static_cast<int>(std::ceil(chord / step)));
        for (int k = 1; k <= steps; ++k) {
            out.push_back(catmull_rom(p0, p1, p2, p3,
                                      static_cast<float>(k) / static_cast<float>(steps)));
        }
    }
    return out;
}

float relief_path_wear(const ReliefPath& path, float dist_m) {
    const float half = std::max(path.half_width_m, PATH_MIN_HALF_WIDTH_M);
    // THE FADE BAND IN METRES, floored — see PATH_MIN_FADE_M for the numbers.
    // A band narrower than the lattice is a 0/1 field, and 0/1 on a lattice is
    // the staircase this whole feature exists not to draw.
    const float fade = std::clamp(std::max(std::clamp(path.edge_softness, 0.0f, 1.0f) * half,
                                           PATH_MIN_FADE_M),
                                  0.0f, half);
    const float flat = half - fade;
    const float d = std::fabs(dist_m);
    if (d <= flat) {
        return 1.0f;
    }
    if (fade <= 0.0f) {
        return 0.0f;
    }
    // ONE CROSS-SECTION FOR THE WHOLE PROJECT (math::path_wear_profile). The
    // softness moves where u = 0 sits, and at softness 1 the flat top is gone
    // and this IS the generated network's profile, sample for sample.
    return math::path_wear_profile((d - flat) / fade);
}

bool relief_path_bounds(const ReliefPath& path, glm::vec2& min_xz, glm::vec2& max_xz) {
    if (path.points.empty()) {
        return false;
    }
    const std::vector<glm::vec2> poly = relief_path_polyline(path, RELIEF_STEP_M);
    glm::vec2 lo = poly.front();
    glm::vec2 hi = poly.front();
    for (const glm::vec2& p : poly) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    const float reach = std::max(path.half_width_m, PATH_MIN_HALF_WIDTH_M) + RELIEF_STEP_M;
    min_xz = lo - glm::vec2{reach, reach};
    max_xz = hi + glm::vec2{reach, reach};
    return true;
}

std::size_t relief_path_pick(const ReliefPath& path, glm::vec2 aim_xz, float grab_m) {
    std::size_t best = path.points.size();
    float best_d = grab_m;
    for (std::size_t i = 0; i < path.points.size(); ++i) {
        const float d = glm::length(path.points[i] - aim_xz);
        // STRICTLY NEARER, so two points inside one grab radius resolve to the
        // one the pointer is actually on rather than to whichever came first.
        if (d <= best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// ========================== THE WEAR CHANNEL ================================

float ReliefLayer::path_wear_of(int32_t x, int32_t z) const {
    const auto it = wear_.find(key_of(x, z));
    return it == wear_.end() ? 0.0f : it->second;
}

float ReliefLayer::path_wear_at(glm::vec2 world_xz) const {
    if (wear_.empty()) {
        return 0.0f;
    }
    const int32_t x0 = relief_index_floor(world_xz.x);
    const int32_t z0 = relief_index_floor(world_xz.y);
    const float fx = (world_xz.x - relief_world_of(x0)) / RELIEF_STEP_M;
    const float fz = (world_xz.y - relief_world_of(z0)) / RELIEF_STEP_M;
    const float w00 = path_wear_of(x0, z0);
    const float w10 = path_wear_of(x0 + 1, z0);
    const float w01 = path_wear_of(x0, z0 + 1);
    const float w11 = path_wear_of(x0 + 1, z0 + 1);
    const float a = w00 + (w10 - w00) * fx;
    const float b = w01 + (w11 - w01) * fx;
    return a + (b - a) * fz;
}

void ReliefLayer::set_path_wear(int32_t x, int32_t z, float wear) {
    const float w = std::clamp(wear, 0.0f, 1.0f);
    const uint64_t k = key_of(x, z);
    if (w <= 0.0f) {
        // ZERO ERASES, for the reason set_delta's zero does: an undone path
        // must leave the map it was drawn on, not a field of zeroes.
        wear_.erase(k);
        return;
    }
    wear_[k] = w;
}

bool ReliefLayer::path_wear_bounds_xz(glm::vec2& min_xz, glm::vec2& max_xz) const {
    if (wear_.empty()) {
        return false;
    }
    int32_t lo_x = 0, lo_z = 0, hi_x = 0, hi_z = 0;
    bool first = true;
    for (const auto& [key, w] : wear_) {
        (void)w;
        const auto x = static_cast<int32_t>(static_cast<uint32_t>(key >> 32));
        const auto z = static_cast<int32_t>(static_cast<uint32_t>(key));
        if (first) {
            lo_x = hi_x = x;
            lo_z = hi_z = z;
            first = false;
            continue;
        }
        lo_x = std::min(lo_x, x);
        hi_x = std::max(hi_x, x);
        lo_z = std::min(lo_z, z);
        hi_z = std::max(hi_z, z);
    }
    min_xz = {relief_world_of(lo_x) - RELIEF_STEP_M, relief_world_of(lo_z) - RELIEF_STEP_M};
    max_xz = {relief_world_of(hi_x) + RELIEF_STEP_M, relief_world_of(hi_z) + RELIEF_STEP_M};
    return true;
}

std::size_t ReliefLayer::add_path(const ReliefPath& path) {
    paths_.push_back(path);
    rebake_paths();
    return paths_.size() - 1;
}

void ReliefLayer::set_path(std::size_t index, const ReliefPath& path) {
    if (index >= paths_.size()) {
        return;
    }
    paths_[index] = path;
    rebake_paths();
}

void ReliefLayer::erase_path(std::size_t index) {
    if (index >= paths_.size()) {
        return;
    }
    paths_.erase(paths_.begin() + static_cast<std::ptrdiff_t>(index));
    rebake_paths();
}

void ReliefLayer::rebake_paths() {
    wear_.clear();
    for (const ReliefPath& path : paths_) {
        const std::vector<glm::vec2> poly =
            relief_path_polyline(path, 0.5f * RELIEF_STEP_M);
        if (poly.empty()) {
            continue;
        }
        const float half = std::max(path.half_width_m, PATH_MIN_HALF_WIDTH_M);
        // ONE LATTICE STEP OF SLACK on the box: the sample just outside the
        // rim still carries a zero that the bilinear filter needs in order to
        // fade — without it the edge would meet untouched ground with a step.
        const float reach = half + RELIEF_STEP_M;
        for (std::size_t i = 0; i + 1 < poly.size() || (poly.size() == 1 && i == 0); ++i) {
            const glm::vec2 a = poly[i];
            const glm::vec2 b = poly.size() == 1 ? poly[0] : poly[i + 1];
            const int32_t x_lo = relief_index_floor(std::min(a.x, b.x) - reach);
            const int32_t x_hi = relief_index_floor(std::max(a.x, b.x) + reach) + 1;
            const int32_t z_lo = relief_index_floor(std::min(a.y, b.y) - reach);
            const int32_t z_hi = relief_index_floor(std::max(a.y, b.y) + reach) + 1;
            for (int32_t z = z_lo; z <= z_hi; ++z) {
                for (int32_t x = x_lo; x <= x_hi; ++x) {
                    const glm::vec2 p{relief_world_of(x), relief_world_of(z)};
                    const float w = relief_path_wear(path, dist_to_segment(p, a, b));
                    if (w <= 0.0f) {
                        continue;
                    }
                    // MAX, NOT SUM. Two paths that cross are one worn crossing,
                    // and a sum would burn a bright square into it — which is
                    // what "the ground is worn" cannot mean twice over.
                    set_path_wear(x, z, std::max(w, path_wear_of(x, z)));
                }
            }
            if (poly.size() == 1) {
                break;
            }
        }
    }
}

bool read_relief(const std::filesystem::path& path, ReliefLayer& out,
                 std::string& error) {
    out.clear();
    std::ifstream f(path);
    if (!f) {
        // REFUSAL, NOT AN EMPTY LAYER. The caller opened this file because the
        // .scene named it; answering "no edits" would make a lost terrain edit
        // look like a map that moved on its own, and the composer would go
        // hunting for the defect inside the generator.
        error = "relief: не могу открыть " + path.string();
        return false;
    }
    std::string line;
    int lineno = 0;
    // WHICH PATH THE NEXT `pp` BELONGS TO. A point before any `path` line is an
    // ERROR and not a point on an invented path: a file that quietly grew a
    // path nobody declared is a map that changed on load.
    std::vector<ReliefPath> paths_read;
    ReliefPath current;
    bool in_path = false;
    const auto flush_path = [&]() {
        if (in_path) {
            paths_read.push_back(current);
        }
        in_path = false;
        current = ReliefPath{};
    };
    while (std::getline(f, line)) {
        ++lineno;
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream in(line);
        std::string tag;
        if (!(in >> tag)) {
            continue; // blank or comment-only
        }
        const auto fail = [&](const char* what) {
            error = "relief: строка " + std::to_string(lineno) + ": " + what;
            return false;
        };
        if (tag == "step") {
            float step = 0.0f;
            if (!(in >> step)) {
                return fail("шаг решётки не число");
            }
            // THE STEP IS DECLARED SO IT CAN BE REFUSED. A file written on a
            // 2 m lattice and read as if it were 1 m would place every edit at
            // half its distance from the origin — a map that quietly slid, and
            // one nobody would read as a format problem.
            if (std::fabs(step - RELIEF_STEP_M) > 1.0e-4f) {
                return fail("шаг решётки не совпадает с шагом карты высот");
            }
            continue;
        }
        if (tag == "dh") {
            int32_t x = 0;
            int32_t z = 0;
            float v = 0.0f;
            if (!(in >> x >> z >> v)) {
                return fail("dh ждёт «x z метры»");
            }
            out.set_delta(x, z, v);
            continue;
        }
        if (tag == "mat") {
            int32_t x = 0;
            int32_t z = 0;
            std::string name;
            if (!(in >> x >> z >> name)) {
                return fail("mat ждёт «x z класс»");
            }
            math::SurfaceClass c{};
            if (!surface_of_name(name, c)) {
                return fail("неизвестный класс поверхности");
            }
            out.set_surface(x, z, c);
            continue;
        }
        if (tag == "path") {
            float half = 0.0f;
            float soft = 0.0f;
            if (!(in >> half >> soft)) {
                return fail("path ждёт «полуширина мягкость»");
            }
            flush_path();
            current.half_width_m = half;
            current.edge_softness = soft;
            // Третье число НЕОБЯЗАТЕЛЬНО — материал полотна (0..3, порядок
            // путевого атласа). Старый файл из двух чисел читается как
            // раньше: укатанный грунт.
            current.path_class = 1;
            if (int cls = 0; in >> cls) {
                if (cls < 0 || cls > 3) {
                    return fail("класс полотна тропы вне 0..3");
                }
                current.path_class = cls;
            } else {
                in.clear();
            }
            in_path = true;
            continue;
        }
        if (tag == "pp") {
            float x = 0.0f;
            float z = 0.0f;
            if (!(in >> x >> z)) {
                return fail("pp ждёт «x z» в метрах");
            }
            if (!in_path) {
                return fail("точка тропы до строки path");
            }
            current.points.push_back({x, z});
            continue;
        }
        // Unknown tag: skipped, not fatal — the format will grow, and a reader
        // that died on a key it had not learned yet would make growing it a
        // flag day for every tool at once.
    }
    flush_path();
    for (const ReliefPath& p : paths_read) {
        out.add_path(p);
    }
    return true;
}

bool write_relief(const ReliefLayer& layer, const std::filesystem::path& path) {
    std::error_code ec;
    // EMPTY MEANS BOTH CHANNELS EMPTY. `empty()` speaks for the height edits
    // alone (its emptiness carries the bit-identity claim), so a map whose only
    // hand edit is a path would otherwise have its path deleted on save.
    if (layer.empty() && layer.paths().empty()) {
        // AN EDIT FULLY UNDONE LEAVES THE TREE AS IT WAS FOUND. Writing a file
        // with a header and no samples would leave a composer with an untracked
        // artefact of an experiment he abandoned.
        std::filesystem::remove(path, ec);
        return true;
    }

    std::vector<ReliefSample> rows = layer.samples();
    // SORTED HERE, BY z THEN x — a property of the WRITER and not of the
    // container. The file's whole worth as a record is that a diff names the
    // samples that moved; straight out of a hash map it would name a rehash.
    std::sort(rows.begin(), rows.end(), [](const ReliefSample& a, const ReliefSample& b) {
        return a.z != b.z ? a.z < b.z : a.x < b.x;
    });

    std::ostringstream out;
    out << "# Daggerfall N relief — авторская правка земли, нарисованная кистью.\n"
        << "# Координаты — ИНДЕКСЫ мировой решётки: мир = индекс * step.\n"
        << "# dh  <x> <z> <метры>  — сдвиг высоты в этом узле\n"
        << "# mat <x> <z> <класс>  — поверхность, назначенная композитором\n"
        << "# path <полуширина> <мягкость> — ТРОПА: её поперечник\n"
        << "# pp <x> <z>            — точка тропы, В МЕТРАХ (не индекс!)\n"
        << "#   Тропа записана ТОЧКАМИ, а не отсчётами: отсчёты износа — то, что\n"
        << "#   точки ЗНАЧАТ, и они пересчитываются при чтении. Две записи одного\n"
        << "#   решения разошлись бы, и тропа на экране перестала бы совпадать с\n"
        << "#   тропой в файле.\n"
        // `step 2` and not `step = 2`: every line in this file is space-
        // separated tokens, and an `=` on one of them was exactly the mismatch
        // the round-trip test caught — the writer emitted it, the reader read
        // the `=` as the number and refused its own output.
        << "step " << RELIEF_STEP_M << "\n";
    // FULL PRECISION ON THE DELTAS. A height written to six digits and read
    // back is a different world from the one that was painted, and the
    // difference shows up as a seam where an edit meets untouched ground.
    out.precision(9);
    for (const ReliefSample& s : rows) {
        if (s.height_delta != 0.0f) {
            out << "dh " << s.x << ' ' << s.z << ' ' << s.height_delta << "\n";
        }
        if (s.has_surface) {
            out << "mat " << s.x << ' ' << s.z << ' ' << surface_name(s.surface) << "\n";
        }
    }
    // THE CURVES, in the order they were drawn. Not sorted: unlike samples,
    // their order IS authored data — it is the order the composer's list shows
    // and the index his tool deletes by.
    for (const ReliefPath& path : layer.paths()) {
        out << "path " << path.half_width_m << ' ' << path.edge_softness;
        if (path.path_class != 1) {
            out << ' ' << path.path_class; // класс полотна; грунт — умолчание
        }
        out << "\n";
        for (const glm::vec2& pt : path.points) {
            out << "pp " << pt.x << ' ' << pt.y << "\n";
        }
    }

    const std::string text = out.str();
    // Atomic, for the reason write_scene is: a half-written edit is a map that
    // opens to ground nobody authored.
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            return false;
        }
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!f) {
            return false;
        }
    }
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

} // namespace dfn::world
