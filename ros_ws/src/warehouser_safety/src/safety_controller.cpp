#include "warehouser_safety/safety_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace warehouser_safety {

SafetyController::SafetyController(const SafetyConfig& config)
    : config_(config) {}

float SafetyController::getMinDistance(
    const LidarData& lidar, float angle_min, float angle_max) const {

    if (lidar.ranges.empty()) {
        return std::numeric_limits<float>::max();
    }

    float min_dist = std::numeric_limits<float>::max();
    const size_t num_rays = lidar.ranges.size();
    const float angle_increment = (lidar.angle_max - lidar.angle_min) /
                                   static_cast<float>(num_rays - 1);

    for (size_t i = 0; i < num_rays; ++i) {
        float angle = lidar.angle_min + static_cast<float>(i) * angle_increment;

        // Skip if outside angle range
        if (angle < angle_min || angle > angle_max) {
            continue;
        }

        float range = lidar.ranges[i];

        // Skip invalid readings
        if (range < lidar.range_min || range > lidar.range_max) {
            continue;
        }

        if (std::isfinite(range) && range < min_dist) {
            min_dist = range;
        }
    }

    return min_dist;
}

float SafetyController::computeScale(float distance) const {
    if (distance <= config_.min_distance) {
        return 0.0f;
    }
    if (distance >= config_.slowdown_distance) {
        return 1.0f;
    }

    // Linear interpolation
    return (distance - config_.min_distance) /
           (config_.slowdown_distance - config_.min_distance);
}

Velocity SafetyController::applySafetyLimits(
    const Velocity& cmd_raw, const LidarData& lidar) {

    Velocity cmd_safe = cmd_raw;

    // Get minimum distance in forward cone (±60 degrees)
    constexpr float kForwardCone = 1.05f;  // ~60 degrees
    float front_dist = getMinDistance(lidar, -kForwardCone, kForwardCone);
    last_min_distance_ = front_dist;

    // Emergency stop
    if (front_dist < config_.min_distance) {
        state_ = SafetyState::EMERGENCY;
        cmd_safe.linear = 0.0f;
        cmd_safe.angular = 0.0f;
        return cmd_safe;
    }

    // Slowdown zone
    if (front_dist < config_.slowdown_distance) {
        state_ = SafetyState::SLOWDOWN;
        float scale = computeScale(front_dist);

        // Only slow down forward motion
        if (cmd_safe.linear > 0.0f) {
            cmd_safe.linear *= scale;
        }
    } else {
        state_ = SafetyState::NOMINAL;
    }

    // Directional safety: check if moving towards obstacle
    if (cmd_raw.linear > 0.0f) {
        // Moving forward - check front
        if (front_dist < config_.min_distance) {
            cmd_safe.linear = 0.0f;
        }
    } else if (cmd_raw.linear < 0.0f) {
        // Moving backward - check rear (if we have rear sensors)
        // For now, allow backward motion
    }

    // Clamp to velocity limits
    cmd_safe.linear = std::clamp(
        cmd_safe.linear, -config_.max_linear_vel, config_.max_linear_vel);
    cmd_safe.angular = std::clamp(
        cmd_safe.angular, -config_.max_angular_vel, config_.max_angular_vel);

    return cmd_safe;
}

}  // namespace warehouser_safety
