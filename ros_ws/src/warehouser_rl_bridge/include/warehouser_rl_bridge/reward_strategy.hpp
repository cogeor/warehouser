#pragma once

#include <memory>
#include <string>
#include <vector>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/world_state.hpp"

namespace warehouser {

/// Result of reward calculation
struct RewardResult {
    float reward = 0.0f;
    bool terminated = false;
    bool truncated = false;
    std::string termination_reason;
};

/// Context for reward calculation
struct RewardContext {
    const warehouser_msgs::msg::WorldState& prev_world;
    const warehouser_msgs::msg::WorldState& curr_world;
    const warehouser_msgs::msg::Goal& goal;
    int step_count;
    int max_steps;
    size_t robot_index = 0;  // For multi-robot support
};

/// Abstract reward strategy interface - Strategy Pattern
/// Enables composable, extensible reward functions without modifying base code.
class IRewardStrategy {
public:
    virtual ~IRewardStrategy() = default;

    /// Calculate reward for state transition
    /// @param ctx Reward context with world states, goal, and step info
    /// @return Reward result with value and termination flags
    virtual RewardResult calculate(const RewardContext& ctx) const = 0;

    /// Human-readable strategy name for logging/debugging
    virtual std::string name() const = 0;
};

// ============ Concrete Strategies ============

/// Navigation reward configuration
struct NavigationConfig {
    float progress_weight = 1.0f;
    float success_bonus = 100.0f;
    float goal_threshold = 0.5f;
};

/// Navigation reward: progress toward goal
class NavigationRewardStrategy : public IRewardStrategy {
public:
    explicit NavigationRewardStrategy(const NavigationConfig& config = {});
    RewardResult calculate(const RewardContext& ctx) const override;
    std::string name() const override { return "navigation"; }

    const NavigationConfig& config() const { return config_; }

private:
    NavigationConfig config_;

    const warehouser_msgs::msg::Entity* findRobotByIndex(
        const warehouser_msgs::msg::WorldState& world, size_t index) const;
    float distanceToGoal(const warehouser_msgs::msg::Entity& robot,
                         const warehouser_msgs::msg::Goal& goal) const;
};

/// Collision penalty configuration
struct CollisionConfig {
    float collision_penalty = -100.0f;
};

/// Collision penalty: penalize when robot not found (collision detected)
class CollisionRewardStrategy : public IRewardStrategy {
public:
    explicit CollisionRewardStrategy(const CollisionConfig& config = {});
    RewardResult calculate(const RewardContext& ctx) const override;
    std::string name() const override { return "collision"; }

private:
    CollisionConfig config_;

    const warehouser_msgs::msg::Entity* findRobotByIndex(
        const warehouser_msgs::msg::WorldState& world, size_t index) const;
};

/// Time penalty configuration
struct TimeConfig {
    float time_penalty = -0.1f;
};

/// Time penalty: small penalty each step to encourage efficiency
class TimeRewardStrategy : public IRewardStrategy {
public:
    explicit TimeRewardStrategy(const TimeConfig& config = {});
    RewardResult calculate(const RewardContext& ctx) const override;
    std::string name() const override { return "time"; }

private:
    TimeConfig config_;
};

/// Pick/Place bonus configuration
struct PickPlaceConfig {
    float pickup_bonus = 50.0f;
    float place_bonus = 50.0f;
};

/// Pick/Place reward: bonus for picking up and placing objects
class PickPlaceRewardStrategy : public IRewardStrategy {
public:
    explicit PickPlaceRewardStrategy(const PickPlaceConfig& config = {});
    RewardResult calculate(const RewardContext& ctx) const override;
    std::string name() const override { return "pick_place"; }

private:
    PickPlaceConfig config_;

    const warehouser_msgs::msg::Entity* findRobotByIndex(
        const warehouser_msgs::msg::WorldState& world, size_t index) const;
};

// ============ Composite Strategy ============

/// Weighted strategy entry for composite
struct StrategyWeight {
    std::shared_ptr<IRewardStrategy> strategy;
    float weight = 1.0f;
};

/// Composite reward strategy - Composite Pattern
/// Combines multiple strategies with configurable weights.
class CompositeRewardStrategy : public IRewardStrategy {
public:
    CompositeRewardStrategy() = default;

    /// Add a strategy with weight
    /// @param strategy Strategy to add
    /// @param weight Weight multiplier for this strategy's reward
    void addStrategy(std::shared_ptr<IRewardStrategy> strategy, float weight = 1.0f);

    /// Calculate combined reward from all strategies
    RewardResult calculate(const RewardContext& ctx) const override;
    std::string name() const override { return "composite"; }

    /// Get number of strategies
    size_t strategyCount() const { return strategies_.size(); }

private:
    std::vector<StrategyWeight> strategies_;
};

// ============ Factory Functions ============

/// Create default reward strategy matching original RewardCalculator behavior
/// Combines: Navigation + Collision + Time + PickPlace
std::unique_ptr<IRewardStrategy> createDefaultRewardStrategy();

/// Create navigation-only reward strategy
std::unique_ptr<IRewardStrategy> createNavigationOnlyStrategy();

}  // namespace warehouser
