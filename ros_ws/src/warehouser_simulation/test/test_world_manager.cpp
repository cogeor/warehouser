#include <gtest/gtest.h>

#include "warehouser_simulation/world_manager.hpp"

using namespace warehouser;

class WorldManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        WorldConfig config;
        config.width = 20.0f;
        config.height = 20.0f;
        config.robot_spawn = {2.0f, 2.0f, 0.0f};

        world_ = std::make_unique<WorldManager>(config);
        world_->loadConfig("");  // Load default world
    }

    std::unique_ptr<WorldManager> world_;
};

TEST_F(WorldManagerTest, HasRobotAfterInit) {
    EXPECT_NE(world_->robot(), nullptr);
    EXPECT_EQ(world_->robot()->id, "robot");
}

TEST_F(WorldManagerTest, RobotSpawnPosition) {
    auto* robot = world_->robot();
    EXPECT_NEAR(robot->x, 2.0f, 0.01f);
    EXPECT_NEAR(robot->y, 2.0f, 0.01f);
    EXPECT_NEAR(robot->theta, 0.0f, 0.01f);
}

TEST_F(WorldManagerTest, HasDefaultObjects) {
    EXPECT_FALSE(world_->objects().empty());
}

TEST_F(WorldManagerTest, HasDefaultWalls) {
    EXPECT_FALSE(world_->walls().empty());
}

TEST_F(WorldManagerTest, StartAndPause) {
    EXPECT_FALSE(world_->isRunning());

    world_->start();
    EXPECT_TRUE(world_->isRunning());

    world_->pause();
    EXPECT_FALSE(world_->isRunning());
}

TEST_F(WorldManagerTest, StepUpdatesSimTime) {
    world_->start();
    float initial_time = world_->simTime();

    world_->step(0.1f);

    EXPECT_GT(world_->simTime(), initial_time);
}

TEST_F(WorldManagerTest, StepDoesNothingWhenPaused) {
    world_->pause();
    float initial_time = world_->simTime();

    world_->step(0.1f);

    EXPECT_FLOAT_EQ(world_->simTime(), initial_time);
}

TEST_F(WorldManagerTest, StepMovesRobot) {
    world_->start();
    auto* robot = world_->robot();
    robot->setCommand(1.0f, 0.0f);

    float initial_x = robot->x;
    world_->step(1.0f);

    EXPECT_GT(robot->x, initial_x);
}

TEST_F(WorldManagerTest, ResetRestoresInitialState) {
    world_->start();
    auto* robot = world_->robot();
    robot->setCommand(1.0f, 0.0f);
    world_->step(5.0f);

    world_->reset();

    EXPECT_NEAR(robot->x, 2.0f, 0.01f);
    EXPECT_NEAR(robot->y, 2.0f, 0.01f);
    EXPECT_FLOAT_EQ(world_->simTime(), 0.0f);
    EXPECT_FALSE(world_->isRunning());
}

TEST_F(WorldManagerTest, MoveEntityMovesRobot) {
    auto result = world_->moveEntity("robot", 5.0f, 5.0f);

    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(world_->robot()->x, 5.0f, 0.01f);
    EXPECT_NEAR(world_->robot()->y, 5.0f, 0.01f);
}

TEST_F(WorldManagerTest, MoveEntityMovesObject) {
    auto result = world_->moveEntity("red_1", 7.0f, 7.0f);

    EXPECT_TRUE(result.has_value());
    auto* obj = world_->findObject("red_1");
    ASSERT_NE(obj, nullptr);
    EXPECT_NEAR(obj->x, 7.0f, 0.01f);
    EXPECT_NEAR(obj->y, 7.0f, 0.01f);
}

TEST_F(WorldManagerTest, MoveEntityFailsForUnknown) {
    auto result = world_->moveEntity("unknown_entity", 5.0f, 5.0f);

    EXPECT_FALSE(result.has_value());
}

