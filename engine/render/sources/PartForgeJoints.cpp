/*
Created: 17:08:2026 - 13:02:08
Last updated: 17:08:2026 - 13:02:08
Module: engine/render
File: engine/render/sources/PartForgeJoints.cpp

Responsibility:
- The CONNECTOR family's geometry (HOUSES.md §3-4): joint posts (the hinge
  every panel ends at), sleepers (floor-to-wall bedding logs) and log-corner
  ties. Moved VERBATIM out of PartForge.cpp the day it hit 999 lines against
  the 800 hard limit (Rule 21); only the namespace wrapper changed.

Key items:
- ngon_prism(): the closed faceted prism both joints build on.
- part_detail::make_joint / make_sleeper / make_log_corner.

Dependencies:
- Uses: PartForgeDetail.h (material table, helpers), HewnBar.h.
- Used by: PartForge.cpp (forge_part dispatch).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO WOBBLE, NO TAPER on a joint's shaft — the facet is a PLANE a panel
  seats flush on, and that is the family's guarantee, not a style choice.
- Facet orientation is a CONTRACT with the judge (engine/world/Scene.cpp):
  at yaw 0 the first facet's outward normal points +X. Move both or neither.
*/
/*
UPD:
- 17:08:2026 - 13:02:08: Вынос из PartForge.cpp дословно (разрез по семьям);
  контроль — перепечка набора не изменила ни одного .dfo.
*/

#include "engine/render/sources/PartForgeDetail.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dfn::render::part_detail {
namespace {

/// Facet count of the round joint's mesh. Not a facet count in the RULE's
/// sense (a round joint accepts ANY angle); 24 is where a 1 m drum stops
/// reading as faceted at the 2-4 m a street camera stands from it.
constexpr int ROUND_JOINT_SEGMENTS = 24;
/// A joint's tone bands per metre of height: masonry reads by its courses,
/// timber by slow colour drift; one band per ~0.35 m serves both.
constexpr float JOINT_BAND_M = 0.35f;

/// One closed upright N-gon prism. `facets` 4/6/8 seat panels flush (across-
/// flats size `d_m`); 0 = round (true diameter `d_m`). NO taper and NO wobble
/// on purpose, and that is a guarantee rather than laziness: a panel seats
/// FLUSH on the facet plane, so the facet must BE a plane the whole way up —
/// the wobble every other kit part wears would turn the flush seat back into
/// the hairline gap this family exists to kill.
///
/// Facet orientation contract (the judge in engine/world/Scene.cpp measures
/// against it): at yaw = 0 one facet's outward normal points +X, the rest
/// every 360/N degrees from it. Vertex k therefore sits at angle
/// (k + 0.5) * 2pi/N, measured from +X toward -Z (the same sense yaw turns).
void ngon_prism(MeshData& m, glm::vec3 base, float height, int facets, float d_m,
                const Material& mat, float wear, Rng& rng) {
    constexpr float PI = 3.14159265359f;
    const int n = facets > 0 ? facets : ROUND_JOINT_SEGMENTS;
    const float r_in = d_m * 0.5f;
    const float R = facets > 0 ? r_in / std::cos(PI / static_cast<float>(n)) : r_in;
    const int bands = std::max(2, static_cast<int>(height / JOINT_BAND_M + 0.5f));
    std::vector<std::vector<glm::vec3>> rings(static_cast<std::size_t>(bands) + 1);
    for (int s = 0; s <= bands; ++s) {
        const float y = base.y + height * static_cast<float>(s) / static_cast<float>(bands);
        auto& ring = rings[static_cast<std::size_t>(s)];
        ring.resize(static_cast<std::size_t>(n));
        for (int k = 0; k < n; ++k) {
            const float ang = (static_cast<float>(k) + 0.5f) * 2.0f * PI
                            / static_cast<float>(n);
            ring[static_cast<std::size_t>(k)] =
                glm::vec3{base.x + R * std::cos(ang), y, base.z - R * std::sin(ang)};
        }
    }
    for (int s = 0; s < bands; ++s) {
        const auto& p = rings[static_cast<std::size_t>(s)];
        const auto& q = rings[static_cast<std::size_t>(s) + 1];
        // Masonry reads by its courses: one tone per band, then a small
        // per-face jitter inside it.
        const uint32_t band_tone = tone(mat, wear, rng);
        for (int k = 0; k < n; ++k) {
            const int j = (k + 1) % n;
            const uint32_t c = (k % 3 == 0) ? tone(mat, wear, rng) : band_tone;
            quad(m, p[static_cast<std::size_t>(k)], p[static_cast<std::size_t>(j)],
                 q[static_cast<std::size_t>(j)], q[static_cast<std::size_t>(k)], c);
        }
    }
    // Caps wound AGAINST the ring order: the fan's boundary edge must run
    // opposite to the side wall's, or every rim half-edge is emitted twice
    // the same way and the hull leaks along both rims (the joint tests'
    // half-edge meter caught exactly that on the first bake).
    const uint32_t cap = tone(mat, wear, rng);
    const glm::vec3 c0{base.x, base.y, base.z};
    const glm::vec3 c1{base.x, base.y + height, base.z};
    const auto& first = rings.front();
    const auto& last = rings.back();
    for (int k = 0; k < n; ++k) {
        const int j = (k + 1) % n;
        tri(m, c0, first[static_cast<std::size_t>(j)], first[static_cast<std::size_t>(k)],
            cap);
        tri(m, c1, last[static_cast<std::size_t>(k)], last[static_cast<std::size_t>(j)],
            cap);
    }
}

/// Один венец сруба по вертикали, in metres. 0.23 = the log wall assembly's
/// own course step (assets/scenes/panels/wall-log-16u.scene: венцы шагом 0.23,
/// нахлёст 0.02 больше макс. прогиба w03) — the corner MUST share the panel's
/// rhythm or the stubs miss the courses they claim to bind.
constexpr float LOG_COURSE_M = 0.23f;
/// How far a corner stub runs past the corner axis (выпуск «в обло»), and how
/// far it reaches back along its wall. Reach 0.50 m: two courses of overlap
/// with the panel's own logs, enough to read as ONE tied wall.
constexpr float LOG_STUB_OUT_M = 0.30f;
constexpr float LOG_STUB_BACK_M = 0.50f;

} // namespace

