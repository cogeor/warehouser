#include <gtest/gtest.h>

#include "warehouser_simulation/robot.hpp"
#include "warehouser_simulation/world_manager.hpp"

using namespace warehouser;

// ============ Robot Collision Detection Tests ============

class RobotCollisionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a world with two robots
        WorldConfig config;
        config.width = 20.0f;
        config.height = 20.0f;

        RobotSpawnConfig spawn1;
        spawn1.id = "robot_1";
        spawn1.x = 5.0f;
        spawn1.y = 5.0f;
        spawn1.theta = 0.0f;

        RobotSpawnConfig spawn2;
        spawn2.id = "robot_2";
        spawn2.x = 10.0f;  // Far from robot_1
        spawn2.y = 10.0f;
        spawn2.theta = 0.0f;

        config.robot_spawns = {spawn1, spawn2};

        world_ = std::make_unique<WorldManager>(config);
    }

    std::unique_ptr<WorldManager> world_;
};

TEST_F(RobotCollisionTest, NoCollisionWhenRobotsAreFarApart) {
    // Robots are at (5,5) and (10,10), far apart
    EXPECT_FALSE(world_->checkRobotCollision(0));
    EXPECT_FALSE(world_->checkRobotCollision(1));
}

TEST_F(RobotCollisionTest, CollisionWhenRobotsOverlap) {
    // Move robot 2 to overlap with robot 1
    // Collision distance is 2 * Robot::kRadius = 2 * 0.3 = 0.6
    auto* robot1 = world_->robot(0);
    auto* robot2 = world_->robot(1);

    robot2->x = robot1->x + 0.5f;  // Within collision distance
    robot2->y = robot1->y;

    EXPECT_TRUE(world_->checkRobotCollision(0));
    EXPECT_TRUE(world_->checkRobotCollision(1));
}

TEST_F(RobotCollisionTest, CollisionAtBoundary) {
    // Place robots exactly at collision boundary
    const float collision_distance = 2.0f * Robot::kRadius;
    auto* robot1 = world_->robot(0);
    auto* robot2 = world_->robot(1);

    robot2->x = robot1->x + collision_distance - 0.01f;  // Just inside
    robot2->y = robot1->y;

    EXPECT_TRUE(world_->checkRobotCollision(0));
    EXPECT_TRUE(world_->checkRobotCollision(1));
}

TEST_F(RobotCollisionTest, NoCollisionJustOutsideBoundary) {
    // Place robots just outside collision boundary
    const float collision_distance = 2.0f * Robot::kRadius;
    auto* robot1 = world_->robot(0);
    auto* robot2 = world_->robot(1);

    robot2->x = robot1->x + collision_distance + 0.01f;  // Just outside
    robot2->y = robot1->y;

    EXPECT_FALSE(world_->checkRobotCollision(0));
    EXPECT_FALSE(world_->checkRobotCollision(1));
}

TEST_F(RobotCollisionTest, InvalidRobotIndexReturnsFalse) {
    // Out of bounds index should return false
    EXPECT_FALSE(world_->checkRobotCollision(100));
}

// ============ Collision Rollback Tests ============

TEST_F(RobotCollisionTest, CollisionRollbackPreservesPositions) {
    world_->start();

    auto* robot1 = world_->robot(0);
    auto* robot2 = world_->robot(1);

    // Position robot 2 close to robot 1's path
    robot1->x = 5.0f;
    robot1->y = 5.0f;
    robot2->x = 6.0f;  // Close but not colliding
    robot2->y = 5.0f;

    // Command robot 1 to move toward robot 2
    float prev_x = robot1->x;
    float prev_y = robot1->y;
    robot1->setCommand(2.0f, 0.0f);  // Move forward fast

    world_->step(1.0f);  // Step would cause overlap

    // Robot 1 should be rolled back (not overlapping robot 2)
    // Either at original position or stopped just before collision
    float dist = std::sqrt(
        (robot1->x - robot2->x) * (robot1->x - robot2->x) +
        (robot1->y - robot2->y) * (robot1->y - robot2->y)
    );

    // Robot should not overlap (collision was detected and rolled back)
    EXPECT_GE(dist, 2.0f * Robot::kRadius - 0.01f);
}

TEST_F(RobotCollisionTest, BothRobotsCanCollideSimultaneously) {
    world_->start();

    auto* robot1 = world_->robot(0);
    auto* robot2 = world_->robot(1);

    // Position robots to collide when both move
    robot1->x = 5.0f;
    robot1->y = 5.0f;
    robot2->x = 6.2f;  // Just outside collision range
    robot2->y = 5.0f;

    // Both robots move toward each other
    robot1->setCommand(1.0f, 0.0f);   // Forward
    robot2->setCommand(-1.0f, 0.0f);  // Backward

    world_->step(0.5f);

    // After step, positions should be rolled back if collision occurred
    float dist = std::sqrt(
        (robot1->x - robot2->x) * (robot1->x - robot2->x) +
        (robot1->y - robot2->y) * (robot1->y - robot2->y)
    );

    // No overlap should remain
    EXPECT_GE(dist, 2.0f * Robot::kRadius - 0.01f);
}

