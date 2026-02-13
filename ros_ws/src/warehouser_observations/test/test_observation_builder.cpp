#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numbers>

#include "warehouser_observations/lidar_simulator.hpp"
#include "warehouser_observations/observation_builder.hpp"

using namespace warehouser;

class ObservationBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create world with robot
        world_.entities.resize(1);
        world_.entities[0].id = "robot";
        world_.entities[0].type = 0;  // TYPE_ROBOT
        world_.entities[0].x = 0.0f;
        world_.entities[0].y = 0.0f;
        world_.entities[0].theta = 0.0f;
        world_.entities[0].is_carrying = false;

        // Create goal
        goal_.x = 5.0f;
        goal_.y = 5.0f;
        goal_.active = true;
    }

    warehouser_msgs::msg::WorldState world_;
    warehouser_msgs::msg::Goal goal_;
};

TEST_F(ObservationBuilderTest, V1ObservationHas5Dimensions) {
    ObservationBuilder builder;

    auto obs = builder.build(world_, goal_);

    EXPECT_EQ(obs.version, 1);
    EXPECT_EQ(obs.data.size(), 5u);
}

TEST_F(ObservationBuilderTest, ObservationDimReturnsCorrectSize) {
    ObservationConfig config;
    config.version = ObservationVersion::V1_Position;
    ObservationBuilder builder(config);

    EXPECT_EQ(builder.observationDim(), 5u);
}

TEST_F(ObservationBuilderTest, GoalDeltaInEgoCentricObservation) {
    world_.entities[0].x = 3.0f;
    world_.entities[0].y = 4.0f;
    world_.entities[0].theta = 0.5f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    // goal at (5, 5), robot at (3, 4) -> dx=2, dy=1
    EXPECT_FLOAT_EQ(obs.data[0], 2.0f);  // goal_dx
    EXPECT_FLOAT_EQ(obs.data[1], 1.0f);  // goal_dy
}

TEST_F(ObservationBuilderTest, GoalDeltaCalculation) {
    world_.entities[0].x = 1.0f;
    world_.entities[0].y = 2.0f;
    goal_.x = 4.0f;
    goal_.y = 6.0f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    // dx = 4 - 1 = 3, dy = 6 - 2 = 4
    EXPECT_FLOAT_EQ(obs.data[0], 3.0f);
    EXPECT_FLOAT_EQ(obs.data[1], 4.0f);
}

TEST_F(ObservationBuilderTest, GoalDistanceCalculation) {
    world_.entities[0].x = 0.0f;
    world_.entities[0].y = 0.0f;
    goal_.x = 3.0f;
    goal_.y = 4.0f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    // Distance = sqrt(3^2 + 4^2) = 5
    EXPECT_NEAR(obs.data[2], 5.0f, 0.001f);
}

TEST_F(ObservationBuilderTest, GoalHeadingWhenFacingGoal) {
    // Robot at origin facing +X, goal directly ahead
    world_.entities[0].x = 0.0f;
    world_.entities[0].y = 0.0f;
    world_.entities[0].theta = 0.0f;
    goal_.x = 5.0f;
    goal_.y = 0.0f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    // Goal heading should be ~0 (directly ahead)
    EXPECT_NEAR(obs.data[3], 0.0f, 0.001f);
}

TEST_F(ObservationBuilderTest, GoalHeadingWhenGoalToLeft) {
    // Robot at origin facing +X, goal to the left (+Y)
    world_.entities[0].x = 0.0f;
    world_.entities[0].y = 0.0f;
    world_.entities[0].theta = 0.0f;
    goal_.x = 0.0f;
    goal_.y = 5.0f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    float pi = std::numbers::pi_v<float>;
    // Goal heading should be ~pi/2 (90 degrees left)
    EXPECT_NEAR(obs.data[3], pi / 2.0f, 0.01f);
}

TEST_F(ObservationBuilderTest, GoalHeadingWhenGoalBehind) {
    // Robot at origin facing +X, goal behind (-X)
    world_.entities[0].x = 0.0f;
    world_.entities[0].y = 0.0f;
    world_.entities[0].theta = 0.0f;
    goal_.x = -5.0f;
    goal_.y = 0.0f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    float pi = std::numbers::pi_v<float>;
    // Goal heading should be ~pi or ~-pi (behind)
    EXPECT_NEAR(std::abs(obs.data[3]), pi, 0.01f);
}

