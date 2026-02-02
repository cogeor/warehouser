#pragma once

#include <cmath>
#include <vector>

#include "warehouser_msgs/msg/lidar_debug.hpp"
#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_observations/sensor_interface.hpp"

namespace warehouser {

/// Configuration for lidar simulation
struct LidarConfig {
    int num_rays = 60;
    float fov = 3.14159265f;  // 180 degrees (π radians)
    float max_range = 10.0f;
    float min_range = 0.1f;
    float step_size = 0.05f;  // Raycast step resolution (5cm)
};

/// Simulates lidar scans from world state for visualization and future training.
/// Even when training on position-based observations (V1), lidar is simulated
/// for visualization in the frontend.
/// Implements ISensor interface for polymorphic sensor handling.
class LidarSimulator : public ISensor {
public:
    explicit LidarSimulator(const LidarConfig& config = {});

    /// Perform lidar scan from robot pose
    /// @param robot_x Robot X position
    /// @param robot_y Robot Y position
    /// @param robot_theta Robot heading angle
    /// @param world World state for collision detection
    /// @return Vector of range values for each ray
    std::vector<float> scan(float robot_x, float robot_y, float robot_theta,
                            const warehouser_msgs::msg::WorldState& world) const;

    /// Build complete LidarDebug message
    /// @param robot_x Robot X position
    /// @param robot_y Robot Y position
    /// @param robot_theta Robot heading angle
    /// @param world World state for collision detection
    /// @return LidarDebug message with ranges and metadata
    warehouser_msgs::msg::LidarDebug buildDebugMsg(
        float robot_x, float robot_y, float robot_theta,
        const warehouser_msgs::msg::WorldState& world) const;

    /// Get the configuration
    const LidarConfig& config() const { return config_; }

    // ISensor interface implementation

    /// Get sensor type
    SensorType type() const override { return SensorType::Lidar; }

    /// Perform lidar scan using ISensor interface
    /// @param pose Sensor pose (x, y, theta)
    /// @param world World state for collision detection
    /// @return SensorReading containing LidarReading
    SensorReading scan(const SensorPose& pose,
                       const warehouser_msgs::msg::WorldState& world) const override;

private:
    LidarConfig config_;

    /// Cast a single ray and return distance to first hit
    /// @param ox Origin X
    /// @param oy Origin Y
    /// @param angle Ray angle (world frame)
    /// @param world World state
    /// @return Distance to first obstacle or max_range
    float raycast(float ox, float oy, float angle,
                  const warehouser_msgs::msg::WorldState& world) const;

    /// Check if a point is inside any wall entity
    bool checkWallCollision(float px, float py,
                            const warehouser_msgs::msg::WorldState& world) const;

    /// Check if point is within world bounds
    bool isInBounds(float px, float py, float world_width, float world_height) const;
};

}  // namespace warehouser
