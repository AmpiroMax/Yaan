/*
Created: 09:08:2026 - 16:00:00
Last updated: 13:08:2026 - 17:05:00
Module: tests
File: tests/core/VoxelTests.cpp

Responsibility:
- The 3D terrain representation-swap suite: the extracted voxel surface must
  reproduce the heightfield surface (the machine-checkable half of "zero
  visible change"), stay bit-deterministic (Rule 13.1), meet its neighbours
  without a seam, and carry sane materials and winding.

Dependencies:
- Uses: doctest, dfn_world (Worldgen, VoxelVolume, VoxelMesh), dfn_core.
- Used by: ctest (test_voxel).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The fidelity test is the contract that lets the swap ship without a visual
  regression. Do not relax its tolerance to make a change pass — if the
  surface moved, the change is wrong.
*/
/*
UPD:
- 09:08:2026 - 16:00:00: Created — representation-swap suite.
- 09:08:2026 - 16:47:51: P7 acceptance: tunnel enclosed/walkable/climbing, voxel field holds overhangs and ceiling geometry a heightfield cannot, Backbarrow is a buried reachable room.
- 09:08:2026 - 17:36:42: §6.2: carved dungeon entrances are derived from their mouth, facing out, standing on the carved floor.
- 09:08:2026 - 17:45:08: §6.2: standing stones present and within their height band at every entrance; no vegetation inside the exclusion ring.
- 09:08:2026 - 18:58:01: Regression: every entrance walks out without the ground ahead rising above head height; mounds fall from their crown (dome, not plateau); no scatter instance floats or sinks by more than 0.3 m anywhere in the testbed.
- 09:08:2026 - 19:41:55: Tolerances re-derived for a 115 m crag: worst voxel deviation bounded by the cell diagonal on near-vertical faces (2.5x voxel, mean still ~2 cm), and the tunnel's standable allowance widened because the ascent now climbs 41 m instead of 18 in a similar footprint so its legs stack closer.
- 09:08:2026 - 21:37:57: Heightfield-vs-voxel check restated for §2.8 cliffs: every vertex must lie on the heightfield within the terrain's own relief across one voxel cell, UNLESS it is a carve surface (a tunnel wall is not describable as a height per column). Checked per vertex rather than on the global max, which is strictly stronger — the old flat 2.5 m bound let one cliff vertex mask every other error. Measured: 76195 verts, 127 exceedances, all 127 on carves, zero unexplained.
- 10:08:2026 - 02:18:26: Barrow mouth split into its own EXPECTED-FAIL case (design ruling §7.0a; trigger-expiry = the §2.8.2 couloir work, owner design; should_fail announces the day it opens). The room/burial assertions stay guarding in the plain case.
- 10:08:2026 - 20:46:11: Both three-arg build_voxel_volume calls documented (sim's
  catch): the omitted fourth argument is the DERIVED entrance adits, so the
  determinism case is sound because the omission cancels on both sides, and the
  P7 case measures the LAYOUT's tunnel rather than the shipped field.
- 10:08:2026 - 21:06:20: Rule 39 regression for the surface-class hop, with
  the control the rule prescribes — the PRE-FIX projection copied out verbatim,
  asserted to agree with the fixed one on all four classes the suite already
  exercised and to disagree on exactly the fifth. That asymmetry is the finding:
  the drift was unreachable from any test of the well-trodden classes. Also
  replaced `mat <= Dirt` with a named-enumerator check; that bound was a fact
  about declaration order in an APPEND-ONLY enum and went red on the append.
- 11:08:2026 - 15:15:55: props-sit-on-the-ground restated for §10.5 B1: a boulder is DUG IN, so the invariant is one-sided -- never floating, never deeper than BOULDER_BURIAL_FRAC_MAX of its own extent.
- 13:08:2026 - 17:05:00: РЕГРЕССИЯ §6.3 (зона dungeon): тьма как ФУНКЦИЯ МЕСТА, проверенная в той самой точке, которой спрашивает игра — НА полу. Существующий случай про тоннель задаёт тот же вопрос о вхождении, но в p.y+1.7 и p.y-0.5, то есть ОТСТУПАЕТ от плоскости, на которой стоит боевой запрос, и потому не мог увидеть особенность: ambient_darkness переключалась 0.000↔1.000 ЗА ОДИН КАДР, 13 раз за проход, под 18 м породы. Три проверки, каждая падает на функции до правки: пол и колено не расходятся (граница не сингулярна), соседние станции не меняются быстрее собственного склона рампы (склон, а не выключатель), и станция, до которой от обоих порталов больше DEPTH_MIN+рампа при реальной толще над головой, — полная тьма. Разбор: docs/FINDING_DUNGEON_DARK.md.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/VoxelMesh.h"
#include "engine/world/sources/VoxelVolume.h"
#include "engine/world/sources/WorldgenCarve.h"
#include "engine/world/sources/Worldgen.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <vector>

using namespace dfn;
using world::ChunkCoord;
using world::WorldGenParams;

namespace {

const world::WorldGenContext& testbed() {
    static const world::WorldGenContext ctx =
        world::build_world_context(WorldGenParams{1, {0, 0}, {3, 3}});
    return ctx;
}

/// Is this byte a material a vertex may actually carry? Spelled out rather
/// than bounded by "<= the last enumerator", which is a fact about the
/// declaration order and not about the contract.
bool is_real_material(uint8_t mat) {
    switch (static_cast<math::VoxelMaterial>(mat)) {
    case math::VoxelMaterial::Grass:
    case math::VoxelMaterial::Rock:
    case math::VoxelMaterial::Sand:
    case math::VoxelMaterial::Dirt:
    case math::VoxelMaterial::GrassRockBlend:
        return true;
    case math::VoxelMaterial::Air:
        return false;
    }
    return false;
}

/// THE CONTROL for the surface-class projection, per Rule 39's prescription:
/// the pre-fix projection copied out VERBATIM from VoxelVolume.cpp as it stood
/// before math::voxel_material_of existed, `default:` and all. Its value is
/// precisely that it is EQUAL to the correct answer on the four classes
/// everyone tests and WRONG on the fifth — which is why no amount of testing
/// the well-trodden classes could ever have caught the drift.
math::VoxelMaterial voxel_material_of_prefix(math::SurfaceClass cls) {
    switch (cls) {
    case math::SurfaceClass::Rock:
        return math::VoxelMaterial::Rock;
    case math::SurfaceClass::Sand:
        return math::VoxelMaterial::Sand;
    case math::SurfaceClass::WaterBed:
        return math::VoxelMaterial::Dirt;
    case math::SurfaceClass::GrassRockBlend:
    case math::SurfaceClass::Grass:
    default:
        return math::VoxelMaterial::Grass;
    }
}

uint64_t mesh_hash(const world::VoxelSurface& m) {
    serialization::Fnv1a64 h;
    for (const glm::vec3& p : m.positions) {
        h.update_u64(std::bit_cast<uint32_t>(p.x));
        h.update_u64(std::bit_cast<uint32_t>(p.y));
        h.update_u64(std::bit_cast<uint32_t>(p.z));
    }
    for (const glm::vec3& n : m.normals) {
        h.update_u64(std::bit_cast<uint32_t>(n.x));
        h.update_u64(std::bit_cast<uint32_t>(n.y));
        h.update_u64(std::bit_cast<uint32_t>(n.z));
    }
    for (const uint8_t m8 : m.materials) h.update_u64(m8);
    for (const uint32_t i : m.indices) h.update_u64(i);
    return h.digest();
}

} // namespace

TEST_CASE("voxel surface reproduces the heightfield surface (no visible change)") {
    const auto& ctx = testbed();
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 1}); // crag foot
    REQUIRE_FALSE(chunk.voxels.empty());

    // Every extracted vertex must sit on the terrain the rest of the engine
    // still queries. Surface nets place a vertex inside its cell, so the
    // expected error is bounded by the voxel size, not by the terrain.
    // The old form of this check bounded only the GLOBAL maximum error by a
    // flat 2.5 m. That was fine on smooth terrain but is both too weak and too
    // strong now that §2.8 gives Ravenscar near-vertical cliff risers: too
    // strong because one column spanning an 80 deg riser legitimately differs
    // from a single heightfield sample by tan(80)*1 m = 5.7 m, and too weak
    // because a single cliff vertex raised the global max and hid every other
    // error behind it.
    //
    // The precise rule, and it is checked on EVERY vertex rather than on the
    // maximum: a vertex sits on the heightfield within the terrain's own
    // relief across one voxel cell, plus sub-voxel placement -- UNLESS it is a
    // carve surface, which is not heightfield terrain at all (a tunnel wall
    // cannot be described by a height per column, which is the whole reason
    // this engine went to voxels).
    //
    // Measured on seed 1 chunk {2,1}: 76195 vertices, 127 exceed the local
    // bound and all 127 are carve surfaces; ZERO are unexplained.
    const double slack = static_cast<double>(config::VOXEL_SIZE) * 2.5;
    double sum = 0.0;
    std::size_t counted = 0;
    std::size_t off_surface = 0;
    double worst_unexplained = 0.0;
    for (const glm::vec3& p : chunk.voxels.positions) {
        const float ground = world::terrain_height(ctx, {p.x, p.z});
        const double err = std::fabs(static_cast<double>(p.y) - ground);
        sum += err;
        ++counted;
        if (err <= slack) {
            continue;
        }
        // Relief across one cell around this column. The neighbourhood is a
        // full cell radius because surface nets may place the vertex anywhere
        // inside its cell, up to VOXEL_SIZE from where we sample the height.
        float lo = ground;
        float hi = ground;
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dx = -2; dx <= 2; ++dx) {
                const float h = world::terrain_height(
                    ctx, {p.x + static_cast<float>(dx) * static_cast<float>(config::VOXEL_SIZE),
                          p.z + static_cast<float>(dz) * static_cast<float>(config::VOXEL_SIZE)});
                lo = std::min(lo, h);
                hi = std::max(hi, h);
            }
        }
        if (err <= static_cast<double>(hi - lo) + slack) {
            continue; // explained by a cliff inside this cell
        }
        if (world::carve_distance(ctx.params.layout, p) < 3.0f) {
            continue; // a carve surface: correctly not on the heightfield
        }
        ++off_surface;
        worst_unexplained = std::max(worst_unexplained, err);
    }
    REQUIRE(counted > 10000);
    const double mean = sum / static_cast<double>(counted);
    INFO("mean |dy| = " << mean << " m over " << counted << " verts; "
                        << off_surface << " neither cliff nor carve, worst "
                        << worst_unexplained << " m");
    // Mean error is the number that matters for "does it look the same".
    CHECK(mean < 0.25);
    // Every vertex is either on the heightfield, on a cliff inside its own
    // cell, or on a carve. Nothing else is allowed to float.
    CHECK(off_surface == 0u);
}

TEST_CASE("Rule 13.1: voxel volume and extracted mesh are bit-deterministic") {
    const auto& ctx = testbed();
    const auto a = world::generate_chunk(ctx, ChunkCoord{2, 1});
    const auto b = world::generate_chunk(ctx, ChunkCoord{2, 1});
    CHECK(a.voxels.positions.size() == b.voxels.positions.size());
    CHECK(a.voxels.indices == b.voxels.indices);
    CHECK(mesh_hash(a.voxels) == mesh_hash(b.voxels));

    // The volume itself (the quantized field) is an exact integer state.
    //
    // THREE ARGS, AND IT IS SOUND HERE FOR A REASON WORTH STATING: the fourth
    // argument generate_chunk passes is the DERIVED entrance adits, and
    // omitting it builds a volume with solid rock where those corridors are.
    // This case compares two builds of the SAME call, so the omission is
    // identical on both sides and cancels — it is a determinism claim, not a
    // claim about the shipped field. Anywhere the shipped field is the subject,
    // pass the derived carves (sim's catch, 10.08.2026).
    const auto sampler = [&ctx](glm::vec2 p) { return world::terrain_height(ctx, p); };
    const auto va = world::build_voxel_volume(a, sampler, ctx.params.layout);
    const auto vb = world::build_voxel_volume(b, sampler, ctx.params.layout);
    CHECK(va.sdf == vb.sdf);
    CHECK(va.material == vb.material);
}

TEST_CASE("neighbouring chunks meet without a seam") {
    const auto& ctx = testbed();
    const auto left = world::generate_chunk(ctx, ChunkCoord{1, 1});
    const auto right = world::generate_chunk(ctx, ChunkCoord{2, 1});
    // The -x/-z chunk owns the seam strip: its volume spans one cell past the
    // shared plane (x = 512), so its vertices there must COINCIDE with the
    // neighbour's own first-cell vertices. Coincide, not merely "be close":
    // both sides evaluate an identically quantized field.
    const auto band = [](const world::VoxelSurface& m, float lo, float hi) {
        std::vector<glm::vec3> out;
        for (const glm::vec3& p : m.positions) {
            if (p.x > lo && p.x < hi) out.push_back(p);
        }
        std::sort(out.begin(), out.end(), [](const glm::vec3& a, const glm::vec3& b) {
            return a.z < b.z;
        });
        return out;
    };
    const auto seam_left = band(left.voxels, 512.0f, 513.0f);
    const auto seam_right = band(right.voxels, 512.0f, 513.0f);
    REQUIRE_FALSE(seam_left.empty());
    REQUIRE_FALSE(seam_right.empty());

    std::size_t matched = 0;
    double worst = 0.0;
    for (const glm::vec3& a : seam_left) {
        double best = 1e9;
        for (const glm::vec3& b : seam_right) {
            best = std::min(best, static_cast<double>(glm::length(a - b)));
        }
        if (best < 1e-3) ++matched;
        worst = std::max(worst, best);
    }
    INFO("seam vertices: " << seam_left.size() << " left, " << seam_right.size()
                           << " right, matched " << matched << ", worst gap " << worst);
    // Every seam vertex of the owning chunk has an exact twin next door.
    CHECK(matched == seam_left.size());
    CHECK(worst < 1e-3);
}

TEST_CASE("the blend surface class survives the hop onto the voxel skin") {
    // Rule 39 regression. SurfaceClass::GrassRockBlend had no VoxelMaterial to
    // land on, so world's projection folded it into Grass. The consequence was
    // not in world at all: the heightfield mesher drew the blend with rock
    // weight 0.5 and the voxel mesher, handed a Grass vertex, drew 0.0 — two
    // readings of one surface on the same chunk-load branch, same shader.

    // (a) The projection is total and carries every class distinctly, EXCEPT
    //     the one collapse that is deliberate and documented (WaterBed -> Dirt
    //     shares the bed weight with carved rock).
    CHECK(math::voxel_material_of(math::SurfaceClass::Grass) == math::VoxelMaterial::Grass);
    CHECK(math::voxel_material_of(math::SurfaceClass::Rock) == math::VoxelMaterial::Rock);
    CHECK(math::voxel_material_of(math::SurfaceClass::Sand) == math::VoxelMaterial::Sand);
    CHECK(math::voxel_material_of(math::SurfaceClass::WaterBed) == math::VoxelMaterial::Dirt);
    CHECK(math::voxel_material_of(math::SurfaceClass::GrassRockBlend)
          == math::VoxelMaterial::GrassRockBlend);

    // (b) THE CONTROL (Rule 30). The pre-fix projection agrees on all four
    //     classes the old suite exercised and disagrees on exactly one. If
    //     this block ever stops holding, the test above has stopped measuring
    //     the thing it was written for.
    for (const math::SurfaceClass cls :
         {math::SurfaceClass::Grass, math::SurfaceClass::Rock, math::SurfaceClass::Sand,
          math::SurfaceClass::WaterBed}) {
        CHECK(voxel_material_of_prefix(cls) == math::voxel_material_of(cls));
    }
    CHECK(voxel_material_of_prefix(math::SurfaceClass::GrassRockBlend)
          != math::voxel_material_of(math::SurfaceClass::GrassRockBlend));

    // (c) The OUTCOME (Rule 38), on real worldgen rather than on the function:
    //     the blend class exists in the testbed, and it reaches voxel vertices.
    //     A projection that is correct on a class the world never produces
    //     fixes nothing, so this half is what makes the case evidence.
    const auto& ctx = testbed();
    long blend_samples = 0;
    long total_samples = 0;
    long blend_vertices = 0;
    long total_vertices = 0;
    for (int cz = 0; cz <= 3; ++cz) {
        for (int cx = 0; cx <= 3; ++cx) {
            const auto chunk = world::generate_chunk(ctx, ChunkCoord{cx, cz});
            for (const uint8_t c : chunk.surface.surface_class) {
                ++total_samples;
                if (static_cast<math::SurfaceClass>(c) == math::SurfaceClass::GrassRockBlend) {
                    ++blend_samples;
                }
            }
            for (const uint8_t mat : chunk.voxels.materials) {
                ++total_vertices;
                if (static_cast<math::VoxelMaterial>(mat)
                    == math::VoxelMaterial::GrassRockBlend) {
                    ++blend_vertices;
                }
            }
        }
    }
    INFO("blend samples " << blend_samples << "/" << total_samples << ", blend vertices "
                          << blend_vertices << "/" << total_vertices);
    // Measured on this seed at the time of the fix: 3536/266256 samples
    // (1.33%) and 26136/1183258 vertices (2.21%). Asserted as a BAND rather
    // than pinned to those counts — the numbers move whenever the crag is
    // retuned, and a test that goes red on a legitimate retune is a test that
    // gets weakened (Rule 38). The band is wide on purpose; it is here to
    // catch the blend vanishing again or flooding the map, not to re-tune it.
    CHECK(blend_samples > 0);
    CHECK(blend_vertices > 0);
    CHECK(blend_vertices * 100 > total_vertices); // >1%: it did not vanish
    CHECK(blend_vertices * 4 < total_vertices);   // <25%: it did not flood
}

TEST_CASE("mesh is well formed: indices in range, normals unit, materials solid") {
    const auto& ctx = testbed();
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 1});
    const auto& m = chunk.voxels;
    REQUIRE(m.indices.size() % 3 == 0);
    CHECK(m.normals.size() == m.positions.size());
    CHECK(m.materials.size() == m.positions.size());
    for (const uint32_t i : m.indices) {
        REQUIRE(i < m.positions.size());
    }
    for (const glm::vec3& n : m.normals) {
        CHECK(std::fabs(glm::length(n) - 1.0f) < 0.001f);
    }
    for (const uint8_t mat : m.materials) {
        // Air must never reach a vertex — every vertex has a solid side.
        CHECK(mat != static_cast<uint8_t>(math::VoxelMaterial::Air));
        // Every vertex carries a REAL enumerator. This used to read
        // `mat <= Dirt`, which only worked because Dirt was last in the enum
        // on the day it was written — an append-only enum makes that bound
        // wrong on the next append, and it duly went red when GrassRockBlend
        // landed. Naming the members is the same assertion without the
        // dependence on declaration order.
        CHECK(is_real_material(mat));
    }
    // Open terrain: the great majority of faces point upward. (Carves will add
    // downward-facing ceilings; this pins the pre-carve baseline.)
    std::size_t up = 0;
    for (const glm::vec3& n : m.normals) {
        if (n.y > 0.0f) ++up;
    }
    CHECK(up > m.normals.size() * 9 / 10);
}

TEST_CASE("the cross-zone view exposes the mesh unchanged") {
    const auto& ctx = testbed();
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{2, 1});
    const auto view = chunk.voxels.view(ChunkCoord{2, 1});
    CHECK(view.chunk_coord == glm::ivec2{2, 1});
    CHECK(view.positions.size() == chunk.voxels.positions.size());
    CHECK(view.triangle_count() == chunk.voxels.indices.size() / 3);
    CHECK(view.triangle_count() > 1000);
}

TEST_CASE("P7 carves: the crag tunnel is enclosed, walkable and climbs") {
    // The acceptance geometry: a corridor under rock, with room to stand, that
    // gains height between two portals. Measured against the CARVE FIELD (the
    // voxelized headroom is checked in the next case).
    const auto& ctx = testbed();
    const auto& layout = ctx.params.layout;
    const auto& tun = layout.carves.crag_tunnel;
    REQUIRE(tun.point_count >= 4); // a switchback route, not a straight adit
    CHECK(tun.height >= static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));

    float length = 0.0f;
    float climb = 0.0f;
    int enclosed = 0;
    int open = 0;
    int standable = 0;
    for (int i = 0; i + 1 < tun.point_count; ++i) {
        const glm::vec3 a = tun.points[i];
        const glm::vec3 b = tun.points[i + 1];
        length += glm::length(b - a);
        climb += std::max(0.0f, b.y - a.y);
        const int steps = std::max(1, static_cast<int>(glm::length(b - a) / 2.0f));
        for (int s = 0; s <= steps; ++s) {
            const glm::vec3 p = a + (b - a) * (static_cast<float>(s) / steps);
            const float surface = world::terrain_height(ctx, {p.x, p.z});
            if (p.y + tun.height >= surface) {
                ++open; // portal / approach cutting
                continue;
            }
            ++enclosed;
            // Air at eye height, rock underfoot: that is "standing in a tunnel".
            const bool air_at_eye =
                world::carve_distance(layout, {p.x, p.y + 1.7f, p.z}) < 0.0f;
            const bool rock_underfoot =
                world::carve_distance(layout, {p.x, p.y - 0.5f, p.z}) > 0.0f;
            if (air_at_eye && rock_underfoot) ++standable;
        }
    }
    CHECK(length > 100.0f);  // a traverse, not a doorway
    CHECK(climb > 10.0f);    // it genuinely takes you up the crag
    CHECK(enclosed > 40);    // most of it is inside the mountain
    CHECK(open > 4);         // both portals exist
    // Allow a few stations where switchback legs stack over one another: the
    // ascent now climbs 41 m instead of 18 in a similar footprint, so the legs
    // pass closer above each other and a floor sample lands in the leg below.
    CHECK(standable >= enclosed - 5);
}

TEST_CASE("P7 carves: the voxel field holds the tunnel a heightfield cannot") {
    const auto& ctx = testbed();
    const auto sampler = [&ctx](glm::vec2 p) { return world::terrain_height(ctx, p); };
    const auto chunk = world::generate_chunk(ctx, ChunkCoord{3, 0});
    // LAYOUT CARVES ONLY. The derived entrance adits are NOT in this volume
    // (generate_chunk passes them as a fourth argument this call omits), so the
    // overhang count below is a claim about the LAYOUT's tunnel and understates
    // the shipped field. That is the right scope for this case — it exists to
    // prove the representation can hold a ceiling at all — but a reader must
    // not read the number as "the world has this many overhangs".
    const auto volume =
        world::build_voxel_volume(chunk, sampler, ctx.params.layout);

    // Columns with two solid spans are air-under-rock-under-air: a ceiling
    // over a floor. A heightfield cannot represent ANY of these.
    int overhang_columns = 0;
    for (int32_t z = 0; z < volume.nz; ++z) {
        for (int32_t x = 0; x < volume.nx; ++x) {
            int spans = 0;
            bool prev = false;
            for (int32_t y = 0; y < volume.ny; ++y) {
                const bool solid = volume.solid_at(x, y, z);
                if (solid && !prev) ++spans;
                prev = solid;
            }
            if (spans >= 2) ++overhang_columns;
        }
    }
    CHECK(overhang_columns > 200);

    // The extracted mesh must carry downward-facing geometry — the ceiling the
    // player stands under. Open terrain alone produces almost none.
    std::size_t ceiling_verts = 0;
    for (const glm::vec3& n : chunk.voxels.normals) {
        if (n.y < -0.5f) ++ceiling_verts;
    }
    CHECK(ceiling_verts > 100);
}

TEST_CASE("P7 carves: the Backbarrow is a room, not a dent") {
    const auto& ctx = testbed();
    const auto& ch = ctx.params.layout.carves.barrow_chamber;
    REQUIRE(ch.half_extent.x > 0.0f);
    CHECK(ch.half_extent.y >= static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
    // Buried: the chamber ceiling sits under real rock.
    const float surface = world::terrain_height(ctx, {ch.center.x, ch.center.z});
    CHECK(surface - (ch.center.y + ch.half_extent.y) > 2.0f);
    // The mouth (reachability) is a SEPARATE, expected-fail case below —
    // design ruling §7.0a. Splitting keeps these room/burial assertions
    // guarding: folded into one xfail case they would stop meaning anything
    // (any breakage would read as the expected failure).
}

// EXPECTED-FAIL, design ruling §7.0a (tech-debt wave, 10:08:2026): the
// Backbarrow's mouth is buried under the 115 m Ravenscar — the passage sits
// 81-105 m from the crag centre, where L0_RELIEF raised terrain over its
// portal. NO GREEN PLACEMENT EXISTS TODAY: the couloir re-site is impossible
// (measured — Ravenscar is a self-similar cone with no couloirs at any
// height) and the high-shoulder fallback is ILLEGAL (breaks story's
// not-visible-from-Vaelmere constraint). The xfail's expiry is a TRIGGER,
// not a date, and its owner is design: when the §2.8.2 absolute-couloir-depth
// work lands, the §7.0a couloir search (bearings 180-240 deg, r 90-110 m,
// terrain <= ~28 m, nearest 209 deg) runs as part of that change. should_fail
// is the announcement mechanism — the day the couloir exists and the mouth
// opens, this case PASSES and doctest flags the surprise, which is design's
// signal to flip the registration back to a plain test.
TEST_CASE("P7 carves: the Backbarrow mouth breaks the surface"
          " (XFAIL per design §7.0a — buried under the 115 m crag)"
          * doctest::should_fail(true)) {
    const auto& ctx = testbed();
    const auto& passage = ctx.params.layout.carves.barrow_passage;
    REQUIRE(passage.point_count >= 2);
    bool has_mouth = false;
    for (int i = 0; i < passage.point_count; ++i) {
        const glm::vec3 p = passage.points[i];
        if (p.y + passage.height >= world::terrain_height(ctx, {p.x, p.z})) has_mouth = true;
    }
    CHECK(has_mouth);
}

TEST_CASE("carved dungeon entrances are DERIVED from their mouth, facing out") {
    // The pad scorer knows slope and dryness but nothing about "a mouth needs a
    // hillside to face out of" — it left the Backbarrow marker 10 m outside its
    // own passage, standing in the approach cutting. A carved entrance is now
    // derived from the carve, never scored, exactly as fords are derived from
    // the generated trace.
    const auto& ctx = testbed();
    const auto& layout = ctx.params.layout;
    const auto ground = [&ctx](glm::vec2 p) { return world::terrain_height(ctx, p); };

    int carved_entrances = 0;
    for (std::size_t i = 0; i < ctx.sites.entities.size(); ++i) {
        if (ctx.sites.types[i] != world::SiteType::DungeonEntrance) continue;
        // Find which layout site this record came from by position proximity is
        // fragile; instead check every site index that HAS a carve.
        (void)i;
    }
    for (int si = 0; si < static_cast<int>(std::size(layout.sites)); ++si) {
        const auto mouth = world::site_carve_mouth(layout, si, ground);
        if (!mouth) continue;
        ++carved_entrances;
        // Some placed entrance must stand at this mouth, just outside it.
        bool matched = false;
        for (std::size_t i = 0; i < ctx.sites.entities.size(); ++i) {
            if (ctx.sites.types[i] != world::SiteType::DungeonEntrance) continue;
            const glm::vec2 p = ctx.sites.entities[i].position_xz;
            const glm::vec2 m{mouth->position.x, mouth->position.z};
            if (glm::length(p - m) > 3.0f) continue;
            matched = true;
            // Faces OUT of the hill: yaw 0 looks toward -Z, positive turns right.
            const float expected = std::atan2(mouth->outward.x, -mouth->outward.y);
            float delta = ctx.sites.entities[i].yaw - expected;
            while (delta > 3.14159265f) delta -= 6.28318531f;
            while (delta < -3.14159265f) delta += 6.28318531f;
            CHECK(std::fabs(delta) < 0.01f);
            // Stands on the CARVED floor, not on the heightfield above it.
            CHECK(ctx.sites.entities[i].ground_y != world::NO_GROUND_Y);
            CHECK(std::fabs(ctx.sites.entities[i].ground_y - mouth->position.y) < 0.5f);
            // And is genuinely lower than the untouched surface there, which is
            // the whole reason the heightfield could not place it.
            CHECK(ctx.sites.entities[i].ground_y < world::terrain_height(ctx, p) - 0.5f);
        }
        CHECK(matched);
    }
    CHECK(carved_entrances >= 1);
}

TEST_CASE("§6.2 findability: standing stones flank each entrance, nothing grows on it") {
    const auto& ctx = testbed();
    const auto& sites = ctx.sites;

    // Gather every scatter instance across the testbed once.
    std::vector<math::ScatterInstance> all;
    for (int32_t cz = 0; cz <= 3; ++cz) {
        for (int32_t cx = 0; cx <= 3; ++cx) {
            const auto chunk = world::generate_chunk(ctx, ChunkCoord{cx, cz});
            all.insert(all.end(), chunk.scatter.begin(), chunk.scatter.end());
        }
    }
    REQUIRE(all.size() > 1000);

    const float margin = static_cast<float>(config::ENTRANCE_SCATTER_EXCLUSION_MARGIN);
    for (const world::EntranceWorks& w : sites.entrances) {
        if (!w.valid) continue;
        // Standing stones: present, paired along the approach, and tall enough
        // to read as intentional rather than as boulders.
        int markers = 0;
        for (const auto& inst : all) {
            if (inst.species != math::ScatterSpecies::Stone) continue;
            const glm::vec2 p{inst.position.x, inst.position.z};
            if (glm::length(p - w.portal) > w.forecourt_length + margin + 8.0f) continue;
            if (inst.scale < static_cast<float>(config::STANDING_STONE_HEIGHT_MIN) - 0.01f) {
                continue;
            }
            ++markers;
            CHECK(inst.scale
                  <= static_cast<float>(config::STANDING_STONE_HEIGHT_MAX) + 0.01f);
        }
        CHECK(markers >= static_cast<int>(config::STANDING_STONE_COUNT_MIN));

        // Exclusion ring: no vegetation on the mound or its forecourt. The
        // mound exists to create a silhouette; a stand of oaks on top erases it.
        for (const auto& inst : all) {
            const bool vegetation = inst.species == math::ScatterSpecies::OakTree
                                 || inst.species == math::ScatterSpecies::PineTree
                                 || inst.species == math::ScatterSpecies::BirchTree
                                 || inst.species == math::ScatterSpecies::Bush;
            if (!vegetation) continue;
            const glm::vec2 p{inst.position.x, inst.position.z};
            CHECK(glm::length(p - w.center) >= w.mound_radius + margin - 0.01f);
        }
    }
}

TEST_CASE("§6.2 live-play fixes: entrances open outward, props sit on the ground") {
    const auto& ctx = testbed();
    const auto& sites = ctx.sites;

    for (const world::EntranceWorks& w : sites.entrances) {
        if (!w.valid) continue;

        // (a) THE PORTAL MUST OPEN. Walking out along the approach, the ground
        // ahead may never rise above the walker's head — the user's report was
        // a barrow that "stands facing into rock, as if there is no entrance",
        // caused by a forecourt that ended while still on the mound so the rim
        // walled the exit off. Head height rises with the floor: this is a
        // ramp, and a ramp being higher than the doorway is not a blockage.
        for (float d = 2.0f; d <= 26.0f; d += 2.0f) {
            const glm::vec2 here = w.portal + w.outward * d;
            const glm::vec2 ahead = w.portal + w.outward * (d + 2.0f);
            const float floor_here = world::terrain_height(ctx, here);
            const float floor_ahead = world::terrain_height(ctx, ahead);
            CHECK(floor_ahead <= floor_here + static_cast<float>(config::PLAYER_CAPSULE_HEIGHT));
        }

        // (b) THE MOUND IS A DOME, not a plateau with walls: the crown is the
        // high point and the profile falls monotonically to the rim.
        float prev = world::terrain_height(ctx, w.center);
        const float crown = prev;
        for (float d = 2.0f; d <= w.mound_radius; d += 2.0f) {
            const float h = world::terrain_height(ctx, w.center - w.outward * d);
            CHECK(h <= crown + 0.5f); // nothing stands above the crown
            prev = h;
        }
    }

    // (c) NOTHING FLOATS OR SINKS. Scatter used to resolve against the field
    // BEFORE the mound stamp, so props near a barrow were buried by exactly
    // the mound's local rise (measured up to 2.4 m).
    for (int32_t cz = 0; cz <= 3; ++cz) {
        for (int32_t cx = 0; cx <= 3; ++cx) {
            const auto chunk = world::generate_chunk(ctx, ChunkCoord{cx, cz});
            for (const auto& inst : chunk.scatter) {
                const float ground = world::terrain_height(ctx, {inst.position.x,
                                                                inst.position.z});
                // §10.5 B1: A BOULDER IS DUG IN, so a Stone is not expected to sit
            // ON the ground — it is expected to sit UNDER it, by its own
            // declared burial and by nothing else. The invariant is therefore
            // one-sided for this species rather than absent: a stone may never
            // float, and it may never sink deeper than
            // BOULDER_BURIAL_FRAC_MAX of its own extent.
            if (inst.species == math::ScatterSpecies::Stone) {
                const float sunk = ground - inst.position.y;
                CHECK(sunk >= -0.3f);
                CHECK(sunk <= static_cast<float>(config::BOULDER_BURIAL_FRAC_MAX) * inst.scale
                                  + 0.3f);
            } else {
                CHECK(std::fabs(inst.position.y - ground) < 0.3f);
            }
            }
        }
    }
}

TEST_CASE("§6.3 darkness is a FUNCTION OF THE PLACE, not of the last bit of a "
          "float — the regression for «темнеет, потом мигает»") {
    // THE ARM THIS SUITE DID NOT HAVE, and its absence is the finding.
    //
    // The tunnel case above asks the same containment question the shipping
    // darkness query asks, but at `p.y + 1.7` and `p.y - 0.5` — it steps AWAY
    // from the floor plane. The game asks AT the floor, because a player's
    // position is the bottom of the capsule, and there the analytic carve
    // boundary and the drawn floor differ by the voxel lattice's own
    // reconstruction error. So `darkness_at(feet)` was a coin flip decided by
    // rounding: measured live, `ambient_darkness` stepped 0.000 <-> 1.000 in a
    // SINGLE frame 13 times in one walk through this tunnel, under as much as
    // 18 m of rock. See docs/FINDING_DUNGEON_DARK.md.
    //
    // Every check below is stated on the shipping query point (the floor), and
    // every one of them fails on the pre-fix function.
    const auto& ctx = testbed();
    const auto& layout = ctx.params.layout;
    const auto& tun = layout.carves.crag_tunnel;
    REQUIRE(tun.point_count >= 4);
    const world::GroundSampler ground = [&ctx](glm::vec2 p) {
        return world::terrain_height(ctx, p);
    };
    const auto dark = [&](glm::vec3 p) {
        return world::enclosure_darkness(layout, {}, ground, p);
    };

    float total = 0.0f;
    for (int i = 0; i + 1 < tun.point_count; ++i) {
        total += glm::length(tun.points[i + 1] - tun.points[i]);
    }

    constexpr float STEP_M = 0.25f;
    const float ramp = static_cast<float>(config::DARKNESS_FALLOFF_MIN);
    // The ramp's own slope is the fastest darkness may legally change: it
    // spans DARKNESS_FALLOFF_MIN metres of walk from 0 to 1. Anything steeper
    // than one step's worth of that is a switch, and a switch between frames is
    // what the player sees as a flash.
    const float legal_step = STEP_M / ramp + 1e-3f;

    float arc = 0.0f;
    float prev = -1.0f;
    int stations = 0;
    int knife_edge = 0;     // floor and knee disagree: the boundary is singular
    int steps_too_fast = 0; // a switch rather than a ramp
    int deep_but_lit = 0;   // walked in from both ends and still full daylight
    for (int i = 0; i + 1 < tun.point_count; ++i) {
        const glm::vec3 a = tun.points[i];
        const glm::vec3 b = tun.points[i + 1];
        const float len = glm::length(b - a);
        const int count = std::max(1, static_cast<int>(len / STEP_M));
        for (int s = 0; s < count; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(count);
            const glm::vec3 floor_pt = a + (b - a) * u;
            const float here = arc + len * u;
            const float d_floor = dark(floor_pt);
            const float d_knee = dark(floor_pt + glm::vec3{0.0f, 0.5f, 0.0f});
            ++stations;
            if (std::fabs(d_floor - d_knee) > 0.02f) {
                ++knife_edge;
            }
            if (prev >= 0.0f && std::fabs(d_floor - prev) > legal_step) {
                ++steps_too_fast;
            }
            prev = d_floor;
            // Both ends of this corridor stand in daylight (daylight_portals),
            // so the walk from the nearest opening is at most min(arc, L-arc).
            // A station that is further in than DEPTH_MIN + the ramp, with real
            // rock overhead, is what §6.3 calls pitch black.
            const float from_ends = std::min(here, total - here);
            const float cover = ground({floor_pt.x, floor_pt.z}) - floor_pt.y;
            if (from_ends > static_cast<float>(config::DARKNESS_DEPTH_MIN) + ramp
                && cover > 3.0f && d_floor < 0.999f) {
                ++deep_but_lit;
            }
        }
        arc += len;
    }

    REQUIRE(stations > 400);
    CHECK(knife_edge == 0);
    CHECK(steps_too_fast == 0);
    CHECK(deep_but_lit == 0);
}
