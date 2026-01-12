#include <gtest/gtest.h>
#include "warehouser_safety/safety_controller.hpp"

using namespace warehouser_safety;

class SafetyControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.min_distance = 0.3f;
        config_.slowdown_distance = 0.8f;
        config_.max_linear_vel = 1.0f;
        config_.max_angular_vel = 2.0f;
        controller_ = std::make_unique<SafetyController>(config_);
    }

    LidarData createLidar(float front_dist) {
        LidarData lidar;
        lidar.angle_min = -1.57f;
        lidar.angle_max = 1.57f;
        lidar.range_min = 0.1f;
        lidar.range_max = 10.0f;

        // Create 60 rays, all at front_dist
        lidar.ranges.resize(60, front_dist);
        return lidar;
    }

    SafetyConfig config_;
    std::unique_ptr<SafetyController> controller_;
};

TEST_F(SafetyControllerTest, InitialStateIsNominal) {
    EXPECT_EQ(controller_->getState(), SafetyState::NOMINAL);
}

TEST_F(SafetyControllerTest, NominalWhenFarFromObstacles) {
    Velocity cmd{1.0f, 0.0f};
    auto lidar = createLidar(5.0f);  // Far from obstacles

    auto result = controller_->applySafetyLimits(cmd, lidar);

    EXPECT_EQ(controller_->getState(), SafetyState::NOMINAL);
    EXPECT_FLOAT_EQ(result.linear, 1.0f);
}

TEST_F(SafetyControllerTest, SlowdownWhenApproaching) {
    Velocity cmd{1.0f, 0.0f};
    auto lidar = createLidar(0.5f);  // In slowdown zone

    auto result = controller_->applySafetyLimits(cmd, lidar);

    EXPECT_EQ(controller_->getState(), SafetyState::SLOWDOWN);
    EXPECT_LT(result.linear, 1.0f);
    EXPECT_GT(result.linear, 0.0f);
}

TEST_F(SafetyControllerTest, EmergencyStopWhenTooClose) {
    Velocity cmd{1.0f, 0.5f};
    auto lidar = createLidar(0.2f);  // Below min distance

    auto result = controller_->applySafetyLimits(cmd, lidar);

    EXPECT_EQ(controller_->getState(), SafetyState::EMERGENCY);
    EXPECT_FLOAT_EQ(result.linear, 0.0f);
    EXPECT_FLOAT_EQ(result.angular, 0.0f);
}

TEST_F(SafetyControllerTest, ClampsToMaxVelocity) {
    Velocity cmd{5.0f, 10.0f};  // Way over limits
    auto lidar = createLidar(5.0f);

    auto result = controller_->applySafetyLimits(cmd, lidar);

    EXPECT_LE(result.linear, config_.max_linear_vel);
    EXPECT_LE(result.angular, config_.max_angular_vel);
}

TEST_F(SafetyControllerTest, AllowsBackwardMotion) {
    Velocity cmd{-0.5f, 0.0f};  // Moving backward
    auto lidar = createLidar(0.2f);  // Obstacle in front

    auto result = controller_->applySafetyLimits(cmd, lidar);

    // Should still allow backward motion even with front obstacle
    // (in MVP, we don't have rear sensors)
    EXPECT_LT(result.linear, 0.0f);
}

TEST_F(SafetyControllerTest, HandlesEmptyLidar) {
    Velocity cmd{1.0f, 0.0f};
    LidarData lidar;  // Empty

    auto result = controller_->applySafetyLimits(cmd, lidar);

    // Should pass through (no obstacles detected)
    EXPECT_EQ(controller_->getState(), SafetyState::NOMINAL);
}

TEST_F(SafetyControllerTest, ReportsMinDistance) {
    auto lidar = createLidar(2.5f);
    Velocity cmd{0.5f, 0.0f};

    controller_->applySafetyLimits(cmd, lidar);

    EXPECT_NEAR(controller_->getMinObstacleDistance(), 2.5f, 0.1f);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
