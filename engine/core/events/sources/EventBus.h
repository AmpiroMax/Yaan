/*
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
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
- STAGE 1 CONTRACT: declarations only; bodies arrive in stage 2.
*/
/*
UPD:
- 09:08:2026 - 00:16:55: Stage 1 contract — typed bus with immediate and queued
  dispatch.
*/

#pragma once

#include "engine/core/types/sources/Handle.h"
#include "engine/core/types/sources/TypeId.h"

#include <functional>
#include <memory>

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
    SubscriptionId subscribe(std::function<void(const E&)> handler);

    /// Removes a subscription; no-op if already removed.
    void unsubscribe(SubscriptionId id);

    /// Immediate dispatch to all current subscribers of E.
    template<typename E>
    void publish(const E& event);

    /// Queues a copy of `event` for the next pump().
    template<typename E>
    void post(const E& event);

    /// Delivers all queued events (including those posted by handlers during
    /// this pump) and clears the queue. Called once per simulation tick by the
    /// app loop.
    void pump();

private:
    struct Impl; // per-TypeId subscriber lists + type-erased event queue
    std::unique_ptr<Impl> impl_;
};

} // namespace dfn::events
