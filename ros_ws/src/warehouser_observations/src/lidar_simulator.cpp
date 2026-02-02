#include "warehouser_observations/lidar_simulator.hpp"

#include <algorithm>
#include <cmath>

namespace warehouser {

LidarSimulator::LidarSimulator(const LidarConfig& config) : config_(config) {}

std::vector<float> LidarSimulator::scan(
    float robot_x, float robot_y, float robot_theta,
    const warehouser_msgs::msg::WorldState& world) const {
    std::vector<float> ranges(config_.num_rays);

    // Calculate angle step
    float angle_step =
        config_.num_rays > 1 ? config_.fov / (config_.num_rays - 1) : 0.0f;
    float start_angle = robot_theta - config_.fov / 2.0f;

    // Cast each ray
    for (int i = 0; i < config_.num_rays; ++i) {
        float angle = start_angle + i * angle_step;
        ranges[i] = raycast(robot_x, robot_y, angle, world);
    }

    return ranges;
}

warehouser_msgs::msg::LidarDebug LidarSimulator::buildDebugMsg(
    float robot_x, float robot_y, float robot_theta,
    const warehouser_msgs::msg::WorldState& world) const {
    warehouser_msgs::msg::LidarDebug msg;

    msg.ranges = scan(robot_x, robot_y, robot_theta, world);
    msg.angle_min = -config_.fov / 2.0f;
    msg.angle_max = config_.fov / 2.0f;
    msg.range_min = config_.min_range;
    msg.range_max = config_.max_range;
    msg.robot_x = robot_x;
    msg.robot_y = robot_y;
    msg.robot_theta = robot_theta;

    return msg;
}

SensorReading LidarSimulator::scan(
    const SensorPose& pose,
    const warehouser_msgs::msg::WorldState& world) const {
    // Delegate to existing scan implementation
    auto ranges = scan(pose.x, pose.y, pose.theta, world);

    // Wrap in LidarReading struct
    LidarReading reading;
    reading.ranges = std::move(ranges);
    reading.angle_min = -config_.fov / 2.0f;
    reading.angle_max = config_.fov / 2.0f;

    return reading;
}

float LidarSimulator::raycast(
    float ox, float oy, float angle,
    const warehouser_msgs::msg::WorldState& world) const {
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float dist = config_.min_range;

    // Determine world bounds from walls or use default
    float world_width = 10.0f;
    float world_height = 10.0f;

    while (dist < config_.max_range) {
        float px = ox + dist * cos_a;
        float py = oy + dist * sin_a;

        // Check wall collision
        if (checkWallCollision(px, py, world)) {
            return dist;
        }

        // Check world bounds
        if (!isInBounds(px, py, world_width, world_height)) {
            return dist;
        }

        dist += config_.step_size;
    }

    return config_.max_range;
}

bool LidarSimulator::checkWallCollision(
    float px, float py, const warehouser_msgs::msg::WorldState& world) const {
    for (const auto& entity : world.entities) {
        if (entity.type == 2) {  // TYPE_WALL = 2
            // Wall is AABB from (x, y) to (x + width, y + height)
            if (px >= entity.x && px <= entity.x + entity.width &&
                py >= entity.y && py <= entity.y + entity.height) {
                return true;
            }
        }
    }
    return false;
}

bool LidarSimulator::isInBounds(float px, float py, float world_width,
                                 float world_height) const {
    return px >= 0 && px <= world_width && py >= 0 && py <= world_height;
}

}  // namespace warehouser
