/*
Created: 17:08:2026 - 19:05:00
Last updated: 17:08:2026 - 19:05:00
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
/*
UPD:
- 17:08:2026 - 19:05:00: Создан вместе с ReliefLayer.h.
*/

#include "engine/world/sources/ReliefLayer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
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
        // Unknown tag: skipped, not fatal — the format will grow, and a reader
        // that died on a key it had not learned yet would make growing it a
        // flag day for every tool at once.
    }
    return true;
}

bool write_relief(const ReliefLayer& layer, const std::filesystem::path& path) {
    std::error_code ec;
    if (layer.empty()) {
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
