#include "warehouser_inference/inference_node.hpp"

#include <chrono>

namespace warehouser_inference {

using namespace std::chrono_literals;

InferenceNode::InferenceNode(const rclcpp::NodeOptions& options)
    : Node("inference", options) {

    // Declare parameters
    v_max_ = declare_parameter("v_max", 1.0);
    omega_max_ = declare_parameter("omega_max", 2.0);
    default_model_path_ = declare_parameter("default_model_path", "");
    auto inference_rate = declare_parameter("inference_rate", 20.0);

    // Subscribers
    obs_sub_ = create_subscription<warehouser_msgs::msg::Observation>(
        "/observations", 10,
        std::bind(&InferenceNode::observationCallback, this, std::placeholders::_1));

    enable_sub_ = create_subscription<std_msgs::msg::Bool>(
        "/inference/enable", 10,
        std::bind(&InferenceNode::enableCallback, this, std::placeholders::_1));

    // Publishers
    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("/cmd_vel_raw", 10);
    action_pub_ = create_publisher<warehouser_msgs::msg::Action>("/inference/action", 10);

    // Services
    load_model_srv_ = create_service<warehouser_msgs::srv::LoadModel>(
        "/inference/load_model",
        std::bind(&InferenceNode::loadModelService, this,
                  std::placeholders::_1, std::placeholders::_2));

    // Inference timer
    auto period = std::chrono::duration<double>(1.0 / inference_rate);
    inference_timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&InferenceNode::inferenceLoop, this));

    // Load default model if specified
    if (!default_model_path_.empty()) {
        auto result = policy_.loadModel(default_model_path_);
        if (result) {
            RCLCPP_INFO(get_logger(), "Loaded default model: %s", default_model_path_.c_str());
        } else {
            RCLCPP_WARN(get_logger(), "Failed to load default model: %s", result.error().c_str());
        }
    }

    RCLCPP_INFO(get_logger(), "Inference node initialized (rate: %.1f Hz)", inference_rate);
}

void InferenceNode::observationCallback(const warehouser_msgs::msg::Observation::SharedPtr msg) {
    last_observation_ = msg;
}

void InferenceNode::enableCallback(const std_msgs::msg::Bool::SharedPtr msg) {
    enabled_ = msg->data;
    RCLCPP_INFO(get_logger(), "Inference %s", enabled_ ? "enabled" : "disabled");
}

void InferenceNode::loadModelService(
    const std::shared_ptr<warehouser_msgs::srv::LoadModel::Request> request,
    std::shared_ptr<warehouser_msgs::srv::LoadModel::Response> response) {

    RCLCPP_INFO(get_logger(), "Loading model: %s", request->model_path.c_str());

    auto result = policy_.loadModel(request->model_path);
    if (result) {
        response->success = true;
        response->message = "Model loaded successfully";
        RCLCPP_INFO(get_logger(), "Model loaded: obs_dim=%ld, action_dim=%ld",
                    policy_.getModelInfo().obs_dim, policy_.getModelInfo().action_dim);
    } else {
        response->success = false;
        response->message = result.error();
        RCLCPP_ERROR(get_logger(), "Failed to load model: %s", result.error().c_str());
    }
}

void InferenceNode::inferenceLoop() {
    // Check if we should run inference
    if (!enabled_ || !policy_.isLoaded() || !last_observation_) {
        return;
    }

    // Run inference
    auto result = policy_.infer(last_observation_->data);
    if (!result) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
                             "Inference failed: %s", result.error().c_str());
        return;
    }

    const auto& action = *result;

    // Publish velocity command
    geometry_msgs::msg::Twist cmd;
    cmd.linear.x = action.linear * v_max_;
    cmd.angular.z = action.angular * omega_max_;
    cmd_pub_->publish(cmd);

    // Publish full action
    warehouser_msgs::msg::Action action_msg;
    action_msg.linear = action.linear;
    action_msg.angular = action.angular;
    action_msg.pick = action.pick > 0.5f;
    action_msg.place = action.place > 0.5f;
    action_pub_->publish(action_msg);
}

}  // namespace warehouser_inference
