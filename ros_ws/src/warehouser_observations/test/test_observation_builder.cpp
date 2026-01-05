#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

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

TEST_F(ObservationBuilderTest, V1ObservationHas8Dimensions) {
    ObservationBuilder builder;

    auto obs = builder.build(world_, goal_);

    EXPECT_EQ(obs.version, 1);
    EXPECT_EQ(obs.data.size(), 8u);
}

TEST_F(ObservationBuilderTest, ObservationDimReturnsCorrectSize) {
    ObservationConfig config;
    config.version = ObservationVersion::V1_Position;
    ObservationBuilder builder(config);

    EXPECT_EQ(builder.observationDim(), 8u);
}

TEST_F(ObservationBuilderTest, RobotPositionInObservation) {
    world_.entities[0].x = 3.0f;
    world_.entities[0].y = 4.0f;
    world_.entities[0].theta = 0.5f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_FLOAT_EQ(obs.data[0], 3.0f);
    EXPECT_FLOAT_EQ(obs.data[1], 4.0f);
    EXPECT_FLOAT_EQ(obs.data[2], 0.5f);
}

TEST_F(ObservationBuilderTest, GoalDeltaCalculation) {
    world_.entities[0].x = 1.0f;
    world_.entities[0].y = 2.0f;
    goal_.x = 4.0f;
    goal_.y = 6.0f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    // dx = 4 - 1 = 3, dy = 6 - 2 = 4
    EXPECT_FLOAT_EQ(obs.data[3], 3.0f);
    EXPECT_FLOAT_EQ(obs.data[4], 4.0f);
}

TEST_F(ObservationBuilderTest, GoalDistanceCalculation) {
    world_.entities[0].x = 0.0f;
    world_.entities[0].y = 0.0f;
    goal_.x = 3.0f;
    goal_.y = 4.0f;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    // Distance = sqrt(3^2 + 4^2) = 5
    EXPECT_NEAR(obs.data[5], 5.0f, 0.001f);
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
    EXPECT_NEAR(obs.data[6], 0.0f, 0.001f);
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
    // Goal heading should be ~π/2 (90 degrees left)
    EXPECT_NEAR(obs.data[6], pi / 2.0f, 0.01f);
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
    // Goal heading should be ~π or ~-π (behind)
    EXPECT_NEAR(std::abs(obs.data[6]), pi, 0.01f);
}

TEST_F(ObservationBuilderTest, CarryingFlagFalse) {
    world_.entities[0].is_carrying = false;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_FLOAT_EQ(obs.data[7], 0.0f);
}

TEST_F(ObservationBuilderTest, CarryingFlagTrue) {
    world_.entities[0].is_carrying = true;

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_FLOAT_EQ(obs.data[7], 1.0f);
}

TEST_F(ObservationBuilderTest, NoRobotReturnsZeros) {
    world_.entities.clear();  // No entities

    ObservationBuilder builder;
    auto obs = builder.build(world_, goal_);

    EXPECT_EQ(obs.data.size(), 8u);
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
