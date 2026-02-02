#include <gtest/gtest.h>

#include <memory>

#include "warehouser_rl_bridge/exploration_reward.hpp"
#include "warehouser_rl_bridge/occupancy_tracker.hpp"

using namespace warehouser;

// ============ OccupancyTracker Tests ============

class OccupancyTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.world_width = 10.0f;
        config_.world_height = 10.0f;
        config_.cell_size = 1.0f;  // 10x10 = 100 cells
    }

    OccupancyConfig config_;
};

TEST_F(OccupancyTrackerTest, GridDimensionsCalculatedCorrectly) {
    OccupancyTracker tracker(config_);

    EXPECT_EQ(tracker.gridWidth(), 10u);
    EXPECT_EQ(tracker.gridHeight(), 10u);
    EXPECT_EQ(tracker.totalCells(), 100u);
}

TEST_F(OccupancyTrackerTest, SmallCellSize) {
    config_.cell_size = 0.5f;  // 20x20 = 400 cells
    OccupancyTracker tracker(config_);

    EXPECT_EQ(tracker.gridWidth(), 20u);
    EXPECT_EQ(tracker.gridHeight(), 20u);
    EXPECT_EQ(tracker.totalCells(), 400u);
}

TEST_F(OccupancyTrackerTest, NewCellDetection) {
    OccupancyTracker tracker(config_);

    // First visit should return true
    EXPECT_TRUE(tracker.markVisited(5.0f, 5.0f));
    // Second visit to same cell should return false
    EXPECT_FALSE(tracker.markVisited(5.0f, 5.0f));
    // New cell should return true
    EXPECT_TRUE(tracker.markVisited(6.0f, 5.0f));
}

TEST_F(OccupancyTrackerTest, PositionsWithinSameCellAreGrouped) {
    OccupancyTracker tracker(config_);

    // All these positions fall within cell (5, 5) with 1m cells
    EXPECT_TRUE(tracker.markVisited(5.0f, 5.0f));
    EXPECT_FALSE(tracker.markVisited(5.1f, 5.1f));
    EXPECT_FALSE(tracker.markVisited(5.5f, 5.5f));
    EXPECT_FALSE(tracker.markVisited(5.9f, 5.9f));

    // This crosses into cell (6, 5)
    EXPECT_TRUE(tracker.markVisited(6.0f, 5.0f));
}

TEST_F(OccupancyTrackerTest, IsVisitedWorks) {
    OccupancyTracker tracker(config_);

    EXPECT_FALSE(tracker.isVisited(5.0f, 5.0f));
    tracker.markVisited(5.0f, 5.0f);
    EXPECT_TRUE(tracker.isVisited(5.0f, 5.0f));
    EXPECT_FALSE(tracker.isVisited(6.0f, 6.0f));
}

TEST_F(OccupancyTrackerTest, VisitCountTracked) {
    OccupancyTracker tracker(config_);

    EXPECT_EQ(tracker.visitCount(5.0f, 5.0f), 0);
    tracker.markVisited(5.0f, 5.0f);
    EXPECT_EQ(tracker.visitCount(5.0f, 5.0f), 1);
    tracker.markVisited(5.1f, 5.1f);  // Same cell
    EXPECT_EQ(tracker.visitCount(5.0f, 5.0f), 2);
    tracker.markVisited(5.2f, 5.2f);  // Same cell
    EXPECT_EQ(tracker.visitCount(5.0f, 5.0f), 3);
}

TEST_F(OccupancyTrackerTest, CoverageCalculation) {
    config_.world_width = 2.0f;
    config_.world_height = 2.0f;
    config_.cell_size = 1.0f;  // 2x2 = 4 cells
    OccupancyTracker tracker(config_);

    EXPECT_FLOAT_EQ(tracker.coverage(), 0.0f);
    EXPECT_EQ(tracker.visitedCells(), 0u);

    tracker.markVisited(0.5f, 0.5f);  // Cell [0,0]
    EXPECT_FLOAT_EQ(tracker.coverage(), 0.25f);
    EXPECT_EQ(tracker.visitedCells(), 1u);

    tracker.markVisited(1.5f, 0.5f);  // Cell [1,0]
    EXPECT_FLOAT_EQ(tracker.coverage(), 0.5f);
    EXPECT_EQ(tracker.visitedCells(), 2u);

    tracker.markVisited(0.5f, 1.5f);  // Cell [0,1]
    EXPECT_FLOAT_EQ(tracker.coverage(), 0.75f);
    EXPECT_EQ(tracker.visitedCells(), 3u);

    tracker.markVisited(1.5f, 1.5f);  // Cell [1,1]
    EXPECT_FLOAT_EQ(tracker.coverage(), 1.0f);
    EXPECT_EQ(tracker.visitedCells(), 4u);
}