TEST_F(WorldManagerTest, FindObjectByColor) {
    auto* obj = world_->findClosestByColor("red");

    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->color, "red");
}

TEST_F(WorldManagerTest, FindObjectByColorReturnsClosest) {
    // Move robot closer to one of the objects
    world_->robot()->x = 6.0f;
    world_->robot()->y = 3.0f;

    // Find closest blue object
    auto* obj = world_->findClosestByColor("blue");

    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->color, "blue");
}

TEST_F(WorldManagerTest, FindObjectByColorReturnsNullIfNoneMatch) {
    auto* obj = world_->findClosestByColor("purple");

    EXPECT_EQ(obj, nullptr);
}

TEST_F(WorldManagerTest, FindObjectByColorSkipsPicked) {
    auto* red = world_->findClosestByColor("red");
    ASSERT_NE(red, nullptr);
    red->is_picked = true;

    auto* red2 = world_->findClosestByColor("red");

    // Should be null since the only red object is picked
    EXPECT_EQ(red2, nullptr);
}

TEST_F(WorldManagerTest, CheckCollisionWithWalls) {
    // Points inside walls should collide
    EXPECT_TRUE(world_->checkCollision(0.05f, 10.0f));  // Left wall
    EXPECT_TRUE(world_->checkCollision(19.95f, 10.0f)); // Right wall

    // Points in open space should not collide
    EXPECT_FALSE(world_->checkCollision(10.0f, 10.0f));
}

TEST_F(WorldManagerTest, IsInBounds) {
    EXPECT_TRUE(world_->isInBounds(10.0f, 10.0f));
    EXPECT_TRUE(world_->isInBounds(0.0f, 0.0f));
    EXPECT_TRUE(world_->isInBounds(20.0f, 20.0f));

    EXPECT_FALSE(world_->isInBounds(-0.1f, 10.0f));
    EXPECT_FALSE(world_->isInBounds(20.1f, 10.0f));
    EXPECT_FALSE(world_->isInBounds(10.0f, -0.1f));
    EXPECT_FALSE(world_->isInBounds(10.0f, 20.1f));
}

TEST_F(WorldManagerTest, CollisionRollback) {
    world_->start();
    auto* robot = world_->robot();

    // Move robot toward left wall
    robot->x = 0.2f;
    robot->y = 5.0f;
    robot->setCommand(-1.0f, 0.0f);  // Move toward wall

    float prev_x = robot->x;
    world_->step(0.5f);

    // Robot should not move past wall
    EXPECT_NEAR(robot->x, prev_x, 0.01f);
}

TEST_F(WorldManagerTest, CarriedObjectFollowsRobot) {
    world_->start();
    auto* robot = world_->robot();
    auto* obj = world_->findObject("red_1");
    ASSERT_NE(obj, nullptr);

    // Move robot near object and pick it up
    robot->x = obj->x;
    robot->y = obj->y;
    robot->tryPick(*obj);

    EXPECT_TRUE(robot->is_carrying);

    // Move robot
    robot->setCommand(1.0f, 0.0f);
    world_->step(1.0f);

    // Object should follow
    EXPECT_NEAR(obj->x, robot->x, 0.01f);
    EXPECT_NEAR(obj->y, robot->y, 0.01f);
}

TEST_F(WorldManagerTest, ToMsgIncludesAllEntities) {
    auto msg = world_->toMsg();

    // Should have robot + objects + walls + zones
    EXPECT_FALSE(msg.entities.empty());
    EXPECT_FLOAT_EQ(msg.sim_time, 0.0f);
    EXPECT_FALSE(msg.running);
}

TEST_F(WorldManagerTest, FindZoneByName) {
    auto* zone = world_->findZone("drop_zone");

    ASSERT_NE(zone, nullptr);
    EXPECT_EQ(zone->zone_name, "drop_zone");
}

