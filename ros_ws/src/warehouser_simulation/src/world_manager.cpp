#include "warehouser_simulation/world_manager.hpp"

#include <fstream>
#include <limits>

// Simple YAML parsing - for production, use yaml-cpp
// This is a minimal implementation for basic config loading

namespace warehouser {

WorldManager::WorldManager(const WorldConfig& config) : config_(config) {
    // Create robot at spawn position
    robot_ = std::make_unique<Robot>("robot", config.robot_spawn[0],
                                      config.robot_spawn[1],
                                      config.robot_spawn[2]);
    initial_robot_pose_ = config.robot_spawn;
}

std::expected<void, std::string> WorldManager::loadConfig(
    const std::string& config_path) {
    config_path_ = config_path;

    // For now, create a default world
    // TODO: Parse YAML config file

    // Create default robot
    robot_ = std::make_unique<Robot>("robot", config_.robot_spawn[0],
                                      config_.robot_spawn[1],
                                      config_.robot_spawn[2]);
    initial_robot_pose_ = config_.robot_spawn;

    // Create some default objects
    auto red = std::make_unique<PickableObject>("red_1", 3.0f, 2.0f, "red");
    initial_object_positions_.emplace_back("red_1", std::make_pair(3.0f, 2.0f));
    objects_.push_back(std::move(red));

    auto green = std::make_unique<PickableObject>("green_1", 5.0f, 4.0f, "green");
    initial_object_positions_.emplace_back("green_1", std::make_pair(5.0f, 4.0f));
    objects_.push_back(std::move(green));

    auto blue = std::make_unique<PickableObject>("blue_1", 7.0f, 3.0f, "blue");
    initial_object_positions_.emplace_back("blue_1", std::make_pair(7.0f, 3.0f));
    objects_.push_back(std::move(blue));

    // Create boundary walls (thin walls around the perimeter)
    walls_.push_back(std::make_unique<Wall>("wall_bottom", 0.0f, 0.0f,
                                             config_.width, 0.1f));
    walls_.push_back(std::make_unique<Wall>("wall_top", 0.0f,
                                             config_.height - 0.1f,
                                             config_.width, 0.1f));
    walls_.push_back(std::make_unique<Wall>("wall_left", 0.0f, 0.0f, 0.1f,
                                             config_.height));
    walls_.push_back(std::make_unique<Wall>("wall_right", config_.width - 0.1f,
                                             0.0f, 0.1f, config_.height));

    // Create drop zone
    zones_.push_back(
        std::make_unique<Zone>("drop_zone", 8.0f, 8.0f, "drop_zone", 0.5f));

    return {};
}

void WorldManager::reset() {
    // Reset simulation time
    sim_time_ = 0.0f;
    running_ = false;

    // Reset robot
    if (robot_) {
        robot_->x = initial_robot_pose_[0];
        robot_->y = initial_robot_pose_[1];
        robot_->theta = initial_robot_pose_[2];
        robot_->v = 0.0f;
        robot_->omega = 0.0f;
        robot_->is_carrying = false;
        robot_->carried_object_id.clear();
    }

    // Reset objects to initial positions
    for (const auto& [id, pos] : initial_object_positions_) {
        if (auto* obj = findObject(id)) {
            obj->x = pos.first;
            obj->y = pos.second;
            obj->is_picked = false;
        }
    }
}

void WorldManager::step(float dt) {
    if (!running_) {
        return;
    }

    // Store previous position for collision rollback
    float prev_x = robot_->x;
    float prev_y = robot_->y;

    // Update robot
    robot_->update(dt);

    // Check collision and rollback if needed
    if (checkCollision(robot_->x, robot_->y) || !isInBounds(robot_->x, robot_->y)) {
        robot_->x = prev_x;
        robot_->y = prev_y;
        robot_->stop();
    }

    // Update carried object position
    if (robot_->is_carrying) {
        if (auto* obj = findObject(robot_->carried_object_id)) {
            obj->x = robot_->x;
            obj->y = robot_->y;
        }
    }

    sim_time_ += dt;
}

std::expected<void, std::string> WorldManager::moveEntity(const std::string& id,
                                                           float new_x,
                                                           float new_y) {
    if (robot_ && robot_->id == id) {
        robot_->x = new_x;
        robot_->y = new_y;
        return {};
    }

    if (auto* obj = findObject(id)) {
        obj->x = new_x;
        obj->y = new_y;
        return {};
    }

    return std::unexpected("Entity not found: " + id);
}

PickableObject* WorldManager::findObject(const std::string& id) {
    for (auto& obj : objects_) {
        if (obj->id == id) {
            return obj.get();
        }
    }
    return nullptr;
}

PickableObject* WorldManager::findClosestByColor(const std::string& color) {
    if (!robot_) {
        return nullptr;
    }

    PickableObject* closest = nullptr;
    float min_dist = std::numeric_limits<float>::max();

    for (auto& obj : objects_) {
        // Skip wrong color or already picked
        if (obj->color != color || obj->is_picked) {
            continue;
        }

        float d = distance(robot_->x, robot_->y, obj->x, obj->y);
        if (d < min_dist) {
            min_dist = d;
            closest = obj.get();
        }
    }

    return closest;
}

Zone* WorldManager::findZone(const std::string& zone_name) {
    for (auto& zone : zones_) {
        if (zone->zone_name == zone_name) {
            return zone.get();
        }
    }
    return nullptr;
}

bool WorldManager::checkCollision(float px, float py) const {
    for (const auto& wall : walls_) {
        if (wall->contains(px, py)) {
            return true;
        }
    }
    return false;
}

warehouser_msgs::msg::WorldState WorldManager::toMsg() const {
    warehouser_msgs::msg::WorldState msg;
    msg.sim_time = sim_time_;
    msg.running = running_;

    // Add robot
    if (robot_) {
        msg.entities.push_back(robot_->toMsg());
    }

    // Add objects
    for (const auto& obj : objects_) {
        msg.entities.push_back(obj->toMsg());
    }

    // Add walls
    for (const auto& wall : walls_) {
        msg.entities.push_back(wall->toMsg());
    }

    // Add zones
    for (const auto& zone : zones_) {
        msg.entities.push_back(zone->toMsg());
    }

    return msg;
}

}  // namespace warehouser
