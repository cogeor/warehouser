#include "warehouser_rl_bridge/rl_bridge_node.hpp"

#include <chrono>

using namespace std::chrono_literals;

namespace warehouser {

RLBridgeNode::RLBridgeNode(const rclcpp::NodeOptions& options)
    : Node("rl_bridge", options) {
    // Declare parameters
    max_steps_ = declare_parameter("max_steps", 500);
    float progress_weight = declare_parameter("progress_weight", 1.0);
    float collision_penalty = declare_parameter("collision_penalty", -100.0);
    float success_bonus = declare_parameter("success_bonus", 100.0);
    float pickup_bonus = declare_parameter("pickup_bonus", 50.0);
    float time_penalty = declare_parameter("time_penalty", -0.1);
    float goal_threshold = declare_parameter("goal_threshold", 0.5);

    // Store reward config for creating per-robot calculators later
    RewardConfig reward_config;
    reward_config.progress_weight = static_cast<float>(progress_weight);
    reward_config.collision_penalty = static_cast<float>(collision_penalty);
    reward_config.success_bonus = static_cast<float>(success_bonus);
    reward_config.pickup_bonus = static_cast<float>(pickup_bonus);
    reward_config.time_penalty = static_cast<float>(time_penalty);
    reward_config.goal_threshold = static_cast<float>(goal_threshold);

    // Initialize with single robot by default (backward compatible)
    robot_count_ = 1;
    prev_world_states_.resize(1);
    reward_calculators_.emplace_back(reward_config);

    // Initialize RNG
    std::random_device rd;
    rng_.seed(rd());

    // Create subscribers
    world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
        "/world/state", 10,
        std::bind(&RLBridgeNode::worldStateCallback, this,
                  std::placeholders::_1));

    goal_sub_ = create_subscription<warehouser_msgs::msg::Goal>(
        "/task/goal", 10,
        std::bind(&RLBridgeNode::goalCallback, this, std::placeholders::_1));

    // Create publishers
    // Initialize per-robot publishers for default single robot
    initializeRobotPublishers(1);
    goal_pub_ = create_publisher<warehouser_msgs::msg::Goal>("/task/goal", 10);

    // Create service clients
    reset_client_ = create_client<warehouser_msgs::srv::SimReset>("/sim/reset");
    step_client_ = create_client<warehouser_msgs::srv::SimStep>("/sim/step");
    obs_client_ =
        create_client<warehouser_msgs::srv::GetObservation>("/observations/get");

    // Create service servers
    step_srv_ = create_service<warehouser_msgs::srv::RLStep>(
        "/rl/step",
        std::bind(&RLBridgeNode::handleRLStep, this, std::placeholders::_1,
                  std::placeholders::_2));

    reset_srv_ = create_service<warehouser_msgs::srv::RLReset>(
        "/rl/reset",
        std::bind(&RLBridgeNode::handleRLReset, this, std::placeholders::_1,
                  std::placeholders::_2));

    RCLCPP_INFO(get_logger(), "RL Bridge node initialized (max_steps=%d)",
                max_steps_);
}

void RLBridgeNode::worldStateCallback(
    const warehouser_msgs::msg::WorldState::SharedPtr msg) {
    curr_world_ = *msg;
    world_received_ = true;
}

void RLBridgeNode::goalCallback(
    const warehouser_msgs::msg::Goal::SharedPtr msg) {
    current_goal_ = *msg;
}

