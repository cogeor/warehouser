#pragma once

#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <std_msgs/msg/string.hpp>

#include "warehouser_msgs/msg/lidar_debug.hpp"

#include "warehouser_safety/safety_controller.hpp"

namespace warehouser_safety {

class SafetyNode : public rclcpp::Node {
public:
    explicit SafetyNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
    void cmdRawCallback(const geometry_msgs::msg::Twist::SharedPtr msg);
    void lidarCallback(const warehouser_msgs::msg::LidarDebug::SharedPtr msg);
    void statusTimerCallback();

    SafetyController controller_;
    LidarData last_lidar_;
    bool lidar_received_{false};

    // ROS interfaces
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_raw_sub_;
    rclcpp::Subscription<warehouser_msgs::msg::LidarDebug>::SharedPtr lidar_sub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
    rclcpp::TimerBase::SharedPtr status_timer_;
};

}  // namespace warehouser_safety
