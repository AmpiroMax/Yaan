/*
Created: 12:08:2026 - 00:47:30
Last updated: 23:08:2026 - 07:20:00
Module: engine/render
File: engine/render/sources/GroundTufts.cpp

Responsibility:
- GroundTufts implementation: harvesting plant spots off the drawn voxel ground
  and growing them into blade geometry.

Key items:
- harvest_tuft_spots(); build_ground_tufts(); the blade/clump builders.

Dependencies:
- Uses: GroundTufts.h, ProcMesh (MeshData + tri), glm.
- Used by: dfn_render target; tests/render/GroundTuftsTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic pure functions: an integer hash, no std::rand, no time. Two
  runs of the same world must plant the same tufts in the same places, or the
  layer becomes a shimmer source all by itself — a tuft that moves between
  frames is worse than no tuft.
- A BLADE IS OPAQUE GEOMETRY AND THAT IS THE ANTI-SHIMMER DECISION. The obvious
  alternative — an alpha-cutout card with a grass mask — puts the silhouette
  edge INSIDE a texture fetch, where MSAA cannot reach it. That is the exact
  mechanism this project already paid for twice (BgfxRendererResources' mip
  note, FLORA_SPECKLE_TREELINE_PCT's 23-fold drop). Real, if coarse, blade
  triangles put the edge back on geometry, where the coverage AA already
  running on the internal target antialiases it for free.
*/
/*
UPD:
- 12:08:2026 - 00:47:30: Created with the layer.
- 23:08:2026 - 07:20:00: фильтр пятен построек в цикле выращивания.
*/

#include "engine/render/sources/GroundTufts.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::render {

namespace {

// lowbias32 (Chris Wellons), the same mixer ProcTexture uses — one hash in the
// zone rather than a second one that drifts.
uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float unit(uint32_t h) {
    return static_cast<float>(h) * (1.0f / 4294967295.0f);
}

// THE TONES, and there are four because the user asked for «разную» grass and
// one clump in copies is what he is complaining about elsewhere in the same
// sentence. They sit close together on purpose: §5.6 rules that grass is
// «exactly the underlying splat color family +-1 value step — grass is texture,
// not information», so these vary in HUE and barely in VALUE. Same discipline
// as the R5 ground tint, and for the same reason: a tuft that separates from
// the ground by 2 rulers of value has become a landmark (§1.3b).
//
// THE LEVEL IS SET AGAINST A MEASURED GROUND, NOT PICKED. The lowland ground
// of the acceptance frame measures (0.34, 0.47, 0.18) in albedo terms
// (tools/measure_ground_colour.py on the shipped frame), so these sit on that
// value and move in HUE around it. The first version sat a third darker and
// the frame came back as a field of dark specks — grass that reads as litter
// is grass that became information, which §5.6 forbids in as many words.
constexpr glm::vec3 TUFT_TONES[4] = {
    {0.33f, 0.46f, 0.18f}, // the meadow green of the ground it stands on
    {0.40f, 0.47f, 0.19f}, // olive, drier
    {0.30f, 0.43f, 0.25f}, // cooler, blue-green
    {0.45f, 0.45f, 0.22f}, // straw, the dead blades in a live clump
};

// One blade: a tapered triangle from a base of `half_w` to a tip, leaning by
// `lean` from the local up and bending a little further at the top. Two
// triangles so the bend reads as a curve rather than a spike at 640x360.
void blade(MeshData& m, glm::vec3 base, glm::vec3 up, glm::vec3 side,
           glm::vec3 out, float height, float half_w, float lean,
           const glm::vec3& tone) {
    const glm::vec3 mid = base
                        + up * (height * 0.55f)
                        + out * (height * std::sin(lean) * 0.45f);
    const glm::vec3 tip = base
                        + up * (height * std::cos(lean))
                        + out * (height * std::sin(lean));
    // Darker at the root, lighter at the tip — the single cue that makes a
    // handful of triangles read as grass rather than as green litter.
    // A gentle root-to-tip ramp. It was 0.72..1.18 and that is a whole value
    // step of spread inside one blade, which §5.6 does not allow a tuft to
    // spend — the cue only has to be readable, not dramatic.
    const uint32_t c_low = pack(tone * 0.86f);
    const uint32_t c_high = pack(tone * 1.12f);
    const glm::vec3 l = base - side * half_w;
    const glm::vec3 r = base + side * half_w;
    const glm::vec3 ml = mid - side * (half_w * 0.45f);
    const glm::vec3 mr = mid + side * (half_w * 0.45f);

    // THE BLADE'S NORMAL IS THE GROUND'S NORMAL, NOT THE BLADE'S OWN, AND THIS
    // IS NOT A SHORTCUT. Two reasons, and the first one is a bug found in the
    // frame rather than reasoned about:
    //  1. A blade is drawn double-sided (it is a surface with no inside, and
    //     the player walks around it). Taking the normal from the winding, as
    //     ProcMesh::tri does, gives the two copies OPPOSITE normals, so one of
    //     them faces away from the sun and comes back black. The first shot of
    //     this layer was a field of dark spikes for exactly that reason.
    //  2. §5.6 rules that grass is «exactly the underlying splat color family
    //     ±1 value step — grass is texture, not information». A tuft lit like
    //     the ground it stands in obeys that by construction; a tuft lit like a
    //     vertical wall cannot, because at midday a vertical surface is a value
    //     step darker than the ground no matter what albedo it is given.
    const auto push = [&m, up](glm::vec3 a, glm::vec3 b, glm::vec3 c, uint32_t col) {
        const auto base = static_cast<uint32_t>(m.vertices.size());
        for (const glm::vec3& p : {a, b, c}) {
            m.vertices.push_back(platform::Vertex{p, up, {0.0f, 0.0f}, col});
        }
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2,
                                           base, base + 2, base + 1});
    };
    push(l, r, mr, c_low);
    push(l, mr, ml, c_low);
    push(ml, mr, tip, c_high);
}