void RLBridgeNode::handleRLStep(
    const warehouser_msgs::srv::RLStep::Request::SharedPtr request,
    warehouser_msgs::srv::RLStep::Response::SharedPtr response) {
    // Validate robot_id
    size_t robot_id = static_cast<size_t>(std::max(0, request->robot_id));
    if (robot_id >= robot_count_) {
        response->robot_id = request->robot_id;
        response->reward = 0.0f;
        response->terminated = true;
        response->truncated = false;
        response->info = "{\"error\": \"Invalid robot_id: " +
                         std::to_string(robot_id) + ", robot_count: " +
                         std::to_string(robot_count_) + "\"}";
        return;
    }

    // Store previous state for this robot
    prev_world_states_[robot_id] = curr_world_;

    // Send action to specific robot
    sendAction(robot_id, request->action_linear, request->action_angular,
               request->action_pick, request->action_place);

    // Step simulation
    int num_steps = request->num_steps > 0 ? request->num_steps : 1;
    stepSimulation(num_steps);
    step_count_++;

    // Calculate reward for this robot
    auto reward_result =
        reward_calculators_[robot_id].calculate(
            prev_world_states_[robot_id], curr_world_, current_goal_,
            step_count_, max_steps_);

    // Get observation for this robot
    response->robot_id = request->robot_id;
    response->observation = getObservation(robot_id);
    response->reward = reward_result.reward;
    response->terminated = reward_result.terminated;
    response->truncated = reward_result.truncated;
    response->info = "{\"step\": " + std::to_string(step_count_) +
                     ", \"robot_id\": " + std::to_string(robot_id) +
                     ", \"reason\": \"" + reward_result.termination_reason +
                     "\"}";
}

void RLBridgeNode::handleRLReset(
    const warehouser_msgs::srv::RLReset::Request::SharedPtr request,
    warehouser_msgs::srv::RLReset::Response::SharedPtr response) {
    // Seed RNG if provided
    if (request->seed != 0) {
        rng_.seed(static_cast<unsigned int>(request->seed));
    }

    // Configure robot count (minimum 1 for backward compatibility)
    size_t new_robot_count = static_cast<size_t>(std::max(1, request->robot_count));

    // Initialize per-robot publishers if count changed
    if (new_robot_count != robot_count_) {
        initializeRobotPublishers(new_robot_count);
    }
    robot_count_ = new_robot_count;

    // Reset simulation with N robots
    resetSimulation(static_cast<int>(robot_count_));

    // Reset episode state
    step_count_ = 0;

    // Initialize per-robot state
    prev_world_states_.resize(robot_count_);
    if (reward_calculators_.size() < robot_count_) {
        // Get config from first calculator and create more
        RewardConfig config = reward_calculators_[0].config();
        while (reward_calculators_.size() < robot_count_) {
            reward_calculators_.emplace_back(config);
        }
    }

    // Set random goal
    setRandomGoal();

    // Wait for world state update
    rclcpp::Rate rate(100);
    for (int i = 0; i < 50 && rclcpp::ok(); ++i) {
        rclcpp::spin_some(shared_from_this());
        rate.sleep();
        if (world_received_) break;
    }

    // Store initial world state for all robots
    for (size_t i = 0; i < robot_count_; ++i) {
        prev_world_states_[i] = curr_world_;
    }

    // Build per-robot observations
    response->success = true;
    response->robot_count = static_cast<int32_t>(robot_count_);
    response->observations.resize(robot_count_);
    for (size_t i = 0; i < robot_count_; ++i) {
        response->observations[i] = getObservation(i);
    }

    // Legacy: first robot observation for backward compatibility
    response->observation = response->observations.empty() ?
        warehouser_msgs::msg::Observation() : response->observations[0];

    response->info = "{\"target_color\": \"" + current_goal_.target_color +
                     "\", \"robot_count\": " + std::to_string(robot_count_) + "}";
}

void RLBridgeNode::initializeRobotPublishers(size_t count) {
    // Clear existing publishers
    cmd_vel_pubs_.clear();
    pick_pubs_.clear();
    unpick_pubs_.clear();

    // Create per-robot publishers
    for (size_t i = 0; i < count; ++i) {
        std::string prefix = "/robot" + std::to_string(i);

        cmd_vel_pubs_.push_back(
            create_publisher<geometry_msgs::msg::Twist>(prefix + "/cmd_vel", 10));
        pick_pubs_.push_back(
            create_publisher<std_msgs::msg::Empty>(prefix + "/sim/pick", 10));
        unpick_pubs_.push_back(
            create_publisher<std_msgs::msg::Empty>(prefix + "/sim/unpick", 10));
    }

    RCLCPP_INFO(get_logger(), "Initialized publishers for %zu robots", count);
}

