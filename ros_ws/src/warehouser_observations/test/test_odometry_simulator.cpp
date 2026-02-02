#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "warehouser_observations/odometry_simulator.hpp"
#include "warehouser_observations/sensor_interface.hpp"

using namespace warehouser;

class OdometrySimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        odom_ = std::make_unique<OdometrySimulator>();
    }

    std::unique_ptr<OdometrySimulator> odom_;
};

TEST_F(OdometrySimulatorTest, InitializesOnFirstCall) {
    EXPECT_FALSE(odom_->isInitialized());

    SensorPose pose{1.0f, 2.0f, 0.5f};
    auto reading = odom_->computeOdometry(pose, 0.02f);

    EXPECT_TRUE(odom_->isInitialized());
    // First call should return zero deltas
    EXPECT_FLOAT_EQ(reading.dx, 0.0f);
    EXPECT_FLOAT_EQ(reading.dy, 0.0f);
    EXPECT_FLOAT_EQ(reading.dtheta, 0.0f);
}

TEST_F(OdometrySimulatorTest, ZeroMotionZeroDelta) {
    SensorPose pose{0.0f, 0.0f, 0.0f};

    // First call initializes
    odom_->computeOdometry(pose, 0.02f);

    // Second call with same pose
    auto reading = odom_->computeOdometry(pose, 0.02f);

    EXPECT_NEAR(reading.dx, 0.0f, 1e-6f);
    EXPECT_NEAR(reading.dy, 0.0f, 1e-6f);
    EXPECT_NEAR(reading.dtheta, 0.0f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, ForwardMotion) {
    // Initialize at origin
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);

    // Move forward 1 meter in X
    auto reading = odom_->computeOdometry({1.0f, 0.0f, 0.0f}, 0.02f);

    EXPECT_NEAR(reading.dx, 1.0f, 1e-6f);
    EXPECT_NEAR(reading.dy, 0.0f, 1e-6f);
    EXPECT_NEAR(reading.dtheta, 0.0f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, SidewaysMotion) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);

    // Move 0.5 meters in Y
    auto reading = odom_->computeOdometry({0.0f, 0.5f, 0.0f}, 0.02f);

    EXPECT_NEAR(reading.dx, 0.0f, 1e-6f);
    EXPECT_NEAR(reading.dy, 0.5f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, Rotation) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);

    // Rotate 0.5 radians
    auto reading = odom_->computeOdometry({0.0f, 0.0f, 0.5f}, 0.02f);

    EXPECT_NEAR(reading.dx, 0.0f, 1e-6f);
    EXPECT_NEAR(reading.dy, 0.0f, 1e-6f);
    EXPECT_NEAR(reading.dtheta, 0.5f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, DiagonalMotion) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);

    // Move diagonally
    auto reading = odom_->computeOdometry({3.0f, 4.0f, 0.0f}, 0.02f);

    EXPECT_NEAR(reading.dx, 3.0f, 1e-6f);
    EXPECT_NEAR(reading.dy, 4.0f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, AccumulatesDeltas) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);

    // First move
    auto r1 = odom_->computeOdometry({1.0f, 0.0f, 0.0f}, 0.02f);
    EXPECT_NEAR(r1.dx, 1.0f, 1e-6f);

    // Second move
    auto r2 = odom_->computeOdometry({3.0f, 0.0f, 0.0f}, 0.02f);
    EXPECT_NEAR(r2.dx, 2.0f, 1e-6f);  // 3 - 1 = 2

    // Third move
    auto r3 = odom_->computeOdometry({3.0f, 1.0f, 0.0f}, 0.02f);
    EXPECT_NEAR(r3.dx, 0.0f, 1e-6f);
    EXPECT_NEAR(r3.dy, 1.0f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, Reset) {
    odom_->computeOdometry({1.0f, 2.0f, 0.5f}, 0.02f);
    EXPECT_TRUE(odom_->isInitialized());

    odom_->reset();

    EXPECT_FALSE(odom_->isInitialized());
}

TEST_F(OdometrySimulatorTest, ResetAndRecompute) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);
    odom_->computeOdometry({1.0f, 0.0f, 0.0f}, 0.02f);

    odom_->reset();

    // After reset, first call initializes again
    auto reading = odom_->computeOdometry({5.0f, 5.0f, 0.0f}, 0.02f);
    EXPECT_FLOAT_EQ(reading.dx, 0.0f);  // First call after reset = zero

    // Next call computes delta from new init
    reading = odom_->computeOdometry({6.0f, 5.0f, 0.0f}, 0.02f);
    EXPECT_NEAR(reading.dx, 1.0f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, TimeRecorded) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);
    auto reading = odom_->computeOdometry({1.0f, 0.0f, 0.0f}, 0.05f);

    EXPECT_FLOAT_EQ(reading.dt, 0.05f);
}

TEST_F(OdometrySimulatorTest, NormalizesThetaPositive) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);

    // Rotate more than 2*PI
    auto reading = odom_->computeOdometry({0.0f, 0.0f, 7.0f}, 0.02f);

    // Should be normalized to [-pi, pi]
    EXPECT_LT(reading.dtheta, 3.14159265f);
    EXPECT_GT(reading.dtheta, -3.14159265f);
}

