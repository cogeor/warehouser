#pragma once

#include <cmath>
#include <vector>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/observation.hpp"
#include "warehouser_msgs/msg/world_state.hpp"

namespace warehouser {

// Forward declaration
class LidarSimulator;

/// Observation version enum
enum class ObservationVersion : int32_t {
    /// Ego-centric observation (5 dims):
    /// [goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
    V1_Position = 1,

    /// Lidar-based observation (63 dims):
    /// [lidar_ranges(60), goal_bearing, goal_dist, is_carrying]
    V2_Lidar = 2,

    /// Multi-robot observation (5 + 3*max_other_robots dims):
    /// [ego_state(5), other_1_rel(3), other_2_rel(3), ...]
    /// Each other_robot_rel = [rel_x, rel_y, rel_theta]
    V3_MultiRobot = 3
};

/// Configuration for observation building
struct ObservationConfig {
    ObservationVersion version = ObservationVersion::V1_Position;
    float world_size = 10.0f;  // For normalization
    size_t max_other_robots = 3;  // Max other robots in V3 observation
};

/// Builds observation vectors from world state and goal.
/// Supports multiple observation versions for different training approaches.
class ObservationBuilder {
public:
    explicit ObservationBuilder(const ObservationConfig& config = {});

    /// Construct with lidar simulator for V2 observations
    /// @param config Observation configuration
    /// @param lidar Pointer to LidarSimulator (must outlive ObservationBuilder)
    ObservationBuilder(const ObservationConfig& config, const LidarSimulator* lidar);

    /// Build observation from world state and goal
    /// @param world Current world state message
    /// @param goal Current goal message
    /// @param robot_index Index of robot to build observation for (default 0)
    /// @return Observation message with data vector
    warehouser_msgs::msg::Observation build(
        const warehouser_msgs::msg::WorldState& world,
        const warehouser_msgs::msg::Goal& goal,
        size_t robot_index = 0) const;

    /// Get the observation version
    ObservationVersion version() const { return config_.version; }

    /// Get expected observation dimension for current version
    size_t observationDim() const;

private:
    ObservationConfig config_;
    const LidarSimulator* lidar_ = nullptr;

    /// Build V1 ego-centric observation (5 dims)
    warehouser_msgs::msg::Observation buildV1(
        const warehouser_msgs::msg::WorldState& world,
        const warehouser_msgs::msg::Goal& goal,
        size_t robot_index) const;

    /// Build V2 lidar-based observation (63 dims):
    /// [lidar_ranges(60), goal_bearing, goal_dist, is_carrying]
    /// Requires lidar_ to be set via constructor
    warehouser_msgs::msg::Observation buildV2(
        const warehouser_msgs::msg::WorldState& world,
        const warehouser_msgs::msg::Goal& goal,
        size_t robot_index) const;

    /// Build V3 multi-robot observation
    /// Includes ego state (5 dims) + relative positions of other robots
    warehouser_msgs::msg::Observation buildV3(
        const warehouser_msgs::msg::WorldState& world,
        const warehouser_msgs::msg::Goal& goal,
        size_t robot_index) const;

    /// Find robot entity by index in world state
    /// @param world World state containing entities
    /// @param index Index of robot to find (0 = first robot, etc.)
    /// @return nullptr if not found
    const warehouser_msgs::msg::Entity* findRobotByIndex(
        const warehouser_msgs::msg::WorldState& world,
        size_t index) const;

    /// Find all other robots (excluding the specified index)
    /// @param world World state containing entities
    /// @param exclude_index Index of robot to exclude
    /// @return Vector of pointers to other robot entities
    std::vector<const warehouser_msgs::msg::Entity*> findOtherRobots(
        const warehouser_msgs::msg::WorldState& world,
        size_t exclude_index) const;

    /// Normalize angle to [-π, π]
    static float normalizeAngle(float angle) {
        return std::atan2(std::sin(angle), std::cos(angle));
    }
};

}  // namespace warehouser
