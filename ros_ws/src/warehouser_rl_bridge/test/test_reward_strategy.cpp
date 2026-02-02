#include <gtest/gtest.h>

#include <memory>

#include "warehouser_rl_bridge/reward_strategy.hpp"

using namespace warehouser;

class RewardStrategyTest : public ::testing::Test {
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

    RewardContext makeContext(int step = 1, int max_steps = 500,
                               size_t robot_index = 0) {
        return RewardContext{prev_world_, curr_world_, goal_,
                             step, max_steps, robot_index};
    }

    warehouser_msgs::msg::WorldState prev_world_;
    warehouser_msgs::msg::WorldState curr_world_;
    warehouser_msgs::msg::Goal goal_;
};

// ============ NavigationRewardStrategy Tests ============

TEST_F(RewardStrategyTest, NavigationProgressReward) {
    // Robot moves closer to goal
    curr_world_.entities[0].x = 1.0f;

    NavigationRewardStrategy strategy;
    auto result = strategy.calculate(makeContext());

    // Should get positive reward for progress
    EXPECT_GT(result.reward, 0.0f);
    EXPECT_FALSE(result.terminated);
}

TEST_F(RewardStrategyTest, NavigationNegativeProgress) {
    // Robot moves away from goal
    prev_world_.entities[0].x = 3.0f;
    curr_world_.entities[0].x = 2.0f;

    NavigationRewardStrategy strategy;
    auto result = strategy.calculate(makeContext());

    // Should get negative reward for moving away
    EXPECT_LT(result.reward, 0.0f);
}

TEST_F(RewardStrategyTest, NavigationGoalReached) {
    // Robot at goal
    curr_world_.entities[0].x = 5.0f;

    NavigationConfig config;
    config.goal_threshold = 0.5f;
    config.success_bonus = 100.0f;
    NavigationRewardStrategy strategy(config);

    auto result = strategy.calculate(makeContext());

    EXPECT_TRUE(result.terminated);
    EXPECT_FLOAT_EQ(result.reward, 100.0f);
    EXPECT_EQ(result.termination_reason, "Goal reached");
}

TEST_F(RewardStrategyTest, NavigationGoalWithinThreshold) {
    // Robot within threshold
    curr_world_.entities[0].x = 4.7f;
    curr_world_.entities[0].y = 0.2f;

    NavigationConfig config;
    config.goal_threshold = 0.5f;
    config.success_bonus = 50.0f;
    NavigationRewardStrategy strategy(config);

    auto result = strategy.calculate(makeContext());

    EXPECT_TRUE(result.terminated);
}

TEST_F(RewardStrategyTest, NavigationNameIsCorrect) {
    NavigationRewardStrategy strategy;
    EXPECT_EQ(strategy.name(), "navigation");
}

// ============ CollisionRewardStrategy Tests ============

TEST_F(RewardStrategyTest, CollisionNoRobotTerminates) {
    curr_world_.entities.clear();  // Remove robot

    CollisionConfig config;
    config.collision_penalty = -100.0f;
    CollisionRewardStrategy strategy(config);

    auto result = strategy.calculate(makeContext());

    EXPECT_TRUE(result.terminated);
    EXPECT_FLOAT_EQ(result.reward, -100.0f);
    EXPECT_EQ(result.termination_reason, "Robot not found");
}

TEST_F(RewardStrategyTest, CollisionWithRobotNoEffect) {
    CollisionRewardStrategy strategy;
    auto result = strategy.calculate(makeContext());

    EXPECT_FALSE(result.terminated);
    EXPECT_FLOAT_EQ(result.reward, 0.0f);
}

TEST_F(RewardStrategyTest, CollisionNameIsCorrect) {
    CollisionRewardStrategy strategy;
    EXPECT_EQ(strategy.name(), "collision");
}

// ============ TimeRewardStrategy Tests ============

TEST_F(RewardStrategyTest, TimeAppliesPenalty) {
    TimeConfig config;
    config.time_penalty = -0.5f;
    TimeRewardStrategy strategy(config);

    auto result = strategy.calculate(makeContext());

    EXPECT_FLOAT_EQ(result.reward, -0.5f);
    EXPECT_FALSE(result.terminated);
}

