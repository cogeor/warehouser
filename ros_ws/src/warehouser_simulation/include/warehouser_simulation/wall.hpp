#pragma once

#include <string>

#include "warehouser_simulation/entity.hpp"

namespace warehouser {

/// Axis-aligned rectangular wall obstacle.
/// Position (x, y) is the bottom-left corner.
class Wall : public Entity {
public:
    float width = 1.0f;
    float height = 1.0f;

    Wall() = default;
    explicit Wall(std::string entity_id, float pos_x = 0.0f, float pos_y = 0.0f,
                  float w = 1.0f, float h = 1.0f)
        : Entity(std::move(entity_id), pos_x, pos_y), width(w), height(h) {}

    EntityType getType() const override { return EntityType::Wall; }

    // Walls don't move
    void update(float /*dt*/) override {}

    /// Check if a point is inside the wall bounds
    bool contains(float px, float py) const override {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }

    warehouser_msgs::msg::Entity toMsg() const override;
};

}  // namespace warehouser
