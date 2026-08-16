/*
Created: 16:08:2026 - 20:52:00
Last updated: 16:08:2026 - 20:52:00
Module: engine/render
File: engine/render/sources/PartForge.cpp

Responsibility:
- The building kit's geometry: every PartKind, cut to the size its params ask
  for, as closed volumes on the grid.

Dependencies:
- Uses: PartForge.h, ProcMesh.h (MeshData, tri/quad/pack).
- Used by: tools/forge_parts.cpp, tests/render/PartForgeTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ONE BUILDER, MANY PARTS. Nearly everything here is a hewn bar: a chamfered
  prism swept along an axis. A beam, a post, a plank, a stair's stringer and a
  wall's studs are that same bar at different sizes and angles. Adding a
  thirteenth part should mean a new arrangement of bars, not a new mesher.
- EVERY SIZE COMES FROM `_u` GRID UNITS, never from metres typed in place. The
  snapping promise in the header is only true while that holds: a part whose
  length is 1.37 m cannot be counted in by an agent placing on a 0.25 m grid.
- WEAR IS GEOMETRY, NOT A TINT. The reference frames are hand-hewn wood whose
  faces are not parallel and whose ends are split. Wear wobbles the section
  ring and jitters per-face colour; a part at wear 0 is a sawn plank and reads
  like one, which is why the kit ships both.
*/
/*
UPD:
- 16:08:2026 - 20:52:00: Создан вместе с PartForge.h.
*/

#include "engine/render/sources/PartForge.h"

#include "engine/render/sources/ProcMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <initializer_list>
#include <vector>

namespace dfn::render {
namespace {

constexpr float TAU = 6.28318530718f;

/// Deterministic, cheap, and NOT std::mt19937: the same params must give the
/// same bytes on every machine that ever rebuilds this kit (Rule 13.1's spirit
/// applied to objects).
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 6364136223846793005ull + 1442695040888963407ull) {}
    float unit() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<float>((s >> 40) & 0xFFFFFF) / 16777216.0f;
    }
    /// Symmetric around zero: the wobble a timber gets must be as likely to
    /// bend one way as the other, or a wall of studs leans.
    float sym(float amount) { return (unit() * 2.0f - 1.0f) * amount; }
};

[[nodiscard]] float m_of(int units) { return static_cast<float>(units) * BUILD_GRID_M; }

struct Material {
    glm::vec3 color;
    float jitter;   ///< per-face tone spread at wear = 1
    float chamfer;  ///< fraction of the half-section cut off the corners
    float wobble;   ///< how far a section ring wanders at wear = 1, metres
};

[[nodiscard]] Material material_of(PartMaterial m) {
    switch (m) {
    // Weathered oak: the reference's timber is grey first and brown second.
    case PartMaterial::Timber: return {{0.44f, 0.37f, 0.29f}, 0.16f, 0.22f, 0.012f};
    case PartMaterial::TimberDark: return {{0.25f, 0.21f, 0.18f}, 0.14f, 0.22f, 0.012f};
    // Infill plaster is flat and pale; it does not wander, it cracks.
    case PartMaterial::Plaster: return {{0.71f, 0.67f, 0.57f}, 0.07f, 0.05f, 0.003f};
    // Field stone: the strongest wobble in the kit, because a footing course
    // that is a smooth box is the one thing that always reads as programmer art.
    case PartMaterial::Stone: return {{0.45f, 0.45f, 0.43f}, 0.13f, 0.30f, 0.030f};
    case PartMaterial::Thatch: return {{0.66f, 0.56f, 0.33f}, 0.18f, 0.10f, 0.020f};
    case PartMaterial::Shingle: return {{0.33f, 0.30f, 0.27f}, 0.15f, 0.08f, 0.006f};
    }
    return {{0.5f, 0.5f, 0.5f}, 0.1f, 0.1f, 0.0f};
}

