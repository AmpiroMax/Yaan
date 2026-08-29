/*
Module: engine/core/events
File: engine/core/events/sources/EventBus.cpp

Responsibility:
- Non-template EventBus internals: dispatch, unsubscribe, queued pump.

Key items:
- EventBus::dispatch / unsubscribe / pump.

Dependencies:
- Uses: EventBus.h.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- pump() must deliver events posted BY handlers in the same pump (index loop
  over a growing queue) — documented header contract.
*/

#include "engine/core/events/sources/EventBus.h"

#include <algorithm>

namespace dfn::events {

EventBus::EventBus() = default;
EventBus::~EventBus() = default;

void EventBus::unsubscribe(SubscriptionId id) {
    const auto type_it = subscription_types_.find(id.value);
    if (type_it == subscription_types_.end()) {
        return;
    }
    const types::TypeId tid = type_it->second;
    subscription_types_.erase(type_it);

    const auto list_it = subscribers_.find(tid);
    if (list_it == subscribers_.end()) {
        return;
    }
    auto& list = list_it->second;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Subscriber& s) { return s.id == id.value; }),
               list.end());
    if (list.empty()) {
        subscribers_.erase(list_it);
    }
}

void EventBus::dispatch(types::TypeId type, const void* event) {
    const auto it = subscribers_.find(type);
    if (it == subscribers_.end()) {
        return;
    }
    // Copy the subscriber list: handlers may subscribe/unsubscribe for OTHER
    // event types, which can rehash the map (mutating the dispatched type's
    // list from inside its own handler is forbidden by the header contract).
    const std::vector<Subscriber> list = it->second;
    for (const Subscriber& s : list) {
        s.fn(event);
    }
}

void EventBus::pump() {
    // Index loop: handlers may post() and grow the queue; those events are
    // delivered within this same pump, in post order.
    for (std::size_t i = 0; i < queue_.size(); ++i) {
        const QueuedEvent current = queue_[i]; // copy: push_back may reallocate
        dispatch(current.type, current.payload.get());
    }
    queue_.clear();
}

} // namespace dfn::events
