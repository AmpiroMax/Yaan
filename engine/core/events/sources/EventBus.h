/*
Module: engine/core/events
File: engine/core/events/sources/EventBus.h

Responsibility:
- Typed publish/subscribe event bus: immediate dispatch (publish) and queued
  dispatch (post + pump) for decoupled cross-module notification.

Key items:
- EventBus: subscribe/unsubscribe/publish/post/pump.
- SubscriptionId: handle for unsubscribing.

Dependencies:
- Uses: engine/core/types (TypeId, Handle).
- Used by: world::ChunkManager (ChunkLoaded/ChunkUnloaded), gameplay events
  (NpcSpoke, SkillUsed, ...), app-layer wiring between sibling modules.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Events are plain structs (Rule 8 spirit): copyable data, no backend types,
  reference entities by EntityId.
- Single-threaded by contract, like ecs::World: publish/post/pump only on the
  simulation thread.
- Public surface frozen per stage-1 contract; template bodies in-header,
  non-template bodies in EventBus.cpp.
*/

#pragma once

#include "engine/core/types/sources/Handle.h"
#include "engine/core/types/sources/TypeId.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dfn::events {

struct SubscriptionTag;
/// Returned by subscribe(); pass to unsubscribe(). 0 = invalid.
using SubscriptionId = types::Handle<SubscriptionTag, uint64_t>;

/// Typed event bus.
///
/// Two delivery modes:
/// - publish(e): synchronous — every subscriber of E runs before publish returns.
///   Use for structural notifications where ordering against the caller matters
///   (e.g. ChunkUnloaded must run handlers before memory is freed).
/// - post(e) + pump(): queued — events are buffered and delivered in post order
///   when the app loop calls pump() at its defined point in the tick. Use for
///   gameplay notifications (SkillUsed, NpcSpoke, ...).
///
/// Handlers may post() further events (delivered in the same pump), but must not
/// subscribe/unsubscribe from inside a handler for the event type being
/// dispatched.
class EventBus {
public:
    EventBus();
    ~EventBus();
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /// Registers `handler` for events of type E. E must be a copyable plain
    /// struct. Returns the id used to unsubscribe. Subscribers are invoked in
    /// subscription order.
    template<typename E>
    SubscriptionId subscribe(std::function<void(const E&)> handler) {
        const types::TypeId tid = types::type_id<E>();
        const SubscriptionId id{next_subscription_++};
        subscribers_[tid].push_back(Subscriber{
            id.value,
            [fn = std::move(handler)](const void* event) {
                fn(*static_cast<const E*>(event));
            }});
        subscription_types_[id.value] = tid;
        return id;
    }

    /// Removes a subscription; no-op if already removed.
    void unsubscribe(SubscriptionId id);

    /// Immediate dispatch to all current subscribers of E.
    template<typename E>
    void publish(const E& event) {
        dispatch(types::type_id<E>(), &event);
    }

    /// Queues a copy of `event` for the next pump().
    template<typename E>
    void post(const E& event) {
        queue_.push_back(QueuedEvent{types::type_id<E>(), std::make_shared<E>(event)});
    }

    /// Delivers all queued events (including those posted by handlers during
    /// this pump) and clears the queue. Called once per simulation tick by the
    /// app loop.
    void pump();

private:
    struct Subscriber {
        uint64_t id = 0;
        std::function<void(const void*)> fn; // type-erased, bound to E at subscribe
    };
    struct QueuedEvent {
        types::TypeId type = 0;
        std::shared_ptr<void> payload; // owns a copy of E (typed deleter)
    };

    /// Invokes every subscriber of `type` with `event` (points at a live E).
    void dispatch(types::TypeId type, const void* event);

    std::unordered_map<types::TypeId, std::vector<Subscriber>> subscribers_;
    std::unordered_map<uint64_t, types::TypeId> subscription_types_;
    std::vector<QueuedEvent> queue_;
    uint64_t next_subscription_ = 1;
};

} // namespace dfn::events
