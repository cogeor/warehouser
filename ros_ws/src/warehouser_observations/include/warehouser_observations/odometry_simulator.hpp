#pragma once

#include "warehouser_observations/sensor_interface.hpp"

namespace warehouser {

/// Configuration for odometry simulation
struct OdometryConfig {
    float linear_noise_stddev = 0.01f;   // meters per meter traveled
    float angular_noise_stddev = 0.02f;  // radians per radian turned
    bool add_noise = false;              // Enable for domain randomization
};

/// Odometry simulator - tracks robot motion deltas.
/// Implements ISensor interface for polymorphic sensor handling.
/// Produces OdometryReading with dx, dy, dtheta from robot motion.
class OdometrySimulator : public ISensor {
public:
    explicit OdometrySimulator(const OdometryConfig& config = {});

    /// Get sensor type
    SensorType type() const override { return SensorType::Odometry; }

    /// Compute odometry from robot pose change.
    /// Note: Uses internal state to track previous pose.
    /// First call initializes state and returns zero deltas.
    /// @param pose Current robot pose (x, y, theta)
    /// @param world World state (unused, for interface compatibility)
    /// @return OdometryReading wrapped in SensorReading variant
    SensorReading scan(const SensorPose& pose,
                       const warehouser_msgs::msg::WorldState& world) const override;

    /// Compute odometry with explicit time delta.
    /// Preferred method when dt is known (e.g., from simulation step).
    /// @param current_pose Current robot pose
    /// @param dt Time delta since last call
    /// @return OdometryReading with motion deltas
    OdometryReading computeOdometry(const SensorPose& current_pose, float dt);

    /// Reset internal state (call on episode reset).
    /// Next scan/computeOdometry will initialize with current pose.
    void reset();

    /// Check if odometry has been initialized with first pose
    bool isInitialized() const { return initialized_; }

    /// Get the last recorded pose
    const SensorPose& lastPose() const { return last_pose_; }

    /// Get configuration
    const OdometryConfig& config() const { return config_; }

private:
    OdometryConfig config_;
    mutable SensorPose last_pose_;
    mutable bool initialized_ = false;
    mutable float last_dt_ = 0.02f;  // Default 50Hz

    /// Add Gaussian noise to a value
    float addNoise(float value, float stddev) const;
};

}  // namespace warehouser
