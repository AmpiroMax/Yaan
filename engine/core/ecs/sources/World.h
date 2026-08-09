/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 23:30:00
Module: engine/core/ecs
File: engine/core/ecs/sources/World.h

Responsibility:
- Central entity/component registry — the single source of truth for all ECS data
  (Rule 10). Entity lifecycle, component CRUD, queries, resources, deferred
  destruction, and the streaming additions: batch spawn/destroy and the
  entity<->group (chunk) index (Q22, Rule 11).

Key items:
- World: the main ECS class systems and game code interact with.

Dependencies:
- Uses: EntityId, ComponentPool, View, engine/core/types (TypeId).
- Used by: every system, engine/world ChunkManager, engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- World stays backend-agnostic: no bgfx/Jolt/ozz/GLFW types, ever.
- Streaming paths use ONLY the batch APIs (Rule 11); per-entity spawn/destroy
  in a chunk load/unload is a violation.
- Public surface frozen per stage-1 contract; template bodies live in-header
  (templates), non-template bodies in World.cpp.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — Quicky ECS World plus batch
  spawn/destroy, batch component attach, and the GroupId index for chunk
  streaming (Q22, Rule 11); shape agreed with sim/render via Rule 26 messages.
- 09:08:2026 - 00:42:03: Stage 2 — implemented. Private section: stage-1 pimpl
  replaced by concrete members (template methods need member access in-header);
  public surface unchanged.
