/*
Module: engine/core/types
File: engine/core/types/sources/Handle.h

Responsibility:
- Generic typed handle: a POD integer id made type-safe by a tag, so a physics
  body handle cannot be passed where a sound handle is expected.

Key items:
- Handle<Tag, Storage>: opaque typed id, 0 = invalid.

Dependencies:
- Uses: <cstdint>, <functional>.
- Used by: components referencing platform resources by value (Rule 8 — no
  pointers, no backend types), engine caches mapping handles to backend objects.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep trivially copyable; components store these by value.
*/

#pragma once

#include <cstdint>
#include <functional>

namespace dfn::types {

/// Type-safe opaque id. `Tag` is any (possibly incomplete) type used purely for
/// discrimination:
///
///   struct PhysicsBodyTag;
///   using PhysicsBodyHandle = types::Handle<PhysicsBodyTag>;
///
/// value 0 is reserved as "invalid / none", matching the platform interface
/// convention (see IRenderer.h handles).
template<typename Tag, typename Storage = uint32_t>
struct Handle {
    Storage value = 0;

    [[nodiscard]] constexpr bool valid() const { return value != 0; }
    [[nodiscard]] constexpr bool operator==(const Handle& o) const { return value == o.value; }
    [[nodiscard]] constexpr bool operator!=(const Handle& o) const { return value != o.value; }

    [[nodiscard]] static constexpr Handle invalid() { return Handle{}; }
};

/// Hash functor for unordered containers keyed by a Handle.
template<typename Tag, typename Storage = uint32_t>
struct HandleHash {
    [[nodiscard]] std::size_t operator()(const Handle<Tag, Storage>& h) const {
        return std::hash<Storage>{}(h.value);
    }
};

} // namespace dfn::types