TEST_F(WorldManagerTest, HasObstacles) {
    // Should have 4 boundary walls + 5 obstacle walls = 9 total
    EXPECT_GE(world_->walls().size(), 9u);
}

TEST_F(WorldManagerTest, ObstacleBlocksMovement) {
    world_->start();
    auto* robot = world_->robot();

    // Position robot next to obstacle_3 (at 6,3 size 2x2)
    robot->x = 5.5f;
    robot->y = 4.0f;
    robot->setCommand(1.0f, 0.0f);  // Move toward obstacle

    float prev_x = robot->x;
    world_->step(0.5f);

    // Robot should be stopped by obstacle
    EXPECT_NEAR(robot->x, prev_x, 0.01f);
}

// ============ Multi-Robot Tests ============

TEST_F(WorldManagerTest, RobotCountDefaultsToOne) {
    EXPECT_EQ(world_->robotCount(), 1u);
}

TEST_F(WorldManagerTest, RobotWithIndexZeroMatchesDefaultRobot) {
    EXPECT_EQ(world_->robot(0), world_->robot());
}

TEST_F(WorldManagerTest, RobotOutOfRangeReturnsNull) {
    EXPECT_EQ(world_->robot(100), nullptr);
}

TEST(MultiRobotTest, MultipleRobotSpawns) {
    WorldConfig config;
    config.width = 10.0f;
    config.height = 10.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_1";
    spawn1.x = 2.0f;
    spawn1.y = 2.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_2";
    spawn2.x = 8.0f;
    spawn2.y = 8.0f;
    spawn2.theta = 3.14159f;

    config.robot_spawns = {spawn1, spawn2};

    WorldManager world(config);

    EXPECT_EQ(world.robotCount(), 2u);
    ASSERT_NE(world.robot(0), nullptr);
    ASSERT_NE(world.robot(1), nullptr);
    EXPECT_EQ(world.robot(0)->id, "robot_1");
    EXPECT_EQ(world.robot(1)->id, "robot_2");
}

TEST(MultiRobotTest, MultiRobotPositions) {
    WorldConfig config;
    config.width = 10.0f;
    config.height = 10.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_1";
    spawn1.x = 2.0f;
    spawn1.y = 3.0f;
    spawn1.theta = 0.5f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_2";
    spawn2.x = 7.0f;
    spawn2.y = 6.0f;
    spawn2.theta = 1.0f;

    config.robot_spawns = {spawn1, spawn2};

    WorldManager world(config);

    EXPECT_NEAR(world.robot(0)->x, 2.0f, 0.01f);
    EXPECT_NEAR(world.robot(0)->y, 3.0f, 0.01f);
    EXPECT_NEAR(world.robot(0)->theta, 0.5f, 0.01f);

    EXPECT_NEAR(world.robot(1)->x, 7.0f, 0.01f);
    EXPECT_NEAR(world.robot(1)->y, 6.0f, 0.01f);
    EXPECT_NEAR(world.robot(1)->theta, 1.0f, 0.01f);
}

TEST(MultiRobotTest, AddRobotDynamically) {
    WorldConfig config;
    config.width = 10.0f;
    config.height = 10.0f;
    config.robot_spawn = {1.0f, 1.0f, 0.0f};

    WorldManager world(config);
    EXPECT_EQ(world.robotCount(), 1u);

    RobotSpawnConfig new_robot;
    new_robot.id = "robot_2";
    new_robot.x = 5.0f;
    new_robot.y = 5.0f;
    new_robot.theta = 0.0f;

    size_t index = world.addRobot(new_robot);

    EXPECT_EQ(index, 1u);
    EXPECT_EQ(world.robotCount(), 2u);
    EXPECT_EQ(world.robot(1)->id, "robot_2");
}

