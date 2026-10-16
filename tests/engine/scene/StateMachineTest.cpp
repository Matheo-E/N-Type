#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <ntype/engine/platform/NullRenderer.hpp>
#include <ntype/engine/scene/StateMachine.hpp>

using ntype::engine::scene::IState;
using ntype::engine::scene::StateMachine;

namespace {

class LoggingState final : public IState {
public:
    LoggingState(std::vector<std::string>& log, std::string name, StateMachine* machine = nullptr)
        : log_(log), name_(std::move(name)), machine_(machine) {}

    void on_enter() override { log_.push_back(name_ + ":enter"); }
    void on_exit() override { log_.push_back(name_ + ":exit"); }
    void handle_event(const ntype::engine::platform::InputEvent&) override {}
    void update(float) override {
        log_.push_back(name_ + ":update");
        if (machine_ != nullptr) {
            // Replacing itself from inside update must be safe.
            machine_->replace(std::make_unique<LoggingState>(log_, "next"));
            machine_ = nullptr;
        }
    }
    void render(ntype::engine::platform::IRenderer&, float) override {}

private:
    std::vector<std::string>& log_;
    std::string name_;
    StateMachine* machine_;
};

} // namespace

TEST(StateMachine, TransitionsAreAppliedAtTheNextUpdate) {
    std::vector<std::string> log;
    StateMachine machine;
    machine.push(std::make_unique<LoggingState>(log, "first", &machine));
    EXPECT_EQ(machine.current(), nullptr);
    machine.update(0.0F);
    machine.update(0.0F);
    EXPECT_EQ(log, (std::vector<std::string>{"first:enter", "first:update", "first:exit",
                                             "next:enter", "next:update"}));
    EXPECT_EQ(machine.depth(), 1U);
}

TEST(StateMachine, PopReturnsToThePreviousState) {
    std::vector<std::string> log;
    StateMachine machine;
    machine.push(std::make_unique<LoggingState>(log, "a"));
    machine.push(std::make_unique<LoggingState>(log, "b"));
    machine.apply_pending();
    machine.pop();
    machine.update(0.0F);
    EXPECT_EQ(log.back(), "a:update");
    ntype::engine::platform::NullRenderer renderer;
    machine.render(renderer, 0.0F);
}