// The kit's own dimensions, in metres unless the name says `_U`. These are
// proportions of a part, not world tuning: they decide what a board looks like,
// and every one of them was set against the reference frames in
// images_examples/houses_outdoors.
constexpr float PLANK_THICK_M = 0.06f;   ///< a sawn board
constexpr float INFILL_THICK_M = 0.08f;  ///< what fills a wall bay
constexpr float BOARD_W_M = 0.30f;       ///< one cladding board's width
constexpr float BOARD_GAP_M = 0.02f;     ///< the shadow line between two
constexpr float LEAF_THICK_M = 0.07f;    ///< a door leaf
constexpr float THATCH_DEEP_M = 0.35f;   ///< straw is DEEP; that is its whole read
constexpr float SHINGLE_DEEP_M = 0.10f;
constexpr float COURSE_M = 0.60f;        ///< one roof course up the slope
constexpr float FRINGE_M = 0.45f;        ///< how far the eaves fringe hangs
constexpr float STONE_W_M = 0.55f;       ///< a field stone in a footing course
constexpr float TREAD_M = 0.08f;
constexpr float NOSING_M = 0.04f;        ///< tread overhang; the step's shadow
/// A stair's rise and going, IN GRID UNITS: 0.25 m up per 0.50 m along, which
/// is 26.5 degrees. Both are integers on purpose — a flight of N steps then
/// lands exactly N units up and 2N along, so the deck it reaches can be
/// counted to rather than measured.
constexpr int STAIR_RISE_U = 1;
constexpr int STAIR_GOING_U = 2;

[[nodiscard]] uint32_t tone(const Material& mat, float wear, Rng& rng) {
    // Wear darkens (weather greys wood down, it never brightens it) and widens
    // the spread, so a worn wall's boards differ from each other and a new
    // one's do not.
    const float dark = 1.0f - 0.22f * wear;
    const float j = 1.0f + rng.sym(mat.jitter * (0.35f + 0.65f * wear));
    glm::vec3 c = mat.color * dark * j;
    c = glm::clamp(c, glm::vec3{0.02f}, glm::vec3{1.0f});
    return pack(c);
}

[[nodiscard]] glm::vec3 perp_of(const glm::vec3& axis) {
    const glm::vec3 ref = std::fabs(axis.y) > 0.9f ? glm::vec3{1.0f, 0.0f, 0.0f}
                                                   : glm::vec3{0.0f, 1.0f, 0.0f};
    return glm::normalize(glm::cross(ref, axis));
}

/// THE ONE BUILDER. A closed chamfered prism of half-width `hw` (across `side`)
/// and half-height `hh` (across `up`), swept `len` metres along `along` from
/// `origin`, which sits at the CENTRE of the starting face. Split into
/// `segments` so wear can bend it; capped at both ends so it is a volume and
/// not a tube (Rule 52).
void hewn_bar(MeshData& m, glm::vec3 origin, glm::vec3 along, glm::vec3 up, float len,
              float hw, float hh, const Material& mat, float wear, Rng& rng,
              int segments = 2, float taper = 1.0f) {
    along = glm::normalize(along);
    glm::vec3 side = glm::cross(up, along);
    if (glm::length(side) < 1e-4f) {
        side = perp_of(along);
    }
    side = glm::normalize(side);
    up = glm::normalize(glm::cross(along, side));

    const float c = mat.chamfer;
    // Section as (u, v) fractions of (hw, hh). Eight points when chamfered,
    // four when not: a zero chamfer through the eight-point path would emit
    // four degenerate quads whose normals are undefined.
    std::vector<glm::vec2> sec;
    if (c > 0.01f) {
        sec = {{-1.0f + c, -1.0f}, {1.0f - c, -1.0f}, {1.0f, -1.0f + c}, {1.0f, 1.0f - c},
               {1.0f - c, 1.0f},   {-1.0f + c, 1.0f}, {-1.0f, 1.0f - c}, {-1.0f, -1.0f + c}};
    } else {
        sec = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    }
    const int n = static_cast<int>(sec.size());
    segments = std::max(1, segments);

    const float wob = mat.wobble * wear;
    std::vector<std::vector<glm::vec3>> rings(static_cast<std::size_t>(segments) + 1);
    std::vector<glm::vec3> centres(static_cast<std::size_t>(segments) + 1);
    for (int s = 0; s <= segments; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(segments);
        // Ends stay put: two bars butted end to end must still meet, so the
        // wobble is zero at t=0 and t=1 and largest in the middle.
        const float bend = std::sin(t * TAU * 0.5f);
        const glm::vec3 centre = origin + along * (len * t)
                               + side * (rng.sym(wob) * bend)
                               + up * (rng.sym(wob) * bend);
        centres[static_cast<std::size_t>(s)] = centre;
        const float scale = 1.0f + (taper - 1.0f) * t;
        auto& ring = rings[static_cast<std::size_t>(s)];
        ring.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            ring[static_cast<std::size_t>(i)] =
                centre + side * (sec[static_cast<std::size_t>(i)].x * hw * scale)
                       + up * (sec[static_cast<std::size_t>(i)].y * hh * scale);
        }
    }

    for (int s = 0; s < segments; ++s) {
        const auto& p = rings[static_cast<std::size_t>(s)];
        const auto& q = rings[static_cast<std::size_t>(s) + 1];
        for (int i = 0; i < n; ++i) {
            const int k = (i + 1) % n;
            quad(m, p[static_cast<std::size_t>(i)], p[static_cast<std::size_t>(k)],
                 q[static_cast<std::size_t>(k)], q[static_cast<std::size_t>(i)],
                 tone(mat, wear, rng));
        }
    }
    const auto& first = rings.front();
    const auto& last = rings.back();
    const uint32_t cap = tone(mat, wear, rng);
    for (int i = 0; i < n; ++i) {
        const int k = (i + 1) % n;
        tri(m, centres.front(), first[static_cast<std::size_t>(k)],
            first[static_cast<std::size_t>(i)], cap);
        tri(m, centres.back(), last[static_cast<std::size_t>(i)],
            last[static_cast<std::size_t>(k)], cap);
    }
}

