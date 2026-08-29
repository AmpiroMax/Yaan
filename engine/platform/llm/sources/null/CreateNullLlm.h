/*
Module: engine/platform/llm
File: engine/platform/llm/sources/null/CreateNullLlm.h

Responsibility:
- Factory for the null LLM backend (Q62 — THE default runnable mode): every
  request completes instantly with its scripted fallback_text.

Key items:
- create_null_llm(): the only symbol; concrete type stays private to the TU.

Dependencies:
- Uses: interfaces/ILlm.h, <memory>.
- Used by: engine/app (backend wiring), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Behavior contract lives in interfaces/ILlm.h notes; keep them in sync.
*/

#pragma once

#include <memory>

#include "engine/platform/llm/interfaces/ILlm.h"

namespace dfn::platform {

// Null backend (Rule 3, Q62): submit succeeds, status is Done immediately,
// try_get_result returns fallback_text with from_fallback = true. Instant and
// deterministic (Rule 13.2) — the game is fully playable on it.
[[nodiscard]] std::unique_ptr<ILlm> create_null_llm();

} // namespace dfn::platform