TEST_F(OccupancyTrackerTest, ResetClearsCoverage) {
    OccupancyTracker tracker(config_);

    tracker.markVisited(1.0f, 1.0f);
    tracker.markVisited(2.0f, 2.0f);
    tracker.markVisited(3.0f, 3.0f);
    EXPECT_EQ(tracker.visitedCells(), 3u);

    tracker.reset();

    EXPECT_EQ(tracker.visitedCells(), 0u);
    EXPECT_FLOAT_EQ(tracker.coverage(), 0.0f);
    EXPECT_FALSE(tracker.isVisited(1.0f, 1.0f));
}

TEST_F(OccupancyTrackerTest, BoundaryConditions) {
    OccupancyTracker tracker(config_);

    // Origin
    EXPECT_TRUE(tracker.markVisited(0.0f, 0.0f));

    // Far corner (should clamp to last cell)
    EXPECT_TRUE(tracker.markVisited(9.9f, 9.9f));

    // Beyond world bounds (should clamp)
    EXPECT_TRUE(tracker.markVisited(15.0f, 15.0f));  // Clamped to (9, 9)
    EXPECT_FALSE(tracker.markVisited(10.0f, 10.0f));  // Same cell

    // Negative (should clamp to 0)
    EXPECT_TRUE(tracker.markVisited(-5.0f, -5.0f));  // Clamped to (0, 0)
    EXPECT_FALSE(tracker.markVisited(-1.0f, 0.0f));  // Same cell
}

// ============ ExplorationRewardStrategy Tests ============

class ExplorationRewardTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Small world for testing
        config_.occupancy.world_width = 4.0f;
        config_.occupancy.world_height = 4.0f;
        config_.occupancy.cell_size = 1.0f;  // 4x4 = 16 cells
        config_.new_cell_bonus = 5.0f;
        config_.revisit_bonus = 0.0f;
        config_.coverage_bonus = 10.0f;
        config_.coverage_target = 0.5f;  // 50% = 8 cells

        // Create world with robot
        world_.entities.resize(1);
        world_.entities[0].id = "robot";
        world_.entities[0].type = 0;  // TYPE_ROBOT
        world_.entities[0].x = 0.5f;
        world_.entities[0].y = 0.5f;
        world_.entities[0].theta = 0.0f;

        goal_.x = 3.5f;
        goal_.y = 3.5f;
        goal_.active = true;
    }

    RewardContext makeContext(float x, float y) {
        world_.entities[0].x = x;
        world_.entities[0].y = y;
        return RewardContext{
            .prev_world = world_,
            .curr_world = world_,
            .goal = goal_,
            .step_count = 0,
            .max_steps = 1000,
            .robot_index = 0
        };
    }

    ExplorationConfig config_;
    warehouser_msgs::msg::WorldState world_;
    warehouser_msgs::msg::Goal goal_;
};

TEST_F(ExplorationRewardTest, NewCellBonus) {
    ExplorationRewardStrategy strategy(config_);

    auto ctx = makeContext(1.5f, 1.5f);
    auto result = strategy.calculate(ctx);

    EXPECT_FLOAT_EQ(result.reward, 5.0f);
    EXPECT_FALSE(result.terminated);
}

TEST_F(ExplorationRewardTest, RevisitNoBonus) {
    ExplorationRewardStrategy strategy(config_);

    auto ctx = makeContext(1.5f, 1.5f);
    strategy.calculate(ctx);  // First visit

    auto result = strategy.calculate(ctx);  // Revisit
    EXPECT_FLOAT_EQ(result.reward, 0.0f);
    EXPECT_FALSE(result.terminated);
}

TEST_F(ExplorationRewardTest, RevisitWithBonus) {
    config_.revisit_bonus = 0.5f;
    ExplorationRewardStrategy strategy(config_);

    auto ctx = makeContext(1.5f, 1.5f);
    strategy.calculate(ctx);  // First visit

    auto result = strategy.calculate(ctx);  // Revisit
    EXPECT_FLOAT_EQ(result.reward, 0.5f);
}

TEST_F(ExplorationRewardTest, CoverageTargetTerminates) {
    ExplorationRewardStrategy strategy(config_);

    // Visit 8 of 16 cells (50%)
    for (int i = 0; i < 8; ++i) {
        float x = static_cast<float>(i % 4) + 0.5f;
        float y = static_cast<float>(i / 4) + 0.5f;
        auto result = strategy.calculate(makeContext(x, y));

        if (i < 7) {
            EXPECT_FALSE(result.terminated) << "Terminated early at cell " << i;
        } else {
            // 8th cell reaches 50% coverage
            EXPECT_TRUE(result.terminated);
            EXPECT_EQ(result.termination_reason, "Coverage target reached");
            // Should get both new cell bonus and coverage bonus
            EXPECT_FLOAT_EQ(result.reward, config_.new_cell_bonus + config_.coverage_bonus);
        }
    }
}

