/*
Created: 10:08:2026 - 21:11:59
Last updated: 10:08:2026 - 21:11:59
Module: engine/world
File: engine/world/sources/LayoutLoad.h

Responsibility:
- Load a TestbedLayout from a content file (Rule 5/6): the MECHANISM for
  reading a map, holding none. The map itself lives in
  games/<game>/assets/world/ as JSON.

Key items:
- LayoutLoadResult: ok + a developer-facing error naming the offending path.
- load_layout(): JSON DOM -> TestbedLayout.
- load_layout_file(): read a file, parse it, load it.

Dependencies:
- Uses: core serialization (Json), TestbedLayout.h, <filesystem>, <string>.
- Used by: the composition root (which map to load is a game decision, not a
  world-generator decision), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THIS FILE APPLIES THE ANCHORED TRANSFORMS AND THE FILE MUST NOT CONTAIN THEM.
  The data carries FRACTIONS of the summit and SURVEY-SPACE tunnel waypoints;
  the summit anchor (config::L0_RELIEF) and the ascent/footprint scaling live
  in the engine. Writing the post-scale metres into the asset would look like
  a finished migration and would be a live Rule 37 defect: the day the summit
  row moves, the splat lines and the tunnel silently stop moving with it, and
  nothing goes red anywhere near the edit that caused it.
- UNKNOWN KEYS ARE AN ERROR, deliberately. A mistyped key in a hand-written
  map would otherwise leave the engine's neutral value in place and generate a
  world quietly missing a feature — the same silent-default failure the JSON
  parser rejects duplicate keys to avoid.
*/
/*
UPD:
- 10:08:2026 - 21:11:59: Created — CODE_AUDIT §3.4: the mechanism half of
  moving 441 lines of one game's map out of the reusable engine.
*/

#pragma once

#include "engine/core/serialization/sources/Json.h"
#include "engine/world/sources/TestbedLayout.h"

#include <filesystem>
#include <string>

namespace dfn::world {

/// Outcome of a layout load. `error` is developer-facing (Rule 5 exemption)
/// and names the JSON path that failed, e.g. "carves.crag_tunnel.points[3]".
struct LayoutLoadResult {
    bool ok = false;
    std::string error{};
};

/// Fills `out` from a parsed layout document. On failure `out` is left in an
/// unspecified state — callers must not use a layout whose load failed.
[[nodiscard]] LayoutLoadResult load_layout(const serialization::JsonValue& root,
                                           TestbedLayout& out);

/// Reads, parses and loads a layout file. Missing file and parse errors come
/// back through the same result, with the file name in the message.
[[nodiscard]] LayoutLoadResult load_layout_file(const std::filesystem::path& path,
                                                TestbedLayout& out);

} // namespace dfn::world
