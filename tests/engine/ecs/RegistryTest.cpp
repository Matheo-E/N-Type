#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/engine/ecs/SystemScheduler.hpp>

using ntype::engine::ecs::Entity;
using ntype::engine::ecs::Registry;

namespace {

struct Position {
    int x = 0;
};
struct Name {
    std::string value;
};

} // namespace

TEST(Registry, CreatedEntitiesAreAliveAndDistinct) {
    Registry registry;
    const Entity a = registry.create();
    const Entity b = registry.create();
    EXPECT_NE(a, b);
    EXPECT_TRUE(registry.is_alive(a));
    EXPECT_TRUE(registry.is_alive(b));
    EXPECT_EQ(registry.alive_count(), 2U);
}

TEST(Registry, DestroyedHandleBecomesStaleWhenSlotIsReused) {
    Registry registry;
    const Entity old = registry.create();
    registry.emplace<Position>(old, 1);
    registry.destroy(old);
    const Entity reused = registry.create();
    EXPECT_EQ(reused.index(), old.index());
    EXPECT_NE(reused.generation(), old.generation());
    EXPECT_FALSE(registry.is_alive(old));
    EXPECT_EQ(registry.try_get<Position>(old), nullptr);
    EXPECT_EQ(registry.try_get<Position>(reused), nullptr);
}

TEST(Registry, EmplaceGetRemove) {
    Registry registry;
    const Entity entity = registry.create();
    registry.emplace<Position>(entity, 3);
    EXPECT_EQ(registry.get<Position>(entity).x, 3);
    registry.emplace<Position>(entity, 4);
    EXPECT_EQ(registry.get<Position>(entity).x, 4);
    registry.remove<Position>(entity);
    EXPECT_FALSE(registry.has<Position>(entity));
    EXPECT_THROW(static_cast<void>(registry.get<Position>(entity)), std::out_of_range);
}

TEST(Registry, EmplaceOnDeadEntityThrows) {
    Registry registry;
    const Entity entity = registry.create();
    registry.destroy(entity);
    EXPECT_THROW(registry.emplace<Position>(entity), std::invalid_argument);
}

TEST(Registry, ViewVisitsOnlyEntitiesWithAllComponents) {
    Registry registry;
    const Entity both = registry.create();
    const Entity only_position = registry.create();
    registry.emplace<Position>(both, 1);
    registry.emplace<Name>(both, "both");
    registry.emplace<Position>(only_position, 2);

    std::vector<Entity> visited;
    registry.view<Position, Name>().each([&](const Entity entity, Position& position, Name& name) {
        visited.push_back(entity);
        EXPECT_EQ(position.x, 1);
        EXPECT_EQ(name.value, "both");
    });
    EXPECT_EQ(visited, std::vector<Entity>{both});
}

TEST(Registry, ViewToleratesDestructionDuringIteration) {
    Registry registry;
    for (int i = 0; i < 10; ++i) {
        registry.emplace<Position>(registry.create(), i);
    }
    int visited = 0;
    registry.view<Position>().each([&](const Entity entity, Position&) {
        ++visited;
        registry.destroy(entity);
    });
    EXPECT_EQ(visited, 10);
    EXPECT_EQ(registry.alive_count(), 0U);
}

TEST(Registry, RemovalKeepsOtherComponentsIntact) {
    Registry registry;
    std::vector<Entity> entities;
    for (int i = 0; i < 5; ++i) {
        entities.push_back(registry.create());
        registry.emplace<Position>(entities.back(), i);
    }
    registry.destroy(entities[1]);
    for (const int i : {0, 2, 3, 4}) {
        EXPECT_EQ(registry.get<Position>(entities[static_cast<std::size_t>(i)]).x, i);
    }
}

TEST(Registry, ClearInvalidatesEveryHandle) {
    Registry registry;
    const Entity entity = registry.create();
    registry.clear();
    EXPECT_FALSE(registry.is_alive(entity));
    EXPECT_EQ(registry.alive_count(), 0U);
}

namespace {

class RecordingSystem final : public ntype::engine::ecs::ISystem {
public:
    RecordingSystem(std::vector<int>& log, int id) : log_(log), id_(id) {}
    void update(Registry&, float) override { log_.push_back(id_); }
    [[nodiscard]] std::string_view name() const override { return "Recording"; }

private:
    std::vector<int>& log_;
    int id_;
};

} // namespace

TEST(SystemScheduler, RunsSystemsInInsertionOrder) {
    std::vector<int> log;
    ntype::engine::ecs::SystemScheduler scheduler;
    scheduler.add<RecordingSystem>(log, 1);
    scheduler.add<RecordingSystem>(log, 2);
    scheduler.add<RecordingSystem>(log, 3);
    Registry registry;
    scheduler.update(registry, 0.016F);
    EXPECT_EQ(log, (std::vector<int>{1, 2, 3}));
}
