#include <gtest/gtest.h>

#include "warehouser_rl_bridge/reward_calculator.hpp"

using namespace warehouser;

class RewardCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create world with robot
        prev_world_.entities.resize(1);
        prev_world_.entities[0].id = "robot";
        prev_world_.entities[0].type = 0;  // TYPE_ROBOT
        prev_world_.entities[0].x = 0.0f;
        prev_world_.entities[0].y = 0.0f;
        prev_world_.entities[0].is_carrying = false;

        curr_world_ = prev_world_;

        // Create goal
        goal_.x = 5.0f;
        goal_.y = 0.0f;
        goal_.active = true;
    }

    warehouser_msgs::msg::WorldState prev_world_;
    warehouser_msgs::msg::WorldState curr_world_;
    warehouser_msgs::msg::Goal goal_;
};

TEST_F(RewardCalculatorTest, ProgressRewardWhenCloser) {
    // Robot moves from x=0 to x=1 toward goal at x=5
    curr_world_.entities[0].x = 1.0f;

    RewardCalculator calc;
    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    // Should get positive progress reward
    EXPECT_GT(result.reward, -0.5f);  // Account for time penalty
    EXPECT_FALSE(result.terminated);
    EXPECT_FALSE(result.truncated);
}

TEST_F(RewardCalculatorTest, NegativeRewardWhenFarther) {
    // Robot moves from x=3 to x=2 (away from goal at x=5)
    prev_world_.entities[0].x = 3.0f;
    curr_world_.entities[0].x = 2.0f;

    RewardCalculator calc;
    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    // Should get negative reward (negative progress + time penalty)
    EXPECT_LT(result.reward, 0.0f);
}

TEST_F(RewardCalculatorTest, SuccessBonusWhenGoalReached) {
    // Robot at goal position
    curr_world_.entities[0].x = 5.0f;
    curr_world_.entities[0].y = 0.0f;

    RewardConfig config;
    config.success_bonus = 100.0f;
    config.goal_threshold = 0.5f;
    RewardCalculator calc(config);

    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    EXPECT_FLOAT_EQ(result.reward, 100.0f);
    EXPECT_TRUE(result.terminated);
    EXPECT_EQ(result.termination_reason, "Goal reached");
}

TEST_F(RewardCalculatorTest, GoalReachedWithinThreshold) {
    // Robot within threshold of goal
    curr_world_.entities[0].x = 4.8f;
    curr_world_.entities[0].y = 0.1f;

    RewardConfig config;
    config.success_bonus = 100.0f;
    config.goal_threshold = 0.5f;
    RewardCalculator calc(config);

    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    EXPECT_TRUE(result.terminated);
    EXPECT_FLOAT_EQ(result.reward, 100.0f);
}

TEST_F(RewardCalculatorTest, TruncationAtMaxSteps) {
    RewardCalculator calc;
    auto result = calc.calculate(prev_world_, curr_world_, goal_, 500, 500);

    EXPECT_TRUE(result.truncated);
    EXPECT_FALSE(result.terminated);
    EXPECT_EQ(result.termination_reason, "Max steps reached");
}

TEST_F(RewardCalculatorTest, PickupBonusWhenPickingObject) {
    // Robot picks up object
    prev_world_.entities[0].is_carrying = false;
    curr_world_.entities[0].is_carrying = true;

    RewardConfig config;
    config.pickup_bonus = 50.0f;
    RewardCalculator calc(config);

    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    // Reward should include pickup bonus
    EXPECT_GT(result.reward, 40.0f);  // pickup_bonus + time_penalty + progress
}

TEST_F(RewardCalculatorTest, NoPickupBonusWhenAlreadyCarrying) {
    // Robot was already carrying
    prev_world_.entities[0].is_carrying = true;
    curr_world_.entities[0].is_carrying = true;

    RewardConfig config;
    config.pickup_bonus = 50.0f;
    RewardCalculator calc(config);

    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    // Should not include pickup bonus
    EXPECT_LT(result.reward, 10.0f);
}

TEST_F(RewardCalculatorTest, TimePenaltyApplied) {
    // Robot doesn't move
    curr_world_.entities[0].x = 0.0f;
    curr_world_.entities[0].y = 0.0f;

    RewardConfig config;
    config.time_penalty = -0.5f;
    config.progress_weight = 0.0f;  // Disable progress to isolate time penalty
    RewardCalculator calc(config);

    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    EXPECT_FLOAT_EQ(result.reward, -0.5f);
}

TEST_F(RewardCalculatorTest, NoRobotTerminates) {
    curr_world_.entities.clear();  // Remove robot

    RewardCalculator calc;
    auto result = calc.calculate(prev_world_, curr_world_, goal_, 1, 500);

    EXPECT_TRUE(result.terminated);
    EXPECT_EQ(result.termination_reason, "Robot not found");
}

TEST_F(RewardCalculatorTest, ConfigAccessible) {
    RewardConfig config;
    config.progress_weight = 2.0f;
    config.goal_threshold = 1.0f;
    RewardCalculator calc(config);

    EXPECT_FLOAT_EQ(calc.config().progress_weight, 2.0f);
    EXPECT_FLOAT_EQ(calc.config().goal_threshold, 1.0f);
}
