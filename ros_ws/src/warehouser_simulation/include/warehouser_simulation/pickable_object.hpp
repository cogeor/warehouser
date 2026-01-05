#pragma once

#include <string>

#include "warehouser_simulation/entity.hpp"

namespace warehouser {

/// Pickable object that can be picked up and carried by the robot.
class PickableObject : public Entity {
public:
    std::string color;
    float pickup_radius = 0.5f;  // Distance within which robot can pick up
    bool is_picked = false;

    PickableObject() = default;
    explicit PickableObject(std::string entity_id, float pos_x = 0.0f,
                            float pos_y = 0.0f, std::string obj_color = "red")
        : Entity(std::move(entity_id), pos_x, pos_y),
          color(std::move(obj_color)) {}

    EntityType getType() const override { return EntityType::Object; }

    // Objects don't move on their own
    void update(float /*dt*/) override {}

    /// Check if a point is within pickup range
    bool contains(float px, float py) const override {
        return distance(x, y, px, py) <= pickup_radius;
    }

    warehouser_msgs::msg::Entity toMsg() const override;
};

}  // namespace warehouser