void clump(MeshData& m, const TuftSpot& spot, const GroundTuftParams& params) {
    const glm::vec3 up = glm::normalize(spot.normal);
    // A stable tangent frame: cross with whichever axis the normal is least
    // aligned to, so it never degenerates on a vertical face.
    const glm::vec3 ref = std::fabs(up.y) < 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f}
                                                 : glm::vec3{1.0f, 0.0f, 0.0f};
    const glm::vec3 side0 = glm::normalize(glm::cross(ref, up));
    const glm::vec3 fwd0 = glm::cross(up, side0);

    const uint32_t h0 = hash_u32(spot.seed);
    const uint32_t h1 = hash_u32(h0 ^ 0x51u);
    const uint32_t h2 = hash_u32(h1 ^ 0x9Du);

    // FOUR SHAPES, not one: 3, 4, 5 and 7 blades, the last a fat clump. The
    // spread matters more than any single silhouette — a screen of identical
    // tufts is a stamp, which is the defect one layer down.
    constexpr int BLADES[4] = {3, 4, 5, 7};
    const int shape = static_cast<int>(h0 & 3u);
    const int count = BLADES[shape];

    // Height spread is wide on purpose (0.35..1.0 of the ceiling): a clump of
    // equal-length blades reads as a brush, and the tallest tuft must still sit
    // under GRASS_HEIGHT_MAX so it cannot hide an interactable (§2.3).
    const float base_h = params.height_max_m * (0.35f + 0.65f * unit(h1));
    const glm::vec3 tone = TUFT_TONES[(h2 >> 5) & 3u];

    // SUNK BY A HAIR. The base disappears into the ground instead of ending in
    // a visible flat cut, and it also absorbs whatever sub-centimetre
    // disagreement is left between the vertex we planted on and the rasterised
    // surface.
    const glm::vec3 root = spot.position - up * (base_h * 0.12f);

    for (int i = 0; i < count; ++i) {
        const uint32_t hb = hash_u32(spot.seed ^ (0x2545F491u * static_cast<uint32_t>(i + 1)));
        const float az = unit(hb) * 6.2831853f;
        const glm::vec3 out = side0 * std::cos(az) + fwd0 * std::sin(az);
        const glm::vec3 side = glm::normalize(glm::cross(up, out));
        const float hb1 = unit(hash_u32(hb ^ 0xB5u));
        const float hb2 = unit(hash_u32(hb ^ 0xC7u));
        const float height = base_h * (0.60f + 0.55f * hb1);
        // Lean 12..48 degrees. Nothing stands straight up: D1 in the reference
        // frames («ничто не стоит по осям») is about the whole world, and a
        // fan of vertical spikes is the most artificial thing a tuft can be.
        const float lean = 0.21f + 0.63f * hb2;
        const float half_w = std::max(0.008f, height * 0.055f);
        // A small spread so the blades leave one root rather than one point.
        const glm::vec3 base = root + out * (height * 0.09f * hb1);
        blade(m, base, up, side, out, height, half_w, lean, tone);
    }
}

} // namespace

