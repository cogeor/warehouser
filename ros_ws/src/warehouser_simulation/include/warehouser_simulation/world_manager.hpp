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

/// Configuration for a single robot spawn
struct RobotSpawnConfig {
    std::string id = "robot";
    float x = 2.0f;
    float y = 2.0f;
    float theta = 0.0f;
};

/// Configuration for the world
struct WorldConfig {
    float width = 20.0f;
    float height = 20.0f;
    // Legacy: single robot spawn (for backward compatibility)
    std::array<float, 3> robot_spawn = {2.0f, 2.0f, 0.0f};  // x, y, theta
    // Multi-robot: list of robot spawns (if empty, uses robot_spawn)
    std::vector<RobotSpawnConfig> robot_spawns;
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

    /// Reset to initial state with a specific number of robots
    /// @param robot_count Number of robots to spawn
    void resetWithRobotCount(size_t robot_count);

    /// Step the simulation forward
    /// @param dt Time step in seconds
    void step(float dt);

    /// Get current simulation time
    float simTime() const { return sim_time_; }

    // Entity access

    /// Get robot by index (default 0 for backward compatibility)
    /// @param index Robot index (0-based)
    /// @return Pointer to robot, or nullptr if index out of range
    Robot* robot(size_t index = 0) {
        return index < robots_.size() ? robots_[index].get() : nullptr;
    }
    const Robot* robot(size_t index = 0) const {
        return index < robots_.size() ? robots_[index].get() : nullptr;
    }

    /// Get number of robots in the world
    size_t robotCount() const { return robots_.size(); }

    /// Add a robot to the world
    /// @param config Robot spawn configuration
    /// @return Index of the new robot
    size_t addRobot(const RobotSpawnConfig& config);

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

    /// Check if a robot collides with any other robot
    /// @param robot_index Index of the robot to check
    /// @return true if the robot collides with another robot
    bool checkRobotCollision(size_t robot_index) const;

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

    std::vector<std::unique_ptr<Robot>> robots_;
    std::vector<std::unique_ptr<PickableObject>> objects_;
    std::vector<std::unique_ptr<Wall>> walls_;
    std::vector<std::unique_ptr<Zone>> zones_;

    // Initial state for reset
    std::vector<RobotSpawnConfig> initial_robot_configs_;
    std::vector<std::pair<std::string, std::pair<float, float>>> initial_object_positions_;

    float sim_time_ = 0.0f;
    bool running_ = false;
};

}  // namespace warehouser
