/*
Module: engine/gameplay
File: engine/gameplay/sources/GameplaySave.h

Responsibility:
- Gameplay's contribution to the save delta: the player inventory and the
  state of interactables (door open flags, spent one-shot usables). Registered
  as sections through world::SaveDeltaCodec (the stage-1 agreement with core).

Key items:
- SECTION_INVENTORY / SECTION_INTERACTABLES: this zone's section tags.
- write_inventory_section / read_inventory_section: the inventory codec.
- write_interactables_section / read_interactables_section: prop state codec.
- register_gameplay_save_sections(): what engine/app calls at startup.

Dependencies:
- Uses: core serialization (BinaryReader/Writer), core ecs, Inventory.h,
  Interaction.h, world SaveDelta.
- Used by: engine/app (registration), tests (direct round-trip).

Notes:
- Section IO is written and tested against BinaryReader/Writer directly, so the
  serialization is proven independently of the container. core's SaveDeltaCodec
  is currently headers-plus-stub (file IO deferred), so nothing round-trips to
  DISK yet: gameplay state is serializable, but "save my game" is not a working
  feature until core lands the codec. Recorded here so it cannot be assumed.
- Entities are identified by EntityId::packed(). That is correct for the
  in-session round trip these sections are tested against; when world entities
  become chunk-derived and stable across regeneration, the interactable section
  should key by the world entity id core assigns instead — flagged for the sync
  that lands real saves, not silently deferred.
- Versioning: every section writes its version; readers migrate from
  `stored_version` (Rule 7). Adding a field means bumping the version and
  handling the older one, never reordering the existing ones.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never memcpy structs (Rule 7): fields are written explicitly, little-endian
  by BinaryWriter.
*/

#pragma once

#include <cstdint>

#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"

namespace dfn::ecs {
class World;
}
namespace dfn::world {
class SaveDeltaCodec;
}

namespace dfn::gameplay {

// Section tags (sim's namespace; uniqueness coordinated through core).
inline constexpr serialization::SectionTag SECTION_INVENTORY =
    serialization::make_tag('I', 'N', 'V', 'T');
inline constexpr serialization::SectionTag SECTION_INTERACTABLES =
    serialization::make_tag('I', 'N', 'T', 'R');

inline constexpr uint16_t INVENTORY_SECTION_VERSION = 1;
inline constexpr uint16_t INTERACTABLES_SECTION_VERSION = 1;

// Inventory: every entity that has one, with its stacks.
void write_inventory_section(serialization::BinaryWriter& writer, const ecs::World& world);
[[nodiscard]] bool read_inventory_section(serialization::BinaryReader& reader,
                                          ecs::World& world, uint16_t stored_version);

// Interactables: Openable and Usable state that the player has changed.
void write_interactables_section(serialization::BinaryWriter& writer,
                                 const ecs::World& world);
[[nodiscard]] bool read_interactables_section(serialization::BinaryReader& reader,
                                              ecs::World& world, uint16_t stored_version);

// Registers both sections with the codec. engine/app calls this once at
// startup, before any save or load.
void register_gameplay_save_sections(world::SaveDeltaCodec& codec);

} // namespace dfn::gameplay
