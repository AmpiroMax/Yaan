/*
Module: engine/app
File: engine/app/sources/Localization.h

Responsibility:
- Resolves a key hash to the user-facing string it names. This is the ONLY
  place a user-facing string enters the running program, which is what makes
  Rule 5 structurally true rather than a discipline: render's draw_text takes
  UTF-8 and has no parameter that could carry a key, so it cannot contain text.

Key items:
- load_localization(path): reads key=value lines; returns false if the file is
  missing, and the game still runs with every string showing as a placeholder.
- localized(key_hash): NEVER returns an empty string. A miss returns a visibly
  broken marker, because "nothing to say" and "the table does not have this"
  must not be the same picture -- the failure this project has paid for all
  evening, in the invisible castle, the blank map and the unregistered mesh.

Dependencies:
- Uses: engine/core/serialization (fnv1a64 -- the SAME hash the content ids and
  the tools use; two copies of a hash function is Rule 35 wearing a hat).
- Used by: engine/app only. Render never sees a key.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace dfn::app {

/// Reads a key=value localization file. Returns false if it could not be
/// opened; every lookup then yields a placeholder, which is loud rather than
/// blank on purpose.
bool load_localization(const std::string& path);

/// The string this key names. NEVER empty: a miss yields "?<0x...>?", which
/// cannot be mistaken for content the way a bare key like item.sword.rusty can.
/// The miss is logged once per key, not once per frame.
[[nodiscard]] std::string_view localized(uint64_t key_hash);

} // namespace dfn::app
