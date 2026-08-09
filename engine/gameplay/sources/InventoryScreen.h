/*
Created: 09:08:2026 - 22:27:49
Last updated: 09:08:2026 - 22:27:49
Module: engine/gameplay
File: engine/gameplay/sources/InventoryScreen.h

Responsibility:
- The state behind the inventory screen the user specified: a LIST OF NAMES on
  one side and a ROTATABLE 3D PREVIEW of the selected item on the other. Not an
  icon grid. Gameplay owns what is shown and what is selected; render owns
  every pixel of it.

Key items:
- InventoryScreen: the World resource render reads (open, entries, selection,
  preview angles).
- refresh_inventory_screen(): rebuilds the entry list from the actor's Inventory.
- move_selection / rotate_preview: the two things input does to it.

Dependencies:
- Uses: core ecs, Inventory.h, Item.h, Ids.h, generated constants.
- Used by: engine/app (input + per-tick refresh), engine/render (drawing),
  tests.

Notes:
- ENTRIES CARRY KEYS, NEVER TEXT (Rule 5). `name_key` is the hashed
  localization key from ItemDef::display_name_key; the screen cannot render a
  word until a font and a string table exist, and gameplay never resolves one.
- STABLE ORDER, and it is a correctness property rather than a nicety: the list
  is sorted by the item id, so picking something up never reshuffles the rows
  under the player's cursor. The selection is tracked by ITEM ID across a
  refresh, not by row index, so taking a potion does not silently move the
  highlight onto the sword you were about to drop.
- The preview angles live here rather than in render because they are input
  state: the same mouse that looks around the world turns the item when the
  screen is open, and the simulation is where input intent is resolved.
- The screen is DATA, not a mode switch: opening it does not pause anything and
  does not stop the fixed tick. Whether the world keeps running while it is open
  is a design decision nobody has made yet, and encoding it here would make it
  for them.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Plain data (Rule 8). No literal user-facing strings (Rule 5). No layout
  numbers here — pixels are render's, and a margin in this file would be a
  second place where the screen is designed.
*/
/*
UPD:
- 09:08:2026 - 22:27:49: Created — the Skyrim-style inventory the user asked
                         for: names on one side, a turnable item on the other.
*/

#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/ecs/sources/EntityId.h"
#include "engine/gameplay/sources/Ids.h"

namespace dfn::ecs {
class World;
}

namespace dfn::gameplay {

// One row of the list.
struct InventoryEntry {
    ItemId item{};
    uint64_t name_key = 0; // hashed localization key (Rule 5), 0 = unnamed content
    uint32_t count = 0;
    uint32_t mesh_id = 0;  // what the preview turns; 0 = nothing to show
    bool held = false;     // currently in the hand
};

// World RESOURCE: the whole screen, read by render.
struct InventoryScreen {
    bool open = false;
    std::vector<InventoryEntry> entries;
    uint32_t selected = 0;      // index into entries; 0 when empty
    float preview_yaw = 0.0f;   // radians, accumulated by dragging
    float preview_pitch = 0.0f; // radians, clamped so the item never inverts
};

// Rebuilds `entries` from the actor's Inventory and ItemDatabase, preserving
// the SELECTED ITEM (not the selected row) across the rebuild. Safe to call
// every tick; cheap when nothing changed.
void refresh_inventory_screen(ecs::World& world, ecs::EntityId actor);

// Moves the highlight by `delta` rows, clamped to the list (no wraparound: a
// list that jumps from the last row to the first makes a long inventory feel
// like it lost your place).
void move_selection(InventoryScreen& screen, int32_t delta);

// Turns the previewed item. Pitch is clamped; yaw is free, because turning an
// item all the way round is the point.
void rotate_preview(InventoryScreen& screen, float delta_yaw, float delta_pitch);

// The currently selected entry, or nullptr when the list is empty.
[[nodiscard]] const InventoryEntry* selected_entry(const InventoryScreen& screen);

} // namespace dfn::gameplay
