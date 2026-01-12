#pragma once

#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <std_msgs/msg/bool.hpp>

#include "warehouser_msgs/msg/observation.hpp"
#include "warehouser_msgs/msg/action.hpp"
#include "warehouser_msgs/srv/load_model.hpp"

#include "warehouser_inference/policy_inference.hpp"

namespace warehouser_inference {

class InferenceNode : public rclcpp::Node {
public:
    explicit InferenceNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
    void observationCallback(const warehouser_msgs::msg::Observation::SharedPtr msg);
    void enableCallback(const std_msgs::msg::Bool::SharedPtr msg);
    void loadModelService(
        const std::shared_ptr<warehouser_msgs::srv::LoadModel::Request> request,
        std::shared_ptr<warehouser_msgs::srv::LoadModel::Response> response);
    void inferenceLoop();

    PolicyInference policy_;
    bool enabled_{false};
    warehouser_msgs::msg::Observation::SharedPtr last_observation_;

    // Parameters
    float v_max_{1.0f};
    float omega_max_{2.0f};
    std::string default_model_path_;

    // ROS interfaces
    rclcpp::Subscription<warehouser_msgs::msg::Observation>::SharedPtr obs_sub_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr enable_sub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
    rclcpp::Publisher<warehouser_msgs::msg::Action>::SharedPtr action_pub_;
    rclcpp::Service<warehouser_msgs::srv::LoadModel>::SharedPtr load_model_srv_;
    rclcpp::TimerBase::SharedPtr inference_timer_;
};

}  // namespace warehouser_inference
