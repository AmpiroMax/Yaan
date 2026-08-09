/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 23:30:00
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
- Header-only (templates). Public surface frozen per stage-1 contract.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — sparse-set pool interface with batch
  removal hook (Rule 11), based on Quicky ECS.
- 09:08:2026 - 00:42:03: Stage 2 — template bodies implemented in-header;
  public surface unchanged.
- 09:08:2026 - 23:30:00: add() gains a const-lvalue overload (sim's report): T&& here is a TRUE rvalue ref, so adding a NAMED LOCAL failed to compile and blamed this header rather than the call site. Both value categories now work; move path unchanged, copy path explicit.
*/

#pragma once

#include "engine/core/ecs/sources/EntityId.h"

#include <cassert>
#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>
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
    ///
    /// Two overloads on purpose: `T&&` here is a TRUE rvalue reference (T is
    /// the class parameter, not deduced), so without the const-lvalue overload
    /// a named local cannot be added at all — and the error surfaces inside
    /// this header rather than at the call site, which reads as "the ECS is
    /// broken" instead of "that was an lvalue". The copy path is explicit and
    /// greppable; the move path stays exact.
    T& add(EntityId id, T&& component) { return emplace(id, std::move(component)); }
    T& add(EntityId id, const T& component) { return emplace(id, component); }

    /// See IComponentPool. O(1) via swap-and-pop.
    void remove(EntityId id) override {
        const auto it = sparse_.find(id.index);
        if (it == sparse_.end()) {
            return;
        }
        const std::size_t index = it->second;
        const std::size_t last = dense_.size() - 1;
        if (index != last) {
            dense_[index] = std::move(dense_[last]);
            owners_[index] = owners_[last];
            sparse_[owners_[index].index] = index;
        }
        dense_.pop_back();
        owners_.pop_back();
        sparse_.erase(it);
    }

    void remove_batch(std::span<const EntityId> ids) override {
        for (const EntityId id : ids) {
            remove(id);
        }
    }

    /// Returns the component of `id`, or nullptr if absent. O(1).
    [[nodiscard]] T* get(EntityId id) {
        const auto it = sparse_.find(id.index);
        return it == sparse_.end() ? nullptr : &dense_[it->second];
    }
    [[nodiscard]] const T* get(EntityId id) const {
        const auto it = sparse_.find(id.index);
        return it == sparse_.end() ? nullptr : &dense_[it->second];
    }

    [[nodiscard]] bool has(EntityId id) const override {
        return sparse_.find(id.index) != sparse_.end();
    }
    [[nodiscard]] std::size_t size() const override { return dense_.size(); }

    void clear() override {
        dense_.clear();
        owners_.clear();
        sparse_.clear();
    }

    /// Direct dense access for View iteration. `i < size()`.
    [[nodiscard]] T& data_at(std::size_t i) { return dense_[i]; }
    [[nodiscard]] const T& data_at(std::size_t i) const { return dense_[i]; }
    [[nodiscard]] EntityId owner_at(std::size_t i) const { return owners_[i]; }

private:
    /// Single insertion path shared by both add() overloads; U deduces to
    /// T or const T& and the push_back does the right thing for each.
    template<typename U>
    T& emplace(EntityId id, U&& component) {
        assert(!has(id) && "Entity already has this component");
        const std::size_t dense_index = dense_.size();
        dense_.push_back(std::forward<U>(component));
        owners_.push_back(id);
        sparse_[id.index] = dense_index;
        return dense_.back();
    }

    std::vector<T> dense_;
    std::vector<EntityId> owners_;
    std::unordered_map<uint32_t, std::size_t> sparse_;
};

} // namespace dfn::ecs