TEST_F(ObservationBuilderTest, CarryingFlagFalse) {
    world_.entities[0].is_carrying = false;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_FLOAT_EQ(obs.data[4], 0.0f);
}

TEST_F(ObservationBuilderTest, CarryingFlagTrue) {
    world_.entities[0].is_carrying = true;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_FLOAT_EQ(obs.data[4], 1.0f);
}

TEST_F(ObservationBuilderTest, NoRobotReturnsZeros) {
    world_.entities.clear();  // No entities

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_EQ(obs.data.size(), 5u);
    for (float val : obs.data) {
        EXPECT_FLOAT_EQ(val, 0.0f);
    }
}

TEST_F(ObservationBuilderTest, VersionReturnsConfiguredVersion) {
    ObservationConfig config;
    config.version = ObservationVersion::V1_Position;
    ObservationBuilder builder(config);

    EXPECT_EQ(builder.version(), ObservationVersion::V1_Position);
}

// ============ Robot Index Tests ============

TEST_F(ObservationBuilderTest, BuildWithRobotIndexZeroDefault) {
    // Build with default index (0) should work as before
    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_EQ(obs.data.size(), 5u);
    // Robot at origin (0,0), goal at (5,5) -> dx=5, dy=5
    EXPECT_FLOAT_EQ(obs.data[0], 5.0f);  // goal_dx
}

TEST_F(ObservationBuilderTest, BuildWithExplicitRobotIndex) {
    // Add second robot
    warehouser_msgs::msg::Entity robot2;
    robot2.id = "robot2";
    robot2.type = 0;
    robot2.x = 5.0f;
    robot2.y = 3.0f;
    robot2.theta = 1.0f;
    world_.entities.push_back(robot2);

    ObservationBuilder builder;

    // Build observation for robot 1 (second robot)
    // Robot at (5, 3), goal at (5, 5) -> dx=0, dy=2
    auto obs = builder.build(world_, goal_, 1);

    EXPECT_FLOAT_EQ(obs.data[0], 0.0f);  // goal_dx
    EXPECT_FLOAT_EQ(obs.data[1], 2.0f);  // goal_dy
}

TEST_F(ObservationBuilderTest, BuildWithInvalidIndexReturnsZeros) {
    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_, 99);  // No robot at index 99

    EXPECT_EQ(obs.data.size(), 5u);
    for (float val : obs.data) {
        EXPECT_FLOAT_EQ(val, 0.0f);
    }
}

// ============ V2 Lidar Tests ============

class V2ObservationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure lidar simulator
        lidar_config_.num_rays = 60;
        lidar_config_.fov = std::numbers::pi_v<float>;  // 180 degrees
        lidar_config_.max_range = 10.0f;
        lidar_config_.min_range = 0.1f;
        lidar_ = std::make_unique<LidarSimulator>(lidar_config_);

        // Configure V2 observation builder
        obs_config_.version = ObservationVersion::V2_Lidar;
        builder_ = std::make_unique<ObservationBuilder>(obs_config_, lidar_.get());

        // Create world with robot at origin facing +X
        warehouser_msgs::msg::Entity robot;
        robot.id = "robot";
        robot.type = 0;  // TYPE_ROBOT
        robot.x = 0.0f;
        robot.y = 0.0f;
        robot.theta = 0.0f;
        robot.is_carrying = false;
        world_.entities.push_back(robot);

        // Add world bounds (required for lidar simulation)
        world_.width = 20.0f;
        world_.height = 20.0f;

        // Create goal
        goal_.x = 3.0f;
        goal_.y = 4.0f;
        goal_.active = true;
    }

    LidarConfig lidar_config_;
    std::unique_ptr<LidarSimulator> lidar_;
    ObservationConfig obs_config_;
    std::unique_ptr<ObservationBuilder> builder_;
    warehouser_msgs::msg::WorldState world_;
    warehouser_msgs::msg::Goal goal_;
};

