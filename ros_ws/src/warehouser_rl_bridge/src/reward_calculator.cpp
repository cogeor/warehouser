#include "warehouser_rl_bridge/reward_calculator.hpp"

namespace warehouser {

RewardCalculator::RewardCalculator(const RewardConfig& config)
    : config_(config), strategy_(createStrategyFromConfig(config)) {}

void RewardCalculator::setStrategy(std::unique_ptr<IRewardStrategy> strategy) {
    strategy_ = std::move(strategy);
}

RewardResult RewardCalculator::calculate(
    const warehouser_msgs::msg::WorldState& prev_world,
    const warehouser_msgs::msg::WorldState& curr_world,
    const warehouser_msgs::msg::Goal& goal,
    int step_count, int max_steps) const {
    // Default to robot index 0 for backward compatibility
    return calculate(prev_world, curr_world, goal, step_count, max_steps, 0);
}

RewardResult RewardCalculator::calculate(
    const warehouser_msgs::msg::WorldState& prev_world,
    const warehouser_msgs::msg::WorldState& curr_world,
    const warehouser_msgs::msg::Goal& goal,
    int step_count, int max_steps,
    size_t robot_index) const {

    RewardContext ctx{
        prev_world,
        curr_world,
        goal,
        step_count,
        max_steps,
        robot_index
    };

    auto result = strategy_->calculate(ctx);

    // Handle truncation (max steps reached)
    if (step_count >= max_steps && !result.terminated) {
        result.truncated = true;
        if (result.termination_reason.empty()) {
            result.termination_reason = "Max steps reached";
        }
    }

    return result;
}

std::unique_ptr<IRewardStrategy> RewardCalculator::createStrategyFromConfig(
    const RewardConfig& config) const {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    // Configure navigation strategy
    NavigationConfig nav_config;
    nav_config.progress_weight = config.progress_weight;
    nav_config.success_bonus = config.success_bonus;
    nav_config.goal_threshold = config.goal_threshold;
    composite->addStrategy(
        std::make_shared<NavigationRewardStrategy>(nav_config), 1.0f);

    // Configure collision strategy
    CollisionConfig coll_config;
    coll_config.collision_penalty = config.collision_penalty;
    composite->addStrategy(
        std::make_shared<CollisionRewardStrategy>(coll_config), 1.0f);

    // Configure time strategy
    TimeConfig time_config;
    time_config.time_penalty = config.time_penalty;
    composite->addStrategy(
        std::make_shared<TimeRewardStrategy>(time_config), 1.0f);

    // Configure pick/place strategy
    PickPlaceConfig pp_config;
    pp_config.pickup_bonus = config.pickup_bonus;
    pp_config.place_bonus = config.pickup_bonus;  // Use same value for place
    composite->addStrategy(
        std::make_shared<PickPlaceRewardStrategy>(pp_config), 1.0f);

    return composite;
}

}  // namespace warehouser
