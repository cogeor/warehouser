#include "warehouser_observations/observation_builder.hpp"

#include <cmath>

namespace warehouser {

ObservationBuilder::ObservationBuilder(const ObservationConfig& config)
    : config_(config) {}

warehouser_msgs::msg::Observation ObservationBuilder::build(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal) const {
    switch (config_.version) {
        case ObservationVersion::V1_Position:
            return buildV1(world, goal);
        case ObservationVersion::V2_Lidar:
            // V2 would be implemented similarly with lidar data
            // For now, fall back to V1
            return buildV1(world, goal);
        default:
            return buildV1(world, goal);
    }
}

size_t ObservationBuilder::observationDim() const {
    switch (config_.version) {
        case ObservationVersion::V1_Position:
            return 8;
        case ObservationVersion::V2_Lidar:
            return 63;  // 60 lidar + 3 (bearing, dist, carrying)
        default:
            return 8;
    }
}

warehouser_msgs::msg::Observation ObservationBuilder::buildV1(
    const warehouser_msgs::msg::WorldState& world,
    const warehouser_msgs::msg::Goal& goal) const {
    warehouser_msgs::msg::Observation obs;
    obs.version = static_cast<int32_t>(ObservationVersion::V1_Position);
    obs.data.resize(8, 0.0f);

    // Find robot in world state
    const auto* robot = findRobot(world);
    if (!robot) {
        return obs;  // Return zeros if no robot found
    }

    // Robot position and heading
    obs.data[0] = robot->x;
    obs.data[1] = robot->y;
    obs.data[2] = robot->theta;

    // Goal relative to robot
    float dx = goal.x - robot->x;
    float dy = goal.y - robot->y;
    obs.data[3] = dx;
    obs.data[4] = dy;

    // Distance to goal
    float dist = std::sqrt(dx * dx + dy * dy);
    obs.data[5] = dist;

    // Goal heading in robot frame
    // World angle to goal
    float world_angle = std::atan2(dy, dx);
    // Relative to robot heading
    float heading = normalizeAngle(world_angle - robot->theta);
    obs.data[6] = heading;

    // Carrying flag
    obs.data[7] = robot->is_carrying ? 1.0f : 0.0f;

    return obs;
}

const warehouser_msgs::msg::Entity* ObservationBuilder::findRobot(
    const warehouser_msgs::msg::WorldState& world) const {
    for (const auto& entity : world.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT = 0
            return &entity;
        }
    }
    return nullptr;
}

}  // namespace warehouser