/// СТОЙКА-ШАРНИР. Origin at the FOOT, axis THROUGH the origin: panels are
/// measured centre-of-post to centre-of-post (§3.2), so the composer places
/// the axis and counts from it. variant 1 = с капителью (two widening drums
/// at the head — the stone colonnade's read).
void make_joint(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float d = static_cast<float>(p.diameter_cm) * 0.01f;
    const float h = m_of(p.length_u);
    ngon_prism(m, {0.0f, 0.0f, 0.0f}, h, p.facets, d, mat, p.wear, rng);
    if (p.variant == 1) {
        // The capital: echinus then abacus, same facet family as the shaft so
        // the head still tells the truth about the angles the joint accepts.
        ngon_prism(m, {0.0f, h - 0.22f, 0.0f}, 0.12f, p.facets, d * 1.18f, mat,
                   p.wear, rng);
        ngon_prism(m, {0.0f, h - 0.10f, 0.0f}, 0.10f, p.facets, d * 1.35f, mat,
                   p.wear, rng);
    }
}

/// ЛЕЖЕНЬ. The same prism laid down along +X, origin at the near end's
/// UNDERSIDE (the face it beds on — the kit's stacking convention). For a
/// 4-facet sleeper one facet faces straight UP, which is the whole point: the
/// floor deck gets a plane to rest on, not a tangent line.
void make_sleeper(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    constexpr float PI = 3.14159265359f;
    const int n = p.facets > 0 ? p.facets : ROUND_JOINT_SEGMENTS;
    const float r_in = static_cast<float>(p.diameter_cm) * 0.005f;
    const float R = p.facets > 0 ? r_in / std::cos(PI / static_cast<float>(n)) : r_in;
    const float len = m_of(p.length_u);
    const int segs = std::max(1, p.length_u / 4);
    std::vector<std::vector<glm::vec3>> rings(static_cast<std::size_t>(segs) + 1);
    for (int s = 0; s <= segs; ++s) {
        const float x = len * static_cast<float>(s) / static_cast<float>(segs);
        auto& ring = rings[static_cast<std::size_t>(s)];
        ring.resize(static_cast<std::size_t>(n));
        for (int k = 0; k < n; ++k) {
            // Vertices offset half a step so facet normals land on +Y/-Y/±Z
            // for n = 4: flat bed below, flat seat above. The MINUS sets the
            // ring's sense so the shared quad pattern winds OUTWARD around a
            // +X axis (the volume meter read the first bake inside-out); the
            // vertex SET is unchanged — the offsets are symmetric.
            const float ang = -(static_cast<float>(k) + 0.5f) * 2.0f * PI
                            / static_cast<float>(n);
            ring[static_cast<std::size_t>(k)] =
                glm::vec3{x, r_in + R * std::sin(ang), R * std::cos(ang)};
        }
    }
    for (int s = 0; s < segs; ++s) {
        const auto& p0 = rings[static_cast<std::size_t>(s)];
        const auto& p1 = rings[static_cast<std::size_t>(s) + 1];
        for (int k = 0; k < n; ++k) {
            const int j = (k + 1) % n;
            quad(m, p0[static_cast<std::size_t>(k)], p0[static_cast<std::size_t>(j)],
                 p1[static_cast<std::size_t>(j)], p1[static_cast<std::size_t>(k)],
                 tone(mat, p.wear, rng));
        }
    }
    const uint32_t cap = tone(mat, p.wear, rng);
    const glm::vec3 c0{0.0f, r_in, 0.0f};
    const glm::vec3 c1{len, r_in, 0.0f};
    const auto& first = rings.front();
    const auto& last = rings.back();
    for (int k = 0; k < n; ++k) {
        const int j = (k + 1) % n;
        tri(m, c0, first[static_cast<std::size_t>(j)], first[static_cast<std::size_t>(k)],
            cap);
        tri(m, c1, last[static_cast<std::size_t>(k)], last[static_cast<std::size_t>(j)],
            cap);
    }
}