/// An axis-aligned block from its min corner. Just the bar with no chamfer and
/// no bend, named for what a composer thinks he is placing.
void block(MeshData& m, glm::vec3 min, glm::vec3 size, const Material& mat, float wear,
           Rng& rng, int segments = 1) {
    hewn_bar(m, min + glm::vec3{size.x * 0.5f, size.y * 0.5f, 0.0f},
             {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, size.z, size.x * 0.5f, size.y * 0.5f,
             mat, wear, rng, segments);
}

// ---------------------------------------------------------------------------
// The parts. Each writes into `wood` (drawn with the prop program) and returns
// with its ORIGIN at the joint a composer places by: a beam's near end, a
// post's foot, a stair's bottom step, a roof's eaves.
// ---------------------------------------------------------------------------

void make_beam(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float len = m_of(p.length_u);
    hewn_bar(m, {0.0f, m_of(p.height_u) * 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f},
             {0.0f, 1.0f, 0.0f}, len, m_of(p.width_u) * 0.5f, m_of(p.height_u) * 0.5f,
             mat, p.wear, rng, std::max(2, p.length_u / 3));
}

void make_post(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    // Posts taper: every standing timber in the reference is a trunk, thicker
    // at the butt. A post with parallel sides is the thing that reads as CAD.
    const float taper = 1.0f - 0.10f * p.wear;
    hewn_bar(m, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
             m_of(p.length_u), m_of(p.width_u) * 0.5f, m_of(p.height_u) * 0.5f, mat,
             p.wear, rng, std::max(2, p.length_u / 3), taper);
}

void make_plank(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    Material sawn = mat;
    sawn.chamfer = 0.0f; // a board has square arrises; that is what makes it a board
    // Origin at the board's UNDERSIDE, not its middle: every part in this kit
    // is placed by the face it rests on, or a composer would have to know each
    // part's thickness to stack anything.
    hewn_bar(m, {0.0f, PLANK_THICK_M * 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f},
             {0.0f, 1.0f, 0.0f}, m_of(p.length_u), m_of(p.width_u) * 0.5f,
             PLANK_THICK_M * 0.5f, sawn, p.wear, rng, std::max(2, p.length_u / 4));
}

/// A wall bay: sill, head, two studs, and the infill between them. Timber
/// infill is BOARDS (each its own tone, each its own tiny gap) because a wall
/// drawn as one slab is the "cartoon" failure the tree work already paid for.
void make_wall(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const Material frame = material_of(PartMaterial::Timber);
    const float fs = std::min(m_of(2), w * 0.25f); // frame member size

    // Frame first, so a wall reads as timber-frame even in silhouette.
    hewn_bar(m, {fs * 0.5f, fs * 0.5f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
             h - fs, fs * 0.5f, t * 0.5f, frame, p.wear, rng, 3);
    hewn_bar(m, {w - fs * 0.5f, fs * 0.5f, t * 0.5f}, {0.0f, 1.0f, 0.0f},
             {0.0f, 0.0f, 1.0f}, h - fs, fs * 0.5f, t * 0.5f, frame, p.wear, rng, 3);
    hewn_bar(m, {0.0f, fs * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
             t * 0.5f, fs * 0.5f, frame, p.wear, rng, 3);
    hewn_bar(m, {0.0f, h - fs * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w, t * 0.5f, fs * 0.5f, frame, p.wear, rng, 3);

    const float in_w = w - 2.0f * fs;
    const float in_h = h - 2.0f * fs;
    if (in_w <= 0.0f || in_h <= 0.0f) {
        return;
    }
    // Solid infill or boarded, decided by the MATERIAL and not by comparing
    // colours: two materials may one day share a tone, and a wall that quietly
    // changed construction because of that would be very hard to explain.
    if (p.material == PartMaterial::Plaster || p.material == PartMaterial::Stone) {
        block(m, {fs, fs, t * 0.5f - INFILL_THICK_M * 0.5f},
              {in_w, in_h, INFILL_THICK_M}, mat, p.wear, rng, 2);
        return;
    }
    const int boards = std::max(2, static_cast<int>(in_w / BOARD_W_M + 0.5f));
    const float bw = in_w / static_cast<float>(boards);
    for (int i = 0; i < boards; ++i) {
        const float x = fs + static_cast<float>(i) * bw;
        const float gap = BOARD_GAP_M * (0.3f + 0.7f * p.wear);
        hewn_bar(m, {x + bw * 0.5f, fs, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                 in_h, (bw - gap) * 0.5f, INFILL_THICK_M * 0.5f, mat, p.wear, rng, 2);
    }
}

/// The triangular end wall. A closed prism (both faces, three sides), boarded
/// vertically like the reference's gables, with a collar beam across it.
void make_gable(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float rise = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const int boards = std::max(3, static_cast<int>(w / BOARD_W_M + 0.5f));
    const float bw = w / static_cast<float>(boards);
    for (int i = 0; i < boards; ++i) {
        const float x0 = static_cast<float>(i) * bw;
        const float xc = x0 + bw * 0.5f;
        // Each board is cut to the roof line it meets — the pitch is the part.
        const float frac = 1.0f - std::fabs(xc - w * 0.5f) / (w * 0.5f);
        const float bh = std::max(rise * frac, BUILD_GRID_M);
        hewn_bar(m, {xc, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, bh,
                 (bw - BOARD_GAP_M) * 0.5f, t * 0.5f, mat, p.wear, rng, 2);
    }
    const Material frame = material_of(PartMaterial::Timber);
    const float fs = m_of(1);
    // Collar beam at a third of the rise: the horizontal timber that makes a
    // Nordic gable read as a gable and not as a pile of boards.
    hewn_bar(m, {0.0f, rise * 0.33f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w * 0.66f + w * 0.17f, t * 0.6f, fs * 0.5f, frame, p.wear, rng, 3);
    // The two barge boards along the rake, which is where the carved dragon
    // heads in the reference frames live.
    for (int s = 0; s < 2; ++s) {
        const glm::vec3 foot{s == 0 ? 0.0f : w, 0.0f, t * 0.5f};
        const glm::vec3 ridge{w * 0.5f, rise, t * 0.5f};
        const glm::vec3 along = ridge - foot;
        hewn_bar(m, foot, along, {0.0f, 0.0f, 1.0f}, glm::length(along), fs * 0.5f,
                 t * 0.55f, frame, p.wear, rng, 3);
    }
}

/// One pitched plane, origin at the EAVES corner. The pitch is baked in
/// (rise over run) because a Placement carries yaw and nothing else — a roof
/// the composer had to tilt would be a roof nobody could place.
void make_roof(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float run = m_of(p.length_u);
    const float rise = m_of(p.height_u);
    const float depth = m_of(p.width_u); // along the ridge
    const glm::vec3 slope = glm::normalize(glm::vec3{run, rise, 0.0f});
    const float slope_len = std::sqrt(run * run + rise * rise);
    const bool thatch = p.material == PartMaterial::Thatch;
    const float deck = thatch ? THATCH_DEEP_M : SHINGLE_DEEP_M;

    // The deck: courses across the slope, each its own tone, so the roof has
    // the banded read every reference thatch has.
    const int courses = std::max(3, static_cast<int>(slope_len / COURSE_M + 0.5f));
    const float cl = slope_len / static_cast<float>(courses);
    const glm::vec3 up = glm::normalize(glm::cross(slope, glm::vec3{0.0f, 0.0f, 1.0f}));
    for (int i = 0; i < courses; ++i) {
        const glm::vec3 at = slope * (static_cast<float>(i) * cl + cl * 0.5f);
        // Courses overlap downslope, thatch more than shingle: the shadow line
        // under each course is what stops a roof looking painted on.
        const float over = thatch ? 1.55f : 1.25f;
        hewn_bar(m, at + up * (deck * 0.5f) - glm::vec3{0.0f, 0.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}, up, depth, cl * over * 0.5f, deck * 0.5f, mat,
                 p.wear, rng, 2);
    }
    // The eaves fringe: the thick shaggy edge in the user's frames, where the
    // thatch hangs a hand's width past the wall.
    if (thatch) {
        const int straws = std::max(4, static_cast<int>(depth / m_of(1) + 0.5f));
        for (int i = 0; i < straws; ++i) {
            const float z = (static_cast<float>(i) + 0.5f) * depth / static_cast<float>(straws);
            hewn_bar(m, {0.0f, deck * 0.5f, z}, {-0.30f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                     FRINGE_M, depth / static_cast<float>(straws) * 0.45f, deck * 0.35f,
                     mat, p.wear, rng, 2, 0.55f);
        }
    }
    // The ridge beam and the rafter under the eaves: the timber a roof rests on.
    const Material frame = material_of(PartMaterial::Timber);
    hewn_bar(m, {run, rise, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, depth,
             m_of(1) * 0.6f, m_of(1) * 0.6f, frame, p.wear, rng, 3);
    hewn_bar(m, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, depth,
             m_of(1) * 0.5f, m_of(1) * 0.5f, frame, p.wear, rng, 3);
}

/// A flight, origin at the foot of the lowest riser. Rise is one grid unit and
/// going is two — 26.5 degrees, which is both climbable and grid-true, so a
/// stair of N steps lands EXACTLY N units up and 2N units along.
void make_stair(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const int steps = std::max(1, p.height_u);
    const float w = m_of(p.width_u);
    const float rise = m_of(STAIR_RISE_U);
    const float going = m_of(STAIR_GOING_U);
    for (int i = 0; i < steps; ++i) {
        const float y = static_cast<float>(i) * rise;
        const float x = static_cast<float>(i) * going;
        // Tread overhangs its riser: the nosing shadow is how a stair reads as
        // steps rather than as a ramp with lines on it.
        hewn_bar(m, {x - NOSING_M, y + rise - TREAD_M * 0.5f, w * 0.5f},
                 {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, going + NOSING_M, w * 0.5f,
                 TREAD_M * 0.5f, mat, p.wear, rng, 2);
        // The riser box under it, so the flight is solid and not floating slabs.
        block(m, {x, y, 0.0f}, {going, rise - TREAD_M, w}, mat, p.wear * 0.6f, rng);
    }
    // Two stringers along the flight's diagonal.
    const glm::vec3 along = glm::normalize(
        glm::vec3{going * static_cast<float>(steps), rise * static_cast<float>(steps), 0.0f});
    const float slen = std::sqrt(std::pow(going * static_cast<float>(steps), 2.0f)
                                 + std::pow(rise * static_cast<float>(steps), 2.0f));
    const Material frame = material_of(PartMaterial::Timber);
    for (int s = 0; s < 2; ++s) {
        const float z = s == 0 ? m_of(1) * 0.5f : w - m_of(1) * 0.5f;
        hewn_bar(m, {0.0f, 0.0f, z}, along, {0.0f, 0.0f, 1.0f}, slen, m_of(1) * 0.5f,
                 m_of(1) * 0.5f, frame, p.wear, rng, std::max(2, steps));
    }
}

/// Jambs, lintel and threshold around an opening `length_u` wide and
/// `height_u` tall. The opening is EMPTY: the leaf is its own part, because a
/// door that cannot be left open is scenery.
void make_door_frame(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const float j = m_of(1);
    hewn_bar(m, {-j * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h,
             j * 0.5f, t * 0.5f, mat, p.wear, rng, 3);
    hewn_bar(m, {w + j * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
             h, j * 0.5f, t * 0.5f, mat, p.wear, rng, 3);
    hewn_bar(m, {-j, h + j * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w + 2.0f * j, t * 0.5f, j * 0.5f, mat, p.wear, rng, 3);
    hewn_bar(m, {-j, -j * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
             w + 2.0f * j, t * 0.5f, j * 0.5f, mat, p.wear, rng, 2);
}

/// Vertical boards on two ledgers, plus a diagonal brace: the plank door of
/// every frame the user supplied.
void make_door_leaf(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const int boards = std::max(2, static_cast<int>(w / BOARD_W_M + 0.5f));
    const float bw = w / static_cast<float>(boards);
    for (int i = 0; i < boards; ++i) {
        hewn_bar(m, {(static_cast<float>(i) + 0.5f) * bw, 0.0f, LEAF_THICK_M * 0.5f},
                 {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h, (bw - BOARD_GAP_M) * 0.5f,
                 LEAF_THICK_M * 0.5f, mat, p.wear, rng, 2);
    }
    const Material iron = material_of(PartMaterial::TimberDark);
    for (int s = 0; s < 2; ++s) {
        const float y = s == 0 ? h * 0.18f : h * 0.80f;
        hewn_bar(m, {0.0f, y, LEAF_THICK_M}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
                 LEAF_THICK_M * 0.6f, m_of(1) * 0.35f, iron, p.wear, rng, 2);
    }
    const glm::vec3 a{0.0f, h * 0.18f, LEAF_THICK_M};
    const glm::vec3 b{w, h * 0.80f, LEAF_THICK_M};
    hewn_bar(m, a, b - a, {0.0f, 0.0f, 1.0f}, glm::length(b - a), LEAF_THICK_M * 0.6f,
             m_of(1) * 0.30f, iron, p.wear, rng, 3);
}

/// Frame plus a cross of mullions — the small many-paned window in the
/// reference's gable.
void make_window(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float w = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float t = m_of(p.width_u);
    const float j = m_of(1) * 0.7f;
    hewn_bar(m, {0.0f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h,
             j * 0.5f, t * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {w, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h, j * 0.5f,
             t * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {0.0f, 0.0f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
             t * 0.5f, j * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {0.0f, h, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w, t * 0.5f,
             j * 0.5f, mat, p.wear, rng, 2);
    hewn_bar(m, {w * 0.5f, 0.0f, t * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h,
             j * 0.3f, t * 0.4f, mat, p.wear, rng, 2);
    hewn_bar(m, {0.0f, h * 0.5f, t * 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, w,
             t * 0.4f, j * 0.3f, mat, p.wear, rng, 2);
}

/// A course of field stones, origin at its near-bottom-left. Individual
/// stones, not one long block: the reference's dry-stone bases are read
/// entirely by the shadows between stones.
void make_footing(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float len = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float d = m_of(p.width_u);
    const int rows = std::max(1, p.height_u / 2);
    const float rh = h / static_cast<float>(rows);
    for (int r = 0; r < rows; ++r) {
        // Every course is offset half a stone, the way a wall is actually laid.
        const float offset = (r % 2 == 0) ? 0.0f : -STONE_W_M * 0.5f;
        const int stones = std::max(1, static_cast<int>((len - offset) / STONE_W_M + 0.5f));
        const float sw = (len - offset) / static_cast<float>(stones);
        for (int i = 0; i < stones; ++i) {
            const float x = offset + static_cast<float>(i) * sw;
            const float x0 = std::max(x, 0.0f);
            const float x1 = std::min(x + sw, len);
            if (x1 - x0 < 0.02f) {
                continue;
            }
            // Same rule as the plank: the course's origin is its BED, so the
            // first course starts at y = 0 and the wall on top starts at h.
            hewn_bar(m, {(x0 + x1) * 0.5f, (static_cast<float>(r) + 0.5f) * rh, 0.0f},
                     {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, d, (x1 - x0) * 0.47f,
                     rh * 0.47f, mat, p.wear, rng, 2);
        }
    }
}

void make_fence(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float len = m_of(p.length_u);
    const float h = m_of(p.height_u);
    const float ps = m_of(1) * 0.8f;
    for (int s = 0; s < 2; ++s) {
        const float x = s == 0 ? 0.0f : len;
        hewn_bar(m, {x, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h, ps * 0.5f,
                 ps * 0.5f, mat, p.wear, rng, 3, 0.85f);
    }
    const int rails = std::max(2, p.height_u / 2);
    for (int r = 0; r < rails; ++r) {
        const float y = h * (static_cast<float>(r) + 1.0f) / (static_cast<float>(rails) + 1.0f);
        hewn_bar(m, {0.0f, y, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, len,
                 ps * 0.35f, ps * 0.45f, mat, p.wear, rng, 3);
    }
}

[[nodiscard]] const char* kind_name(PartKind k) {
    switch (k) {
    case PartKind::Beam: return "beam";
    case PartKind::Post: return "post";
    case PartKind::Plank: return "plank";
    case PartKind::WallPanel: return "wall";
    case PartKind::Gable: return "gable";
    case PartKind::RoofSlope: return "roof";
    case PartKind::Stair: return "stair";
    case PartKind::DoorFrame: return "doorframe";
    case PartKind::DoorLeaf: return "door";
    case PartKind::WindowFrame: return "window";
    case PartKind::Footing: return "footing";
    case PartKind::Fence: return "fence";
    }
    return "part";
}

[[nodiscard]] const char* material_name(PartMaterial m) {
    switch (m) {
    case PartMaterial::Timber: return "timber";
    case PartMaterial::TimberDark: return "dark";
    case PartMaterial::Plaster: return "plaster";
    case PartMaterial::Stone: return "stone";
    case PartMaterial::Thatch: return "thatch";
    case PartMaterial::Shingle: return "shingle";
    }
    return "mat";
}

} // namespace

std::string part_name(const PartParams& p) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s-%s-%dx%dx%d-w%02d", kind_name(p.kind),
                  material_name(p.material), p.length_u, p.width_u, p.height_u,
                  static_cast<int>(p.wear * 10.0f + 0.5f));
    return buf;
}

RegistryObject forge_part(const PartParams& params) {
    RegistryObject obj;
    obj.name = params.name.empty() ? part_name(params) : params.name;
    obj.kind = "part";
    {
        char src[192];
        std::snprintf(src, sizeof(src), "kit:%s %s %dx%dx%du wear=%.2f seed=%llu",
                      kind_name(params.kind), material_name(params.material),
                      params.length_u, params.width_u, params.height_u,
                      static_cast<double>(params.wear),
                      static_cast<unsigned long long>(params.seed));
        obj.source = src;
    }
    const Material mat = material_of(params.material);
    Rng rng(params.seed);
    switch (params.kind) {
    case PartKind::Beam: make_beam(obj.wood, params, mat, rng); break;
    case PartKind::Post: make_post(obj.wood, params, mat, rng); break;
    case PartKind::Plank: make_plank(obj.wood, params, mat, rng); break;
    case PartKind::WallPanel: make_wall(obj.wood, params, mat, rng); break;
    case PartKind::Gable: make_gable(obj.wood, params, mat, rng); break;
    case PartKind::RoofSlope: make_roof(obj.wood, params, mat, rng); break;
    case PartKind::Stair: make_stair(obj.wood, params, mat, rng); break;
    case PartKind::DoorFrame: make_door_frame(obj.wood, params, mat, rng); break;
    case PartKind::DoorLeaf: make_door_leaf(obj.wood, params, mat, rng); break;
    case PartKind::WindowFrame: make_window(obj.wood, params, mat, rng); break;
    case PartKind::Footing: make_footing(obj.wood, params, mat, rng); break;
    case PartKind::Fence: make_fence(obj.wood, params, mat, rng); break;
    }
    return obj;
}

std::vector<PartParams> kit_catalogue() {
    std::vector<PartParams> out;
    // One row per FAMILY, expanded as length x SECTION x material x wear. The
    // section is a PAIR and not two independent lists on purpose: real timber
    // comes in a handful of sizes (square post, laid-flat beam, batten), and a
    // free cross product would fill the kit with 200 near-identical sticks
    // nobody would ever choose between while the useful pieces stayed rare.
    struct Sec {
        int w;
        int h;
    };
    const auto add = [&out](PartKind kind, std::initializer_list<int> lengths,
                            std::initializer_list<Sec> sections,
                            std::initializer_list<PartMaterial> mats,
                            std::initializer_list<float> wears) {
        for (int L : lengths) {
            for (Sec s : sections) {
                for (PartMaterial m : mats) {
                    for (float w : wears) {
                        PartParams p;
                        p.kind = kind;
                        p.material = m;
                        p.length_u = L;
                        p.width_u = s.w;
                        p.height_u = s.h;
                        p.wear = w;
                        p.name = part_name(p);
                        // The seed is the NAME, so a part's wobble is its own
                        // and stays its own when the catalogue grows.
                        uint64_t h = 1469598103934665603ull;
                        for (unsigned char c : p.name) {
                            h = (h ^ c) * 1099511628211ull;
                        }
                        p.seed = h;
                        out.push_back(std::move(p));
                    }
                }
            }
        }
    };
    using PM = PartMaterial;
    const std::initializer_list<PM> woods = {PM::Timber, PM::TimberDark};
    const std::initializer_list<float> wear2 = {0.3f, 0.8f};

    // STICKS — the user's word, and the kit's spine: nearly everything else is
    // these at an angle. Sections in grid units: 1x1 batten, 2x1 laid flat,
    // 2x2 beam, 3x3 main post (25/50/75 cm).
    add(PartKind::Beam, {2, 4, 6, 8, 12, 16}, {{1, 1}, {2, 1}, {2, 2}, {3, 3}}, woods, wear2);
    add(PartKind::Post, {4, 6, 8, 10, 12, 16}, {{1, 1}, {2, 2}, {3, 3}}, woods, wear2);
    add(PartKind::Plank, {4, 8, 12, 16}, {{1, 1}, {2, 1}},
        {PM::Timber, PM::TimberDark, PM::Stone}, wear2);

    // ENCLOSURE. For a wall the section pair means (thickness, HEIGHT); for a
    // gable and a roof it means (thickness/depth, RISE) — the part's second
    // dimension is whatever that part is actually measured by.
    add(PartKind::WallPanel, {8, 12, 16}, {{1, 8}, {1, 10}, {1, 12}},
        {PM::Timber, PM::TimberDark, PM::Plaster, PM::Stone}, wear2);
    add(PartKind::Gable, {8, 12, 16}, {{1, 4}, {1, 6}, {1, 8}}, {PM::Timber, PM::Plaster},
        wear2);
    add(PartKind::RoofSlope, {8, 12, 16}, {{8, 6}, {8, 8}, {12, 8}, {12, 12}},
        {PM::Thatch, PM::Shingle}, wear2);

    // GETTING IN, GETTING UP. A stair's length is unused (its run follows from
    // the step count), so it stays 1 and the pair carries (width, STEPS).
    // Two steps is in the list because a footing course is half a metre tall
    // and a three-step flight overshoots it — the kit has to be able to reach
    // the heights the kit's own parts make.
    add(PartKind::Stair, {1},
        {{4, 2}, {4, 3}, {4, 5}, {4, 7}, {6, 2}, {6, 5}, {6, 7}, {6, 9}, {8, 7}},
        {PM::Timber, PM::Stone}, wear2);
    add(PartKind::DoorFrame, {4, 6}, {{1, 8}, {1, 10}, {2, 10}}, {PM::Timber, PM::Stone},
        wear2);
    add(PartKind::DoorLeaf, {3, 5}, {{1, 7}, {1, 9}}, woods, wear2);
    add(PartKind::WindowFrame, {3, 4, 6}, {{1, 3}, {1, 4}, {1, 6}}, woods, {0.5f});

    // GROUND AND YARD.
    add(PartKind::Footing, {4, 8, 12}, {{2, 2}, {2, 4}, {4, 4}}, {PM::Stone}, wear2);
    add(PartKind::Fence, {6, 8, 12}, {{1, 4}, {1, 6}}, woods, wear2);
    return out;
}

} // namespace dfn::render
