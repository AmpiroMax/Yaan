/*
Created: 17:08:2026 - 13:21:37
Last updated: 17:08:2026 - 14:29:43
Module: engine/render
File: engine/render/sources/PartForgeRoofs.cpp

Responsibility:
- THE ROOF VARIANTS (user, 17.08: «...кучу разных вариантов крыш»): the
  pitched slope in five coverings — солома, дранка, тёс, черепица, дёрн — the
  hip slope (вальма) with its half-hip variant, and the smoke vent (дымник)
  that rides a ridge. make_roof moved VERBATIM from PartForge.cpp (family
  split, Rule 21) and then grew its coverings.

Key items:
- part_detail::make_roof(): one pitched plane, covering by material.
- part_detail::make_roof_hip(): triangle/half-hip end slope.
- part_detail::make_smoke_vent(): the louvred ridge hood.

Dependencies:
- Uses: PartForgeDetail.h, HewnBar.h.
- Used by: PartForge.cpp (forge_part dispatch).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Courses overlap DOWNSLOPE and each is a closed bar (Rule 52): the shadow
  line under a course is what stops a roof reading painted-on, and the
  overlap is what keeps daylight out of the seam.
- A slope's pitch is BAKED IN (rise over run): a Placement carries yaw and
  nothing else, so a roof a composer had to tilt is a roof nobody can place.
*/
/*
UPD:
- 17:08:2026 - 13:21:37: make_roof перенесён дословно и расширен покрытиями
  тёс/черепица/дёрн; добавлены вальма (вариант полувальмы) и дымник.
- 17:08:2026 - 14:29:43: material_of получил wear — ряд атласа несёт износ, и обрешётка под
  соломой должна стареть вместе с покрытием, а не оставаться свежей.
*/

#include "engine/render/sources/PartForgeDetail.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::render::part_detail {
namespace {

// The roof family's dimensions, metres. THATCH/SHINGLE numbers moved verbatim
// with make_roof; the new coverings' depths are read the same way — straw is
// deep, boards are thin, turf is a living mat.
constexpr float THATCH_DEEP_M = 0.35f;  ///< straw is DEEP; that is its whole read
constexpr float SHINGLE_DEEP_M = 0.10f;
constexpr float BOARD_DEEP_M = 0.07f;   ///< тёс: sawn boards laid down the slope
constexpr float TILE_DEEP_M = 0.09f;    ///< черепица: crisp thin courses
constexpr float TURF_DEEP_M = 0.28f;    ///< дёрн: a thick living mat
constexpr float COURSE_M = 0.60f;       ///< one roof course up the slope
constexpr float FRINGE_M = 0.45f;       ///< how far the eaves fringe hangs

struct Covering {
    float deck;
    float over;    ///< how far a course overlaps downslope
    bool boards;   ///< laid down-the-slope instead of coursed
    bool fringe;   ///< the shaggy thatch eaves
};

[[nodiscard]] Covering covering_of(PartMaterial m) {
    switch (m) {
    case PartMaterial::Thatch: return {THATCH_DEEP_M, 1.55f, false, true};
    case PartMaterial::Timber:
    case PartMaterial::TimberDark: return {BOARD_DEEP_M, 1.0f, true, false};
    case PartMaterial::Tile: return {TILE_DEEP_M, 1.15f, false, false};
    case PartMaterial::Turf: return {TURF_DEEP_M, 1.10f, false, false};
    default: return {SHINGLE_DEEP_M, 1.25f, false, false};
    }
}

} // namespace