// ============ Collision Flag Tests ============

TEST_F(RobotCollisionTest, CollisionFlagSetCorrectly) {
    world_->start();

    auto* robot1 = world_->robot(0);
    auto* robot2 = world_->robot(1);

    // Initially no collision
    EXPECT_FALSE(robot1->in_robot_collision);
    EXPECT_FALSE(robot2->in_robot_collision);

    // Position robots to collide
    robot1->x = 5.0f;
    robot1->y = 5.0f;
    robot2->x = 5.4f;  // Within collision distance (< 0.6)
    robot2->y = 5.0f;

    // Command robot 1 to move toward robot 2
    robot1->setCommand(1.0f, 0.0f);

    world_->step(0.1f);

    // Collision flag should be set for at least one robot
    // (collision is detected and position rolled back)
    bool any_collision = robot1->in_robot_collision || robot2->in_robot_collision;
    EXPECT_TRUE(any_collision);
}

TEST_F(RobotCollisionTest, CollisionFlagClearedAfterSeparation) {
    world_->start();

    auto* robot1 = world_->robot(0);
    auto* robot2 = world_->robot(1);

    // Force collision flag on
    robot1->in_robot_collision = true;
    robot2->in_robot_collision = true;

    // Position robots far apart
    robot1->x = 2.0f;
    robot1->y = 2.0f;
    robot2->x = 15.0f;
    robot2->y = 15.0f;

    // Step without movement
    robot1->setCommand(0.0f, 0.0f);
    robot2->setCommand(0.0f, 0.0f);

    world_->step(0.1f);

    // Flags should be cleared at start of step
    EXPECT_FALSE(robot1->in_robot_collision);
    EXPECT_FALSE(robot2->in_robot_collision);
}

// ============ Multi-Robot Collision Tests ============

TEST(ThreeRobotCollisionTest, ThreeRobotsNoCollision) {
    WorldConfig config;
    config.width = 20.0f;
    config.height = 20.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_0";
    spawn1.x = 3.0f;
    spawn1.y = 3.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_1";
    spawn2.x = 10.0f;
    spawn2.y = 10.0f;
    spawn2.theta = 0.0f;

    RobotSpawnConfig spawn3;
    spawn3.id = "robot_2";
    spawn3.x = 17.0f;
    spawn3.y = 17.0f;
    spawn3.theta = 0.0f;

    config.robot_spawns = {spawn1, spawn2, spawn3};

    WorldManager world(config);

    EXPECT_EQ(world.robotCount(), 3u);
    EXPECT_FALSE(world.checkRobotCollision(0));
    EXPECT_FALSE(world.checkRobotCollision(1));
    EXPECT_FALSE(world.checkRobotCollision(2));
}

TEST(ThreeRobotCollisionTest, OneRobotCollidesWithMultiple) {
    WorldConfig config;
    config.width = 20.0f;
    config.height = 20.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_0";
    spawn1.x = 5.0f;
    spawn1.y = 5.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_1";
    spawn2.x = 5.3f;  // Close to robot 0
    spawn2.y = 5.0f;
    spawn2.theta = 0.0f;

    RobotSpawnConfig spawn3;
    spawn3.id = "robot_2";
    spawn3.x = 5.0f;
    spawn3.y = 5.3f;  // Close to robot 0
    spawn3.theta = 0.0f;

    config.robot_spawns = {spawn1, spawn2, spawn3};

    WorldManager world(config);

    // Robot 0 collides with both robot 1 and robot 2
    EXPECT_TRUE(world.checkRobotCollision(0));
    EXPECT_TRUE(world.checkRobotCollision(1));
    EXPECT_TRUE(world.checkRobotCollision(2));
}

TEST(ThreeRobotCollisionTest, TwoRobotsCollideThirdSafe) {
    WorldConfig config;
    config.width = 20.0f;
    config.height = 20.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_0";
    spawn1.x = 5.0f;
    spawn1.y = 5.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_1";
    spawn2.x = 5.3f;  // Close to robot 0
    spawn2.y = 5.0f;
    spawn2.theta = 0.0f;

    RobotSpawnConfig spawn3;
    spawn3.id = "robot_2";
    spawn3.x = 15.0f;  // Far from both
    spawn3.y = 15.0f;
    spawn3.theta = 0.0f;

    config.robot_spawns = {spawn1, spawn2, spawn3};

    WorldManager world(config);

    // Robot 0 and 1 collide, robot 2 is safe
    EXPECT_TRUE(world.checkRobotCollision(0));
    EXPECT_TRUE(world.checkRobotCollision(1));
    EXPECT_FALSE(world.checkRobotCollision(2));
}
