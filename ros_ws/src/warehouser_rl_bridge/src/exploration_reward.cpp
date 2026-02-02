#include "warehouser_rl_bridge/exploration_reward.hpp"

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

}  // namespace

// ============ ExplorationRewardStrategy ============

ExplorationRewardStrategy::ExplorationRewardStrategy(
    const ExplorationConfig& config)
    : config_(config), tracker_(config.occupancy) {}

RewardResult ExplorationRewardStrategy::calculate(
    const RewardContext& ctx) const {
    RewardResult result;

    const auto* robot = findRobotByIndex(ctx.curr_world, ctx.robot_index);
    if (!robot) {
        // Robot not found - let collision strategy handle this
        return result;
    }

    // Mark current position as visited and check if new
    bool is_new = tracker_.markVisited(robot->x, robot->y);

    if (is_new) {
        result.reward = config_.new_cell_bonus;
    } else {
        result.reward = config_.revisit_bonus;
    }

    // Check coverage target
    if (tracker_.coverage() >= config_.coverage_target) {
        result.reward += config_.coverage_bonus;
        result.terminated = true;
        result.termination_reason = "Coverage target reached";
    }

    return result;
}

void ExplorationRewardStrategy::reset() {
    tracker_.reset();
}

const warehouser_msgs::msg::Entity* ExplorationRewardStrategy::findRobotByIndex(
    const warehouser_msgs::msg::WorldState& world, size_t index) const {
    return findRobotByIndexImpl(world, index);
}

// ============ Factory Functions ============

std::unique_ptr<IRewardStrategy> createExplorationOnlyStrategy() {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    // Exploration-focused: no navigation goal
    composite->addStrategy(std::make_shared<ExplorationRewardStrategy>(), 1.0f);
    composite->addStrategy(std::make_shared<CollisionRewardStrategy>(), 1.0f);
    composite->addStrategy(std::make_shared<TimeRewardStrategy>(), 0.1f);

    return composite;
}

std::unique_ptr<IRewardStrategy> createMultiTaskRewardStrategy() {
    auto composite = std::make_unique<CompositeRewardStrategy>();

    // Balanced multi-task: navigation + exploration
    composite->addStrategy(std::make_shared<NavigationRewardStrategy>(), 0.5f);
    composite->addStrategy(std::make_shared<ExplorationRewardStrategy>(), 0.3f);
    composite->addStrategy(std::make_shared<CollisionRewardStrategy>(), 1.0f);
    composite->addStrategy(std::make_shared<TimeRewardStrategy>(), 0.1f);

    return composite;
}

}  // namespace warehouser