/// One pitched plane, origin at the EAVES corner. The pitch is baked in
/// (rise over run) because a Placement carries yaw and nothing else — a roof
/// the composer had to tilt would be a roof nobody could place.
void make_roof(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float run = m_of(p.length_u);
    const float rise = m_of(p.height_u);
    const float depth = m_of(p.width_u); // along the ridge
    const glm::vec3 slope = glm::normalize(glm::vec3{run, rise, 0.0f});
    const float slope_len = std::sqrt(run * run + rise * rise);
    const Covering cov = covering_of(p.material);
    const glm::vec3 up = glm::normalize(glm::cross(slope, glm::vec3{0.0f, 0.0f, 1.0f}));

    if (cov.boards) {
        // ТЁС: boards run DOWN the slope the way water does, over a sealed
        // underdeck that keeps the seams shadows (same core trick as walls).
        Material deck_mat = mat;
        deck_mat.chamfer = 0.0f;
        deck_mat.wobble = 0.0f;
        Material dark = deck_mat;
        dark.color *= 0.55f;
        // The underdeck bar sweeps along +Z with the slope for its width, so
        // its origin must sit at MID-slope — at the eaves corner it would
        // hang half its length past the eaves.
        hewn_bar(m, slope * (slope_len * 0.5f) + up * (cov.deck * 0.25f),
                 {0.0f, 0.0f, 1.0f}, up, depth, slope_len * 0.5f, cov.deck * 0.25f,
                 dark, p.wear * 0.5f, rng, 2);
        const int boards = std::max(3, static_cast<int>(depth / 0.30f + 0.5f));
        const float bw = depth / static_cast<float>(boards);
        for (int i = 0; i < boards; ++i) {
            const float z = (static_cast<float>(i) + 0.5f) * bw;
            hewn_bar(m, glm::vec3{0.0f, 0.0f, z} + up * (cov.deck * 0.75f), slope, up,
                     slope_len, bw * 0.47f, cov.deck * 0.25f, mat, p.wear, rng, 3);
        }
    } else {
        // Courses across the slope, each its own tone, so the roof has the
        // banded read every reference thatch has.
        const int courses = std::max(3, static_cast<int>(slope_len / COURSE_M + 0.5f));
        const float cl = slope_len / static_cast<float>(courses);
        for (int i = 0; i < courses; ++i) {
            const glm::vec3 at = slope * (static_cast<float>(i) * cl + cl * 0.5f);
            // Courses overlap downslope, thatch more than shingle: the shadow
            // line under each course is what stops a roof looking painted on.
            hewn_bar(m, at + up * (cov.deck * 0.5f), {0.0f, 0.0f, 1.0f}, up, depth,
                     cl * cov.over * 0.5f, cov.deck * 0.5f, mat, p.wear, rng, 2);
        }
    }
    // The eaves fringe: the thick shaggy edge in the user's frames, where the
    // thatch hangs a hand's width past the wall.
    if (cov.fringe) {
        const int straws = std::max(4, static_cast<int>(depth / m_of(1) + 0.5f));
        for (int i = 0; i < straws; ++i) {
            const float z = (static_cast<float>(i) + 0.5f) * depth / static_cast<float>(straws);
            hewn_bar(m, {0.0f, cov.deck * 0.5f, z}, {-0.30f, -1.0f, 0.0f},
                     {0.0f, 0.0f, 1.0f}, FRINGE_M,
                     depth / static_cast<float>(straws) * 0.45f, cov.deck * 0.35f,
                     mat, p.wear, rng, 2, 0.55f);
        }
    }
    // ДЁРН wears a timber eaves board holding the mat (ветровница) — without
    // it the turf edge reads as a green slab, not a grown roof.
    if (p.material == PartMaterial::Turf) {
        hewn_bar(m, {0.0f, cov.deck * 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f},
                 {0.0f, 1.0f, 0.0f}, depth, 0.05f, cov.deck * 0.55f,
                 material_of(PartMaterial::Timber, p.wear), p.wear, rng, 3);
    }
    // The ridge beam and the rafter under the eaves: the timber a roof rests on.
    const Material frame = material_of(PartMaterial::Timber, p.wear);
    hewn_bar(m, {run, rise, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, depth,
             m_of(1) * 0.6f, m_of(1) * 0.6f, frame, p.wear, rng, 3);
    hewn_bar(m, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, depth,
             m_of(1) * 0.5f, m_of(1) * 0.5f, frame, p.wear, rng, 3);
}

/// ВАЛЬМОВЫЙ СКАТ: the end slope of a hipped roof — a triangle from the eaves
/// line to the ridge point (variant 0), or the HALF-HIP trapezoid whose top
/// edge keeps 45% of the eaves width (variant 1, полувальма — the gable keeps
/// its lower half, the roof pinches the top). Origin at the eaves' near
/// corner; eaves run along +Z (depth), rise toward +X like the plain slope.
void make_roof_hip(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float run = m_of(p.width_u);
    const float rise = m_of(p.height_u);
    const float depth = m_of(p.length_u);
    const float top_frac = p.variant == 1 ? 0.45f : 0.02f;
    const glm::vec3 slope = glm::normalize(glm::vec3{run, rise, 0.0f});
    const float slope_len = std::sqrt(run * run + rise * rise);
    const glm::vec3 up = glm::normalize(glm::cross(slope, glm::vec3{0.0f, 0.0f, 1.0f}));
    const Covering cov = covering_of(p.material);
    const int courses = std::max(3, static_cast<int>(slope_len / COURSE_M + 0.5f));
    const float cl = slope_len / static_cast<float>(courses);
    for (int i = 0; i < courses; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(courses);
        // The course shrinks toward the apex; its centre stays on the mid
        // plane, which is what makes the hip read as a PYRAMID face.
        const float half_w = depth * 0.5f * (1.0f - t * (1.0f - top_frac));
        if (half_w < 0.05f) {
            continue;
        }
        const glm::vec3 at = slope * (static_cast<float>(i) * cl + cl * 0.5f)
                           + glm::vec3{0.0f, 0.0f, depth * 0.5f - half_w};
        hewn_bar(m, at + up * (cov.deck * 0.5f), {0.0f, 0.0f, 1.0f}, up,
                 half_w * 2.0f, cl * cov.over * 0.5f, cov.deck * 0.5f, mat, p.wear,
                 rng, 2);
    }
    // Накосные стропила: the two slanted edge timbers a hip is built on —
    // they also seal the stepped course ends the way a barge board seals a
    // gable's.
    const Material frame = material_of(PartMaterial::Timber, p.wear);
    for (int s = 0; s < 2; ++s) {
        const float z0 = s == 0 ? 0.0f : depth;
        const float z1 = s == 0 ? depth * 0.5f * (1.0f - top_frac)
                                : depth - depth * 0.5f * (1.0f - top_frac);
        const glm::vec3 a{0.0f, 0.0f, z0};
        const glm::vec3 b{run, rise, z1};
        const glm::vec3 d = b - a;
        hewn_bar(m, a, d, up, glm::length(d), m_of(1) * 0.45f, m_of(1) * 0.55f,
                 frame, p.wear, rng, 3);
    }
    hewn_bar(m, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, depth,
             m_of(1) * 0.5f, m_of(1) * 0.5f, frame, p.wear, rng, 3);
}

/// ДЫМНИК: the louvred hood that lets smoke out where a chimney would be too
/// grand. Origin at the BASE CENTRE so a composer sets it astride a ridge by
/// the ridge's own coordinates. Four posts, louvre slats on all four sides
/// (closed bars — smoke is imaginary, daylight is not), a pyramid cap with a
/// hand of overhang.
void make_smoke_vent(MeshData& m, const PartParams& p, const Material& mat, Rng& rng) {
    const float s = m_of(p.length_u);      // base side
    const float h = std::max(0.5f, s * 0.9f);
    const float half = s * 0.5f;
    const Material dark = material_of(PartMaterial::TimberDark, p.wear);
    for (int cx = 0; cx < 2; ++cx) {
        for (int cz = 0; cz < 2; ++cz) {
            hewn_bar(m, {(cx == 0 ? -half : half) * 0.9f, 0.0f,
                         (cz == 0 ? -half : half) * 0.9f},
                     {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, h, 0.04f, 0.04f, mat,
                     p.wear, rng, 2);
        }
    }
    // Louvres: three sloped slats per side, biting each other's shadow line.
    const int slats = 3;
    for (int i = 0; i < slats; ++i) {
        const float y = h * (0.25f + 0.55f * static_cast<float>(i)
                             / static_cast<float>(slats - 1));
        hewn_bar(m, {-half * 0.92f, y, -half * 0.86f}, {1.0f, 0.0f, 0.0f},
                 {0.0f, 1.0f, 0.35f}, half * 1.84f, 0.02f, half * 0.16f, dark,
                 p.wear, rng, 2);
        hewn_bar(m, {-half * 0.92f, y, half * 0.86f}, {1.0f, 0.0f, 0.0f},
                 {0.0f, 1.0f, -0.35f}, half * 1.84f, 0.02f, half * 0.16f, dark,
                 p.wear, rng, 2);
        hewn_bar(m, {-half * 0.86f, y, -half * 0.92f}, {0.0f, 0.0f, 1.0f},
                 {0.35f, 1.0f, 0.0f}, half * 1.84f, half * 0.16f, 0.02f, dark,
                 p.wear, rng, 2);
        hewn_bar(m, {half * 0.86f, y, -half * 0.92f}, {0.0f, 0.0f, 1.0f},
                 {-0.35f, 1.0f, 0.0f}, half * 1.84f, half * 0.16f, 0.02f, dark,
                 p.wear, rng, 2);
    }
    // The pyramid cap: four closed triangular petals over a small flat top —
    // built as four tapered bars meeting at the peak (structure, not a
    // contour: Rule 52's recipe).
    const float peak = h + s * 0.45f;
    const float over = half * 1.25f;
    for (int side = 0; side < 4; ++side) {
        const float ang = static_cast<float>(side) * 1.5707963f;
        const glm::vec3 out{std::cos(ang), 0.0f, std::sin(ang)};
        const glm::vec3 a = out * over + glm::vec3{0.0f, h - 0.05f, 0.0f};
        const glm::vec3 b{0.0f, peak, 0.0f};
        const glm::vec3 d = b - a;
        hewn_bar(m, a, d, out, glm::length(d), over * 0.9f, 0.035f, mat, p.wear,
                 rng, 2, 0.15f);
    }
}

} // namespace dfn::render::part_detail
