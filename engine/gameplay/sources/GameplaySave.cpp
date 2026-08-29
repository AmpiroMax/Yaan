/*
Module: engine/gameplay
File: engine/gameplay/sources/GameplaySave.cpp

Responsibility:
- Implements the inventory and interactable save sections and their
  registration with world::SaveDeltaCodec.

Key items:
- write/read_inventory_section, write/read_interactables_section.

Dependencies:
- Uses: GameplaySave.h, core ecs World, Inventory.h, Interaction.h.
- Used by: engine/app, tests. (Codec registration lives in
  GameplaySaveRegistration.cpp — see its header for why the split is load
  bearing rather than cosmetic.)

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Fields are written explicitly in a fixed order (Rule 7). Changing the order
  or meaning requires a version bump plus migration in the reader.
*/

#include "engine/gameplay/sources/GameplaySave.h"

#include <vector>

#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/Inventory.h"

namespace dfn::gameplay {

namespace {

// EntityId <-> packed u64, the identity these sections key by (see header note
// about migrating to world entity ids when real saves land).
[[nodiscard]] ecs::EntityId unpack_entity(uint64_t packed) {
    return ecs::EntityId{static_cast<uint32_t>(packed >> 32),
                         static_cast<uint32_t>(packed & 0xFFFFFFFFull)};
}

} // namespace

// --- Inventory ---------------------------------------------------------------

void write_inventory_section(serialization::BinaryWriter& writer,
                             const ecs::World& world) {
    // Collect first so the entity count is known before it is written.
    struct Row {
        uint64_t entity;
        const Inventory* inventory;
    };
    std::vector<Row> rows;
    for (auto [id, inventory] : const_cast<ecs::World&>(world).view<Inventory>()) {
        rows.push_back(Row{id.packed(), &inventory});
    }

    writer.write_u32(static_cast<uint32_t>(rows.size()));
    for (const Row& row : rows) {
        writer.write_u64(row.entity);
        writer.write_u32(static_cast<uint32_t>(row.inventory->stacks.size()));
        for (const ItemStack& stack : row.inventory->stacks) {
            writer.write_u64(stack.item.value);
            writer.write_u32(stack.count);
        }
    }
}

bool read_inventory_section(serialization::BinaryReader& reader, ecs::World& world,
                            uint16_t stored_version) {
    if (stored_version > INVENTORY_SECTION_VERSION) {
        return false; // written by a newer build: unrecoverable, abort the load
    }
    const uint32_t entity_count = reader.read_u32();
    for (uint32_t e = 0; e < entity_count; ++e) {
        const ecs::EntityId id = unpack_entity(reader.read_u64());
        const uint32_t stack_count = reader.read_u32();

        Inventory restored;
        restored.stacks.reserve(stack_count);
        for (uint32_t s = 0; s < stack_count; ++s) {
            ItemStack stack;
            stack.item.value = reader.read_u64();
            stack.count = reader.read_u32();
            restored.stacks.push_back(stack);
        }
        if (!reader.ok()) {
            return false;
        }
        // A dead entity in the save means content moved under us: skip its row
        // rather than resurrect an entity the world no longer has.
        if (!world.alive(id)) {
            continue;
        }
        if (auto* existing = world.get<Inventory>(id); existing != nullptr) {
            *existing = std::move(restored);
        } else {
            world.add(id, std::move(restored));
        }
    }
    return reader.ok();
}

// --- Interactables -----------------------------------------------------------

void write_interactables_section(serialization::BinaryWriter& writer,
                                 const ecs::World& world) {
    auto& mutable_world = const_cast<ecs::World&>(world);

    struct OpenRow {
        uint64_t entity;
        bool open;
        bool locked;
    };
    std::vector<OpenRow> open_rows;
    for (auto [id, openable] : mutable_world.view<Openable>()) {
        open_rows.push_back(OpenRow{id.packed(), openable.open, openable.locked});
    }
    writer.write_u32(static_cast<uint32_t>(open_rows.size()));
    for (const OpenRow& row : open_rows) {
        writer.write_u64(row.entity);
        writer.write_bool(row.open);
        writer.write_bool(row.locked);
    }

    struct UsedRow {
        uint64_t entity;
        bool used;
    };
    std::vector<UsedRow> used_rows;
    for (auto [id, usable] : mutable_world.view<Usable>()) {
        used_rows.push_back(UsedRow{id.packed(), usable.used});
    }
    writer.write_u32(static_cast<uint32_t>(used_rows.size()));
    for (const UsedRow& row : used_rows) {
        writer.write_u64(row.entity);
        writer.write_bool(row.used);
    }
}

bool read_interactables_section(serialization::BinaryReader& reader, ecs::World& world,
                                uint16_t stored_version) {
    if (stored_version > INTERACTABLES_SECTION_VERSION) {
        return false;
    }
    const uint32_t open_count = reader.read_u32();
    for (uint32_t i = 0; i < open_count; ++i) {
        const ecs::EntityId id = unpack_entity(reader.read_u64());
        const bool open = reader.read_bool();
        const bool locked = reader.read_bool();
        if (!reader.ok()) {
            return false;
        }
        if (auto* openable = world.get<Openable>(id); openable != nullptr) {
            openable->open = open;
            openable->locked = locked;
        }
    }

    const uint32_t used_count = reader.read_u32();
    for (uint32_t i = 0; i < used_count; ++i) {
        const ecs::EntityId id = unpack_entity(reader.read_u64());
        const bool used = reader.read_bool();
        if (!reader.ok()) {
            return false;
        }
        if (auto* usable = world.get<Usable>(id); usable != nullptr) {
            usable->used = used;
        }
    }
    return reader.ok();
}

} // namespace dfn::gameplay
