/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
Module: engine/core/ecs
File: engine/core/ecs/sources/ComponentPool.h

Responsibility:
- Dense sparse-set storage for one component type: O(1) add/remove/get with
  swap-and-pop deletion; packed dense array for cache-friendly iteration.

Key items:
- IComponentPool: type-erased base so World can own pools uniformly.
- ComponentPool<T>: concrete typed pool (dense array + owners + sparse map).

Dependencies:
- Uses: EntityId.
- Used by: World (owns pools), View (iterates pools).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Components are plain movable structs (Rule 8): no virtuals, no backend types,
  no pointers to other components.
- STAGE 1 CONTRACT: declarations only; method bodies arrive in stage 2 (in this
  header — templates stay header-only).
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — sparse-set pool interface with batch
  removal hook (Rule 11), based on Quicky ECS.
*/

#pragma once

#include "engine/core/ecs/sources/EntityId.h"

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

namespace dfn::ecs {

/// Type-erased pool base. World stores pools as IComponentPool and downcasts by
/// TypeId; only operations needed without knowing T live here.
class IComponentPool {
public:
    virtual ~IComponentPool() = default;

    /// Removes the component of `id` if present (swap-and-pop). No-op otherwise.
    virtual void remove(EntityId id) = 0;

    /// Batch removal for streaming paths (Rule 11): one call per pool per batch
    /// instead of N virtual calls. Ids without this component are skipped.
    virtual void remove_batch(std::span<const EntityId> ids) = 0;

    [[nodiscard]] virtual bool has(EntityId id) const = 0;
    [[nodiscard]] virtual std::size_t size() const = 0;
    virtual void clear() = 0;
};

/// Dense sparse-set pool for component type T.
///
/// Layout:
///   sparse:  entity.index -> dense index        (O(1) lookup)
///   dense:   [c0, c1, c2, ...]                  (packed, no holes)
///   owners:  [e0, e1, e2, ...]                  (parallel to dense)
///
/// Removal is swap-and-pop: the last dense element moves into the freed slot, so
/// the array stays packed. Iteration over `dense` is linear and cache-friendly.
/// T must be movable; T must be a plain struct per Rule 8.
template<typename T>
class ComponentPool final : public IComponentPool {
public:
    /// Adds a component for `id`. Precondition: !has(id) (asserted).
    /// Returns a reference valid until the next add/remove on this pool.
    T& add(EntityId id, T&& component);

    /// See IComponentPool. O(1) via swap-and-pop.
    void remove(EntityId id) override;
    void remove_batch(std::span<const EntityId> ids) override;

    /// Returns the component of `id`, or nullptr if absent. O(1).
    [[nodiscard]] T* get(EntityId id);
    [[nodiscard]] const T* get(EntityId id) const;

    [[nodiscard]] bool has(EntityId id) const override;
    [[nodiscard]] std::size_t size() const override;
    void clear() override;

    /// Direct dense access for View iteration. `i < size()`.
    [[nodiscard]] T& data_at(std::size_t i);
    [[nodiscard]] const T& data_at(std::size_t i) const;
    [[nodiscard]] EntityId owner_at(std::size_t i) const;

private:
    std::vector<T> dense_;
    std::vector<EntityId> owners_;
    std::unordered_map<uint32_t, std::size_t> sparse_;
};

} // namespace dfn::ecs
