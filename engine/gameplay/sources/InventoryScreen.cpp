/*
Module: engine/gameplay
File: engine/gameplay/sources/InventoryScreen.cpp

Responsibility:
- Builds the inventory list and maintains the selection and preview angles.

Key items:
- refresh_inventory_screen / move_selection / rotate_preview / selected_entry.

Dependencies:
- Uses: InventoryScreen.h, Inventory.h, Item.h, HeldItem.h, core ecs,
  core serialization (the frozen content hash for name keys), constants.
- Used by: engine/app, engine/render, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Sort order is part of the contract: stable, by item id. Do not "optimise" it
  into insertion order — the selection guarantee depends on it.
*/

#include "engine/gameplay/sources/InventoryScreen.h"

#include <algorithm>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/Inventory.h"
#include "engine/gameplay/sources/Item.h"

namespace dfn::gameplay {

namespace {

constexpr float PITCH_LIMIT = static_cast<float>(config::CAMERA_PITCH_LIMIT);

} // namespace

void refresh_inventory_screen(ecs::World& world, ecs::EntityId actor) {
    if (!world.has_resource<InventoryScreen>()) {
        world.add_resource(InventoryScreen{});
    }
    auto& screen = world.resource<InventoryScreen>();

    // Remember the selected ITEM, not the row: a refresh that moved the
    // highlight to whatever now sits at the old index would let a pickup
    // silently change what the next keypress acts on.
    const ItemId previously_selected =
        screen.selected < screen.entries.size() ? screen.entries[screen.selected].item : ItemId{};

    screen.entries.clear();

    const auto* inventory = world.get<Inventory>(actor);
    if (inventory == nullptr) {
        screen.selected = 0;
        return;
    }
    const ItemDatabase* items =
        world.has_resource<ItemDatabase>() ? &world.resource<ItemDatabase>() : nullptr;
    const auto* held = world.get<HeldItem>(actor);

    for (const ItemStack& stack : inventory->stacks) {
        if (stack.count == 0) {
            continue;
        }
        // Stacks may be split across entries (max_stack); the LIST shows one
        // row per distinct item, so merge as we go.
        const auto existing =
            std::find_if(screen.entries.begin(), screen.entries.end(),
                         [&](const InventoryEntry& e) { return e.item.value == stack.item.value; });
        if (existing != screen.entries.end()) {
            existing->count += stack.count;
            continue;
        }

        InventoryEntry entry;
        entry.item = stack.item;
        entry.count = stack.count;
        if (const ItemDef* def = items != nullptr ? items->find(stack.item) : nullptr) {
            entry.name_key = def->display_name_key.empty()
                                 ? 0
                                 : serialization::fnv1a64(def->display_name_key);
            entry.mesh_id = def->mesh_id;
        }
        entry.held = held != nullptr && held->item.value == stack.item.value;
        screen.entries.push_back(entry);
    }

    std::sort(screen.entries.begin(), screen.entries.end(),
              [](const InventoryEntry& a, const InventoryEntry& b) {
                  return a.item.value < b.item.value;
              });

    screen.selected = 0;
    if (previously_selected.valid()) {
        for (std::size_t i = 0; i < screen.entries.size(); ++i) {
            if (screen.entries[i].item.value == previously_selected.value) {
                screen.selected = static_cast<uint32_t>(i);
                break;
            }
        }
    }
}

void move_selection(InventoryScreen& screen, int32_t delta) {
    if (screen.entries.empty()) {
        screen.selected = 0;
        return;
    }
    const int32_t last = static_cast<int32_t>(screen.entries.size()) - 1;
    const int32_t next = std::clamp(static_cast<int32_t>(screen.selected) + delta, 0, last);
    screen.selected = static_cast<uint32_t>(next);
}

void rotate_preview(InventoryScreen& screen, float delta_yaw, float delta_pitch) {
    screen.preview_yaw += delta_yaw;
    screen.preview_pitch =
        std::clamp(screen.preview_pitch + delta_pitch, -PITCH_LIMIT, PITCH_LIMIT);
}

const InventoryEntry* selected_entry(const InventoryScreen& screen) {
    if (screen.selected >= screen.entries.size()) {
        return nullptr;
    }
    return &screen.entries[screen.selected];
}

} // namespace dfn::gameplay
