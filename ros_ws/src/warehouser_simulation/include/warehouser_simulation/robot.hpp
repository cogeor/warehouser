#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#include "warehouser_simulation/entity.hpp"

namespace warehouser {

// Forward declaration
class PickableObject;

/// Robot entity with differential drive kinematics.
/// The robot can navigate, pick up objects, and carry them.
class Robot : public Entity {
public:
    // Pose
    float theta = 0.0f;   // Heading angle (radians)

    // Velocities
    float v = 0.0f;       // Linear velocity (m/s)
    float omega = 0.0f;   // Angular velocity (rad/s)

    // Carrying state
    bool is_carrying = false;
    std::string carried_object_id;

    // Collision state (set by WorldManager each step)
    bool in_robot_collision = false;

    // Physical parameters
    static constexpr float kVMax = 1.0f;      // Max linear velocity (m/s)
    static constexpr float kOmegaMax = 2.0f;  // Max angular velocity (rad/s)
    static constexpr float kRadius = 0.3f;    // Robot radius for collision (m)

    Robot() = default;
    explicit Robot(std::string entity_id, float pos_x = 0.0f, float pos_y = 0.0f,
                   float heading = 0.0f)
        : Entity(std::move(entity_id), pos_x, pos_y), theta(heading) {}

    EntityType getType() const override { return EntityType::Robot; }

    /// Update robot position using differential drive kinematics
    /// @param dt Time step in seconds
    void update(float dt) override {
        x += v * std::cos(theta) * dt;
        y += v * std::sin(theta) * dt;
        theta = normalizeAngle(theta + omega * dt);
    }

    /// Set velocity command (clamped to limits)
    /// @param linear Linear velocity command
    /// @param angular Angular velocity command
    void setCommand(float linear, float angular) {
        v = std::clamp(linear, -kVMax, kVMax);
        omega = std::clamp(angular, -kOmegaMax, kOmegaMax);
    }

    /// Stop the robot (set velocities to zero)
    void stop() {
        v = 0.0f;
        omega = 0.0f;
    }

    /// Attempt to pick up an object
    /// @param obj Object to pick
    /// @return true if pick was successful
    bool tryPick(PickableObject& obj);

    /// Drop the currently carried object at robot's position
    /// @param obj Object to drop (must match carried_object_id)
    void unpick(PickableObject& obj);

    /// Check if robot contains a point (for collision detection)
    bool contains(float px, float py) const override {
        return distance(x, y, px, py) <= kRadius;
    }

    warehouser_msgs::msg::Entity toMsg() const override;
};

}  // namespace warehouser
