/*
Module: engine/platform/anim
File: engine/platform/anim/sources/null/CreateNullAnim.h

Responsibility:
- Factory for the null animation backend (Rule 3): loads succeed, evaluation
  fills identity matrices (bind pose) — headless tours never crash.

Key items:
- create_null_anim(): the only symbol; concrete type stays private to the TU.

Dependencies:
- Uses: interfaces/IAnim.h, <memory>.
- Used by: engine/app (backend wiring), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Behavior contract lives in interfaces/IAnim.h notes; keep them in sync.
*/

#pragma once

#include <memory>

#include "engine/platform/anim/interfaces/IAnim.h"

namespace dfn::platform {

// Null backend (Rule 3): valid-but-inert handles, joint_count() == 0,
// clip_duration() == 0, evaluate() fills the whole span with identity.
[[nodiscard]] std::unique_ptr<IAnim> create_null_anim();

} // namespace dfn::platform