void RLBridgeNode::sendAction(size_t robot_id, float linear, float angular,
                               float pick, float place) {
    // Validate robot_id against publisher count
    if (robot_id >= cmd_vel_pubs_.size()) {
        RCLCPP_WARN(get_logger(),
            "Invalid robot_id %zu for sendAction (publisher count: %zu)",
            robot_id, cmd_vel_pubs_.size());
        return;
    }

    // Velocity command to per-robot topic
    geometry_msgs::msg::Twist cmd;
    cmd.linear.x = linear;
    cmd.angular.z = angular;
    cmd_vel_pubs_[robot_id]->publish(cmd);

    // Pick action (threshold at 0.5)
    if (pick > 0.5f) {
        pick_pubs_[robot_id]->publish(std_msgs::msg::Empty());
    }

    // Place action (threshold at 0.5)
    if (place > 0.5f) {
        unpick_pubs_[robot_id]->publish(std_msgs::msg::Empty());
    }
}

void RLBridgeNode::stepSimulation(int num_steps) {
    if (!step_client_->wait_for_service(1s)) {
        RCLCPP_WARN(get_logger(), "SimStep service not available");
        return;
    }

    auto request = std::make_shared<warehouser_msgs::srv::SimStep::Request>();
    request->num_ticks = num_steps;

    auto future = step_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(shared_from_this(), future, 1s) ==
        rclcpp::FutureReturnCode::SUCCESS) {
        auto result = future.get();
        curr_world_ = result->state;
    }
}

void RLBridgeNode::resetSimulation(int robot_count) {
    if (!reset_client_->wait_for_service(1s)) {
        RCLCPP_WARN(get_logger(), "Reset service not available");
        return;
    }

    // Use SimReset service to spawn requested number of robots
    auto request = std::make_shared<warehouser_msgs::srv::SimReset::Request>();
    request->robot_count = robot_count;

    auto future = reset_client_->async_send_request(request);
    if (rclcpp::spin_until_future_complete(shared_from_this(), future, 1s) ==
        rclcpp::FutureReturnCode::SUCCESS) {
        auto result = future.get();
        if (result->success) {
            RCLCPP_INFO(get_logger(), "Simulation reset with %d robots",
                        result->actual_robot_count);
        } else {
            RCLCPP_WARN(get_logger(), "Simulation reset failed: %s",
                        result->message.c_str());
        }
    }
}

warehouser_msgs::msg::Observation RLBridgeNode::getObservation(size_t robot_id) {
    if (!obs_client_->wait_for_service(1s)) {
        RCLCPP_WARN(get_logger(), "GetObservation service not available");
        return warehouser_msgs::msg::Observation();
    }

    auto request =
        std::make_shared<warehouser_msgs::srv::GetObservation::Request>();
    // TODO: Add robot_id to GetObservation.srv when per-robot obs service is available
    // For now, robot_id is used locally to build observation from curr_world_
    (void)robot_id;

    auto future = obs_client_->async_send_request(request);

    if (rclcpp::spin_until_future_complete(shared_from_this(), future, 1s) ==
        rclcpp::FutureReturnCode::SUCCESS) {
        return future.get()->observation;
    }

    return warehouser_msgs::msg::Observation();
}

void RLBridgeNode::setRandomGoal() {
    // Pick random color
    std::uniform_int_distribution<size_t> dist(0, target_colors_.size() - 1);
    std::string color = target_colors_[dist(rng_)];

    // Find object of that color and set as goal
    for (const auto& entity : curr_world_.entities) {
        if (entity.type == 1 && entity.color == color && !entity.is_picked) {
            current_goal_.x = entity.x;
            current_goal_.y = entity.y;
            current_goal_.target_color = color;
            current_goal_.active = true;
            goal_pub_->publish(current_goal_);
            return;
        }
    }

    // Fallback: set any unpicked object as goal
    for (const auto& entity : curr_world_.entities) {
        if (entity.type == 1 && !entity.is_picked) {
            current_goal_.x = entity.x;
            current_goal_.y = entity.y;
            current_goal_.target_color = entity.color;
            current_goal_.active = true;
            goal_pub_->publish(current_goal_);
            return;
        }
    }
}

}  // namespace warehouser

// Main entry point
int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<warehouser::RLBridgeNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
