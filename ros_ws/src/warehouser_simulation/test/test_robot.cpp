#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

#include "warehouser_simulation/pickable_object.hpp"
#include "warehouser_simulation/robot.hpp"

using namespace warehouser;

class RobotTest : public ::testing::Test {
protected:
    Robot robot_{"robot", 0.0f, 0.0f, 0.0f};
};

TEST_F(RobotTest, UpdateMovesRobotForward) {
    robot_.setCommand(1.0f, 0.0f);  // Forward, no rotation
    robot_.update(1.0f);            // 1 second

    EXPECT_NEAR(robot_.x, 1.0f, 0.01f);
    EXPECT_NEAR(robot_.y, 0.0f, 0.01f);
    EXPECT_NEAR(robot_.theta, 0.0f, 0.01f);
}

TEST_F(RobotTest, UpdateMovesRobotBackward) {
    robot_.setCommand(-0.5f, 0.0f);
    robot_.update(2.0f);

    EXPECT_NEAR(robot_.x, -1.0f, 0.01f);
    EXPECT_NEAR(robot_.y, 0.0f, 0.01f);
}

TEST_F(RobotTest, UpdateRotatesRobot) {
    // Use max angular velocity (2.0 rad/s) for 1 second = 2.0 radians rotation
    robot_.setCommand(0.0f, Robot::kOmegaMax);
    robot_.update(1.0f);

    EXPECT_NEAR(robot_.x, 0.0f, 0.01f);
    EXPECT_NEAR(robot_.y, 0.0f, 0.01f);
    EXPECT_NEAR(robot_.theta, Robot::kOmegaMax, 0.01f);  // Should be 2.0 rad
}

TEST_F(RobotTest, UpdateRotatesAndMoves) {
    float pi = std::numbers::pi_v<float>;
    robot_.theta = pi / 2;  // Facing +Y
    robot_.setCommand(1.0f, 0.0f);
    robot_.update(1.0f);

    EXPECT_NEAR(robot_.x, 0.0f, 0.01f);
    EXPECT_NEAR(robot_.y, 1.0f, 0.01f);
}

TEST_F(RobotTest, SetCommandClampsVelocity) {
    robot_.setCommand(10.0f, 10.0f);  // Way over limits

    EXPECT_LE(robot_.v, Robot::kVMax);
    EXPECT_LE(robot_.omega, Robot::kOmegaMax);
}

TEST_F(RobotTest, SetCommandClampsNegativeVelocity) {
    robot_.setCommand(-10.0f, -10.0f);

    EXPECT_GE(robot_.v, -Robot::kVMax);
    EXPECT_GE(robot_.omega, -Robot::kOmegaMax);
}

TEST_F(RobotTest, StopSetsVelocitiesToZero) {
    robot_.setCommand(1.0f, 1.0f);
    robot_.stop();

    EXPECT_FLOAT_EQ(robot_.v, 0.0f);
    EXPECT_FLOAT_EQ(robot_.omega, 0.0f);
}

TEST_F(RobotTest, TryPickSucceedsWhenInRange) {
    PickableObject obj("obj", 0.3f, 0.0f, "red");
    obj.pickup_radius = 0.5f;

    EXPECT_TRUE(robot_.tryPick(obj));
    EXPECT_TRUE(robot_.is_carrying);
    EXPECT_EQ(robot_.carried_object_id, "obj");
    EXPECT_TRUE(obj.is_picked);
}

TEST_F(RobotTest, TryPickFailsWhenOutOfRange) {
    PickableObject obj("obj", 10.0f, 0.0f, "red");
    obj.pickup_radius = 0.5f;

    EXPECT_FALSE(robot_.tryPick(obj));
    EXPECT_FALSE(robot_.is_carrying);
    EXPECT_FALSE(obj.is_picked);
}

TEST_F(RobotTest, TryPickFailsWhenAlreadyCarrying) {
    PickableObject obj1("obj1", 0.3f, 0.0f, "red");
    PickableObject obj2("obj2", 0.0f, 0.3f, "blue");
    obj1.pickup_radius = 0.5f;
    obj2.pickup_radius = 0.5f;

    EXPECT_TRUE(robot_.tryPick(obj1));
    EXPECT_FALSE(robot_.tryPick(obj2));
}

TEST_F(RobotTest, TryPickFailsWhenObjectAlreadyPicked) {
    PickableObject obj("obj", 0.3f, 0.0f, "red");
    obj.pickup_radius = 0.5f;
    obj.is_picked = true;

    EXPECT_FALSE(robot_.tryPick(obj));
}

TEST_F(RobotTest, UnpickDropsObjectAtRobotPosition) {
    robot_.x = 5.0f;
    robot_.y = 5.0f;

    PickableObject obj("obj", 0.0f, 0.0f, "red");
    obj.pickup_radius = 10.0f;  // Large range for test

    robot_.tryPick(obj);
    robot_.x = 7.0f;
    robot_.y = 7.0f;
    robot_.unpick(obj);

    EXPECT_FALSE(robot_.is_carrying);
    EXPECT_TRUE(robot_.carried_object_id.empty());
    EXPECT_FALSE(obj.is_picked);
    EXPECT_NEAR(obj.x, 7.0f, 0.01f);
    EXPECT_NEAR(obj.y, 7.0f, 0.01f);
}

TEST_F(RobotTest, UnpickDoesNothingWhenNotCarrying) {
    PickableObject obj("obj", 3.0f, 3.0f, "red");

    robot_.unpick(obj);  // Should be no-op

    EXPECT_NEAR(obj.x, 3.0f, 0.01f);
    EXPECT_NEAR(obj.y, 3.0f, 0.01f);
}

TEST_F(RobotTest, GetTypeReturnsRobot) {
    EXPECT_EQ(robot_.getType(), EntityType::Robot);
}

TEST_F(RobotTest, ToMsgIncludesRobotFields) {
    robot_.x = 1.0f;
    robot_.y = 2.0f;
    robot_.theta = 0.5f;
    robot_.v = 0.3f;
    robot_.omega = 0.1f;
    robot_.is_carrying = true;
    robot_.carried_object_id = "obj1";

    auto msg = robot_.toMsg();

    EXPECT_EQ(msg.id, "robot");
    EXPECT_EQ(msg.type, static_cast<uint8_t>(EntityType::Robot));
    EXPECT_FLOAT_EQ(msg.x, 1.0f);
    EXPECT_FLOAT_EQ(msg.y, 2.0f);
    EXPECT_FLOAT_EQ(msg.theta, 0.5f);
    EXPECT_FLOAT_EQ(msg.v, 0.3f);
    EXPECT_FLOAT_EQ(msg.omega, 0.1f);
    EXPECT_TRUE(msg.is_carrying);
    EXPECT_EQ(msg.carried_object_id, "obj1");
}
