#include "warehouser_rl_bridge/reward_calculator.hpp"

#include <cmath>

namespace warehouser {

RewardCalculator::RewardCalculator(const RewardConfig& config)
    : config_(config) {}

RewardResult RewardCalculator::calculate(
    const warehouser_msgs::msg::WorldState& prev_world,
    const warehouser_msgs::msg::WorldState& curr_world,
    const warehouser_msgs::msg::Goal& goal, int step_count,
    int max_steps) const {
    RewardResult result;

    // Find robot in current and previous state
    const auto* prev_robot = findRobot(prev_world);
    const auto* curr_robot = findRobot(curr_world);

    if (!curr_robot) {
        result.terminated = true;
        result.termination_reason = "Robot not found";
        result.reward = config_.collision_penalty;
        return result;
    }

    // Calculate distance to goal
    float curr_dist = distanceToGoal(*curr_robot, goal);

    // Check if goal reached
    if (curr_dist < config_.goal_threshold) {
        result.terminated = true;
        result.termination_reason = "Goal reached";
        result.reward = config_.success_bonus;
        return result;
    }

    // Check for max steps (truncation)
    if (step_count >= max_steps) {
        result.truncated = true;
        result.termination_reason = "Max steps reached";
        result.reward = config_.time_penalty;
        return result;
    }

    // Progress reward (getting closer to goal)
    if (prev_robot) {
        float prev_dist = distanceToGoal(*prev_robot, goal);
        float progress = prev_dist - curr_dist;
        result.reward += progress * config_.progress_weight;
    }

    // Pickup reward
    if (!prev_robot->is_carrying && curr_robot->is_carrying) {
        result.reward += config_.pickup_bonus;
    }

    // Time penalty
    result.reward += config_.time_penalty;

    return result;
}

const warehouser_msgs::msg::Entity* RewardCalculator::findRobot(
    const warehouser_msgs::msg::WorldState& world) const {
    for (const auto& entity : world.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT
            return &entity;
        }
    }
    return nullptr;
}

float RewardCalculator::distanceToGoal(
    const warehouser_msgs::msg::Entity& robot,
    const warehouser_msgs::msg::Goal& goal) const {
    float dx = goal.x - robot.x;
    float dy = goal.y - robot.y;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace warehouser
