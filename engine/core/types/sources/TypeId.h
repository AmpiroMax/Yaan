/*
Module: engine/core/types
File: engine/core/types/sources/TypeId.h

Responsibility:
- Compile-time-cheap type identification without RTTI: each distinct T gets a
  unique runtime TypeId on first request.

Key items:
- TypeId: integer type identifier.
- type_id<T>(): the unique id of T.

Dependencies:
- Uses: <cstddef>.
- Used by: ecs::World (pool/resource keys), events::EventBus (event keys).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- No RTTI (typeid/dynamic_cast) anywhere in core; this header is the substitute.
- Ids are NOT stable across runs — never serialize a TypeId (Rule 7 formats use
  explicit tags instead).
*/

#pragma once

#include <cstddef>

namespace dfn::types {

/// Process-local type identifier. Dense, starts at 0, assigned in first-call
/// order — therefore NOT stable across runs or builds. Key for in-memory maps
/// only; serialized formats must use explicit section tags (Rule 7).
using TypeId = std::size_t;

namespace detail {
/// Monotonic counter behind type_id<T>(). Defined in stage 2.
[[nodiscard]] TypeId next_type_id();
} // namespace detail

/// Unique id of T (cv/ref-stripped T's are distinct types on purpose — callers
/// normalize). Thread-safe via static-local initialization.
template<typename T>
[[nodiscard]] inline TypeId type_id() {
    static const TypeId id = detail::next_type_id();
    return id;
}

} // namespace dfn::types