TEST_F(V2ObservationTest, V2ObservationHas63Dimensions) {
    auto obs = builder_->build(world_, goal_);

    EXPECT_EQ(obs.version, 2);
    EXPECT_EQ(obs.data.size(), 63u);
}

TEST_F(V2ObservationTest, V2LidarRangesInValidRange) {
    auto obs = builder_->build(world_, goal_);

    // First 60 values are lidar ranges
    for (size_t i = 0; i < 60; ++i) {
        EXPECT_GE(obs.data[i], 0.0f) << "Range at index " << i << " is negative";
        EXPECT_LE(obs.data[i], lidar_config_.max_range)
            << "Range at index " << i << " exceeds max_range";
    }
}

TEST_F(V2ObservationTest, V2GoalBearingEgoCentric) {
    // Robot at origin facing +X, goal at (3, 4)
    // World angle to goal: atan2(4, 3) ~ 0.927 rad
    // Robot theta: 0
    // Ego-centric bearing: 0.927 - 0 = 0.927

    auto obs = builder_->build(world_, goal_);

    float expected_bearing = std::atan2(4.0f, 3.0f);
    EXPECT_NEAR(obs.data[60], expected_bearing, 0.001f);

    // Now rotate robot to face +Y (theta = pi/2)
    // Ego-centric bearing should be: 0.927 - pi/2 ~ -0.644
    world_.entities[0].theta = std::numbers::pi_v<float> / 2.0f;
    auto obs_rotated = builder_->build(world_, goal_);

    float world_angle = std::atan2(4.0f, 3.0f);
    float expected_bearing_rotated = world_angle - (std::numbers::pi_v<float> / 2.0f);
    EXPECT_NEAR(obs_rotated.data[60], expected_bearing_rotated, 0.001f);
}

TEST_F(V2ObservationTest, V2GoalDistanceCorrect) {
    // Robot at origin, goal at (3, 4)
    // Distance = sqrt(3^2 + 4^2) = 5

    auto obs = builder_->build(world_, goal_);

    EXPECT_NEAR(obs.data[61], 5.0f, 0.001f);

    // Move robot to (1, 1), goal still at (3, 4)
    // Distance = sqrt(2^2 + 3^2) = sqrt(13) ~ 3.606
    world_.entities[0].x = 1.0f;
    world_.entities[0].y = 1.0f;
    auto obs2 = builder_->build(world_, goal_);

    EXPECT_NEAR(obs2.data[61], std::sqrt(13.0f), 0.001f);
}

TEST_F(V2ObservationTest, V2CarryingFlag) {
    // is_carrying = false
    world_.entities[0].is_carrying = false;
    auto obs_not_carrying = builder_->build(world_, goal_);
    EXPECT_FLOAT_EQ(obs_not_carrying.data[62], 0.0f);

    // is_carrying = true
    world_.entities[0].is_carrying = true;
    auto obs_carrying = builder_->build(world_, goal_);
    EXPECT_FLOAT_EQ(obs_carrying.data[62], 1.0f);
}

TEST_F(V2ObservationTest, V2ObservationDimReturnsCorrectSize) {
    EXPECT_EQ(builder_->observationDim(), 63u);
}

TEST_F(V2ObservationTest, V2NoRobotFoundReturnsZeros) {
    warehouser_msgs::msg::WorldState empty_world;
    empty_world.width = 20.0f;
    empty_world.height = 20.0f;

    auto obs = builder_->build(empty_world, goal_);

    EXPECT_EQ(obs.data.size(), 63u);
    for (float val : obs.data) {
        EXPECT_FLOAT_EQ(val, 0.0f);
    }
}

// ============ V3 Multi-Robot Tests ============

