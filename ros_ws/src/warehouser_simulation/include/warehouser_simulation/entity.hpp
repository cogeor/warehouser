#pragma once

#include <cmath>
#include <expected>
#include <string>

#include "warehouser_msgs/msg/entity.hpp"

namespace warehouser {

/// Entity types matching the message definition
enum class EntityType : uint8_t {
    Robot = 0,
    Object = 1,
    Wall = 2,
    Zone = 3
};

/// Normalize angle to [-π, π]
inline float normalizeAngle(float angle) {
    return std::atan2(std::sin(angle), std::cos(angle));
}

/// Calculate distance between two points
inline float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

/// Base class for all entities in the simulation world.
/// All entities have an id and position. Derived classes add specific behavior.
class Entity {
public:
    std::string id;
    float x = 0.0f;
    float y = 0.0f;

    Entity() = default;
    explicit Entity(std::string entity_id, float pos_x = 0.0f, float pos_y = 0.0f)
        : id(std::move(entity_id)), x(pos_x), y(pos_y) {}

    virtual ~Entity() = default;

    // Prevent copying, allow moving
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = default;
    Entity& operator=(Entity&&) = default;

    /// Get the entity type
    virtual EntityType getType() const = 0;

    /// Update entity state for one simulation step
    /// @param dt Time step in seconds
    virtual void update(float dt) { (void)dt; }

    /// Check if a point is inside this entity's bounds
    /// @return true if point (px, py) is inside entity
    virtual bool contains(float px, float py) const {
        (void)px;
        (void)py;
        return false;
    }

    /// Convert to ROS message
    virtual warehouser_msgs::msg::Entity toMsg() const;
};

}  // namespace warehouser
