/*
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
- Header-only (templates). Public surface frozen per stage-1 contract.
*/

#pragma once

#include "engine/core/ecs/sources/ComponentPool.h"

#include <cstddef>
#include <tuple>
#include <utility>

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
    explicit View(ComponentPool<Ts>*... pools) : pools_(pools...) {
        const std::size_t sizes[] = {pools->size()...};
        driving_index_ = 0;
        driving_size_ = sizes[0];
        for (std::size_t i = 1; i < sizeof...(Ts); ++i) {
            if (sizes[i] < driving_size_) {
                driving_size_ = sizes[i];
                driving_index_ = i;
            }
        }
    }

    /// Forward iterator yielding std::tuple<EntityId, Ts&...>.
    class Iterator {
    public:
        Iterator(View* view, std::size_t index) : view_(view), index_(index) {
            advance_to_valid();
        }

        [[nodiscard]] std::tuple<EntityId, Ts&...> operator*() const {
            const EntityId id = view_->entity_at(index_);
            return std::tuple<EntityId, Ts&...>(
                id, *std::get<ComponentPool<Ts>*>(view_->pools_)->get(id)...);
        }

        Iterator& operator++() {
            ++index_;
            advance_to_valid();
            return *this;
        }

        [[nodiscard]] bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }

    private:
        void advance_to_valid() {
            while (index_ < view_->driving_size_
                   && !view_->all_have(view_->entity_at(index_))) {
                ++index_;
            }
        }

        View* view_;
        std::size_t index_; // position in the driving pool's dense array
    };

    [[nodiscard]] Iterator begin() { return Iterator(this, 0); }
    [[nodiscard]] Iterator end() { return Iterator(this, driving_size_); }

    /// Runs `fn(EntityId, Ts&...)` for every matching entity. Same semantics as
    /// range-for; sometimes reads better inside systems.
    template<typename Fn>
    void each(Fn&& fn) {
        for (auto&& row : *this) {
            std::apply(fn, row);
        }
    }

private:
    friend class Iterator;

    /// Owner entity at dense position `i` of the driving pool. Dispatched by
    /// driving_index_ over the pool tuple (comma-fold; no allocation).
    [[nodiscard]] EntityId entity_at(std::size_t i) const {
        EntityId result = EntityId::null();
        std::size_t k = 0;
        ((k++ == driving_index_
              ? (result = std::get<ComponentPool<Ts>*>(pools_)->owner_at(i), 0)
              : 0),
         ...);
        return result;
    }

    [[nodiscard]] bool all_have(EntityId id) const {
        return (std::get<ComponentPool<Ts>*>(pools_)->has(id) && ...);
    }

    std::tuple<ComponentPool<Ts>*...> pools_;
    std::size_t driving_size_ = 0;  // dense size of the smallest pool
    std::size_t driving_index_ = 0; // which pool in Ts... drives iteration
};

} // namespace dfn::ecs
