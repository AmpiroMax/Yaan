/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: engine/core/types
File: engine/core/types/sources/TypeId.cpp

Responsibility:
- Defines the monotonic counter behind type_id<T>().

Key items:
- detail::next_type_id().

Dependencies:
- Uses: TypeId.h, <atomic>.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — atomic counter implementation.
*/

#include "engine/core/types/sources/TypeId.h"

#include <atomic>

namespace dfn::types::detail {

TypeId next_type_id() {
    static std::atomic<TypeId> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace dfn::types::detail
