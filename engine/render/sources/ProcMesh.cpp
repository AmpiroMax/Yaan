/*
Created: 09:08:2026 - 11:57:20
Last updated: 09:08:2026 - 20:05:00
Module: engine/render
File: engine/render/sources/ProcMesh.cpp

Responsibility:
- Placeholder mesh building: flat-shaded primitive assembly (box, cone ring,
  blob, gable) composed into the LANDSCAPE §5 species and §6 structure
  silhouettes.

Key items:
- build_scatter_mesh / build_site_mesh / append_transformed; primitive helpers.

Dependencies:
- Uses: ProcMesh.h, glm.
- Used by: dfn_render target; tests/render/ProcMeshTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure and deterministic — no RNG beyond fixed tables, no GPU, no ECS.
- Dimensions/colors cite LANDSCAPE.md §5/§6 (design owns the values; real
  content moves to data files per Rule 5).
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial placeholder mesh catalog.
- 09:08:2026 - 14:11:37: Micro-relief (user decision в3): stone rebuilt as a
  ~0.9 m chunky faceted boulder (position-hash crush, re-derived flat normals)
  — the crushed 0.42 m box read as a flat speck.
- 09:08:2026 - 20:05:00: pack/tri/quad promoted out of the anonymous namespace
  and declared in the header — the flora agent's ProcFlora (new zone, same
  directory) builds its tubes and clusters on them (agreed in-session).
*/

#include "engine/render/sources/ProcMesh.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