TEST_F(RewardStrategyTest, TimeDefaultPenalty) {
    TimeRewardStrategy strategy;
    auto result = strategy.calculate(makeContext());

    EXPECT_FLOAT_EQ(result.reward, -0.1f);  // Default
}

TEST_F(RewardStrategyTest, TimeNameIsCorrect) {
    TimeRewardStrategy strategy;
    EXPECT_EQ(strategy.name(), "time");
}

// ============ PickPlaceRewardStrategy Tests ============

TEST_F(RewardStrategyTest, PickPlacePickupBonus) {
    prev_world_.entities[0].is_carrying = false;
    curr_world_.entities[0].is_carrying = true;

    PickPlaceConfig config;
    config.pickup_bonus = 50.0f;
    PickPlaceRewardStrategy strategy(config);

    auto result = strategy.calculate(makeContext());

    EXPECT_FLOAT_EQ(result.reward, 50.0f);
}

TEST_F(RewardStrategyTest, PickPlacePlaceBonus) {
    prev_world_.entities[0].is_carrying = true;
    curr_world_.entities[0].is_carrying = false;

    PickPlaceConfig config;
    config.place_bonus = 30.0f;
    PickPlaceRewardStrategy strategy(config);

    auto result = strategy.calculate(makeContext());

    EXPECT_FLOAT_EQ(result.reward, 30.0f);
}

TEST_F(RewardStrategyTest, PickPlaceNoChangeNoBonus) {
    // Already carrying
    prev_world_.entities[0].is_carrying = true;
    curr_world_.entities[0].is_carrying = true;

    PickPlaceConfig config;
    config.pickup_bonus = 50.0f;
    PickPlaceRewardStrategy strategy(config);

    auto result = strategy.calculate(makeContext());

    EXPECT_FLOAT_EQ(result.reward, 0.0f);
}

TEST_F(RewardStrategyTest, PickPlaceNameIsCorrect) {
    PickPlaceRewardStrategy strategy;
    EXPECT_EQ(strategy.name(), "pick_place");
}

// ============ CompositeRewardStrategy Tests ============

TEST_F(RewardStrategyTest, CompositeAddsRewards) {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    // Add time penalty
    TimeConfig time_config;
    time_config.time_penalty = -0.1f;
    composite->addStrategy(std::make_shared<TimeRewardStrategy>(time_config), 1.0f);

    // Robot moves closer to goal
    curr_world_.entities[0].x = 1.0f;

    // Add navigation reward
    composite->addStrategy(std::make_shared<NavigationRewardStrategy>(), 1.0f);

    auto result = composite->calculate(makeContext());

    // Should be positive progress minus time penalty
    // Progress = 1.0 (moved 1 unit closer), time = -0.1
    EXPECT_NEAR(result.reward, 0.9f, 0.01f);
}

TEST_F(RewardStrategyTest, CompositeWeightedRewards) {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    TimeConfig time_config;
    time_config.time_penalty = -1.0f;
    composite->addStrategy(std::make_shared<TimeRewardStrategy>(time_config), 0.5f);

    auto result = composite->calculate(makeContext());

    // Weight 0.5 * -1.0 = -0.5
    EXPECT_FLOAT_EQ(result.reward, -0.5f);
}

TEST_F(RewardStrategyTest, CompositeTerminationPropagates) {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    // Add navigation strategy with goal reached
    curr_world_.entities[0].x = 5.0f;
    NavigationConfig nav_config;
    nav_config.goal_threshold = 0.5f;
    nav_config.success_bonus = 100.0f;
    composite->addStrategy(std::make_shared<NavigationRewardStrategy>(nav_config), 1.0f);

    // Add time penalty
    composite->addStrategy(std::make_shared<TimeRewardStrategy>(), 1.0f);

    auto result = composite->calculate(makeContext());

    EXPECT_TRUE(result.terminated);
    EXPECT_EQ(result.termination_reason, "Goal reached");
    // 100 (success) + -0.1 (time) = 99.9
    EXPECT_NEAR(result.reward, 99.9f, 0.01f);
}