class V3ObservationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create config for V3
        config_.version = ObservationVersion::V3_MultiRobot;
        config_.max_other_robots = 3;

        // Create world with multiple robots
        // Robot 0: at origin, facing +X
        warehouser_msgs::msg::Entity robot0;
        robot0.id = "robot0";
        robot0.type = 0;
        robot0.x = 0.0f;
        robot0.y = 0.0f;
        robot0.theta = 0.0f;
        robot0.is_carrying = false;
        world_.entities.push_back(robot0);

        // Robot 1: at (3, 4), facing +Y
        warehouser_msgs::msg::Entity robot1;
        robot1.id = "robot1";
        robot1.type = 0;
        robot1.x = 3.0f;
        robot1.y = 4.0f;
        robot1.theta = std::numbers::pi_v<float> / 2.0f;
        robot1.is_carrying = true;
        world_.entities.push_back(robot1);

        // Goal
        goal_.x = 5.0f;
        goal_.y = 5.0f;
        goal_.active = true;
    }

    ObservationConfig config_;
    warehouser_msgs::msg::WorldState world_;
    warehouser_msgs::msg::Goal goal_;
};

TEST_F(V3ObservationTest, ObservationDimWithMaxOtherRobots) {
    ObservationBuilder builder(config_);

    // 5 (ego) + 3 * 3 (max_other_robots) = 14
    EXPECT_EQ(builder.observationDim(), 14u);
}

TEST_F(V3ObservationTest, ObservationDimWithDifferentMaxRobots) {
    config_.max_other_robots = 5;
    ObservationBuilder builder(config_);

    // 5 + 3 * 5 = 20
    EXPECT_EQ(builder.observationDim(), 20u);
}

TEST_F(V3ObservationTest, V3ObservationHasCorrectVersion) {
    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_);

    EXPECT_EQ(obs.version, 3);
}

TEST_F(V3ObservationTest, V3ObservationHasCorrectSize) {
    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_);

    EXPECT_EQ(obs.data.size(), 14u);
}

TEST_F(V3ObservationTest, V3EgoStateFirst5DimsMatchV1) {
    ObservationBuilder builder_v3(config_);

    ObservationConfig v1_config;
    v1_config.version = ObservationVersion::V1_Position;
    ObservationBuilder builder_v1(v1_config);

    auto obs_v3 = builder_v3.build(world_, goal_, 0);
    auto obs_v1 = builder_v1.build(world_, goal_, 0);

    // First 5 dimensions should match exactly
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_FLOAT_EQ(obs_v3.data[i], obs_v1.data[i])
            << "Mismatch at index " << i;
    }
}

TEST_F(V3ObservationTest, V3OtherRobotRelativePositionNoRotation) {
    // Robot 0 at origin with theta=0, robot 1 at (3, 4)
    // In robot 0's frame, robot 1 should be at (3, 4)
    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_, 0);

    // Other robot 0 (which is robot index 1) relative position
    EXPECT_NEAR(obs.data[5], 3.0f, 0.001f);   // rel_x
    EXPECT_NEAR(obs.data[6], 4.0f, 0.001f);   // rel_y
    // Relative theta: pi/2 - 0 = pi/2
    EXPECT_NEAR(obs.data[7], std::numbers::pi_v<float> / 2.0f, 0.001f);
}

TEST_F(V3ObservationTest, V3OtherRobotRelativePositionWithRotation) {
    // Rotate robot 0 to face +Y (theta = pi/2)
    world_.entities[0].theta = std::numbers::pi_v<float> / 2.0f;
    // Robot 1 at (3, 4) should now appear at different relative coords
    // World delta: (3, 4)
    // In robot 0's frame (rotated 90 deg CCW):
    // rel_x = 3*cos(-pi/2) - 4*sin(-pi/2) = 0 + 4 = 4
    // rel_y = 3*sin(-pi/2) + 4*cos(-pi/2) = -3 + 0 = -3

    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_, 0);

    EXPECT_NEAR(obs.data[5], 4.0f, 0.001f);   // rel_x
    EXPECT_NEAR(obs.data[6], -3.0f, 0.001f);  // rel_y
}