std::vector<TuftSpot> harvest_tuft_spots(const math::VoxelMeshView& mesh,
                                         const GroundTuftParams& params) {
    std::vector<TuftSpot> spots;
    const size_t tri_count = mesh.indices.size() / 3;
    if (tri_count == 0 || mesh.positions.empty() || params.density_per_m2 <= 0.0f) {
        return spots;
    }
    const bool has_material = mesh.materials.size() >= mesh.positions.size();
    // cos of the steepest ground that still carries grass (§4 rule 4 /
    // SLOPE_GRASS_MAX): slope is the angle off vertical of the surface normal.
    const float cos_max = std::cos(params.slope_max_rad);

    for (size_t t = 0; t < tri_count; ++t) {
        const uint32_t i0 = mesh.indices[t * 3 + 0];
        const uint32_t i1 = mesh.indices[t * 3 + 1];
        const uint32_t i2 = mesh.indices[t * 3 + 2];
        if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size()
            || i2 >= mesh.positions.size()) {
            continue;
        }
        if (has_material) {
            // Core's surface truth, per vertex. All three must be grassy —
            // a triangle straddling the rock line belongs to the rock.
            const auto grassy = [&](uint32_t i) {
                const auto m = static_cast<math::VoxelMaterial>(mesh.materials[i]);
                return m == math::VoxelMaterial::Grass
                    || m == math::VoxelMaterial::GrassRockBlend;
            };
            if (!grassy(i0) || !grassy(i1) || !grassy(i2)) {
                continue;
            }
        }
        const glm::vec3 a = mesh.positions[i0];
        const glm::vec3 b = mesh.positions[i1];
        const glm::vec3 c = mesh.positions[i2];
        const glm::vec3 cross = glm::cross(b - a, c - a);
        const float len = glm::length(cross);
        if (len <= 1e-6f) {
            continue;
        }
        glm::vec3 n = cross / len;
        if (n.y < 0.0f) {
            n = -n; // winding is not load-bearing here; the ground faces up
        }
        if (n.y < cos_max) {
            continue; // too steep for grass
        }
        const float area = 0.5f * len;

        // THE PLANTING DECISION, PER TRIANGLE AND BY AREA. Expected tufts on
        // this triangle is density * area; on a 1 m voxel lattice that is well
        // under one, so it is a coin flip against its own hash. Where triangles
        // are large enough to expect more than one, the loop plants more —
        // otherwise density would silently cap at one per triangle and the
        // layer would thin out on exactly the flattest, largest-triangle
        // ground, which is the ground the user is complaining about.
        const float expected = params.density_per_m2 * area;
        const uint32_t base_hash = hash_u32(static_cast<uint32_t>(t) * 0x9E3779B9u
                                            ^ hash_u32(params.seed));
        const int whole = static_cast<int>(expected);
        const float frac = expected - static_cast<float>(whole);
        int plant = whole;
        if (unit(base_hash) < frac) {
            ++plant;
        }
        for (int k = 0; k < plant; ++k) {
            const uint32_t hk = hash_u32(base_hash ^ (0x85EBCA6Bu
                                                      * static_cast<uint32_t>(k + 1)));
            // Uniform barycentric sample (the sqrt keeps it uniform over the
            // triangle instead of bunching at one corner).
            float u = unit(hk);
            float v = unit(hash_u32(hk ^ 0x27u));
            const float su = std::sqrt(u);
            u = 1.0f - su;
            v = v * su;
            spots.push_back(TuftSpot{a + (b - a) * u + (c - a) * v, n, hk});
        }
    }
    return spots;
}

MeshData build_ground_tufts(std::span<const TuftSpot> spots, glm::vec3 eye,
                            const GroundTuftParams& params,
                            std::span<const glm::vec4> exclusions) {
    MeshData mesh;
    const float r = params.view_distance_m;
    if (r <= 0.0f) {
        return mesh;
    }
    const float r2 = r * r;
    for (const TuftSpot& s : spots) {
        // Пятно постройки — пол, а не луг («былинки сквозь пол», 23.08).
        bool inside_building = false;
        for (const glm::vec4& rect : exclusions) {
            if (std::abs(s.position.x - rect.x) <= rect.z
                && std::abs(s.position.z - rect.y) <= rect.w) {
                inside_building = true;
                break;
            }
        }
        if (inside_building) {
            continue;
        }
        const glm::vec3 d = s.position - eye;
        if (glm::dot(d, d) > r2) {
            continue;
        }
        clump(mesh, s, params);
    }
    return mesh;
}

} // namespace dfn::render