TEST_F(RewardStrategyTest, CompositeStrategyCount) {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    EXPECT_EQ(composite->strategyCount(), 0u);

    composite->addStrategy(std::make_shared<TimeRewardStrategy>(), 1.0f);
    EXPECT_EQ(composite->strategyCount(), 1u);

    composite->addStrategy(std::make_shared<NavigationRewardStrategy>(), 1.0f);
    EXPECT_EQ(composite->strategyCount(), 2u);
}

TEST_F(RewardStrategyTest, CompositeNameIsCorrect) {
    CompositeRewardStrategy composite;
    EXPECT_EQ(composite.name(), "composite");
}

// ============ Factory Function Tests ============

TEST_F(RewardStrategyTest, CreateDefaultRewardStrategy) {
    auto strategy = createDefaultRewardStrategy();

    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->name(), "composite");

    // Should have 4 strategies
    auto* composite = dynamic_cast<CompositeRewardStrategy*>(strategy.get());
    ASSERT_NE(composite, nullptr);
    EXPECT_EQ(composite->strategyCount(), 4u);
}

TEST_F(RewardStrategyTest, CreateNavigationOnlyStrategy) {
    auto strategy = createNavigationOnlyStrategy();

    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->name(), "navigation");
}

TEST_F(RewardStrategyTest, DefaultStrategyMatchesOldBehavior) {
    // Test that default strategy produces similar behavior to old calculator
    auto strategy = createDefaultRewardStrategy();

    // Robot moves closer to goal
    curr_world_.entities[0].x = 1.0f;

    auto result = strategy->calculate(makeContext());

    // Should have: progress (1.0) + time (-0.1) = 0.9
    EXPECT_NEAR(result.reward, 0.9f, 0.01f);
    EXPECT_FALSE(result.terminated);
}

// ============ Multi-Robot Tests ============

TEST_F(RewardStrategyTest, NavigationMultiRobotIndex) {
    // Add second robot
    warehouser_msgs::msg::Entity robot2;
    robot2.id = "robot_2";
    robot2.type = 0;
    robot2.x = 4.0f;  // Closer to goal
    robot2.y = 0.0f;

    curr_world_.entities.push_back(robot2);
    prev_world_.entities.push_back(robot2);

    // Move robot 2 closer
    curr_world_.entities[1].x = 4.5f;

    NavigationRewardStrategy strategy;

    // Check robot 0 (didn't move)
    auto result0 = strategy.calculate(makeContext(1, 500, 0));
    EXPECT_NEAR(result0.reward, 0.0f, 0.01f);

    // Check robot 1 (moved closer)
    auto result1 = strategy.calculate(makeContext(1, 500, 1));
    EXPECT_GT(result1.reward, 0.0f);
}

TEST_F(RewardStrategyTest, CollisionMultiRobotIndex) {
    // Add second robot
    warehouser_msgs::msg::Entity robot2;
    robot2.id = "robot_2";
    robot2.type = 0;
    robot2.x = 4.0f;
    robot2.y = 0.0f;

    curr_world_.entities.push_back(robot2);

    CollisionRewardStrategy strategy;

    // Robot 0 exists - no collision
    auto result0 = strategy.calculate(makeContext(1, 500, 0));
    EXPECT_FALSE(result0.terminated);

    // Robot 1 exists - no collision
    auto result1 = strategy.calculate(makeContext(1, 500, 1));
    EXPECT_FALSE(result1.terminated);

    // Robot 2 doesn't exist - collision
    auto result2 = strategy.calculate(makeContext(1, 500, 2));
    EXPECT_TRUE(result2.terminated);
}

// ============ Interface Tests ============

TEST_F(RewardStrategyTest, PolymorphicUsage) {
    std::vector<std::unique_ptr<IRewardStrategy>> strategies;
    strategies.push_back(std::make_unique<NavigationRewardStrategy>());
    strategies.push_back(std::make_unique<TimeRewardStrategy>());
    strategies.push_back(std::make_unique<CollisionRewardStrategy>());

    // All should work through interface
    for (const auto& strategy : strategies) {
        auto result = strategy->calculate(makeContext());
        // Just verify it doesn't crash
        EXPECT_FALSE(result.truncated);
    }
}
