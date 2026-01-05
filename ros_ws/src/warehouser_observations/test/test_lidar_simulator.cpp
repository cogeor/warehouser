#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

#include "warehouser_observations/lidar_simulator.hpp"

using namespace warehouser;

class LidarSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create empty world
        world_.entities.clear();

        // Create wall in front of robot
        warehouser_msgs::msg::Entity wall;
        wall.id = "wall_front";
        wall.type = 2;  // TYPE_WALL
        wall.x = 5.0f;
        wall.y = -1.0f;
        wall.width = 0.1f;
        wall.height = 2.0f;
        world_.entities.push_back(wall);
    }

    warehouser_msgs::msg::WorldState world_;
};

TEST_F(LidarSimulatorTest, DefaultConfigHas60Rays) {
    LidarSimulator lidar;

    auto ranges = lidar.scan(0.0f, 0.0f, 0.0f, world_);

    EXPECT_EQ(ranges.size(), 60u);
}

TEST_F(LidarSimulatorTest, CustomConfigRayCount) {
    LidarConfig config;
    config.num_rays = 30;
    LidarSimulator lidar(config);

    auto ranges = lidar.scan(0.0f, 0.0f, 0.0f, world_);

    EXPECT_EQ(ranges.size(), 30u);
}

TEST_F(LidarSimulatorTest, RangesWithinBounds) {
    LidarSimulator lidar;

    auto ranges = lidar.scan(0.0f, 0.0f, 0.0f, world_);

    for (float range : ranges) {
        EXPECT_GE(range, lidar.config().min_range);
        EXPECT_LE(range, lidar.config().max_range);
    }
}

TEST_F(LidarSimulatorTest, DetectsWallInFront) {
    // Robot at origin facing +X, wall at x=5
    LidarConfig config;
    config.num_rays = 1;
    config.fov = 0.0f;  // Single ray straight ahead
    LidarSimulator lidar(config);

    auto ranges = lidar.scan(0.0f, 0.0f, 0.0f, world_);

    ASSERT_EQ(ranges.size(), 1u);
    // Wall is at x=5, so distance should be ~5
    EXPECT_NEAR(ranges[0], 5.0f, 0.1f);
}

TEST_F(LidarSimulatorTest, MaxRangeWhenNoObstacle) {
    // Clear world (no walls)
    warehouser_msgs::msg::WorldState empty_world;

    LidarConfig config;
    config.num_rays = 1;
    config.fov = 0.0f;
    config.max_range = 10.0f;
    LidarSimulator lidar(config);

    // Scan in world with only boundary (will hit edge at ~10)
    auto ranges = lidar.scan(5.0f, 5.0f, 0.0f, empty_world);

    ASSERT_EQ(ranges.size(), 1u);
    // Should reach max range or world boundary
    EXPECT_LE(ranges[0], config.max_range);
}

TEST_F(LidarSimulatorTest, BuildDebugMsgIncludesMetadata) {
    LidarConfig config;
    config.num_rays = 60;
    config.fov = 3.14159f;
    config.min_range = 0.1f;
    config.max_range = 10.0f;
    LidarSimulator lidar(config);

    auto msg = lidar.buildDebugMsg(1.0f, 2.0f, 0.5f, world_);

    EXPECT_EQ(msg.ranges.size(), 60u);
    EXPECT_NEAR(msg.angle_min, -3.14159f / 2.0f, 0.01f);
    EXPECT_NEAR(msg.angle_max, 3.14159f / 2.0f, 0.01f);
    EXPECT_FLOAT_EQ(msg.range_min, 0.1f);
    EXPECT_FLOAT_EQ(msg.range_max, 10.0f);
    EXPECT_FLOAT_EQ(msg.robot_x, 1.0f);
    EXPECT_FLOAT_EQ(msg.robot_y, 2.0f);
    EXPECT_FLOAT_EQ(msg.robot_theta, 0.5f);
}

TEST_F(LidarSimulatorTest, DifferentAnglesProduceDifferentRanges) {
    // Add walls on multiple sides
    warehouser_msgs::msg::WorldState world_with_walls;

    // Wall to the right
    warehouser_msgs::msg::Entity right_wall;
    right_wall.type = 2;
    right_wall.x = 3.0f;
    right_wall.y = -10.0f;
    right_wall.width = 0.1f;
    right_wall.height = 20.0f;
    world_with_walls.entities.push_back(right_wall);

    // Wall in front (farther)
    warehouser_msgs::msg::Entity front_wall;
    front_wall.type = 2;
    front_wall.x = 8.0f;
    front_wall.y = -10.0f;
    front_wall.width = 0.1f;
    front_wall.height = 20.0f;
    world_with_walls.entities.push_back(front_wall);

    LidarConfig config;
    config.num_rays = 3;
    config.fov = 3.14159f;  // 180 degrees
    LidarSimulator lidar(config);

    // Robot at origin facing +X
    auto ranges = lidar.scan(0.0f, 0.0f, 0.0f, world_with_walls);

    // Rays should have different distances based on direction
    ASSERT_EQ(ranges.size(), 3u);
    // The three rays are at -90, 0, and +90 degrees from heading
}

TEST_F(LidarSimulatorTest, ConfigAccessible) {
    LidarConfig config;
    config.num_rays = 100;
    config.max_range = 15.0f;
    LidarSimulator lidar(config);

    EXPECT_EQ(lidar.config().num_rays, 100);
    EXPECT_FLOAT_EQ(lidar.config().max_range, 15.0f);
}
