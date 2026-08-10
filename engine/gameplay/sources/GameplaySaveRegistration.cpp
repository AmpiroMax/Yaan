/*
Created: 10:08:2026 - 21:07:24
Last updated: 10:08:2026 - 21:07:24
Module: engine/gameplay
File: engine/gameplay/sources/GameplaySaveRegistration.cpp

Responsibility:
- The single call that hands gameplay's save sections to world's SaveDeltaCodec.

Key items:
- register_gameplay_save_sections.

Dependencies:
- Uses: GameplaySave.h, engine/world SaveDelta.h (SaveDeltaCodec).
- Used by: engine/app at startup.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- WHY THIS IS ITS OWN TRANSLATION UNIT, so nobody merges it back: it is the
  only thing in engine/gameplay that touches world::SaveDeltaCodec, whose
  implementation does not exist yet (engine/world/sources/SaveDelta.h is
  declaration-only, core's zone). While this function shared an object file
  with the section writers/readers, ANY consumer of a section — including the
  two round-trip tests — dragged the missing codec symbol into its link and
  failed. Splitting it means the sections are usable today and the codec is
  pulled in only by whoever actually registers. Merge this back the day
  SaveDelta.cpp exists, and not before.
*/
/*
UPD:
- 10:08:2026 - 21:07:24: Split out of GameplaySave.cpp so the persistence
  tests could link once core's Binary IO landed.
*/

#include "engine/gameplay/sources/GameplaySave.h"

#include "engine/world/sources/SaveDelta.h"

namespace dfn::gameplay {

void register_gameplay_save_sections(world::SaveDeltaCodec& codec) {
    codec.register_section(world::SaveSectionHooks{
        SECTION_INVENTORY, INVENTORY_SECTION_VERSION, write_inventory_section,
        read_inventory_section});
    codec.register_section(world::SaveSectionHooks{
        SECTION_INTERACTABLES, INTERACTABLES_SECTION_VERSION,
        write_interactables_section, read_interactables_section});
}

} // namespace dfn::gameplay
