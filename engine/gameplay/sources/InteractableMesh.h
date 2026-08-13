/*
Created: 13:08:2026 - 17:20:00
Last updated: 13:08:2026 - 17:20:00
Module: engine/gameplay
File: engine/gameplay/sources/InteractableMesh.h

Responsibility:
- Placeholder geometry for the things the player can interact with, so a prop
  that answers the crosshair is also a prop the player can SEE.

Key items:
- INTERACTABLE_MESH_DOOR / _LEVER / _TORCH: the RenderMesh ids.
- InteractableMesh: one id and its geometry, for the app's registry ferry.
- interactable_meshes(): everything this zone asks render to upload.
- build_interactable_mesh(): the geometry of one id, on its own, for tests.

Dependencies:
- Uses: engine/render ProcMesh (MeshData + tri/quad/pack — pure geometry, the
  same permission PropCollision already uses; no GPU, no third-party).
- Used by: InteractableSpawn (which id a prop gets), engine/app (the ferry that
  uploads them), tests.

Notes:
- WHY THIS EXISTS AT ALL. `spawn_interactable` gave every prop a Transform, a
  Highlightable, its verb component and a LAYER_INTERACTABLE box — and no
  RenderMesh. The crosshair ray hit the box, HoverTarget filled in honestly and
  the app honestly drew "Open", so the whole chain worked and there was nothing
  to see. A 1.8 x 2.0 m door stood 2.5 m in front of the spawn, dead ahead, and
  was not drawn by anything. Found by ui.
- AND ONE RENDERMESH WOULD NOT HAVE BEEN ENOUGH: render's ECS pass selects on
  Transform + PreviousTransform + RenderMesh, and the spawn had no
  PreviousTransform either. A prop with a mesh and no previous transform is
  exactly as invisible, and the fix would have looked complete.
- THE MODEL SPACE IS THE UNIT CUBE, and that is what makes the drawn shape and
  the solid box the same object rather than two numbers that agree today. Every
  mesh here is authored inside [-1, 1]^3 and touches all six faces; the spawn
  scales it by the prop's collision half-extents. The door is then EXACTLY its
  box; the lever and the torch are inscribed in theirs, which is measured and
  named rather than described as "about right" (see the suite).
- Placeholders, deliberately: a door is a slab, a lever is a plate with an arm,
  a torch is a shaft with a head. Recognisable at the 2.5 m these stand at, and
  cheap enough that replacing them with content meshes costs one id.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The ids come from the map in engine/render/sources/ProcMesh.h and are RENDER'S
  to allocate. Do not invent one; ask the lead (Rule 25/26).
- Nothing here may protrude beyond the unit cube: the cube IS the collision box
  after scaling, and drawn geometry outside it is a thing you can see, aim at,
  and never hit.
*/
/*
UPD:
- 13:08:2026 - 17:20:00: Created — the three demo props become visible.
*/

#pragma once

#include <cstdint>
#include <vector>

#include "engine/render/sources/ProcMesh.h"

namespace dfn::gameplay {

// RenderMesh ids for this zone's interactable placeholders.
//
// BLESSED BY THE LEAD (13.08.2026) and written into the id map in render's
// ProcMesh.h, which is where ids are allocated: 50..63 belongs to gameplay,
// 50 = door, 51 = lever, 52 = torch, 53..63 spare.
//
// Not 64..127, which is where an item mesh belongs and where these would
// naturally have gone: that range is RESERVED for this zone and simultaneously
// REFUSED by `RenderSystem::register_mesh`, so nothing has ever been uploaded
// into it — the same root as the view-model hand that "drew as nothing".
// Reported; render's code, render's call, and not a thing to fix in passing on
// a tree three agents are editing.
inline constexpr uint32_t INTERACTABLE_MESH_DOOR = 50;
inline constexpr uint32_t INTERACTABLE_MESH_LEVER = 51;
inline constexpr uint32_t INTERACTABLE_MESH_TORCH = 52;

// One id and the geometry the app must upload for it.
struct InteractableMesh {
    uint32_t mesh_asset = 0;
    std::vector<platform::Vertex> vertices;
    std::vector<uint32_t> indices;
};

// Everything this zone needs in render's registry, in a fixed order. The app
// ferries these once at startup (render cannot include gameplay).
[[nodiscard]] std::vector<InteractableMesh> interactable_meshes();

// The geometry of one id in model space (the unit cube). Empty for an id this
// zone does not own — which is a decision, not a failure, and the caller must
// treat an empty mesh as "do not register" rather than as "register nothing".
[[nodiscard]] render::MeshData build_interactable_mesh(uint32_t mesh_asset);

} // namespace dfn::gameplay
