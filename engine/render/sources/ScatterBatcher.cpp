/*
Created: 09:08:2026 - 11:57:20
Last updated: 13:08:2026 - 20:55:00
Module: engine/render
File: engine/render/sources/ScatterBatcher.cpp

Responsibility:
- build_scatter_batches implementation: species mesh cache, world-space
  baking, micro tile assignment and bounding radii.

Key items:
- build_scatter_batches().

Dependencies:
- Uses: ScatterBatcher.h, ProcMesh.
- Used by: dfn_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Deterministic pure function; covered by ScatterBatcherTests.
*/
/*
UPD:
- 09:08:2026 - 11:57:20: Stage 3b — initial implementation.
- 09:08:2026 - 19:46:00: Flora generator integrated: trees build per-instance
  geometry via build_flora_mesh (variant by position, shape from
  analyse_neighbourhood), appended at scale 1.0 because maturity is
  already inside the mesh, and with NO ground sink now that they have a
  root flare. species_radius updated to flora's measured envelopes.
- 09:08:2026 - 19:54:00: birch footprint 2.4 -> 3.1 m (flora's crown fix gave
  the birch a real 6.1 m crown; the old radius was measured off a bald tree).
- 09:08:2026 - 20:21:13: Trees bake into TWO streams via flora's append_flora
  (opaque wood + alpha-cutout leaf cards). EDITED BY THE FLORA AGENT under an
  explicit lead-granted Rule 25 exception while render's zone was unowned;
  wiring only, no material or shader change.
- 09:08:2026 - 22:27:17: GROUND_SINK_FRAC now reads config::SCATTER_GROUND_SINK_FRAC
  (lead landed the row on sim's request): render draws the sink and sim builds
  collision from the same triangles, so the two cannot hold separate copies.
- 10:08:2026 - 01:47:53: species_radius TABLE DELETED — the tile bounding
  circle is measured over the baked vertices instead. The table under-covered
  what was actually drawn (stone 0.5 tabled vs ~0.78 built, birch 3.1 vs
  ~4.94), which is a pop-in bug, and a table that must agree with a mesh will
  disagree again; a measured radius cannot. Old values kept in the test suite
  as the failing control (Rule 30).
- 10:08:2026 - 12:12:40: routing asks flora_owns() instead of a hand-written three-tree
  predicate — that predicate is why §5.10's floor and §5.11's edge set drew as
  bare earth while every suite was green.
- 13:08:2026 - 20:10:00: DFN_FLORA_FORCE_LOD -- a verification hook at the one
  line that selects a flora LOD, added by the FLORA agent under an explicit
  lead-granted Rule 25 cut (render notified; the file's 09.08 entry records the
  same arrangement). NO SHIPPING BEHAVIOUR CHANGES: the default is still
  FloraLod::Full, byte for byte.
  WHY IT EXISTS. This line passes Full unconditionally and always has, so
  Reduced and Silhouette have never been drawn -- across the engine FloraLod
  appears outside flora's files exactly twice and both are Full. The hook makes
  the ladder's effect answerable BY A FRAME before any selection machinery is
  written, and the answer it gave is why no machinery was written: forcing the
  whole scatter to the far LOD moves the treeline's composition by nothing.
  Measured with the MATERIAL door (DFN_FLORA_ONLY), not a value threshold:
      wood coverage of the band   Full 72.1 %   far LOD 73.1 %
      card coverage of the band   Full 41.0 %   far LOD 41.0 %
  Cutting 60 % of every tree's branch segments does not thin the stand at all,
  because coverage is a UNION OVER DEPTH: at 6 m spacing there are dozens of
  trunk rows in any line of sight, so branches removed from one tree only
  uncover the tree behind it. A union of many sparse sets saturates.
  The far LOD remains worth wiring for COST -- 579 triangles against 1000, on
  13446 trees -- and that is render's call, not this hook's.
- 13:08:2026 - 20:55:00: STAMP CORRECTION ONLY, no code and no content change:
  this session's own UPD entries above were written AHEAD of the wall clock (one
  said 22:00 for work committed at 20:24) and are corrected against the commit
  times. Recorded rather than done silently -- a record whose stamps are
  invented cannot be put in order afterwards, and the entries it would mislead
  are this zone's own.
*/

#include "engine/render/sources/ScatterBatcher.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include "engine/render/sources/ProcFlora.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>