/// ПЕРЕВЯЗКА ТОРЦОВ. Alternating stub courses: even courses run along +X,
/// odd along +Z, every stub crossing the corner axis by LOG_STUB_OUT_M. The
/// stub section is the log panel's own 2x1u section, so a corner placed at a
/// log wall's end continues its coursing instead of arguing with it.
void make_log_corner(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float h = m_of(p.length_u);
    const int courses = std::max(2, static_cast<int>(h / LOG_COURSE_M + 0.5f));
    const float hw = m_of(2) * 0.5f;   // half-width of a 2u log, laid flat
    // A course fills its own vertical step plus a 1 cm bite into the next,
    // exactly the panel's overlap trick: no daylight between stub courses.
    const float half_h = LOG_COURSE_M * 0.5f + 0.01f;
    for (int i = 0; i < courses; ++i) {
        const float y = (static_cast<float>(i) + 0.5f) * LOG_COURSE_M;
        const float run = LOG_STUB_OUT_M + LOG_STUB_BACK_M;
        if (i % 2 == 0) {
            hewn_bar(m, {-LOG_STUB_OUT_M, y, 0.0f}, {1.0f, 0.0f, 0.0f},
                     {0.0f, 1.0f, 0.0f}, run, hw, half_h, mat, p.wear, rng, 2);
        } else {
            hewn_bar(m, {0.0f, y, -LOG_STUB_OUT_M}, {0.0f, 0.0f, 1.0f},
                     {0.0f, 1.0f, 0.0f}, run, hw, half_h, mat, p.wear, rng, 2);
        }
    }
}

} // namespace dfn::render::part_detail