namespace dfn::render {

namespace {

constexpr float TAU = 6.28318530718f;

// --- Look-dev palette (LANDSCAPE §5/§6 value language; placeholder assets) ---
constexpr glm::vec3 OAK_CROWN{0.30f, 0.42f, 0.18f};   // mid-green, darker than meadow
constexpr glm::vec3 OAK_TRUNK{0.14f, 0.11f, 0.08f};   // near-black
constexpr glm::vec3 PINE_DARK{0.12f, 0.22f, 0.19f};   // darkest flora value
constexpr glm::vec3 PINE_TRUNK{0.20f, 0.15f, 0.10f};
constexpr glm::vec3 BIRCH_TRUNK{0.88f, 0.87f, 0.82f}; // brightest flora value
constexpr glm::vec3 BIRCH_CROWN{0.55f, 0.62f, 0.30f}; // light yellow-green
constexpr glm::vec3 BUSH_GREEN{0.35f, 0.47f, 0.22f};  // oak-green, slightly lighter
constexpr glm::vec3 STONE_GREY{0.46f, 0.45f, 0.42f};
constexpr glm::vec3 PLASTER{0.72f, 0.66f, 0.54f};
constexpr glm::vec3 TIMBER{0.36f, 0.28f, 0.20f};
constexpr glm::vec3 MASONRY{0.48f, 0.46f, 0.44f};
constexpr glm::vec3 MASONRY_DARK{0.38f, 0.36f, 0.34f};
constexpr glm::vec3 ROOF_THATCH{0.55f, 0.44f, 0.24f}; // dwelling purpose code
constexpr glm::vec3 ROOF_RED{0.58f, 0.30f, 0.22f};    // trader
constexpr glm::vec3 ROOF_RED_DARK{0.44f, 0.22f, 0.17f}; // tavern (the big roof)
constexpr glm::vec3 ROOF_BARN{0.30f, 0.23f, 0.15f};   // barn (tall dark triangle)
constexpr glm::vec3 SHRINE_PALE{0.82f, 0.80f, 0.74f}; // skyline-breaking spire
constexpr glm::vec3 PORTAL_DARK{0.07f, 0.07f, 0.09f}; // dungeon mouth

// Axis-aligned box, all 6 faces.
void box(MeshData& m, glm::vec3 mn, glm::vec3 mx, uint32_t color) {
    const glm::vec3 a{mn.x, mn.y, mn.z}, b{mx.x, mn.y, mn.z};
    const glm::vec3 c{mx.x, mn.y, mx.z}, d{mn.x, mn.y, mx.z};
    const glm::vec3 e{mn.x, mx.y, mn.z}, f{mx.x, mx.y, mn.z};
    const glm::vec3 g{mx.x, mx.y, mx.z}, h{mn.x, mx.y, mx.z};
    quad(m, e, f, g, h, color); // top (+Y)
    quad(m, d, c, b, a, color); // bottom (-Y)
    quad(m, a, b, f, e, color); // north (-Z)
    quad(m, c, d, h, g, color); // south (+Z)
    quad(m, b, c, g, f, color); // east (+X)
    quad(m, d, a, e, h, color); // west (-X)
}

// Cone-frustum ring: sides from radius r0 at y0 to r1 at y1 around (cx, cz).
// r1 == 0 collapses to an apex. Optional flat top cap when r1 > 0.
void ring(MeshData& m, glm::vec2 center, float y0, float r0, float y1, float r1,
          int sides, uint32_t color, bool cap_top = false) {
    for (int i = 0; i < sides; ++i) {
        const float a0 = TAU * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = TAU * static_cast<float>(i + 1) / static_cast<float>(sides);
        const glm::vec3 b0{center.x + r0 * std::cos(a0), y0, center.y + r0 * std::sin(a0)};
        const glm::vec3 b1{center.x + r0 * std::cos(a1), y0, center.y + r0 * std::sin(a1)};
        if (r1 <= 1e-6f) {
            tri(m, b0, glm::vec3{center.x, y1, center.y}, b1, color);
        } else {
            const glm::vec3 t0{center.x + r1 * std::cos(a0), y1,
                               center.y + r1 * std::sin(a0)};
            const glm::vec3 t1{center.x + r1 * std::cos(a1), y1,
                               center.y + r1 * std::sin(a1)};
            quad(m, b0, t0, t1, b1, color);
            if (cap_top) {
                tri(m, t0, glm::vec3{center.x, y1, center.y}, t1, color);
            }
        }
    }
}

// Faceted ellipsoid blob (crowns, bushes): `bands` latitudes from theta_min
// (-pi/2 = full sphere, 0 = hemisphere) to +pi/2, `slices` longitudes.
void blob(MeshData& m, glm::vec3 center, glm::vec3 radii, int slices, int bands,
          uint32_t color, float theta_min) {
    const float theta_span = glm::half_pi<float>() - theta_min;
    auto point = [&](int band, int slice) {
        const float t = theta_min
            + theta_span * static_cast<float>(band) / static_cast<float>(bands);
        const float p = TAU * static_cast<float>(slice) / static_cast<float>(slices);
        return center + glm::vec3{radii.x * std::cos(t) * std::cos(p),
                                  radii.y * std::sin(t),
                                  radii.z * std::cos(t) * std::sin(p)};
    };
    for (int b = 0; b < bands; ++b) {
        for (int s = 0; s < slices; ++s) {
            const glm::vec3 p00 = point(b, s), p01 = point(b, s + 1);
            const glm::vec3 p10 = point(b + 1, s), p11 = point(b + 1, s + 1);
            if (b + 1 == bands) {
                tri(m, p00, p10, p01, color); // pole cap
            } else {
                quad(m, p00, p10, p11, p01, color);
            }
        }
    }
    if (theta_min > -glm::half_pi<float>() + 1e-4f) {
        // Open bottom (hemisphere): close with a fan so no hole shows on slopes.
        for (int s = 0; s < slices; ++s) {
            tri(m, point(0, s), point(0, s + 1),
                glm::vec3{center.x, center.y + radii.y * std::sin(theta_min), center.z},
                color);
        }
    }
}

// Gable roof volume over footprint [mn.x,mx.x]x[mn.z,mx.z]: eaves at y_eave,
// ridge (along Z) at y_ridge. Two slopes + two gable-end triangles.
void gable(MeshData& m, glm::vec2 mn, glm::vec2 mx, float y_eave, float y_ridge,
           uint32_t roof_color, uint32_t wall_color) {
    const float cx = (mn.x + mx.x) * 0.5f;
    const glm::vec3 e0{mn.x, y_eave, mn.y}, e1{mn.x, y_eave, mx.y};
    const glm::vec3 e2{mx.x, y_eave, mn.y}, e3{mx.x, y_eave, mx.y};
    const glm::vec3 r0{cx, y_ridge, mn.y}, r1{cx, y_ridge, mx.y};
    quad(m, e0, e1, r1, r0, roof_color); // west slope
    quad(m, e3, e2, r0, r1, roof_color); // east slope
    tri(m, e2, e0, r0, wall_color);      // north gable end (-Z)
    tri(m, e1, e3, r1, wall_color);      // south gable end (+Z)
}

// --- Species meshes (§5) ----------------------------------------------------

MeshData oak() {
    // §5.1: 10 m nominal, crown 8 m wide, wider-than-tall ball on a 1/3 trunk.
    MeshData m;
    ring(m, {0.0f, 0.0f}, 0.0f, 0.55f, 3.6f, 0.4f, 6, pack(OAK_TRUNK));
    blob(m, {0.0f, 6.2f, 0.0f}, {4.0f, 3.6f, 4.0f}, 7, 5, pack(OAK_CROWN),
         -glm::half_pi<float>());
    return m;
}

MeshData pine() {
    // §5.2: 15 m nominal, base 5 m wide, three stacked cone tiers, thick tip.
    MeshData m;
    const uint32_t green = pack(PINE_DARK);
    ring(m, {0.0f, 0.0f}, 0.0f, 0.30f, 2.2f, 0.24f, 5, pack(PINE_TRUNK));
    ring(m, {0.0f, 0.0f}, 1.4f, 2.5f, 7.0f, 0.0f, 8, green);
    ring(m, {0.0f, 0.0f}, 4.8f, 2.0f, 10.8f, 0.0f, 8, green);
    ring(m, {0.0f, 0.0f}, 8.2f, 1.4f, 15.0f, 0.0f, 8, green);
    return m;
}

MeshData birch() {
    // §5.3: 8 m nominal, slim pale trunk, small loose crown, slight lean.
    MeshData m;
    ring(m, {0.0f, 0.0f}, 0.0f, 0.22f, 5.6f, 0.14f, 5, pack(BIRCH_TRUNK));
    blob(m, {0.0f, 6.1f, 0.0f}, {1.8f, 2.0f, 1.8f}, 6, 4, pack(BIRCH_CROWN),
         -glm::half_pi<float>());
    for (platform::Vertex& v : m.vertices) { // lean: shear +X with height
        v.position.x += v.position.y * 0.07f;
    }
    return m;
}

MeshData bush() {
    // §5.4: 1.3 m hemisphere lump.
    MeshData m;
    blob(m, {0.0f, 0.15f, 0.0f}, {0.9f, 1.1f, 0.9f}, 6, 3, pack(BUSH_GREEN), 0.0f);
    return m;
}

MeshData stone() {
    // Chunky boulder, ~0.9 m nominal (user decision в3: the old 0.42 m crushed
    // box read as a flat speck at eye level — stones now carry mass and a
    // proud silhouette). Low-poly faceted lump, deformed by a deterministic
    // position-keyed crush so the shape is asymmetric: per-instance yaw from
    // the batcher then gives each placement a different outline.
    MeshData m;
    blob(m, {0.0f, 0.35f, 0.0f}, {0.62f, 0.50f, 0.48f}, 6, 3, pack(STONE_GREY),
         -0.8f);
    for (platform::Vertex& v : m.vertices) {
        glm::vec3& p = v.position;
        // Hash the quantized position: flat-shaded duplicates share the key,
        // so faces stay welded while corners crush independently.
        const auto xi = static_cast<uint32_t>(std::lround(p.x * 41.0f) + 512);
        const auto yi = static_cast<uint32_t>(std::lround(p.y * 41.0f) + 512);
        const auto zi = static_cast<uint32_t>(std::lround(p.z * 41.0f) + 512);
        uint32_t h = xi * 73856093u ^ yi * 19349663u ^ zi * 83492791u;
        h = (h ^ (h >> 13)) * 0x5bd1e995u;
        const float r = static_cast<float>(h % 1024u) / 1023.0f; // [0,1)
        const float squash = 0.78f + 0.40f * r;
        p.x = p.x * squash + 0.10f * (r - 0.5f);
        p.z = p.z * squash - 0.08f * (r - 0.5f);
        if (p.y > 0.15f) { // uneven crown; base ring stays grounded
            p.y *= 0.82f + 0.34f * r;
        }
    }
    // Re-derive flat normals after the crush (each face owns its vertices).
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        platform::Vertex& a = m.vertices[m.indices[i]];
        platform::Vertex& b = m.vertices[m.indices[i + 1]];
        platform::Vertex& c = m.vertices[m.indices[i + 2]];
        const glm::vec3 cross =
            glm::cross(b.position - a.position, c.position - a.position);
        const float len = glm::length(cross);
        const glm::vec3 n = len > 1e-8f ? cross / len : glm::vec3{0.0f, 1.0f, 0.0f};
        a.normal = n;
        b.normal = n;
        c.normal = n;
    }
    return m;
}

// --- Structure meshes (§6; bounds mirror SiteComponents) --------------------

MeshData dwelling() { // 6x8, gable + one chimney, h <= 5.5
    MeshData m;
    box(m, {-3.0f, 0.0f, -4.0f}, {3.0f, 3.2f, 4.0f}, pack(PLASTER));
    gable(m, {-3.0f, -4.0f}, {3.0f, 4.0f}, 3.2f, 5.2f, pack(ROOF_THATCH),
          pack(PLASTER));
    box(m, {1.0f, 3.2f, -2.0f}, {1.6f, 5.5f, -1.4f}, pack(MASONRY_DARK)); // chimney
    return m;
}

MeshData trader() { // 8x10, gable + full-width porch awning on -Z, h <= 6.5
    MeshData m;
    box(m, {-4.0f, 0.0f, -4.0f}, {4.0f, 3.8f, 5.0f}, pack(PLASTER));
    gable(m, {-4.0f, -4.0f}, {4.0f, 5.0f}, 3.8f, 6.2f, pack(ROOF_RED), pack(PLASTER));
    // Porch: flat awning slab over the recessed front strip + two posts.
    box(m, {-4.0f, 2.6f, -5.0f}, {4.0f, 2.9f, -4.0f}, pack(ROOF_RED));
    box(m, {-3.6f, 0.0f, -4.9f}, {-3.3f, 2.6f, -4.6f}, pack(TIMBER));
    box(m, {3.3f, 0.0f, -4.9f}, {3.6f, 2.6f, -4.6f}, pack(TIMBER));
    box(m, {1.8f, 2.0f, -5.0f}, {2.6f, 2.6f, -4.85f}, pack(TIMBER)); // hanging sign
    return m;
}

MeshData tavern() { // 10x14 L-shape, two storeys, two chimneys, h <= 8.5
    MeshData m;
    box(m, {-5.0f, 0.0f, -7.0f}, {5.0f, 5.4f, 1.0f}, pack(PLASTER)); // main block
    gable(m, {-5.0f, -7.0f}, {5.0f, 1.0f}, 5.4f, 8.2f, pack(ROOF_RED_DARK),
          pack(PLASTER));
    box(m, {-5.0f, 0.0f, 1.0f}, {0.0f, 3.4f, 7.0f}, pack(PLASTER)); // wing
    gable(m, {-5.0f, 1.0f}, {0.0f, 7.0f}, 3.4f, 5.6f, pack(ROOF_RED_DARK),
          pack(PLASTER));
    box(m, {-3.6f, 5.4f, -5.4f}, {-3.0f, 8.5f, -4.8f}, pack(MASONRY_DARK));
    box(m, {3.0f, 5.4f, -0.8f}, {3.6f, 8.5f, -0.2f}, pack(MASONRY_DARK));
    return m;
}

MeshData barn() { // 8x12, roof = 2/3 of the silhouette, gable-on doors, h <= 7.5
    MeshData m;
    box(m, {-4.0f, 0.0f, -5.94f}, {4.0f, 2.6f, 5.94f}, pack(TIMBER));
    gable(m, {-4.0f, -6.0f}, {4.0f, 6.0f}, 2.6f, 7.3f, pack(ROOF_BARN), pack(TIMBER));
    // Full-height door: dark slab proud of the recessed north gable wall
    // (stays inside the agreed bounds box).
    box(m, {-1.2f, 0.0f, -6.0f}, {1.2f, 2.5f, -5.95f}, pack(PORTAL_DARK));
    return m;
}

MeshData shrine() { // 5x5, spire to 12 — the smallest footprint, strongest vertical
    MeshData m;
    box(m, {-2.0f, 0.0f, -2.0f}, {2.0f, 2.8f, 2.0f}, pack(MASONRY));
    box(m, {-1.2f, 2.8f, -1.2f}, {1.2f, 4.2f, 1.2f}, pack(MASONRY));
    ring(m, {0.0f, 0.0f}, 4.2f, 1.35f, 12.0f, 0.0f, 4, pack(SHRINE_PALE));
    return m;
}

MeshData dungeon_entrance() { // 4x4, dark portal frame against a stone mound, h <= 4
    MeshData m;
    box(m, {-2.0f, 0.0f, -0.6f}, {2.0f, 3.4f, 2.0f}, pack(MASONRY_DARK)); // mound
    box(m, {-1.4f, 0.0f, -1.0f}, {-0.9f, 2.6f, -0.5f}, pack(MASONRY));   // posts
    box(m, {0.9f, 0.0f, -1.0f}, {1.4f, 2.6f, -0.5f}, pack(MASONRY));
    box(m, {-1.6f, 2.6f, -1.1f}, {1.6f, 3.3f, -0.4f}, pack(MASONRY));    // lintel
    box(m, {-0.9f, 0.0f, -0.7f}, {0.9f, 2.6f, -0.62f}, pack(PORTAL_DARK)); // mouth
    return m;
}

MeshData tower_ruin() { // r=2 broken cylinder to 12 m — the L0 crag topper
    MeshData m;
    // Per-side broken-top heights (fixed deterministic profile, "broken" read).
    constexpr float TOP[8] = {12.0f, 11.2f, 9.4f, 7.8f, 7.2f, 8.0f, 9.2f, 10.8f};
    constexpr float R_OUT = 2.0f;
    constexpr float R_IN = 1.55f;
    const uint32_t out_c = pack(MASONRY);
    const uint32_t in_c = pack(MASONRY_DARK);
    for (int i = 0; i < 8; ++i) {
        const float a0 = TAU * static_cast<float>(i) / 8.0f;
        const float a1 = TAU * static_cast<float>(i + 1) / 8.0f;
        const float h0 = TOP[i];
        const glm::vec2 d0{std::cos(a0), std::sin(a0)}, d1{std::cos(a1), std::sin(a1)};
        const glm::vec3 ob0{R_OUT * d0.x, 0.0f, R_OUT * d0.y};
        const glm::vec3 ob1{R_OUT * d1.x, 0.0f, R_OUT * d1.y};
        const glm::vec3 ot0{R_OUT * d0.x, h0, R_OUT * d0.y};
        const glm::vec3 ot1{R_OUT * d1.x, h0, R_OUT * d1.y};
        const glm::vec3 ib0{R_IN * d0.x, 0.4f, R_IN * d0.y};
        const glm::vec3 ib1{R_IN * d1.x, 0.4f, R_IN * d1.y};
        const glm::vec3 it0{R_IN * d0.x, h0, R_IN * d0.y};
        const glm::vec3 it1{R_IN * d1.x, h0, R_IN * d1.y};
        quad(m, ob0, ot0, ot1, ob1, out_c);  // outer wall
        quad(m, ib1, it1, it0, ib0, in_c);   // inner wall (flipped)
        quad(m, ot0, it0, it1, ot1, out_c);  // rim
    }
    return m;
}

} // namespace