TEST(MultiRobotTest, ResetRestoresAllRobots) {
    WorldConfig config;
    config.width = 10.0f;
    config.height = 10.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_1";
    spawn1.x = 2.0f;
    spawn1.y = 2.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_2";
    spawn2.x = 8.0f;
    spawn2.y = 8.0f;
    spawn2.theta = 0.0f;

    config.robot_spawns = {spawn1, spawn2};

    WorldManager world(config);
    world.start();

    // Move both robots
    world.robot(0)->x = 5.0f;
    world.robot(1)->x = 5.0f;

    world.reset();

    // Both should be back to initial positions
    EXPECT_NEAR(world.robot(0)->x, 2.0f, 0.01f);
    EXPECT_NEAR(world.robot(1)->x, 8.0f, 0.01f);
}

TEST(MultiRobotTest, StepUpdatesAllRobots) {
    WorldConfig config;
    config.width = 20.0f;  // Larger world to avoid wall collisions
    config.height = 20.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_1";
    spawn1.x = 5.0f;
    spawn1.y = 5.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_2";
    spawn2.x = 15.0f;
    spawn2.y = 15.0f;
    spawn2.theta = 0.0f;

    config.robot_spawns = {spawn1, spawn2};

    WorldManager world(config);
    world.start();

    // Set commands for both robots
    world.robot(0)->setCommand(1.0f, 0.0f);
    world.robot(1)->setCommand(0.5f, 0.0f);

    float initial_x0 = world.robot(0)->x;
    float initial_x1 = world.robot(1)->x;

    world.step(1.0f);

    // Both robots should have moved
    EXPECT_GT(world.robot(0)->x, initial_x0);
    EXPECT_GT(world.robot(1)->x, initial_x1);
}

TEST(MultiRobotTest, ToMsgIncludesAllRobots) {
    WorldConfig config;
    config.width = 10.0f;
    config.height = 10.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_1";
    spawn1.x = 2.0f;
    spawn1.y = 2.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_2";
    spawn2.x = 8.0f;
    spawn2.y = 8.0f;
    spawn2.theta = 0.0f;

    config.robot_spawns = {spawn1, spawn2};

    WorldManager world(config);
    auto msg = world.toMsg();

    // Count robot entities
    int robot_count = 0;
    for (const auto& entity : msg.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT = 0
            robot_count++;
        }
    }

    EXPECT_EQ(robot_count, 2);
}

TEST(MultiRobotTest, MoveEntityFindsCorrectRobot) {
    WorldConfig config;
    config.width = 10.0f;
    config.height = 10.0f;

    RobotSpawnConfig spawn1;
    spawn1.id = "robot_1";
    spawn1.x = 2.0f;
    spawn1.y = 2.0f;
    spawn1.theta = 0.0f;

    RobotSpawnConfig spawn2;
    spawn2.id = "robot_2";
    spawn2.x = 8.0f;
    spawn2.y = 8.0f;
    spawn2.theta = 0.0f;

    config.robot_spawns = {spawn1, spawn2};

    WorldManager world(config);

    auto result = world.moveEntity("robot_2", 5.0f, 5.0f);

    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(world.robot(1)->x, 5.0f, 0.01f);
    EXPECT_NEAR(world.robot(1)->y, 5.0f, 0.01f);
    // robot_1 should be unchanged
    EXPECT_NEAR(world.robot(0)->x, 2.0f, 0.01f);
}

TEST_F(WorldManagerTest, BackwardCompatibility_LegacySpawn) {
    // Test that legacy robot_spawn still works when robot_spawns is empty
    WorldConfig config;
    config.width = 10.0f;
    config.height = 10.0f;
    config.robot_spawn = {3.0f, 4.0f, 1.5f};
    // robot_spawns left empty

    WorldManager world(config);

    EXPECT_EQ(world.robotCount(), 1u);
    EXPECT_EQ(world.robot()->id, "robot");
    EXPECT_NEAR(world.robot()->x, 3.0f, 0.01f);
    EXPECT_NEAR(world.robot()->y, 4.0f, 0.01f);
    EXPECT_NEAR(world.robot()->theta, 1.5f, 0.01f);
}
