#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <ntype/engine/events/EventBus.hpp>

using ntype::engine::events::EventBus;
using ntype::engine::events::Subscription;

namespace {

struct Hit {
    int damage = 0;
};
struct Other {};

} // namespace

TEST(EventBus, PublishReachesSubscribersOfThatTypeOnly) {
    EventBus bus;
    int total = 0;
    int others = 0;
    const auto hits = bus.subscribe<Hit>([&](const Hit& hit) { total += hit.damage; });
    const auto other = bus.subscribe<Other>([&](const Other&) { ++others; });
    bus.publish(Hit{3});
    bus.publish(Hit{4});
    EXPECT_EQ(total, 7);
    EXPECT_EQ(others, 0);
}

TEST(EventBus, SubscriptionUnsubscribesWhenDestroyed) {
    EventBus bus;
    int calls = 0;
    {
        const auto subscription = bus.subscribe<Hit>([&](const Hit&) { ++calls; });
        bus.publish(Hit{});
    }
    bus.publish(Hit{});
    EXPECT_EQ(calls, 1);
}

TEST(EventBus, QueuedEventsWaitForDispatch) {
    EventBus bus;
    std::vector<int> received;
    const auto subscription =
        bus.subscribe<Hit>([&](const Hit& hit) { received.push_back(hit.damage); });
    bus.enqueue(Hit{1});
    bus.enqueue(Hit{2});
    EXPECT_TRUE(received.empty());
    bus.dispatch_queued();
    EXPECT_EQ(received, (std::vector<int>{1, 2}));
    EXPECT_EQ(bus.queued_count(), 0U);
}

TEST(EventBus, HandlersMayEnqueueDuringDispatch) {
    EventBus bus;
    int others = 0;
    const auto chain = bus.subscribe<Hit>([&](const Hit&) { bus.enqueue(Other{}); });
    const auto count = bus.subscribe<Other>([&](const Other&) { ++others; });
    bus.enqueue(Hit{});
    bus.dispatch_queued();
    EXPECT_EQ(others, 1);
}

TEST(EventBus, HandlerMayUnsubscribeItselfAndSubscribeOthers) {
    EventBus bus;
    std::optional<Subscription> self;
    std::vector<Subscription> added;
    int calls = 0;
    self = bus.subscribe<Hit>([&](const Hit&) {
        ++calls;
        self.reset();
        added.push_back(bus.subscribe<Hit>([&](const Hit&) { ++calls; }));
    });
    bus.publish(Hit{});
    EXPECT_EQ(calls, 1);
    bus.publish(Hit{});
    EXPECT_EQ(calls, 2);
}

TEST(EventBus, SubscriptionMayOutliveTheBus) {
    Subscription subscription;
    {
        EventBus bus;
        subscription = bus.subscribe<Hit>([](const Hit&) {});
    }
    EXPECT_FALSE(subscription.is_active());
    subscription.reset();
}