namespace dfn::render {

namespace {

// Sink fraction of the instance scale: hides the downhill gap under a mesh
// placed at the sample-point terrain height on sloped ground. TREES NO LONGER
// SINK AT ALL: the flora generator gives them a root flare reaching ~1 m below
// the model origin, which covers far more ground drop than this ever did, and
// sinking a tree only makes it shorter — the opposite of the point of the 4x
// height stage. Kept for bushes and stones, which have no flare.
//
// THE VALUE IS NOT RENDER'S ANY MORE. It was a literal here while it was
// cosmetic; it stopped being cosmetic when sim began building boulder
// collision from THESE triangles. Sink on one side and not the other and every
// rock's solid surface sits 12 cm above its visible one, with the player
// standing in the air. One number, two zones (Rule 14).
constexpr float GROUND_SINK_FRAC = static_cast<float>(config::SCATTER_GROUND_SINK_FRAC);

// THERE IS NO SPECIES RADIUS TABLE ANY MORE, ON PURPOSE. The tile bounding
// circle is MEASURED from the baked vertices after everything is appended
// (see the post-loop pass): a table that must agree with a mesh will disagree
// again — this one already had (stone 0.5 m tabled vs ~0.78 m built, birch
// 3.1 vs ~4.94 after flora's crown fix), and an under-covering radius is a
// pop-in bug: the tile culls while its geometry is still on screen. The test
// suite keeps the old tabled values as the Rule 30 control — the geometry
// they claimed to cover FAILS them.

// One line per species, once per process — a per-instance warning would print
// hundreds of thousands of times and be scrolled past, which is the same as
// silence.
void report_missing_species(size_t ordinal) {
    static std::set<size_t> reported;
    if (!reported.insert(ordinal).second) {
        return;
    }
    std::fprintf(stderr,
                 "[render] SCATTER SPECIES %zu HAS NO MESH — worldgen places "
                 "instances of it and they draw as NOTHING. Add it to "
                 "build_scatter_mesh (ProcMesh.cpp) or to the flora path.\n",
                 ordinal);
}

} // namespace

ScatterBatches build_scatter_batches(std::span<const math::ScatterInstance> instances,
                                     glm::vec2 chunk_origin, float chunk_size,
                                     uint32_t micro_tiles_per_axis) {
    ScatterBatches out;
    if (instances.empty() || micro_tiles_per_axis == 0 || chunk_size <= 0.0f) {
        return out;
    }

    // Species meshes built once per call (cheap; caching across calls is the
    // caller's option — batches dominate the cost anyway).
    //
    // THE LENGTH IS NOT A CONSTANT, AND THAT IS THE FIX, NOT A STYLE CHOICE.
    // `math::ScatterSpecies` is an APPEND-ONLY enum in core's zone; this was a
    // `std::array<MeshData, 5>` indexed by the ordinal, which was true on the
    // day it was written and became a heap-corrupting out-of-bounds WRITE the
    // moment core added the §5.10 forest floor and §5.11 edge set (5 species ->
    // 18). It did not crash where it was wrong: it scribbled over whatever
    // followed on the stack and aborted later inside an unrelated vector
    // assignment, so the backtrace named this function and no line of it looked
    // suspicious. A sibling zone must never mirror an append-only enum with a
    // fixed length — grow to the ordinal instead, and the whole failure mode
    // stops being representable.
    std::vector<MeshData> species_mesh;
    std::vector<char> built;

    const auto mesh_of = [&](math::ScatterSpecies s) -> const MeshData& {
        const auto i = static_cast<size_t>(s);
        if (i >= built.size()) {
            species_mesh.resize(i + 1);
            built.resize(i + 1, 0);
        }
        if (built[i] == 0) {
            species_mesh[i] = build_scatter_mesh(s);
            built[i] = 1;
            if (species_mesh[i].vertices.empty()) {
                // AND THE SECOND HALF: a species core places and render cannot
                // build must be LOUD. Silently skipping it is how the forest
                // floor ships as bare earth while every test is green — the
                // same "absence presenting as a neutral state" that hid the
                // missing site meshes for a whole stage.
                report_missing_species(i);
            }
        }
        return species_mesh[i];
    };

    const float tile_size = chunk_size / static_cast<float>(micro_tiles_per_axis);
    const uint32_t n = micro_tiles_per_axis;
    struct TileScratch {
        MeshData mesh;
    };
    std::vector<TileScratch> tiles(static_cast<size_t>(n) * n);

    // Per-instance shape (crowding lean, crown shyness, maturity, understory)
    // is derived from the neighbourhood once, up front — it needs every
    // instance to see its neighbours, so it cannot be done inside the loop.
    const std::vector<FloraShape> shapes =
        analyse_neighbourhood(instances, instances.size());

    for (size_t i = 0; i < instances.size(); ++i) {
        const math::ScatterInstance& inst = instances[i];
        // ROUTING ASKS FLORA, IT DOES NOT RE-DERIVE THE ANSWER. This used to
        // be `is_tree()` — oak, pine, birch by name — which was the whole
        // reason §5.10's forest floor and §5.11's edge set drew as bare earth:
        // flora had built every one of those meshes and nothing routed to them.
        // `flora_owns()` is an exhaustive switch with NO default in flora's
        // zone, so the next time core grows the enum that file fails to COMPILE
        // rather than quietly answering for a species nobody considered.
        //
        // Note it is a predicate and not `flora_species_of(...) != something`:
        // that mapping returns Bush through its default, so a Stone taking this
        // branch would draw a shrub where a boulder belongs — a routing bug
        // wearing a placement bug's clothes.
        if (flora_owns(inst.species)) {
            const FloraSpecies fs = flora_species_of(inst.species);
            const uint32_t variant =
                flora_variant_for({inst.position.x, inst.position.z});
            // Scale 1.0: the generator has ALREADY applied FloraShape::maturity
            // (which is inst.scale) to the height. Passing inst.scale here too
            // would square it — a 1.25 giant becomes 1.56 and still looks
            // plausible, which is exactly why it would survive review.
            //
            // AND NO GROUND SINK ON THIS PATH, for every flora class and not
            // only for trees: append_flora's contract is that the mesh already
            // stands on its own root flare, and the ground-cover classes bury
            // their own lower half (moss domes, part-buried pebbles and logs).
            // Sinking them again would put the moss under the terrain, and
            // "the moss did not draw" and "the moss drew 12 cm underground"
            // are the same screenshot.
            // VERIFICATION HOOK, NEVER A SHIPPING PATH (the standing of
            // DFN_NO_SCATTER above and of flora's DFN_FLORA_ONLY): build the
            // whole scatter at the FAR level of detail, so the question "would
            // wiring the LOD ladder change the treeline at all" can be answered
            // by a frame before any selection machinery is written.
            //
            // IT EXISTS BECAUSE THE LADDER IS NOT WIRED AND THAT WAS NOT KNOWN.
            // This call site passes Full unconditionally, and across the whole
            // engine FloraLod appears outside flora's own files exactly twice,
            // both Full — so Reduced and Silhouette have never been drawn. A
            // flora change measured against the far LOD produced a before/after
            // pair INSIDE THE RUN-TO-RUN NOISE (455 px of 230400 between arms,
            // 242 px between two runs of the same arm), which is how it
            // surfaced. Ask a door not whether it works but whether it moves
            // the quantity you are measuring with.
            static const FloraLod forced = [] {
                const char* e = std::getenv("DFN_FLORA_FORCE_LOD");
                if (e == nullptr) return FloraLod::Full;
                return e[0] == '1' ? FloraLod::Reduced
                                   : (e[0] == '2' ? FloraLod::Silhouette : FloraLod::Full);
            }();
            append_flora(out.trees, out.foliage, fs, variant, shapes[i],
                         forced, inst.position, inst.yaw);
            continue;
        }
        const MeshData& src = mesh_of(inst.species);
        if (src.vertices.empty()) {
            continue;
        }
        const glm::vec3 pos{inst.position.x,
                            inst.position.y - GROUND_SINK_FRAC * inst.scale,
                            inst.position.z};
        // Micro: clamp the tile index so border instances never fall outside.
        const auto tx = static_cast<uint32_t>(std::clamp(
            static_cast<int>((inst.position.x - chunk_origin.x) / tile_size), 0,
            static_cast<int>(n) - 1));
        const auto tz = static_cast<uint32_t>(std::clamp(
            static_cast<int>((inst.position.z - chunk_origin.y) / tile_size), 0,
            static_cast<int>(n) - 1));
        TileScratch& tile = tiles[static_cast<size_t>(tz) * n + tx];
        append_transformed(tile.mesh, src, pos, inst.yaw, inst.scale);
    }

    for (uint32_t tz = 0; tz < n; ++tz) {
        for (uint32_t tx = 0; tx < n; ++tx) {
            TileScratch& tile = tiles[static_cast<size_t>(tz) * n + tx];
            if (tile.mesh.vertices.empty()) {
                continue;
            }
            MicroTile micro;
            micro.center_xz = {
                chunk_origin.x + (static_cast<float>(tx) + 0.5f) * tile_size,
                chunk_origin.y + (static_cast<float>(tz) + 0.5f) * tile_size};
            // The bounding circle is MEASURED over the baked vertices, never
            // derived from a per-species table: whatever geometry a species
            // build produces — today's or any future one — is covered by
            // construction, and the radius cannot drift from the mesh.
            float radius = 0.0f;
            for (const platform::Vertex& v : tile.mesh.vertices) {
                const glm::vec2 d{v.position.x - micro.center_xz.x,
                                  v.position.z - micro.center_xz.y};
                radius = std::max(radius, glm::length(d));
            }
            micro.radius_m = radius;
            micro.mesh = std::move(tile.mesh);
            out.micro.push_back(std::move(micro));
        }
    }
    return out;
}

} // namespace dfn::render
