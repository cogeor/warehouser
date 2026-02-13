#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rosgraph_msgs/msg/clock.hpp>
#include <std_msgs/msg/empty.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>

#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_msgs/srv/sim_reset.hpp"
#include "warehouser_msgs/srv/sim_step.hpp"
#include "warehouser_simulation/world_manager.hpp"

namespace warehouser {

/// Main simulation ROS2 node.
/// Manages world state, processes commands, and publishes state updates.
class SimulationNode : public rclcpp::Node {
public:
    explicit SimulationNode(
        const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
    // Timer callback for simulation loop
    void tick();

    // Subscriber callbacks
    void cmdVelCallback(const geometry_msgs::msg::Twist::SharedPtr msg);
    void moveEntityCallback(const std_msgs::msg::String::SharedPtr msg);
    void pickCallback(const std_msgs::msg::Empty::SharedPtr msg);
    void unpickCallback(const std_msgs::msg::Empty::SharedPtr msg);

    // Service callbacks
    void handleStart(const std_srvs::srv::Trigger::Request::SharedPtr request,
                     std_srvs::srv::Trigger::Response::SharedPtr response);
    void handlePause(const std_srvs::srv::Trigger::Request::SharedPtr request,
                     std_srvs::srv::Trigger::Response::SharedPtr response);
    void handleReset(
        const warehouser_msgs::srv::SimReset::Request::SharedPtr request,
        warehouser_msgs::srv::SimReset::Response::SharedPtr response);
    void handleStep(
        const warehouser_msgs::srv::SimStep::Request::SharedPtr request,
        warehouser_msgs::srv::SimStep::Response::SharedPtr response);

    // World state
    WorldManager world_;
    float dt_ = 0.02f;  // 50 Hz default

    // ROS interfaces
    rclcpp::TimerBase::SharedPtr timer_;

    // Subscribers
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr move_sub_;
    rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr pick_sub_;
    rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr unpick_sub_;

    // Publishers
    rclcpp::Publisher<warehouser_msgs::msg::WorldState>::SharedPtr state_pub_;
    rclcpp::Publisher<rosgraph_msgs::msg::Clock>::SharedPtr clock_pub_;

    // Services
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_srv_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr pause_srv_;
    rclcpp::Service<warehouser_msgs::srv::SimReset>::SharedPtr reset_srv_;
    rclcpp::Service<warehouser_msgs::srv::SimStep>::SharedPtr step_srv_;
};

}  // namespace warehouser
