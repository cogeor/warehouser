#pragma once

#include <cmath>
#include <string>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/world_state.hpp"

namespace warehouser {

/// Reward configuration
struct RewardConfig {
    float progress_weight = 1.0f;     // Reward for getting closer to goal
    float collision_penalty = -100.0f; // Penalty for collision
    float success_bonus = 100.0f;      // Bonus for reaching goal
    float pickup_bonus = 50.0f;        // Bonus for picking up object
    float time_penalty = -0.1f;        // Small penalty per step
    float goal_threshold = 0.5f;       // Distance to consider goal reached
};

/// Result of reward calculation
struct RewardResult {
    float reward = 0.0f;
    bool terminated = false;
    bool truncated = false;
    std::string termination_reason;
};

/// Calculates rewards for RL training.
/// Rewards progress toward goal, penalizes collisions and time.
class RewardCalculator {
public:
    explicit RewardCalculator(const RewardConfig& config = {});

    /// Calculate reward for a state transition
    /// @param prev_world Previous world state
    /// @param curr_world Current world state
    /// @param goal Current goal
    /// @param step_count Current episode step count
    /// @param max_steps Maximum steps per episode
    /// @return Reward result with reward value and termination flags
    RewardResult calculate(const warehouser_msgs::msg::WorldState& prev_world,
                           const warehouser_msgs::msg::WorldState& curr_world,
                           const warehouser_msgs::msg::Goal& goal,
                           int step_count, int max_steps) const;

    /// Get configuration
    const RewardConfig& config() const { return config_; }

private:
    RewardConfig config_;

    /// Find robot in world state
    const warehouser_msgs::msg::Entity* findRobot(
        const warehouser_msgs::msg::WorldState& world) const;

    /// Calculate distance from robot to goal
    float distanceToGoal(const warehouser_msgs::msg::Entity& robot,
                         const warehouser_msgs::msg::Goal& goal) const;
};

}  // namespace warehouser