TEST_F(ExplorationRewardTest, CoverageTracking) {
    ExplorationRewardStrategy strategy(config_);

    EXPECT_FLOAT_EQ(strategy.coverage(), 0.0f);

    strategy.calculate(makeContext(0.5f, 0.5f));
    EXPECT_NEAR(strategy.coverage(), 1.0f / 16.0f, 0.01f);

    strategy.calculate(makeContext(1.5f, 0.5f));
    EXPECT_NEAR(strategy.coverage(), 2.0f / 16.0f, 0.01f);
}

TEST_F(ExplorationRewardTest, ResetClearsCoverage) {
    ExplorationRewardStrategy strategy(config_);

    strategy.calculate(makeContext(0.5f, 0.5f));
    strategy.calculate(makeContext(1.5f, 0.5f));
    strategy.calculate(makeContext(2.5f, 0.5f));
    EXPECT_GT(strategy.coverage(), 0.0f);

    strategy.reset();

    EXPECT_FLOAT_EQ(strategy.coverage(), 0.0f);
    // After reset, same positions should be new again
    auto result = strategy.calculate(makeContext(0.5f, 0.5f));
    EXPECT_FLOAT_EQ(result.reward, 5.0f);
}

TEST_F(ExplorationRewardTest, NoRobotReturnsZero) {
    ExplorationRewardStrategy strategy(config_);

    warehouser_msgs::msg::WorldState empty_world;
    RewardContext ctx{
        .prev_world = empty_world,
        .curr_world = empty_world,
        .goal = goal_,
        .step_count = 0,
        .max_steps = 1000,
        .robot_index = 0
    };

    auto result = strategy.calculate(ctx);
    EXPECT_FLOAT_EQ(result.reward, 0.0f);
    EXPECT_FALSE(result.terminated);
}

TEST_F(ExplorationRewardTest, MultiRobotIndexing) {
    // Add second robot
    warehouser_msgs::msg::Entity robot2;
    robot2.id = "robot2";
    robot2.type = 0;
    robot2.x = 2.5f;
    robot2.y = 2.5f;
    world_.entities.push_back(robot2);

    ExplorationRewardStrategy strategy(config_);

    // Build context for robot 1 (second robot)
    RewardContext ctx{
        .prev_world = world_,
        .curr_world = world_,
        .goal = goal_,
        .step_count = 0,
        .max_steps = 1000,
        .robot_index = 1
    };

    auto result = strategy.calculate(ctx);
    EXPECT_FLOAT_EQ(result.reward, 5.0f);

    // Robot 0 at (0.5, 0.5) should still be unvisited from robot 1's perspective
    // (same tracker, but we're visiting robot 1's position)
    EXPECT_EQ(strategy.tracker().visitCount(2.5f, 2.5f), 1);
    EXPECT_EQ(strategy.tracker().visitCount(0.5f, 0.5f), 0);  // Robot 0's position unvisited
}

TEST_F(ExplorationRewardTest, NameReturnsExploration) {
    ExplorationRewardStrategy strategy;
    EXPECT_EQ(strategy.name(), "exploration");
}

TEST_F(ExplorationRewardTest, TrackerAccessible) {
    ExplorationRewardStrategy strategy(config_);

    strategy.calculate(makeContext(1.5f, 1.5f));

    const auto& tracker = strategy.tracker();
    EXPECT_EQ(tracker.visitedCells(), 1u);
    EXPECT_TRUE(tracker.isVisited(1.5f, 1.5f));
}

// ============ Factory Function Tests ============

TEST(ExplorationFactoryTest, CreateExplorationOnlyStrategy) {
    auto strategy = createExplorationOnlyStrategy();
    ASSERT_NE(strategy, nullptr);

    // Should be a composite
    auto* composite = dynamic_cast<CompositeRewardStrategy*>(strategy.get());
    ASSERT_NE(composite, nullptr);
    EXPECT_EQ(composite->strategyCount(), 3u);  // Exploration + Collision + Time
}

TEST(ExplorationFactoryTest, CreateMultiTaskRewardStrategy) {
    auto strategy = createMultiTaskRewardStrategy();
    ASSERT_NE(strategy, nullptr);

    auto* composite = dynamic_cast<CompositeRewardStrategy*>(strategy.get());
    ASSERT_NE(composite, nullptr);
    EXPECT_EQ(composite->strategyCount(), 4u);  // Navigation + Exploration + Collision + Time
}
