#pragma once

#include <memory>
#include <random>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/empty.hpp>
#include <std_srvs/srv/trigger.hpp>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/observation.hpp"
#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_msgs/srv/get_observation.hpp"
#include "warehouser_msgs/srv/rl_reset.hpp"
#include "warehouser_msgs/srv/rl_step.hpp"
#include "warehouser_msgs/srv/sim_step.hpp"
#include "warehouser_rl_bridge/reward_calculator.hpp"

namespace warehouser {

/// ROS2 node that provides RL training interface.
/// Wraps simulation step/reset with reward calculation and observations.
class RLBridgeNode : public rclcpp::Node {
public:
    explicit RLBridgeNode(
        const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
    // Subscriber callbacks
    void worldStateCallback(
        const warehouser_msgs::msg::WorldState::SharedPtr msg);
    void goalCallback(const warehouser_msgs::msg::Goal::SharedPtr msg);

    // Service callbacks
    void handleRLStep(
        const warehouser_msgs::srv::RLStep::Request::SharedPtr request,
        warehouser_msgs::srv::RLStep::Response::SharedPtr response);
    void handleRLReset(
        const warehouser_msgs::srv::RLReset::Request::SharedPtr request,
        warehouser_msgs::srv::RLReset::Response::SharedPtr response);

    // Helper methods
    void sendAction(float linear, float angular, float pick, float place);
    void stepSimulation(int num_steps);
    void resetSimulation();
    warehouser_msgs::msg::Observation getObservation();
    void setRandomGoal();

    // Reward calculator
    RewardCalculator reward_calc_;

    // Episode state
    warehouser_msgs::msg::WorldState prev_world_;
    warehouser_msgs::msg::WorldState curr_world_;
    warehouser_msgs::msg::Goal current_goal_;
    int step_count_ = 0;
    int max_steps_ = 500;
    bool world_received_ = false;

    // Random number generator for goal selection
    std::mt19937 rng_;
    std::vector<std::string> target_colors_ = {"red", "green", "blue"};

    // ROS interfaces
    // Subscribers
    rclcpp::Subscription<warehouser_msgs::msg::WorldState>::SharedPtr world_sub_;
    rclcpp::Subscription<warehouser_msgs::msg::Goal>::SharedPtr goal_sub_;

    // Publishers
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
    rclcpp::Publisher<std_msgs::msg::Empty>::SharedPtr pick_pub_;
    rclcpp::Publisher<std_msgs::msg::Empty>::SharedPtr unpick_pub_;
    rclcpp::Publisher<warehouser_msgs::msg::Goal>::SharedPtr goal_pub_;

    // Service clients
    rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr reset_client_;
    rclcpp::Client<warehouser_msgs::srv::SimStep>::SharedPtr step_client_;
    rclcpp::Client<warehouser_msgs::srv::GetObservation>::SharedPtr obs_client_;

    // Service servers
    rclcpp::Service<warehouser_msgs::srv::RLStep>::SharedPtr step_srv_;
    rclcpp::Service<warehouser_msgs::srv::RLReset>::SharedPtr reset_srv_;
};

}  // namespace warehouser
