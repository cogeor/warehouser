#include "warehouser_observations/odometry_simulator.hpp"

#include <cmath>
#include <random>

namespace warehouser {

namespace {

// Thread-local random generator for noise
thread_local std::mt19937 rng{std::random_device{}()};

}  // namespace

OdometrySimulator::OdometrySimulator(const OdometryConfig& config)
    : config_(config) {}

SensorReading OdometrySimulator::scan(
    const SensorPose& pose,
    const warehouser_msgs::msg::WorldState& /*world*/) const {
    // Use const_cast since scan() is const but we need to update internal state
    // This is a design compromise for the ISensor interface
    auto* self = const_cast<OdometrySimulator*>(this);
    return self->computeOdometry(pose, last_dt_);
}

OdometryReading OdometrySimulator::computeOdometry(
    const SensorPose& current_pose, float dt) {
    OdometryReading reading;
    reading.dt = dt;
    last_dt_ = dt;

    if (!initialized_) {
        // First call - initialize with current pose
        last_pose_ = current_pose;
        initialized_ = true;
        return reading;  // Zero deltas
    }

    // Calculate deltas
    float dx = current_pose.x - last_pose_.x;
    float dy = current_pose.y - last_pose_.y;
    float dtheta = current_pose.theta - last_pose_.theta;

    // Normalize dtheta to [-pi, pi]
    while (dtheta > 3.14159265f) dtheta -= 2.0f * 3.14159265f;
    while (dtheta < -3.14159265f) dtheta += 2.0f * 3.14159265f;

    // Apply noise if enabled
    if (config_.add_noise) {
        float linear_dist = std::sqrt(dx * dx + dy * dy);
        float linear_noise = addNoise(0.0f, config_.linear_noise_stddev * linear_dist);
        float angular_noise = addNoise(0.0f, config_.angular_noise_stddev * std::abs(dtheta));

        // Add noise proportional to movement
        if (linear_dist > 1e-6f) {
            dx += linear_noise * (dx / linear_dist);
            dy += linear_noise * (dy / linear_dist);
        }
        dtheta += angular_noise;
    }

    reading.dx = dx;
    reading.dy = dy;
    reading.dtheta = dtheta;

    // Update covariance based on noise config
    if (config_.add_noise) {
        float linear_var = config_.linear_noise_stddev * config_.linear_noise_stddev;
        float angular_var = config_.angular_noise_stddev * config_.angular_noise_stddev;
        reading.covariance = {linear_var, linear_var, 0.0f, 0.0f, 0.0f, angular_var};
    }

    // Update last pose
    last_pose_ = current_pose;

    return reading;
}

void OdometrySimulator::reset() {
    initialized_ = false;
    last_pose_ = SensorPose{};
}

float OdometrySimulator::addNoise(float value, float stddev) const {
    if (stddev <= 0.0f) {
        return value;
    }
    std::normal_distribution<float> dist(value, stddev);
    return dist(rng);
}

}  // namespace warehouser
