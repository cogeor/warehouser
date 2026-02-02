#pragma once

#include <cmath>
#include <string>
#include <vector>

#include <rclcpp/time.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include "warehouser_msgs/msg/lidar_debug.hpp"
#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_observations/noise_model.hpp"
#include "warehouser_observations/sensor_interface.hpp"

namespace warehouser {

/// Configuration for lidar simulation
struct LidarConfig {
    int num_rays = 60;
    float fov = 3.14159265f;  // 180 degrees (π radians)
    float max_range = 10.0f;
    float min_range = 0.1f;
    float step_size = 0.05f;  // Raycast step resolution (5cm)

    // Noise configuration for domain randomization
    LidarNoiseConfig noise;
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

    /// Build complete LidarDebug message (for frontend visualization)
    /// @param robot_x Robot X position
    /// @param robot_y Robot Y position
    /// @param robot_theta Robot heading angle
    /// @param world World state for collision detection
    /// @return LidarDebug message with ranges and metadata
    warehouser_msgs::msg::LidarDebug buildDebugMsg(
        float robot_x, float robot_y, float robot_theta,
        const warehouser_msgs::msg::WorldState& world) const;

    /// Build standard LaserScan message (for SLAM compatibility)
    /// @param robot_x Robot X position
    /// @param robot_y Robot Y position
    /// @param robot_theta Robot heading angle
    /// @param world World state for collision detection
    /// @param stamp Timestamp for the message header
    /// @param frame_id TF frame ID (default: "base_laser")
    /// @return LaserScan message compatible with SLAM systems
    sensor_msgs::msg::LaserScan buildLaserScanMsg(
        float robot_x, float robot_y, float robot_theta,
        const warehouser_msgs::msg::WorldState& world,
        const rclcpp::Time& stamp,
        const std::string& frame_id = "base_laser") const;

    /// Get the configuration
    const LidarConfig& config() const { return config_; }

    /// Enable or disable noise
    /// @param enabled True to enable noise
    void setNoiseEnabled(bool enabled);

    /// Set noise seed for reproducibility
    /// @param seed Random seed value
    void setNoiseSeed(unsigned int seed);

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
    mutable NoiseModel range_noise_;  // Mutable for const scan methods

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
