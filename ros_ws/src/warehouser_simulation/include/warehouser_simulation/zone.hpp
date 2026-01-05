#pragma once

#include <string>

#include "warehouser_simulation/entity.hpp"

namespace warehouser {

/// Circular zone area (e.g., drop zone, spawn area).
class Zone : public Entity {
public:
    std::string zone_name;
    float radius = 0.5f;

    Zone() = default;
    explicit Zone(std::string entity_id, float pos_x = 0.0f, float pos_y = 0.0f,
                  std::string name = "", float r = 0.5f)
        : Entity(std::move(entity_id), pos_x, pos_y),
          zone_name(std::move(name)),
          radius(r) {}

    EntityType getType() const override { return EntityType::Zone; }

    // Zones don't move
    void update(float /*dt*/) override {}

    /// Check if a point is inside the zone
    bool contains(float px, float py) const override {
        return distance(x, y, px, py) <= radius;
    }

    warehouser_msgs::msg::Entity toMsg() const override;
};

}  // namespace warehouser