- 09:08:2026 - 23:30:00: add() documents the value categories it accepts (sim's report); the lvalue path is fixed in ComponentPool via a const-lvalue overload.
*/

#pragma once

#include "engine/core/ecs/sources/ComponentPool.h"
#include "engine/core/ecs/sources/EntityId.h"
#include "engine/core/ecs/sources/View.h"
#include "engine/core/types/sources/TypeId.h"

#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dfn::ecs {

/// The ECS registry. One World per running game (plus throwaway Worlds in tests).
///
/// Threading: World is single-threaded by contract. All structural mutation and
/// iteration happen on the simulation thread; other threads (LLM, audio) never
/// touch it directly — they communicate via queues owned by systems.
class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // --- Entity lifecycle -----------------------------------------------------

    /// Allocates one entity (reuses a free slot or grows storage).
    /// NOT for streaming paths — chunk load/unload must use the batch API (Rule 11).
    [[nodiscard]] EntityId spawn();

    /// Destroys one entity immediately: removes all its components, increments the
    /// slot generation, releases the slot. Never call during view iteration —
    /// use destroy_deferred() there.
    void destroy(EntityId id);

    /// Queues `id` for destruction at the next flush_destroyed(). Safe during
    /// iteration (Rule 9); double-queueing is harmless.
    void destroy_deferred(EntityId id);

    /// Destroys everything queued by destroy_deferred(). The app loop calls this
    /// once per simulation tick, after all systems have run.
    void flush_destroyed();

    /// True iff `id` refers to a live entity (index valid AND generation matches).
    [[nodiscard]] bool alive(EntityId id) const;

    /// Number of currently live entities.
    [[nodiscard]] std::size_t entity_count() const;

    /// Destroys all entities, components, and resources. Test/teardown helper.
    void clear();

    // --- Batch lifecycle (Rule 11, Q22 — the streaming path) ------------------

    /// Spawns exactly `out_ids.size()` entities in one operation and writes their
    /// ids into `out_ids`. All spawned entities are assigned to `group`
    /// (NO_GROUP = ungrouped). This is the ONLY legal way to create entities on a
    /// chunk-load path; ChunkManager passes the chunk's GroupId.
    void spawn_batch(std::span<EntityId> out_ids, GroupId group = NO_GROUP);

    /// Destroys all listed entities in one operation: each component pool is
    /// visited once with the whole batch (not once per entity). Dead/null ids are
    /// skipped silently.
    void destroy_batch(std::span<const EntityId> ids);

    /// Destroys every live entity currently assigned to `group` (one batch).
    /// This is the chunk-unload path. Returns the number destroyed.
    std::size_t destroy_group(GroupId group);

    // --- Entity <-> group (chunk) association (Q22) ---------------------------

    /// Assigns `id` to `group`, replacing any previous assignment.
    /// NO_GROUP detaches. Used when an entity crosses a chunk boundary.
    void set_group(EntityId id, GroupId group);

    /// Group of `id`, or NO_GROUP if unassigned/dead.
    [[nodiscard]] GroupId group_of(EntityId id) const;

    /// All live entities currently assigned to `group`. The span is valid until
    /// the next structural change (spawn/destroy/set_group) on this World.
    [[nodiscard]] std::span<const EntityId> entities_in_group(GroupId group) const;

    // --- Components -----------------------------------------------------------

    /// Adds a component to a live entity. Precondition: alive(id) && !has<T>(id).
    /// The reference stays valid until the next add/remove of a T on any entity.
    ///
    /// Accepts BOTH value categories — a temporary is moved, a named local is
    /// copied:
    ///     world.add(id, components::CarriedLight{...});  // moved
    ///     components::CarriedLight l{}; world.add(id, l); // copied
    /// Building the value in a local first is the natural style whenever it
    /// takes more than one statement to assemble, and it used to fail to
    /// compile with an error pointing into this header.
    template<typename T>
    T& add(EntityId id, T&& component) {
        assert(alive(id) && "Cannot add a component to a dead entity");
        return get_or_create_pool<std::remove_cvref_t<T>>().add(
            id, std::forward<T>(component));
    }

    /// Attaches a copy of `prototype` to every id in the batch (one pool visit).
    /// Chunk streaming attaches Transform/PreviousTransform/RenderMesh/LocalBounds
    /// this way — cheap by construction.
    template<typename T>
    void add_batch(std::span<const EntityId> ids, const T& prototype) {
        auto& pool = get_or_create_pool<T>();
        for (const EntityId id : ids) {
            assert(alive(id) && "Cannot add a component to a dead entity");
            pool.add(id, T(prototype));
        }
    }

    /// Attaches per-entity values: values[i] goes to ids[i]. Sizes must match.
    template<typename T>
    void add_batch(std::span<const EntityId> ids, std::span<const T> values) {
        assert(ids.size() == values.size() && "add_batch: ids/values size mismatch");
        auto& pool = get_or_create_pool<T>();
        for (std::size_t i = 0; i < ids.size(); ++i) {
            assert(alive(ids[i]) && "Cannot add a component to a dead entity");
            pool.add(ids[i], T(values[i]));
        }
    }

    /// Removes T from `id` if present; no-op otherwise.
    template<typename T>
    void remove(EntityId id) {
        if (auto* pool = get_pool<T>()) {
            pool->remove(id);
        }
    }

    /// The T of `id`, or nullptr if absent/dead. O(1).
    template<typename T>
    [[nodiscard]] T* get(EntityId id) {
        if (!alive(id)) {
            return nullptr;
        }
        auto* pool = get_pool<T>();
        return pool ? pool->get(id) : nullptr;
    }
    template<typename T>
    [[nodiscard]] const T* get(EntityId id) const {
        if (!alive(id)) {
            return nullptr;
        }
        const auto* pool = get_pool<T>();
        return pool ? pool->get(id) : nullptr;
    }

    template<typename T>
    [[nodiscard]] bool has(EntityId id) const {
        const auto* pool = get_pool<T>();
        return pool != nullptr && alive(id) && pool->has(id);
    }

    // --- Queries --------------------------------------------------------------

    /// All entities that have every component in Ts... See View.h for iteration
    /// semantics. Cheap to construct; do not store across frames.
    template<typename... Ts>
    [[nodiscard]] View<Ts...> view() {
        return View<Ts...>(&get_or_create_pool<Ts>()...);
    }

    // --- Resources (type-keyed singletons, Rule 10) ---------------------------

    /// Registers/replaces the unique T resource (shared read-only data such as
    /// loaded content tables; never per-entity state).
    template<typename T>
    void add_resource(T&& resource) {
        using Stored = std::remove_cvref_t<T>;
        resources_[types::type_id<Stored>()] =
            std::make_shared<Stored>(std::forward<T>(resource));
    }

    /// The unique T resource. Precondition: has_resource<T>() (asserted).
    template<typename T>
    [[nodiscard]] T& resource() {
        const auto it = resources_.find(types::type_id<T>());
        assert(it != resources_.end() && "Resource not found");
        return *static_cast<T*>(it->second.get());
    }
    template<typename T>
    [[nodiscard]] const T& resource() const {
        const auto it = resources_.find(types::type_id<T>());
        assert(it != resources_.end() && "Resource not found");
        return *static_cast<const T*>(it->second.get());
    }

    template<typename T>
    [[nodiscard]] bool has_resource() const {
        return resources_.find(types::type_id<T>()) != resources_.end();
    }

private:
    template<typename T>
    [[nodiscard]] ComponentPool<T>& get_or_create_pool() {
        const types::TypeId tid = types::type_id<T>();
        auto it = pools_.find(tid);
        if (it == pools_.end()) {
            it = pools_.emplace(tid, std::make_unique<ComponentPool<T>>()).first;
        }
        return static_cast<ComponentPool<T>&>(*it->second);
    }

    template<typename T>
    [[nodiscard]] ComponentPool<T>* get_pool() const {
        const auto it = pools_.find(types::type_id<T>());
        return it == pools_.end()
                   ? nullptr
                   : static_cast<ComponentPool<T>*>(it->second.get());
    }

    // Non-template internals (World.cpp).
    void detach_from_group(EntityId id);
    void attach_to_group(EntityId id, GroupId group);
    void release_slot(EntityId id);

    // Entity slots (index-aligned vectors).
    std::vector<uint32_t> generations_;
    std::vector<bool> alive_;
    std::vector<GroupId> slot_group_;       // NO_GROUP when unassigned
    std::vector<uint32_t> slot_group_pos_;  // position inside its group's vector
    std::vector<uint32_t> free_list_;
    std::vector<EntityId> deferred_destroy_;

    std::unordered_map<types::TypeId, std::unique_ptr<IComponentPool>> pools_;
    std::unordered_map<GroupId, std::vector<EntityId>> groups_;
    std::unordered_map<types::TypeId, std::shared_ptr<void>> resources_;
    std::size_t live_count_ = 0;
};

} // namespace dfn::ecs
