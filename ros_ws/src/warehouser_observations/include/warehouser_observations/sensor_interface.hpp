#pragma once

#include <array>
#include <variant>
#include <vector>

#include "warehouser_msgs/msg/world_state.hpp"

namespace warehouser {

/// Sensor types for identification
enum class SensorType : uint8_t {
    Lidar = 0,
    Odometry = 1,
    Imu = 2
};

/// Pose for sensor origin
struct SensorPose {
    float x = 0.0f;
    float y = 0.0f;
    float theta = 0.0f;
};

/// Lidar-specific reading
struct LidarReading {
    std::vector<float> ranges;
    float angle_min = 0.0f;
    float angle_max = 0.0f;
};

/// Odometry-specific reading
struct OdometryReading {
    float dx = 0.0f;         // Delta X in world frame
    float dy = 0.0f;         // Delta Y in world frame
    float dtheta = 0.0f;     // Delta heading
    float dt = 0.0f;         // Time delta
    // Covariance diagonal (x, y, z, roll, pitch, yaw) for nav_msgs::Odometry
    std::array<float, 6> covariance = {0.01f, 0.01f, 0.0f, 0.0f, 0.0f, 0.01f};
};

/// IMU-specific reading (for future ImuSimulator)
struct ImuReading {
    float angular_velocity = 0.0f;
    float linear_acceleration_x = 0.0f;
    float linear_acceleration_y = 0.0f;
};

/// Unified sensor reading (variant for type safety)
using SensorReading = std::variant<LidarReading, OdometryReading, ImuReading>;

/// Abstract sensor interface - Strategy pattern for sensors.
/// All sensors implement this interface, enabling polymorphic handling
/// and easy addition of new sensor types without modifying existing code.
class ISensor {
public:
    virtual ~ISensor() = default;

    /// Get the sensor type
    virtual SensorType type() const = 0;

    /// Perform a sensor scan from the given pose
    /// @param pose Sensor origin pose (x, y, theta)
    /// @param world Current world state for collision/measurement
    /// @return Sensor-specific reading wrapped in SensorReading variant
    virtual SensorReading scan(
        const SensorPose& pose,
        const warehouser_msgs::msg::WorldState& world) const = 0;
};

}  // namespace warehouser
