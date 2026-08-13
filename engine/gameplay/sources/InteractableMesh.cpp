/*
Created: 13:08:2026 - 17:20:00
Last updated: 13:08:2026 - 17:20:00
Module: engine/gameplay
File: engine/gameplay/sources/InteractableMesh.cpp

Responsibility:
- The placeholder geometry of a door, a lever and a torch, authored in the unit
  cube so that scaling by a prop's collision half-extents makes the drawn shape
  and the solid shape the same object.

Key items:
- box(): an axis-aligned box with outward-facing windings and a face mask.
- door_mesh() / lever_mesh() / torch_mesh().

Dependencies:
- Uses: InteractableMesh.h, render ProcMesh (tri/quad/pack).
- Used by: InteractableSpawn, engine/app's registry ferry, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NOTHING may leave [-1, 1]^3. That cube becomes the collision box, and drawn
  geometry outside it is a thing the player can see, aim at and never hit.
- Every mesh must TOUCH all six faces of the cube. A face with nothing on it is
  a face where the crosshair reports a prop and the screen shows empty air.
*/
/*
UPD:
- 13:08:2026 - 17:20:00: Created.
*/

#include "engine/gameplay/sources/InteractableMesh.h"

#include <glm/vec3.hpp>

namespace dfn::gameplay {

namespace {

using render::MeshData;
using render::pack;
using render::quad;

// Face bits for box(). Named rather than positional: a bare 0x2F in a call is
// a mask nobody can read back.
constexpr uint8_t FACE_POS_X = 1u << 0;
constexpr uint8_t FACE_NEG_X = 1u << 1;
constexpr uint8_t FACE_POS_Y = 1u << 2;
constexpr uint8_t FACE_NEG_Y = 1u << 3;
constexpr uint8_t FACE_POS_Z = 1u << 4;
constexpr uint8_t FACE_NEG_Z = 1u << 5;
constexpr uint8_t FACE_ALL = 0x3Fu;

// An axis-aligned box. Windings are CCW seen from OUTSIDE on every face, which
// is what `quad` derives its flat normal from — the suite asserts all six
// point outward rather than trusting the six orderings below to have been
// typed correctly.
void box(MeshData& m, glm::vec3 lo, glm::vec3 hi, uint32_t color,
         uint8_t faces = FACE_ALL) {
    if (faces & FACE_POS_X) {
        quad(m, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {hi.x, hi.y, hi.z},
             {hi.x, lo.y, hi.z}, color);
    }
    if (faces & FACE_NEG_X) {
        quad(m, {lo.x, lo.y, lo.z}, {lo.x, lo.y, hi.z}, {lo.x, hi.y, hi.z},
             {lo.x, hi.y, lo.z}, color);
    }
    if (faces & FACE_POS_Y) {
        quad(m, {lo.x, hi.y, lo.z}, {lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z},
             {hi.x, hi.y, lo.z}, color);
    }
    if (faces & FACE_NEG_Y) {
        quad(m, {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, lo.y, hi.z},
             {lo.x, lo.y, hi.z}, color);
    }
    if (faces & FACE_POS_Z) {
        quad(m, {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z},
             {lo.x, hi.y, hi.z}, color);
    }
    if (faces & FACE_NEG_Z) {
        quad(m, {lo.x, lo.y, lo.z}, {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z},
             {hi.x, lo.y, lo.z}, color);
    }
}

// --- The three props -------------------------------------------------------

// A DOOR IS A SLAB, and being exactly a slab is the point: it fills its unit
// cube completely, so after scaling it IS its collision box — a door is the one
// prop where "what you see is what the ray hits" can be exact rather than
// close, and it is also the one standing dead ahead of the spawn.
//
// The face the player meets is TILED into panels instead of carrying relief:
// planks modelled as separate boxes would either sit proud of the cube (drawn
// where nothing is solid) or recessed into it (solid where nothing is drawn),
// and both are the defect this whole file is about. Coplanar tiles that do not
// overlap cost nothing, cannot z-fight, and read as a panelled door at the
// 2.5 m this one stands at.
MeshData door_mesh() {
    MeshData m;
    const uint32_t plank_a = pack({0.32f, 0.20f, 0.11f});
    const uint32_t plank_b = pack({0.26f, 0.16f, 0.09f});
    const uint32_t frame = pack({0.18f, 0.11f, 0.06f});
    const uint32_t handle = pack({0.72f, 0.60f, 0.22f});

    // The slab minus its front face; the front is tiled below.
    box(m, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, frame,
        static_cast<uint8_t>(FACE_ALL & ~FACE_NEG_Z));

    // The front face, tiled 3 x 4. One tile is the handle: a colour, not a
    // knob, because a knob is geometry outside the box.
    constexpr int COLS = 3;
    constexpr int ROWS = 4;
    for (int c = 0; c < COLS; ++c) {
        for (int r = 0; r < ROWS; ++r) {
            const float x0 = -1.0f + 2.0f * static_cast<float>(c) / COLS;
            const float x1 = -1.0f + 2.0f * static_cast<float>(c + 1) / COLS;
            const float y0 = -1.0f + 2.0f * static_cast<float>(r) / ROWS;
            const float y1 = -1.0f + 2.0f * static_cast<float>(r + 1) / ROWS;
            uint32_t color = ((c + r) % 2 == 0) ? plank_a : plank_b;
            if (c == COLS - 1 && r == 2) {
                color = handle;
            }
            quad(m, {x0, y0, -1.0f}, {x0, y1, -1.0f}, {x1, y1, -1.0f}, {x1, y0, -1.0f},
                 color);
        }
    }
    return m;
}

// A LEVER IS A WALL PLATE WITH AN ARM. The plate carries the four side faces
// and the back; the arm reaches forward to the front face. Between them the six
// faces of the cube all have geometry on them, which is the property that
// matters: wherever inside its box the player aims, there is something drawn
// where the crosshair says there is a prop.
MeshData lever_mesh() {
    MeshData m;
    const uint32_t iron = pack({0.20f, 0.21f, 0.24f});
    const uint32_t rim = pack({0.13f, 0.14f, 0.16f});
    const uint32_t brass = pack({0.62f, 0.45f, 0.16f});

    // Back plate: the whole cross-section, and MOST of the depth. It is thick
    // rather than thin on purpose — these props are seen from an angle far more
    // often than head-on, and a thin plate leaves most of its own target box
    // empty when you look at it from the side. Measured from the spawn eye,
    // moving the plate's front from z = 0.10 to z = -0.15 takes this prop's
    // target-box coverage from 64.4 % to 76.4 % (the suite prints both the
    // number and where it was taken from).
    box(m, {-1.0f, -1.0f, -0.15f}, {1.0f, 1.0f, 1.0f}, iron);
    // A raised rim down each side so the plate is not one flat rectangle.
    box(m, {-1.0f, -1.0f, -0.42f}, {-0.72f, 1.0f, -0.15f}, rim);
    box(m, {0.72f, -1.0f, -0.42f}, {1.0f, 1.0f, -0.15f}, rim);

    // The arm: from the plate to the FRONT face of the cube, sitting higher at
    // its far end. Square section, so its own normals stay axis-aligned under
    // the non-uniform scale the spawn applies.
    box(m, {-0.26f, -0.10f, -0.62f}, {0.26f, 0.42f, -0.15f}, iron);
    box(m, {-0.30f, 0.18f, -1.0f}, {0.30f, 0.78f, -0.62f}, brass);
    return m;
}

// A TORCH IS A SHAFT IN A STAND. The shaft carries the top and bottom faces,
// the stand's foot carries the four sides — same rule as the lever, reached a
// different way, because a bare stick touches two faces out of six and the
// other four would be solid air.
MeshData torch_mesh() {
    MeshData m;
    const uint32_t wood = pack({0.24f, 0.16f, 0.09f});
    const uint32_t stone = pack({0.30f, 0.30f, 0.31f});
    const uint32_t flame = pack({0.95f, 0.52f, 0.13f});
    const uint32_t ember = pack({0.85f, 0.24f, 0.06f});

    // Foot: the four side faces and the bottom.
    box(m, {-1.0f, -1.0f, -1.0f}, {1.0f, -0.72f, 1.0f}, stone);
    // Collar, so the foot does not read as a loose plate.
    box(m, {-0.55f, -0.72f, -0.55f}, {0.55f, -0.48f, 0.55f}, stone);
    // Shaft, up to the head.
    box(m, {-0.20f, -0.48f, -0.20f}, {0.20f, 0.34f, 0.20f}, wood);
    // Head: two stacked blocks, the upper one reaching the top face.
    box(m, {-0.42f, 0.34f, -0.42f}, {0.42f, 0.70f, 0.42f}, ember);
    box(m, {-0.26f, 0.70f, -0.26f}, {0.26f, 1.0f, 0.26f}, flame);
    return m;
}

} // namespace

render::MeshData build_interactable_mesh(uint32_t mesh_asset) {
    switch (mesh_asset) {
    case INTERACTABLE_MESH_DOOR:
        return door_mesh();
    case INTERACTABLE_MESH_LEVER:
        return lever_mesh();
    case INTERACTABLE_MESH_TORCH:
        return torch_mesh();
    default:
        return {};
    }
}

std::vector<InteractableMesh> interactable_meshes() {
    std::vector<InteractableMesh> out;
    for (const uint32_t id : {INTERACTABLE_MESH_DOOR, INTERACTABLE_MESH_LEVER,
                              INTERACTABLE_MESH_TORCH}) {
        render::MeshData data = build_interactable_mesh(id);
        if (data.indices.empty()) {
            continue; // unreachable for the ids above; the suite proves it
        }
        out.push_back(InteractableMesh{.mesh_asset = id,
                                       .vertices = std::move(data.vertices),
                                       .indices = std::move(data.indices)});
    }
    return out;
}

} // namespace dfn::gameplay
