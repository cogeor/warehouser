#include "warehouser_rl_bridge/reward_strategy.hpp"

#include <cmath>

namespace warehouser {

// ============ Helper Functions ============

namespace {

const warehouser_msgs::msg::Entity* findRobotByIndexImpl(
    const warehouser_msgs::msg::WorldState& world, size_t index) {
    size_t robot_count = 0;
    for (const auto& entity : world.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT
            if (robot_count == index) {
                return &entity;
            }
            robot_count++;
        }
    }
    return nullptr;
}

float calculateDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

// ============ NavigationRewardStrategy ============

NavigationRewardStrategy::NavigationRewardStrategy(const NavigationConfig& config)
    : config_(config) {}

RewardResult NavigationRewardStrategy::calculate(const RewardContext& ctx) const {
    RewardResult result;

    const auto* prev_robot = findRobotByIndex(ctx.prev_world, ctx.robot_index);
    const auto* curr_robot = findRobotByIndex(ctx.curr_world, ctx.robot_index);

    if (!curr_robot) {
        // Robot not found - let collision strategy handle this
        return result;
    }

    float curr_dist = distanceToGoal(*curr_robot, ctx.goal);

    // Check if goal reached
    if (curr_dist < config_.goal_threshold) {
        result.terminated = true;
        result.termination_reason = "Goal reached";
        result.reward = config_.success_bonus;
        return result;
    }

    // Progress reward (getting closer to goal)
    if (prev_robot) {
        float prev_dist = distanceToGoal(*prev_robot, ctx.goal);
        float progress = prev_dist - curr_dist;
        result.reward = progress * config_.progress_weight;
    }

    return result;
}

const warehouser_msgs::msg::Entity* NavigationRewardStrategy::findRobotByIndex(
    const warehouser_msgs::msg::WorldState& world, size_t index) const {
    return findRobotByIndexImpl(world, index);
}

float NavigationRewardStrategy::distanceToGoal(
    const warehouser_msgs::msg::Entity& robot,
    const warehouser_msgs::msg::Goal& goal) const {
    return calculateDistance(robot.x, robot.y, goal.x, goal.y);
}

// ============ CollisionRewardStrategy ============

CollisionRewardStrategy::CollisionRewardStrategy(const CollisionConfig& config)
    : config_(config) {}

RewardResult CollisionRewardStrategy::calculate(const RewardContext& ctx) const {
    RewardResult result;

    const auto* curr_robot = findRobotByIndex(ctx.curr_world, ctx.robot_index);

    if (!curr_robot) {
        // Robot not found indicates collision/failure
        result.terminated = true;
        result.termination_reason = "Robot not found";
        result.reward = config_.collision_penalty;
    }

    return result;
}

const warehouser_msgs::msg::Entity* CollisionRewardStrategy::findRobotByIndex(
    const warehouser_msgs::msg::WorldState& world, size_t index) const {
    return findRobotByIndexImpl(world, index);
}

// ============ TimeRewardStrategy ============

TimeRewardStrategy::TimeRewardStrategy(const TimeConfig& config)
    : config_(config) {}

RewardResult TimeRewardStrategy::calculate(const RewardContext& ctx) const {
    RewardResult result;

    // Constant time penalty to encourage efficiency
    result.reward = config_.time_penalty;

    return result;
}

// ============ PickPlaceRewardStrategy ============

PickPlaceRewardStrategy::PickPlaceRewardStrategy(const PickPlaceConfig& config)
    : config_(config) {}

RewardResult PickPlaceRewardStrategy::calculate(const RewardContext& ctx) const {
    RewardResult result;

    const auto* prev_robot = findRobotByIndex(ctx.prev_world, ctx.robot_index);
    const auto* curr_robot = findRobotByIndex(ctx.curr_world, ctx.robot_index);

    if (!prev_robot || !curr_robot) {
        return result;
    }

    // Pickup bonus: wasn't carrying, now is
    if (!prev_robot->is_carrying && curr_robot->is_carrying) {
        result.reward += config_.pickup_bonus;
    }

    // Place bonus: was carrying, now isn't (and not at same position = dropped)
    if (prev_robot->is_carrying && !curr_robot->is_carrying) {
        result.reward += config_.place_bonus;
    }

    return result;
}

const warehouser_msgs::msg::Entity* PickPlaceRewardStrategy::findRobotByIndex(
    const warehouser_msgs::msg::WorldState& world, size_t index) const {
    return findRobotByIndexImpl(world, index);
}

// ============ RobotCollisionRewardStrategy ============

RobotCollisionRewardStrategy::RobotCollisionRewardStrategy(const RobotCollisionConfig& config)
    : config_(config) {}

RewardResult RobotCollisionRewardStrategy::calculate(const RewardContext& ctx) const {
    RewardResult result;

    const auto* curr_robot = findRobotByIndex(ctx.curr_world, ctx.robot_index);

    if (curr_robot && curr_robot->in_robot_collision) {
        // Robot is colliding with another robot
        result.reward = config_.robot_collision_penalty;
    }

    return result;
}

const warehouser_msgs::msg::Entity* RobotCollisionRewardStrategy::findRobotByIndex(
    const warehouser_msgs::msg::WorldState& world, size_t index) const {
    return findRobotByIndexImpl(world, index);
}

// ============ CompositeRewardStrategy ============

void CompositeRewardStrategy::addStrategy(
    std::shared_ptr<IRewardStrategy> strategy, float weight) {
    strategies_.push_back({std::move(strategy), weight});
}

RewardResult CompositeRewardStrategy::calculate(const RewardContext& ctx) const {
    RewardResult combined;

    for (const auto& sw : strategies_) {
        auto result = sw.strategy->calculate(ctx);

        // Accumulate weighted reward
        combined.reward += sw.weight * result.reward;

        // Any strategy can terminate the episode
        if (result.terminated) {
            combined.terminated = true;
            if (combined.termination_reason.empty()) {
                combined.termination_reason = result.termination_reason;
            }
        }
        if (result.truncated) {
            combined.truncated = true;
        }
    }

    return combined;
}

// ============ Factory Functions ============

std::unique_ptr<IRewardStrategy> createDefaultRewardStrategy() {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    // Match original RewardCalculator weights
    composite->addStrategy(std::make_shared<NavigationRewardStrategy>(), 1.0f);
    composite->addStrategy(std::make_shared<CollisionRewardStrategy>(), 1.0f);
    composite->addStrategy(std::make_shared<TimeRewardStrategy>(), 1.0f);
    composite->addStrategy(std::make_shared<PickPlaceRewardStrategy>(), 1.0f);
    composite->addStrategy(std::make_shared<RobotCollisionRewardStrategy>(), 1.0f);

    return composite;
}

std::unique_ptr<IRewardStrategy> createNavigationOnlyStrategy() {
    return std::make_unique<NavigationRewardStrategy>();
}

}  // namespace warehouser
