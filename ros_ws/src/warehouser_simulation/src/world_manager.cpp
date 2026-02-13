#include "warehouser_simulation/world_manager.hpp"

#include <fstream>
#include <limits>

// Simple YAML parsing - for production, use yaml-cpp
// This is a minimal implementation for basic config loading

namespace warehouser {

WorldManager::WorldManager(const WorldConfig& config) : config_(config) {
    // Use robot_spawns if provided, otherwise use legacy robot_spawn
    if (!config.robot_spawns.empty()) {
        for (const auto& spawn : config.robot_spawns) {
            addRobot(spawn);
        }
    } else {
        // Legacy single robot support
        RobotSpawnConfig legacy_spawn;
        legacy_spawn.id = "robot";
        legacy_spawn.x = config.robot_spawn[0];
        legacy_spawn.y = config.robot_spawn[1];
        legacy_spawn.theta = config.robot_spawn[2];
        addRobot(legacy_spawn);
    }
}

size_t WorldManager::addRobot(const RobotSpawnConfig& config) {
    robots_.push_back(std::make_unique<Robot>(
        config.id, config.x, config.y, config.theta));
    initial_robot_configs_.push_back(config);
    return robots_.size() - 1;
}

std::expected<void, std::string> WorldManager::loadConfig(
    const std::string& config_path) {
    config_path_ = config_path;

    // For now, create a default world
    // TODO: Parse YAML config file

    // Create default robot using legacy spawn position
    RobotSpawnConfig default_spawn;
    default_spawn.id = "robot";
    default_spawn.x = config_.robot_spawn[0];
    default_spawn.y = config_.robot_spawn[1];
    default_spawn.theta = config_.robot_spawn[2];
    addRobot(default_spawn);

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

    // Reset all robots to their initial configurations
    for (size_t i = 0; i < robots_.size() && i < initial_robot_configs_.size(); ++i) {
        const auto& config = initial_robot_configs_[i];
        robots_[i]->x = config.x;
        robots_[i]->y = config.y;
        robots_[i]->theta = config.theta;
        robots_[i]->v = 0.0f;
        robots_[i]->omega = 0.0f;
        robots_[i]->is_carrying = false;
        robots_[i]->carried_object_id.clear();
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

void WorldManager::resetWithRobotCount(size_t robot_count) {
    // Reset simulation time
    sim_time_ = 0.0f;
    running_ = false;

    // Clear existing robots and their configs
    robots_.clear();
    initial_robot_configs_.clear();

    // Spawn requested number of robots with distributed positions
    const float spacing = 2.0f;  // Minimum spacing between robots
    const float start_x = 1.0f;
    const float start_y = 1.0f;

    for (size_t i = 0; i < robot_count; ++i) {
        RobotSpawnConfig spawn;
        spawn.id = "robot" + std::to_string(i);
        // Distribute robots in a grid pattern
        size_t cols = static_cast<size_t>(std::sqrt(static_cast<double>(robot_count))) + 1;
        spawn.x = start_x + static_cast<float>(i % cols) * spacing;
        spawn.y = start_y + static_cast<float>(i / cols) * spacing;
        spawn.theta = 0.0f;

        // Clamp to world bounds
        spawn.x = std::min(spawn.x, config_.width - 1.0f);
        spawn.y = std::min(spawn.y, config_.height - 1.0f);

        addRobot(spawn);
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

    // Clear collision flags at start of step
    for (auto& robot : robots_) {
        robot->in_robot_collision = false;
    }

    // Update all robots
    for (size_t i = 0; i < robots_.size(); ++i) {
        auto& robot = robots_[i];

        // Store previous position for collision rollback
        float prev_x = robot->x;
        float prev_y = robot->y;

        // Update robot
        robot->update(dt);

        // Check wall collision, bounds, and robot-robot collision
        bool wall_collision = checkCollision(robot->x, robot->y);
        bool out_of_bounds = !isInBounds(robot->x, robot->y);
        bool robot_collision = checkRobotCollision(i);

        // Set collision flag for reward calculation
        if (robot_collision) {
            robot->in_robot_collision = true;
        }

        // Rollback if any collision detected
        if (wall_collision || out_of_bounds || robot_collision) {
            robot->x = prev_x;
            robot->y = prev_y;
            robot->stop();
        }

        // Update carried object position
        if (robot->is_carrying) {
            if (auto* obj = findObject(robot->carried_object_id)) {
                obj->x = robot->x;
                obj->y = robot->y;
            }
        }
    }

    sim_time_ += dt;
}

std::expected<void, std::string> WorldManager::moveEntity(const std::string& id,
                                                           float new_x,
                                                           float new_y) {
    // Search all robots
    for (auto& robot : robots_) {
        if (robot->id == id) {
            robot->x = new_x;
            robot->y = new_y;
            return {};
        }
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
    // Use first robot as reference (backward compatible behavior)
    if (robots_.empty()) {
        return nullptr;
    }
    const auto& ref_robot = robots_[0];

    PickableObject* closest = nullptr;
    float min_dist = std::numeric_limits<float>::max();

    for (auto& obj : objects_) {
        // Skip wrong color or already picked
        if (obj->color != color || obj->is_picked) {
            continue;
        }

        float d = distance(ref_robot->x, ref_robot->y, obj->x, obj->y);
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

bool WorldManager::checkRobotCollision(size_t robot_index) const {
    if (robot_index >= robots_.size()) {
        return false;
    }

    const auto& robot = robots_[robot_index];
    const float collision_distance = 2.0f * Robot::kRadius;

    for (size_t i = 0; i < robots_.size(); ++i) {
        if (i == robot_index) {
            continue;
        }

        const auto& other = robots_[i];
        float dist = distance(robot->x, robot->y, other->x, other->y);
        if (dist < collision_distance) {
            return true;
        }
    }

    return false;
}

warehouser_msgs::msg::WorldState WorldManager::toMsg() const {
    warehouser_msgs::msg::WorldState msg;
    msg.sim_time = sim_time_;
    msg.running = running_;

    // Add all robots
    for (const auto& robot : robots_) {
        msg.entities.push_back(robot->toMsg());
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