TEST_F(OdometrySimulatorTest, NormalizesThetaNegative) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);

    // Rotate negative more than -2*PI
    auto reading = odom_->computeOdometry({0.0f, 0.0f, -7.0f}, 0.02f);

    EXPECT_LT(reading.dtheta, 3.14159265f);
    EXPECT_GT(reading.dtheta, -3.14159265f);
}

TEST_F(OdometrySimulatorTest, LastPoseUpdated) {
    SensorPose pose1{1.0f, 2.0f, 0.5f};
    odom_->computeOdometry(pose1, 0.02f);

    auto last = odom_->lastPose();
    EXPECT_FLOAT_EQ(last.x, 1.0f);
    EXPECT_FLOAT_EQ(last.y, 2.0f);
    EXPECT_FLOAT_EQ(last.theta, 0.5f);

    SensorPose pose2{3.0f, 4.0f, 1.0f};
    odom_->computeOdometry(pose2, 0.02f);

    last = odom_->lastPose();
    EXPECT_FLOAT_EQ(last.x, 3.0f);
    EXPECT_FLOAT_EQ(last.y, 4.0f);
    EXPECT_FLOAT_EQ(last.theta, 1.0f);
}

TEST_F(OdometrySimulatorTest, ConfigAccessible) {
    OdometryConfig config;
    config.add_noise = true;
    config.linear_noise_stddev = 0.05f;

    OdometrySimulator odom(config);

    EXPECT_TRUE(odom.config().add_noise);
    EXPECT_FLOAT_EQ(odom.config().linear_noise_stddev, 0.05f);
}

TEST_F(OdometrySimulatorTest, CovarianceSet) {
    odom_->computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);
    auto reading = odom_->computeOdometry({1.0f, 0.0f, 0.0f}, 0.02f);

    // Default covariance should be set
    EXPECT_GT(reading.covariance[0], 0.0f);  // x variance
    EXPECT_GT(reading.covariance[1], 0.0f);  // y variance
}

// ============ ISensor Interface Tests ============

TEST_F(OdometrySimulatorTest, ImplementsISensor) {
    std::unique_ptr<ISensor> sensor = std::make_unique<OdometrySimulator>();
    EXPECT_NE(sensor, nullptr);
}

TEST_F(OdometrySimulatorTest, TypeReturnsOdometry) {
    EXPECT_EQ(odom_->type(), SensorType::Odometry);

    std::unique_ptr<ISensor> sensor = std::make_unique<OdometrySimulator>();
    EXPECT_EQ(sensor->type(), SensorType::Odometry);
}

TEST_F(OdometrySimulatorTest, ISensorScanReturnsOdometryReading) {
    warehouser_msgs::msg::WorldState world;
    SensorPose pose{1.0f, 2.0f, 0.5f};

    auto reading = odom_->scan(pose, world);

    ASSERT_TRUE(std::holds_alternative<OdometryReading>(reading));
}

TEST_F(OdometrySimulatorTest, ISensorScanComputesDeltas) {
    warehouser_msgs::msg::WorldState world;

    // First call initializes
    odom_->scan({0.0f, 0.0f, 0.0f}, world);

    // Second call computes delta
    auto reading = odom_->scan({1.0f, 0.0f, 0.0f}, world);
    auto odom_reading = std::get<OdometryReading>(reading);

    EXPECT_NEAR(odom_reading.dx, 1.0f, 1e-6f);
}

TEST_F(OdometrySimulatorTest, PolymorphicUsage) {
    std::vector<std::unique_ptr<ISensor>> sensors;
    sensors.push_back(std::make_unique<OdometrySimulator>());

    warehouser_msgs::msg::WorldState world;
    SensorPose pose{0.0f, 0.0f, 0.0f};

    for (const auto& sensor : sensors) {
        EXPECT_EQ(sensor->type(), SensorType::Odometry);
        auto reading = sensor->scan(pose, world);
        EXPECT_TRUE(std::holds_alternative<OdometryReading>(reading));
    }
}

// ============ Noise Tests ============

TEST_F(OdometrySimulatorTest, NoiseDisabledByDefault) {
    EXPECT_FALSE(odom_->config().add_noise);
}

TEST_F(OdometrySimulatorTest, NoiseCanBeEnabled) {
    OdometryConfig config;
    config.add_noise = true;
    OdometrySimulator odom(config);

    EXPECT_TRUE(odom.config().add_noise);
}

TEST_F(OdometrySimulatorTest, NoiseAddsVariation) {
    OdometryConfig config;
    config.add_noise = true;
    config.linear_noise_stddev = 0.1f;  // 10% noise
    config.angular_noise_stddev = 0.1f;

    OdometrySimulator odom(config);

    // With noise, multiple readings of same motion should vary
    std::vector<float> deltas;
    for (int i = 0; i < 10; ++i) {
        odom.reset();
        odom.computeOdometry({0.0f, 0.0f, 0.0f}, 0.02f);
        auto reading = odom.computeOdometry({1.0f, 0.0f, 0.0f}, 0.02f);
        deltas.push_back(reading.dx);
    }

    // With noise, not all readings should be exactly the same
    bool has_variation = false;
    for (size_t i = 1; i < deltas.size(); ++i) {
        if (std::abs(deltas[i] - deltas[0]) > 1e-6f) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation);
}