// --- Shared primitives (public: the flora agent's ProcFlora builds on these;
// duplicating them would let a winding fix land in one copy only) ------

uint32_t pack(const glm::vec3& c) {
    const auto r = static_cast<uint32_t>(glm::clamp(c.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto g = static_cast<uint32_t>(glm::clamp(c.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    const auto b = static_cast<uint32_t>(glm::clamp(c.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    return 0xFF000000u | (b << 16) | (g << 8) | r; // 0xAABBGGRR (frozen Vertex)
}

// Flat-shaded triangle: normal from winding (CCW seen from outside).
void tri(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, uint32_t color) {
    const glm::vec3 cross = glm::cross(b - a, c - a);
    const float len = glm::length(cross);
    const glm::vec3 n = len > 1e-8f ? cross / len : glm::vec3{0.0f, 1.0f, 0.0f};
    const auto base = static_cast<uint32_t>(m.vertices.size());
    m.vertices.push_back({a, n, {0.0f, 0.0f}, color});
    m.vertices.push_back({b, n, {0.0f, 0.0f}, color});
    m.vertices.push_back({c, n, {0.0f, 0.0f}, color});
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
}

void quad(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
          uint32_t color) {
    tri(m, a, b, c, color);
    tri(m, a, c, d, color);
}


void append_transformed(MeshData& dst, const MeshData& src, glm::vec3 translation,
                        float yaw, float scale) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const auto base = static_cast<uint32_t>(dst.vertices.size());
    dst.vertices.reserve(dst.vertices.size() + src.vertices.size());
    for (const platform::Vertex& v : src.vertices) {
        platform::Vertex out = v;
        const glm::vec3 p = v.position * scale;
        out.position = {c * p.x + s * p.z + translation.x, p.y + translation.y,
                        -s * p.x + c * p.z + translation.z};
        out.normal = {c * v.normal.x + s * v.normal.z, v.normal.y,
                      -s * v.normal.x + c * v.normal.z};
        dst.vertices.push_back(out);
    }
    dst.indices.reserve(dst.indices.size() + src.indices.size());
    for (const uint32_t i : src.indices) {
        dst.indices.push_back(base + i);
    }
}

MeshData build_scatter_mesh(math::ScatterSpecies species) {
    switch (species) {
    case math::ScatterSpecies::OakTree: return oak();
    case math::ScatterSpecies::PineTree: return pine();
    case math::ScatterSpecies::BirchTree: return birch();
    case math::ScatterSpecies::Bush: return bush();
    case math::ScatterSpecies::Stone: return stone();
    }
    return {};
}

MeshData build_site_mesh(uint32_t mesh_id) {
    switch (mesh_id) {
    case 1: return dwelling();
    case 2: return trader();
    case 3: return tavern();
    case 4: return barn();
    case 5: return shrine();
    case 6: return dungeon_entrance();
    case 7: return tower_ruin();
    default: return {};
    }
}

} // namespace dfn::render
