#pragma once

#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>

#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/action.hpp"
#include "warehouser_msgs/msg/task_status.hpp"

#include "warehouser_task/task_state_machine.hpp"

namespace warehouser_task {

class TaskManagerNode : public rclcpp::Node {
public:
    explicit TaskManagerNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
    void worldStateCallback(const warehouser_msgs::msg::WorldState::SharedPtr msg);
    void goalCallback(const warehouser_msgs::msg::Goal::SharedPtr msg);
    void actionCallback(const warehouser_msgs::msg::Action::SharedPtr msg);

    void cancelService(
        const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response);

    void onStateChange(TaskState old_state, TaskState new_state);
    void statusTimerCallback();

    void publishCurrentGoal();
    void publishInferenceEnable(bool enable);

    float distanceToGoal() const;

    TaskStateMachine state_machine_;

    // Robot state from world
    float robot_x_{0.0f};
    float robot_y_{0.0f};
    bool robot_is_carrying_{false};

    // Current goal
    float current_goal_x_{0.0f};
    float current_goal_y_{0.0f};

    // Parameters
    float pickup_radius_{0.5f};
    float place_radius_{0.5f};

    // ROS interfaces
    rclcpp::Subscription<warehouser_msgs::msg::WorldState>::SharedPtr world_sub_;
    rclcpp::Subscription<warehouser_msgs::msg::Goal>::SharedPtr goal_sub_;
    rclcpp::Subscription<warehouser_msgs::msg::Action>::SharedPtr action_sub_;

    rclcpp::Publisher<warehouser_msgs::msg::Goal>::SharedPtr goal_pub_;
    rclcpp::Publisher<warehouser_msgs::msg::TaskStatus>::SharedPtr status_pub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr enable_pub_;

    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr cancel_srv_;
    rclcpp::TimerBase::SharedPtr status_timer_;
};

}  // namespace warehouser_task
