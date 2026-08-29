/*
Module: engine/platform/audio
File: engine/platform/audio/sources/null/CreateNullAudio.h

Responsibility:
- Factory for the null audio backend (Rule 3): everything succeeds silently;
  the game is fully playable with no audio device.

Key items:
- create_null_audio(): the only symbol; concrete type stays private to the TU.

Dependencies:
- Uses: interfaces/IAudio.h, <memory>.
- Used by: engine/app (backend wiring), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Behavior contract lives in interfaces/IAudio.h notes; keep them in sync.
*/

#pragma once

#include <memory>

#include "engine/platform/audio/interfaces/IAudio.h"

namespace dfn::platform {

// Null backend (Rule 3): silent success everywhere, valid-but-inert handles,
// is_playing() always false.
[[nodiscard]] std::unique_ptr<IAudio> create_null_audio();

} // namespace dfn::platform
