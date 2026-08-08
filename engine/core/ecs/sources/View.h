/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/ecs
File: engine/core/ecs/sources/View.h

Responsibility:
- Multi-component query result: iterates the intersection of component pools,
  driving from the smallest pool and probing the rest in O(1).

Key items:
- View<Ts...>: iterable range yielding (EntityId, T1&, T2&, ...) tuples.

Dependencies:
- Uses: ComponentPool, EntityId.
- Used by: systems, via World::view<Ts...>().

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Iteration order is UNSPECIFIED; never rely on it (Rule 13 determinism comes from
  fixed system order, not from pool layout).
- Structural changes during iteration are forbidden except destroy_deferred()
  (Rule 9); adding/removing the viewed component types invalidates the view.
- STAGE 1 CONTRACT: declarations only; bodies arrive in stage 2 (header-only).
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — smallest-pool-driven view, based on
  Quicky ECS.
*/

#pragma once

#include "engine/core/ecs/sources/ComponentPool.h"

#include <cstddef>
#include <tuple>

namespace dfn::ecs {

/// Query over all entities that have ALL of the component types Ts...
///
/// Strategy: the smallest pool among Ts... drives the iteration (linear walk of
/// its dense array); every other pool is probed via O(1) sparse lookup. Total
/// cost is O(size_of_smallest_pool * sizeof...(Ts)).
///
/// Usage:
///   for (auto [id, pos, vel] : world.view<Position, Velocity>()) {
///       pos.value += vel.value * dt;
///   }
///
/// A View is a lightweight non-owning object; create it fresh each time you
/// iterate (Worlds hand them out cheaply), do not store it across frames.
template<typename... Ts>
class View {
    static_assert(sizeof...(Ts) > 0, "View needs at least one component type");

public:
    /// Constructed by World::view(); pools are never null (World creates empty
    /// pools on demand).
    explicit View(ComponentPool<Ts>*... pools);

    /// Forward iterator yielding std::tuple<EntityId, Ts&...>.
    class Iterator {
    public:
        Iterator(View* view, std::size_t index);

        [[nodiscard]] std::tuple<EntityId, Ts&...> operator*() const;
        Iterator& operator++();
        [[nodiscard]] bool operator!=(const Iterator& other) const;

    private:
        View* view_;
        std::size_t index_; // position in the driving pool's dense array
    };

    [[nodiscard]] Iterator begin();
    [[nodiscard]] Iterator end();

    /// Runs `fn(EntityId, Ts&...)` for every matching entity. Same semantics as
    /// range-for; sometimes reads better inside systems.
    template<typename Fn>
    void each(Fn&& fn);

private:
    std::tuple<ComponentPool<Ts>*...> pools_;
    std::size_t driving_size_ = 0; // dense size of the smallest pool
};

} // namespace dfn::ecs
