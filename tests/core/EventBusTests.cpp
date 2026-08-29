/*
Module: tests
File: tests/core/EventBusTests.cpp

Responsibility:
- EventBus suite: immediate publish order, unsubscribe, queued post/pump,
  handler-posted events delivered in the same pump.

Dependencies:
- Uses: doctest, dfn_core (events).
- Used by: ctest (test_events).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/core/events/sources/EventBus.h"

#include <doctest/doctest.h>
#include <string>
#include <vector>

using dfn::events::EventBus;
using dfn::events::SubscriptionId;

namespace {
struct Ping {
    int value = 0;
};
struct Pong {
    int value = 0;
};
} // namespace

TEST_CASE("publish dispatches synchronously in subscription order") {
    EventBus bus;
    std::vector<int> log;
    bus.subscribe<Ping>([&](const Ping& p) { log.push_back(p.value * 10); });
    bus.subscribe<Ping>([&](const Ping& p) { log.push_back(p.value * 100); });

    bus.publish(Ping{3});
    REQUIRE(log.size() == 2);
    CHECK(log[0] == 30);   // first subscriber first
    CHECK(log[1] == 300);
    // Different event type: no cross-talk.
    bus.publish(Pong{1});
    CHECK(log.size() == 2);
}

TEST_CASE("unsubscribe stops delivery; double unsubscribe is a no-op") {
    EventBus bus;
    int hits = 0;
    const SubscriptionId id = bus.subscribe<Ping>([&](const Ping&) { ++hits; });
    bus.publish(Ping{});
    CHECK(hits == 1);
    bus.unsubscribe(id);
    bus.unsubscribe(id);
    bus.publish(Ping{});
    CHECK(hits == 1);
}

TEST_CASE("post is queued until pump; delivered in post order") {
    EventBus bus;
    std::vector<int> log;
    bus.subscribe<Ping>([&](const Ping& p) { log.push_back(p.value); });

    bus.post(Ping{1});
    bus.post(Ping{2});
    CHECK(log.empty()); // nothing until pump
    bus.pump();
    REQUIRE(log.size() == 2);
    CHECK(log[0] == 1);
    CHECK(log[1] == 2);
    bus.pump(); // queue was cleared
    CHECK(log.size() == 2);
}

TEST_CASE("events posted by handlers are delivered in the same pump") {
    EventBus bus;
    std::vector<std::string> log;
    bus.subscribe<Ping>([&](const Ping& p) {
        log.push_back("ping" + std::to_string(p.value));
        if (p.value < 3) {
            bus.post(Ping{p.value + 1}); // chain within one pump
        }
    });
    bus.post(Ping{1});
    bus.pump();
    REQUIRE(log.size() == 3);
    CHECK(log[2] == "ping3");
}

TEST_CASE("publish inside a handler of another type works") {
    EventBus bus;
    int pongs = 0;
    bus.subscribe<Pong>([&](const Pong&) { ++pongs; });
    bus.subscribe<Ping>([&](const Ping&) { bus.publish(Pong{}); });
    bus.publish(Ping{});
    CHECK(pongs == 1);
}
