#include "warehouser_observations/observation_builder.hpp"

#include <cmath>

#include "warehouser_observations/lidar_simulator.hpp"

namespace warehouser {

ObservationBuilder::ObservationBuilder(const ObservationConfig& config)
    : config_(config) {}

ObservationBuilder::ObservationBuilder(const ObservationConfig& config,
                                       const LidarSimulator* lidar)
    : config_(config), lidar_(lidar) {}

warehouser_msgs::msg::Observation ObservationBuilder::build(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal,
    size_t robot_index) const {
    switch (config_.version) {
        case ObservationVersion::V1_Position:
            return buildV1(world, goal, robot_index);
        case ObservationVersion::V2_Lidar:
            return buildV2(world, goal, robot_index);
        case ObservationVersion::V3_MultiRobot:
            return buildV3(world, goal, robot_index);
        default:
            return buildV1(world, goal, robot_index);
    }
}

size_t ObservationBuilder::observationDim() const {
    switch (config_.version) {
        case ObservationVersion::V1_Position:
            return 5;
        case ObservationVersion::V2_Lidar:
            return 63;  // 60 lidar + 3 (bearing, dist, carrying)
        case ObservationVersion::V3_MultiRobot:
            // 5 (ego state) + 3 * max_other_robots (rel_x, rel_y, rel_theta)
            return 5 + 3 * config_.max_other_robots;
        default:
            return 5;
    }
}

warehouser_msgs::msg::Observation ObservationBuilder::buildV1(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal,
    size_t robot_index) const {
    warehouser_msgs::msg::Observation obs;
    obs.version = static_cast<int32_t>(ObservationVersion::V1_Position);
    obs.data.resize(5, 0.0f);

    // Find robot in world state by index
    const auto* robot = findRobotByIndex(world, robot_index);
    if (!robot) {
        return obs;  // Return zeros if no robot found
    }

    // Goal relative to robot (ego-centric)
    float dx = goal.x - robot->x;
    float dy = goal.y - robot->y;
    obs.data[0] = dx;
    obs.data[1] = dy;

    // Distance to goal
    float dist = std::sqrt(dx * dx + dy * dy);
    obs.data[2] = dist;

    // Goal heading in robot frame
    // World angle to goal
    float world_angle = std::atan2(dy, dx);
    // Relative to robot heading
    float heading = normalizeAngle(world_angle - robot->theta);
    obs.data[3] = heading;

    // Carrying flag
    obs.data[4] = robot->is_carrying ? 1.0f : 0.0f;

    return obs;
}

warehouser_msgs::msg::Observation ObservationBuilder::buildV2(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal,
    size_t robot_index) const {
    warehouser_msgs::msg::Observation obs;
    obs.version = static_cast<int32_t>(ObservationVersion::V2_Lidar);
    obs.data.resize(63, 0.0f);  // 60 lidar + 3 (bearing, dist, carrying)

    // Find robot in world state by index
    const auto* robot = findRobotByIndex(world, robot_index);
    if (!robot) {
        return obs;  // Return zeros if no robot found
    }

    // Get lidar scan (first 60 dims)
    if (lidar_) {
        auto ranges = lidar_->scan(robot->x, robot->y, robot->theta, world);
        for (size_t i = 0; i < ranges.size() && i < 60; ++i) {
            obs.data[i] = ranges[i];
        }
    }

    // Goal bearing in robot frame (dim 60)
    float dx = goal.x - robot->x;
    float dy = goal.y - robot->y;
    float world_angle = std::atan2(dy, dx);
    float bearing = normalizeAngle(world_angle - robot->theta);
    obs.data[60] = bearing;

    // Goal distance (dim 61)
    float dist = std::sqrt(dx * dx + dy * dy);
    obs.data[61] = dist;

    // Carrying flag (dim 62)
    obs.data[62] = robot->is_carrying ? 1.0f : 0.0f;

    return obs;
}

warehouser_msgs::msg::Observation ObservationBuilder::buildV3(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal,
    size_t robot_index) const {
    warehouser_msgs::msg::Observation obs;
    obs.version = static_cast<int32_t>(ObservationVersion::V3_MultiRobot);
    obs.data.resize(observationDim(), 0.0f);

    // Find ego robot
    const auto* ego = findRobotByIndex(world, robot_index);
    if (!ego) {
        return obs;  // Return zeros if no robot found
    }

    // First 5 dims: ego state (same as V1, ego-centric)
    float dx = goal.x - ego->x;
    float dy = goal.y - ego->y;
    obs.data[0] = dx;
    obs.data[1] = dy;

    float dist = std::sqrt(dx * dx + dy * dy);
    obs.data[2] = dist;

    float world_angle = std::atan2(dy, dx);
    float heading = normalizeAngle(world_angle - ego->theta);
    obs.data[3] = heading;

    obs.data[4] = ego->is_carrying ? 1.0f : 0.0f;

    // Remaining dims: relative positions of other robots
    auto others = findOtherRobots(world, robot_index);
    float cos_ego = std::cos(-ego->theta);
    float sin_ego = std::sin(-ego->theta);

    for (size_t i = 0; i < config_.max_other_robots; ++i) {
        size_t base = 5 + i * 3;
        if (i < others.size()) {
            const auto* other = others[i];
            // World-frame delta
            float world_dx = other->x - ego->x;
            float world_dy = other->y - ego->y;
            // Transform to ego's frame
            obs.data[base + 0] = cos_ego * world_dx - sin_ego * world_dy;
            obs.data[base + 1] = sin_ego * world_dx + cos_ego * world_dy;
            // Relative heading
            obs.data[base + 2] = normalizeAngle(other->theta - ego->theta);
        }
        // else: already zero-initialized (no robot at this slot)
    }

    return obs;
}

const warehouser_msgs::msg::Entity* ObservationBuilder::findRobotByIndex(
    const warehouser_msgs::msg::WorldState& world,
    size_t index) const {
    size_t robot_count = 0;
    for (const auto& entity : world.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT = 0
            if (robot_count == index) {
                return &entity;
            }
            ++robot_count;
        }
    }
    return nullptr;
}

std::vector<const warehouser_msgs::msg::Entity*> ObservationBuilder::findOtherRobots(
    const warehouser_msgs::msg::WorldState& world,
    size_t exclude_index) const {
    std::vector<const warehouser_msgs::msg::Entity*> others;
    size_t robot_count = 0;
    for (const auto& entity : world.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT = 0
            if (robot_count != exclude_index) {
                others.push_back(&entity);
            }
            ++robot_count;
        }
    }
    return others;
}

}  // namespace warehouser
