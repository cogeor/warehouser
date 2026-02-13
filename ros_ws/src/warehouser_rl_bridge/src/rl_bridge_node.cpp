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

    // Velocity limit parameters
    // Actions from RL are normalized [-1, 1], scaled by these limits:
    //   linear_vel = action_linear * v_max
    //   angular_vel = action_angular * omega_max
    v_max_ = static_cast<float>(declare_parameter("v_max", 1.0));
    omega_max_ = static_cast<float>(declare_parameter("omega_max", 2.0));

    // Safety controller configuration
    safety_config_.max_linear_vel = v_max_;
    safety_config_.max_angular_vel = omega_max_;
    safety_config_.min_distance = static_cast<float>(
        declare_parameter("safety_min_distance", 0.3));
    safety_config_.slowdown_distance = static_cast<float>(
        declare_parameter("safety_slowdown_distance", 0.8));
    safety_controller_.setConfig(safety_config_);

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

    // Action feedback: safety state from safety controller
    response->safety_state = static_cast<uint8_t>(safety_controller_.getState());

    // Action feedback: determine pick/place success and carrying state
    // Find robot entity in current world state
    bool is_carrying = false;
    bool prev_carrying = false;
    for (const auto& entity : curr_world_.entities) {
        if (entity.type == 0 && entity.id == "robot" + std::to_string(robot_id)) {
            is_carrying = entity.is_carrying;
            break;
        }
    }
    for (const auto& entity : prev_world_states_[robot_id].entities) {
        if (entity.type == 0 && entity.id == "robot" + std::to_string(robot_id)) {
            prev_carrying = entity.is_carrying;
            break;
        }
    }

    // Pick succeeded if action requested and now carrying (wasn't before)
    response->pick_success = (request->action_pick > 0.5f) &&
                             is_carrying && !prev_carrying;

    // Place succeeded if action requested and no longer carrying (was before)
    response->place_success = (request->action_place > 0.5f) &&
                              !is_carrying && prev_carrying;

    response->is_carrying = is_carrying;
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
    // ==========================================================================
    // ACTION PROCESSING PIPELINE
    // ==========================================================================
    // This function transforms RL policy outputs into safe robot commands.
    //
    // Input: Normalized actions from policy network (all in [-1, 1] range)
    //   - linear:  Normalized linear velocity command
    //   - angular: Normalized angular velocity command
    //   - pick:    Continuous signal for pick action (discrete at threshold)
    //   - place:   Continuous signal for place action (discrete at threshold)
    //
    // Processing stages:
    //   1. Velocity Scaling: Convert normalized [-1, 1] to physical units
    //   2. Safety Limiting:  Apply obstacle avoidance via SafetyController
    //   3. Discrete Actions: Threshold continuous signals for pick/place
    //
    // Output: Commands published to robot topics
    //   - /robot{N}/cmd_vel: geometry_msgs::Twist
    //   - /robot{N}/sim/pick: std_msgs::Empty (if triggered)
    //   - /robot{N}/sim/unpick: std_msgs::Empty (if triggered)
    // ==========================================================================

    // Validate robot_id against publisher count
    if (robot_id >= cmd_vel_pubs_.size()) {
        RCLCPP_WARN(get_logger(),
            "Invalid robot_id %zu for sendAction (publisher count: %zu)",
            robot_id, cmd_vel_pubs_.size());
        return;
    }

    // -------------------------------------------------------------------------
    // STAGE 1: VELOCITY SCALING
    // -------------------------------------------------------------------------
    // Scale normalized actions [-1, 1] to physical velocity limits.
    // The policy outputs normalized values for stable training; we convert
    // these to real-world velocity commands using configured max velocities.
    //
    // Formula:
    //   linear_vel  = action_linear  * v_max     (m/s)
    //   angular_vel = action_angular * omega_max (rad/s)
    //
    // Note: Python wrappers (ActionScalingWrapper) may also perform scaling,
    // but this ensures proper scaling even for direct service calls.
    // -------------------------------------------------------------------------
    float scaled_linear = linear * v_max_;
    float scaled_angular = angular * omega_max_;

    // -------------------------------------------------------------------------
    // STAGE 2: SAFETY LIMITING (SafetyController)
    // -------------------------------------------------------------------------
    // Apply lidar-based obstacle avoidance to prevent collisions.
    // The SafetyController operates in several states:
    //
    //   - NOMINAL:   No nearby obstacles, full speed allowed
    //   - SLOWDOWN:  Obstacle in slowdown_distance, velocity scaled down
    //   - EMERGENCY: Obstacle within min_distance, forward motion stopped
    //   - STOPPED:   Complete stop for safety
    //
    // Safety scaling is applied based on minimum obstacle distance:
    //   scale = (distance - min_distance) / (slowdown_distance - min_distance)
    //   safe_vel = raw_vel * clamp(scale, 0, 1)
    //
    // TODO: Subscribe to /robot{N}/lidar for per-robot lidar data.
    // Currently using empty lidar data which defaults to NOMINAL state.
    // -------------------------------------------------------------------------
    warehouser_safety::LidarData lidar_data;
    lidar_data.angle_min = -1.57f;  // -90 degrees (front-left)
    lidar_data.angle_max = 1.57f;   // +90 degrees (front-right)

    warehouser_safety::Velocity raw_vel{scaled_linear, scaled_angular};
    auto safe_vel = safety_controller_.applySafetyLimits(raw_vel, lidar_data);

    // Publish velocity command to per-robot topic
    geometry_msgs::msg::Twist cmd;
    cmd.linear.x = safe_vel.linear;
    cmd.angular.z = safe_vel.angular;
    cmd_vel_pubs_[robot_id]->publish(cmd);

    // -------------------------------------------------------------------------
    // STAGE 3: DISCRETE ACTION TRIGGERING
    // -------------------------------------------------------------------------
    // Convert continuous pick/place signals to discrete actions.
    // The policy outputs continuous signals in [-1, 1], but pick/place are
    // inherently discrete operations (you either do them or you don't).
    //
    // Threshold: 0.5
    //   - signal > 0.5:  Action is triggered
    //   - signal <= 0.5: Action is not triggered
    //
    // This allows the policy to express confidence in its discrete decisions
    // while maintaining a differentiable output during training.
    //
    // Note: Action masking happens in ros_env.py BEFORE this function:
    //   - If carrying:     pick signal is masked to 0 (can't pick again)
    //   - If not carrying: place signal is masked to 0 (can't place nothing)
    // -------------------------------------------------------------------------
    if (pick > 0.5f) {
        pick_pubs_[robot_id]->publish(std_msgs::msg::Empty());
    }

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
