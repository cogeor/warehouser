#pragma once

#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_simulation/pickable_object.hpp"
#include "warehouser_simulation/robot.hpp"
#include "warehouser_simulation/wall.hpp"
#include "warehouser_simulation/zone.hpp"

namespace warehouser {

/// Configuration for the world
struct WorldConfig {
    float width = 10.0f;
    float height = 10.0f;
    std::array<float, 3> robot_spawn = {1.0f, 1.0f, 0.0f};  // x, y, theta
};

/// Manages all entities in the simulation world.
/// Single source of truth for world state.
class WorldManager {
public:
    WorldManager() = default;
    explicit WorldManager(const WorldConfig& config);

    /// Load world configuration from YAML file
    /// @return Error message if failed
    std::expected<void, std::string> loadConfig(const std::string& config_path);

    /// Start the simulation
    void start() { running_ = true; }

    /// Pause the simulation
    void pause() { running_ = false; }

    /// Check if simulation is running
    bool isRunning() const { return running_; }

    /// Reset to initial state
    void reset();

    /// Step the simulation forward
    /// @param dt Time step in seconds
    void step(float dt);

    /// Get current simulation time
    float simTime() const { return sim_time_; }

    // Entity access

    /// Get the robot (never null after initialization)
    Robot* robot() { return robot_.get(); }
    const Robot* robot() const { return robot_.get(); }

    /// Get all pickable objects
    const std::vector<std::unique_ptr<PickableObject>>& objects() const {
        return objects_;
    }

    /// Get all walls
    const std::vector<std::unique_ptr<Wall>>& walls() const { return walls_; }

    /// Get all zones
    const std::vector<std::unique_ptr<Zone>>& zones() const { return zones_; }

    /// Get world dimensions
    float width() const { return config_.width; }
    float height() const { return config_.height; }

    // Entity manipulation

    /// Move an entity to a new position
    /// @return Error if entity not found
    std::expected<void, std::string> moveEntity(const std::string& id, float x,
                                                 float y);

    /// Find an object by ID
    PickableObject* findObject(const std::string& id);

    /// Find the closest unpicked object with the given color
    /// @return nullptr if no matching object found
    PickableObject* findClosestByColor(const std::string& color);

    /// Find a zone by name
    Zone* findZone(const std::string& zone_name);

    /// Check if a position collides with any wall
    bool checkCollision(float px, float py) const;

    /// Check if position is within world bounds
    bool isInBounds(float px, float py) const {
        return px >= 0 && px <= config_.width && py >= 0 && py <= config_.height;
    }

    // Serialization

    /// Convert current state to ROS message
    warehouser_msgs::msg::WorldState toMsg() const;

private:
    WorldConfig config_;
    std::string config_path_;

    std::unique_ptr<Robot> robot_;
    std::vector<std::unique_ptr<PickableObject>> objects_;
    std::vector<std::unique_ptr<Wall>> walls_;
    std::vector<std::unique_ptr<Zone>> zones_;

    // Initial state for reset
    std::array<float, 3> initial_robot_pose_;
    std::vector<std::pair<std::string, std::pair<float, float>>> initial_object_positions_;

    float sim_time_ = 0.0f;
    bool running_ = false;
};

}  // namespace warehouser
