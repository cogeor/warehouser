#pragma once

#include <cmath>
#include <vector>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/observation.hpp"
#include "warehouser_msgs/msg/world_state.hpp"

namespace warehouser {

/// Observation version enum
enum class ObservationVersion : int32_t {
    /// Position-based observation (8 dims):
    /// [robot_x, robot_y, robot_theta, goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
    V1_Position = 1,

    /// Lidar-based observation (63 dims):
    /// [lidar_ranges(60), goal_bearing, goal_dist, is_carrying]
    V2_Lidar = 2
};

/// Configuration for observation building
struct ObservationConfig {
    ObservationVersion version = ObservationVersion::V1_Position;
    float world_size = 10.0f;  // For normalization
};

/// Builds observation vectors from world state and goal.
/// Supports multiple observation versions for different training approaches.
class ObservationBuilder {
public:
    explicit ObservationBuilder(const ObservationConfig& config = {});

    /// Build observation from world state and goal
    /// @param world Current world state message
    /// @param goal Current goal message
    /// @return Observation message with data vector
    warehouser_msgs::msg::Observation build(
        const warehouser_msgs::msg::WorldState& world,
        const warehouser_msgs::msg::Goal& goal) const;

    /// Get the observation version
    ObservationVersion version() const { return config_.version; }

    /// Get expected observation dimension for current version
    size_t observationDim() const;

private:
    ObservationConfig config_;

    /// Build V1 position-based observation
    warehouser_msgs::msg::Observation buildV1(
        const warehouser_msgs::msg::WorldState& world,
        const warehouser_msgs::msg::Goal& goal) const;

    /// Find robot entity in world state
    /// @return nullptr if not found
    const warehouser_msgs::msg::Entity* findRobot(
        const warehouser_msgs::msg::WorldState& world) const;

    /// Normalize angle to [-π, π]
    static float normalizeAngle(float angle) {
        return std::atan2(std::sin(angle), std::cos(angle));
    }
};

}  // namespace warehouser
