#include "warehouser_safety/safety_node.hpp"

#include <chrono>
#include <sstream>

namespace warehouser_safety {

using namespace std::chrono_literals;

SafetyNode::SafetyNode(const rclcpp::NodeOptions& options)
    : Node("safety", options) {

    // Declare parameters
    SafetyConfig config;
    config.min_distance = declare_parameter("min_distance", 0.3);
    config.slowdown_distance = declare_parameter("slowdown_distance", 0.8);
    config.max_linear_vel = declare_parameter("max_linear_vel", 1.0);
    config.max_angular_vel = declare_parameter("max_angular_vel", 2.0);

    controller_.setConfig(config);

    // Subscribers
    cmd_raw_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "/cmd_vel_raw", 10,
        std::bind(&SafetyNode::cmdRawCallback, this, std::placeholders::_1));

    lidar_sub_ = create_subscription<warehouser_msgs::msg::LidarDebug>(
        "/observations/lidar_debug", 10,
        std::bind(&SafetyNode::lidarCallback, this, std::placeholders::_1));

    // Publishers
    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    status_pub_ = create_publisher<std_msgs::msg::String>("/safety/status", 10);

    // Status timer
    status_timer_ = create_wall_timer(
        100ms, std::bind(&SafetyNode::statusTimerCallback, this));

    RCLCPP_INFO(get_logger(), "Safety node initialized");
}

void SafetyNode::cmdRawCallback(const geometry_msgs::msg::Twist::SharedPtr msg) {
    Velocity cmd_raw{
        static_cast<float>(msg->linear.x),
        static_cast<float>(msg->angular.z)
    };

    Velocity cmd_safe;

    if (lidar_received_) {
        cmd_safe = controller_.applySafetyLimits(cmd_raw, last_lidar_);
    } else {
        // No lidar data - pass through but log warning
        cmd_safe = cmd_raw;
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                             "No lidar data received - safety checks disabled");
    }

    // Publish safe command
    geometry_msgs::msg::Twist cmd_msg;
    cmd_msg.linear.x = cmd_safe.linear;
    cmd_msg.angular.z = cmd_safe.angular;
    cmd_pub_->publish(cmd_msg);
}

void SafetyNode::lidarCallback(const warehouser_msgs::msg::LidarDebug::SharedPtr msg) {
    last_lidar_.ranges = msg->ranges;
    last_lidar_.angle_min = msg->angle_min;
    last_lidar_.angle_max = msg->angle_max;
    lidar_received_ = true;
}

void SafetyNode::statusTimerCallback() {
    std_msgs::msg::String status_msg;
    std::ostringstream ss;

    const char* state_str = "UNKNOWN";
    switch (controller_.getState()) {
        case SafetyState::NOMINAL:   state_str = "NOMINAL"; break;
        case SafetyState::SLOWDOWN:  state_str = "SLOWDOWN"; break;
        case SafetyState::EMERGENCY: state_str = "EMERGENCY"; break;
        case SafetyState::STOPPED:   state_str = "STOPPED"; break;
    }

    ss << "state:" << state_str
       << ",min_dist:" << controller_.getMinObstacleDistance();

    status_msg.data = ss.str();
    status_pub_->publish(status_msg);
}

}  // namespace warehouser_safety