TEST_F(V3ObservationTest, V3PaddingWhenFewerRobots) {
    // Only 2 robots, max_other_robots = 3
    // Robot 0's observation should have 1 real other robot, 2 zeros
    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_, 0);

    // First other robot slot filled
    EXPECT_NE(obs.data[5], 0.0f);  // Has position data

    // Second and third slots should be zero-padded
    EXPECT_FLOAT_EQ(obs.data[8], 0.0f);
    EXPECT_FLOAT_EQ(obs.data[9], 0.0f);
    EXPECT_FLOAT_EQ(obs.data[10], 0.0f);
    EXPECT_FLOAT_EQ(obs.data[11], 0.0f);
    EXPECT_FLOAT_EQ(obs.data[12], 0.0f);
    EXPECT_FLOAT_EQ(obs.data[13], 0.0f);
}

TEST_F(V3ObservationTest, V3FromDifferentRobotPerspective) {
    // Build observation from robot 1's perspective
    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_, 1);

    // Ego state should be robot 1's goal-relative position (ego-centric)
    // Robot 1 at (3, 4), goal at (5, 5) -> dx=2, dy=1
    EXPECT_FLOAT_EQ(obs.data[0], 2.0f);  // goal_dx
    EXPECT_FLOAT_EQ(obs.data[1], 1.0f);  // goal_dy
    EXPECT_FLOAT_EQ(obs.data[4], 1.0f);  // is_carrying = true

    // Other robot (robot 0) should be in the observation
    // World delta from robot 1: (0-3, 0-4) = (-3, -4)
    // Robot 1 faces +Y (theta = pi/2), so transform:
    // rel_x = -3*cos(-pi/2) - (-4)*sin(-pi/2) = 0 - 4 = -4
    // rel_y = -3*sin(-pi/2) + (-4)*cos(-pi/2) = 3 + 0 = 3
    EXPECT_NEAR(obs.data[5], -4.0f, 0.001f);
    EXPECT_NEAR(obs.data[6], 3.0f, 0.001f);
}

TEST_F(V3ObservationTest, V3WithThreeRobots) {
    // Add a third robot
    warehouser_msgs::msg::Entity robot2;
    robot2.id = "robot2";
    robot2.type = 0;
    robot2.x = -2.0f;
    robot2.y = 1.0f;
    robot2.theta = std::numbers::pi_v<float>;
    robot2.is_carrying = false;
    world_.entities.push_back(robot2);

    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_, 0);

    // Should have 2 other robots filled, 1 zero-padded
    // First other robot at (3, 4)
    EXPECT_NEAR(obs.data[5], 3.0f, 0.001f);
    EXPECT_NEAR(obs.data[6], 4.0f, 0.001f);

    // Second other robot at (-2, 1)
    EXPECT_NEAR(obs.data[8], -2.0f, 0.001f);
    EXPECT_NEAR(obs.data[9], 1.0f, 0.001f);
    EXPECT_NEAR(obs.data[10], std::numbers::pi_v<float>, 0.001f);

    // Third slot zero-padded
    EXPECT_FLOAT_EQ(obs.data[11], 0.0f);
    EXPECT_FLOAT_EQ(obs.data[12], 0.0f);
    EXPECT_FLOAT_EQ(obs.data[13], 0.0f);
}

TEST_F(V3ObservationTest, V3NoRobotFoundReturnsZeros) {
    warehouser_msgs::msg::WorldState empty_world;
    ObservationBuilder builder(config_);
    auto obs = builder.build(empty_world, goal_, 0);

    EXPECT_EQ(obs.data.size(), 14u);
    for (float val : obs.data) {
        EXPECT_FLOAT_EQ(val, 0.0f);
    }
}

TEST_F(V3ObservationTest, V3SingleRobotNoOthers) {
    // Remove second robot
    world_.entities.pop_back();

    ObservationBuilder builder(config_);
    auto obs = builder.build(world_, goal_, 0);

    // Ego state should be filled (goal-relative)
    // Robot at origin, goal at (5, 5) -> dx=5, dy=5
    EXPECT_FLOAT_EQ(obs.data[0], 5.0f);  // goal_dx
    EXPECT_FLOAT_EQ(obs.data[1], 5.0f);  // goal_dy

    // All other robot slots should be zero
    for (size_t i = 5; i < 14; ++i) {
        EXPECT_FLOAT_EQ(obs.data[i], 0.0f) << "Non-zero at index " << i;
    }
}
