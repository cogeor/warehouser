#pragma once

#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_msgs/msg/goal.hpp"

#include "warehouser_command/command_parser.hpp"
#include "warehouser_command/object_resolver.hpp"

namespace warehouser_command {

class CommandNode : public rclcpp::Node {
public:
    explicit CommandNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
    void commandCallback(const std_msgs::msg::String::SharedPtr msg);
    void worldStateCallback(const warehouser_msgs::msg::WorldState::SharedPtr msg);

    void executeCommand(const Command& cmd);

    CommandParser parser_;
    ObjectResolver object_resolver_;
    ZoneResolver zone_resolver_;

    // ROS interfaces
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr cmd_sub_;
    rclcpp::Subscription<warehouser_msgs::msg::WorldState>::SharedPtr world_sub_;

    rclcpp::Publisher<warehouser_msgs::msg::Goal>::SharedPtr goal_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr response_pub_;
};

}  // namespace warehouser_command
