#pragma once

#include <memory>
#include <string>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_rl_bridge/reward_strategy.hpp"

namespace warehouser {

/// Reward configuration (legacy - kept for backward compatibility)
struct RewardConfig {
    float progress_weight = 1.0f;           // Reward for getting closer to goal
    float collision_penalty = -100.0f;      // Penalty for wall collision
    float robot_collision_penalty = -50.0f; // Penalty for robot-robot collision
    float success_bonus = 100.0f;           // Bonus for reaching goal
    float pickup_bonus = 50.0f;             // Bonus for picking up object
    float time_penalty = -0.1f;             // Small penalty per step
    float goal_threshold = 0.5f;            // Distance to consider goal reached
};

/// Calculates rewards for RL training.
/// Facade over IRewardStrategy for backward compatibility.
/// Use setStrategy() to customize reward behavior.
class RewardCalculator {
public:
    explicit RewardCalculator(const RewardConfig& config = {});

    /// Set custom reward strategy
    /// @param strategy Strategy to use (takes ownership)
    void setStrategy(std::unique_ptr<IRewardStrategy> strategy);

    /// Get current strategy (for inspection/testing)
    const IRewardStrategy* strategy() const { return strategy_.get(); }

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

    /// Calculate reward for specific robot (multi-robot support)
    /// @param prev_world Previous world state
    /// @param curr_world Current world state
    /// @param goal Current goal
    /// @param step_count Current episode step count
    /// @param max_steps Maximum steps per episode
    /// @param robot_index Index of robot to calculate reward for
    /// @return Reward result with reward value and termination flags
    RewardResult calculate(const warehouser_msgs::msg::WorldState& prev_world,
                           const warehouser_msgs::msg::WorldState& curr_world,
                           const warehouser_msgs::msg::Goal& goal,
                           int step_count, int max_steps,
                           size_t robot_index) const;

    /// Get configuration (legacy)
    const RewardConfig& config() const { return config_; }

private:
    RewardConfig config_;
    std::unique_ptr<IRewardStrategy> strategy_;

    /// Create strategy from legacy config
    std::unique_ptr<IRewardStrategy> createStrategyFromConfig(
        const RewardConfig& config) const;
};

}  // namespace warehouser
