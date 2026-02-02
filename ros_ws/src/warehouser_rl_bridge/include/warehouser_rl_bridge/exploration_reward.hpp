#pragma once

#include <string>

#include "warehouser_rl_bridge/occupancy_tracker.hpp"
#include "warehouser_rl_bridge/reward_strategy.hpp"

namespace warehouser {

/// Configuration for exploration reward
struct ExplorationConfig {
    float new_cell_bonus = 1.0f;     // Reward for first visit to a cell
    float revisit_bonus = 0.0f;      // Reward for revisiting a cell (0 = no reward)
    float coverage_bonus = 10.0f;    // Bonus for reaching coverage target
    float coverage_target = 0.8f;    // Coverage target (0.8 = 80%)
    OccupancyConfig occupancy = {};  // Occupancy grid configuration
};

/// Exploration reward strategy using coverage tracking.
///
/// Rewards the agent for visiting new cells in a discretized grid.
/// This is essential for:
/// - SLAM map building
/// - Search and rescue tasks
/// - Patrol/coverage tasks
/// - Curriculum learning (explore before navigate)
///
/// Note: This strategy has mutable state (the coverage tracker).
/// Call reset() at the start of each episode.
class ExplorationRewardStrategy : public IRewardStrategy {
public:
    explicit ExplorationRewardStrategy(const ExplorationConfig& config = {});

    /// Calculate exploration reward based on robot position
    /// @param ctx Reward context (uses curr_world to find robot position)
    /// @return Reward for visiting new/revisited cells, plus coverage bonus
    RewardResult calculate(const RewardContext& ctx) const override;

    /// Strategy name for logging
    std::string name() const override { return "exploration"; }

    /// Reset coverage tracking (call at episode start)
    void reset();

    /// Get current coverage percentage [0, 1]
    float coverage() const { return tracker_.coverage(); }

    /// Get the occupancy tracker for visualization/debugging
    const OccupancyTracker& tracker() const { return tracker_; }

    /// Get configuration
    const ExplorationConfig& config() const { return config_; }

private:
    ExplorationConfig config_;
    mutable OccupancyTracker tracker_;  // Mutable for const calculate()

    /// Find robot by index (shared helper)
    const warehouser_msgs::msg::Entity* findRobotByIndex(
        const warehouser_msgs::msg::WorldState& world, size_t index) const;
};

// ============ Factory Functions ============

/// Create exploration-focused reward strategy
/// Combines: Exploration + Collision + Time (no navigation goal)
std::unique_ptr<IRewardStrategy> createExplorationOnlyStrategy();

/// Create multi-task reward strategy with exploration
/// Combines: Navigation + Exploration + Collision + Time
std::unique_ptr<IRewardStrategy> createMultiTaskRewardStrategy();

}  // namespace warehouser
