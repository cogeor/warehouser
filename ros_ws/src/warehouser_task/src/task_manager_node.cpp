#include "warehouser_task/task_manager_node.hpp"

#include <chrono>
#include <cmath>

namespace warehouser_task {

using namespace std::chrono_literals;

TaskManagerNode::TaskManagerNode(const rclcpp::NodeOptions& options)
    : Node("task_manager", options) {

    // Parameters
    pickup_radius_ = declare_parameter("pickup_radius", 0.5);
    place_radius_ = declare_parameter("place_radius", 0.5);

    // Set up state change callback
    state_machine_.setStateChangeCallback(
        std::bind(&TaskManagerNode::onStateChange, this,
                  std::placeholders::_1, std::placeholders::_2));

    // Subscribers
    world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
        "/world/state", 10,
        std::bind(&TaskManagerNode::worldStateCallback, this, std::placeholders::_1));

    goal_sub_ = create_subscription<warehouser_msgs::msg::Goal>(
        "/task/goal_input", 10,
        std::bind(&TaskManagerNode::goalCallback, this, std::placeholders::_1));

    action_sub_ = create_subscription<warehouser_msgs::msg::Action>(
        "/inference/action", 10,
        std::bind(&TaskManagerNode::actionCallback, this, std::placeholders::_1));

    // Publishers
    goal_pub_ = create_publisher<warehouser_msgs::msg::Goal>("/task/goal", 10);
    status_pub_ = create_publisher<warehouser_msgs::msg::TaskStatus>("/task/status", 10);
    enable_pub_ = create_publisher<std_msgs::msg::Bool>("/inference/enable", 10);

    // Services
    cancel_srv_ = create_service<std_srvs::srv::Trigger>(
        "/task/cancel",
        std::bind(&TaskManagerNode::cancelService, this,
                  std::placeholders::_1, std::placeholders::_2));

    // Status timer
    status_timer_ = create_wall_timer(
        500ms, std::bind(&TaskManagerNode::statusTimerCallback, this));

    RCLCPP_INFO(get_logger(), "Task manager initialized");
}

void TaskManagerNode::worldStateCallback(
    const warehouser_msgs::msg::WorldState::SharedPtr msg) {

    // Extract robot state
    for (const auto& entity : msg->entities) {
        if (entity.type == 0) {  // Robot
            robot_x_ = entity.x;
            robot_y_ = entity.y;
            robot_is_carrying_ = entity.is_carrying;
            break;
        }
    }

    // Check proximity for state transitions
    auto state = state_machine_.getState();
    auto task = state_machine_.getTask();

    if (!task) return;

    float dist = distanceToGoal();

    if (state == TaskState::NAVIGATING_TO_PICK) {
        if (dist < pickup_radius_) {
            state_machine_.handleEvent(TaskEvent::REACHED_OBJECT);
        }
    } else if (state == TaskState::NAVIGATING_TO_PLACE) {
        if (dist < place_radius_) {
            state_machine_.handleEvent(TaskEvent::REACHED_DESTINATION);
        }
    }
}

void TaskManagerNode::goalCallback(const warehouser_msgs::msg::Goal::SharedPtr msg) {
    // Create task from incoming goal
    Task task;
    task.task_id = std::to_string(now().nanoseconds());
    task.target_color = msg->target_color;
    task.object_x = msg->x;
    task.object_y = msg->y;
    task.pickup_radius = pickup_radius_;

    // Determine intent
    if (msg->target_color.empty()) {
        task.intent = "navigate";
        task.dest_x = msg->x;
        task.dest_y = msg->y;
    } else {
        task.intent = "pick";  // Just pick for MVP
        task.dest_x = msg->x;
        task.dest_y = msg->y;
    }

    // Set current goal
    current_goal_x_ = msg->x;
    current_goal_y_ = msg->y;

    // Start task
    state_machine_.setTask(task);
    state_machine_.handleEvent(TaskEvent::COMMAND_RECEIVED);

    RCLCPP_INFO(get_logger(), "Task started: %s to (%.2f, %.2f)",
                task.intent.c_str(), current_goal_x_, current_goal_y_);
}

void TaskManagerNode::actionCallback(const warehouser_msgs::msg::Action::SharedPtr msg) {
    auto state = state_machine_.getState();

    if (state == TaskState::PICKING && msg->pick) {
        // Check if robot is actually carrying now
        if (robot_is_carrying_) {
            state_machine_.handleEvent(TaskEvent::PICK_SUCCESS);
        }
    } else if (state == TaskState::PLACING && msg->place) {
        // Check if robot dropped the object
        if (!robot_is_carrying_) {
            state_machine_.handleEvent(TaskEvent::PLACE_SUCCESS);
        }
    }
}

void TaskManagerNode::cancelService(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> /*request*/,
    std::shared_ptr<std_srvs::srv::Trigger::Response> response) {

    state_machine_.handleEvent(TaskEvent::CANCEL_REQUESTED);
    response->success = true;
    response->message = "Task cancelled";

    RCLCPP_INFO(get_logger(), "Task cancelled by user");
}

void TaskManagerNode::onStateChange(TaskState old_state, TaskState new_state) {
    RCLCPP_INFO(get_logger(), "State: %s -> %s",
                TaskStateMachine::stateToString(old_state),
                TaskStateMachine::stateToString(new_state));

    // Enable/disable inference based on state
    bool should_enable = (new_state == TaskState::NAVIGATING_TO_PICK ||
                          new_state == TaskState::NAVIGATING_TO_PLACE ||
                          new_state == TaskState::PICKING ||
                          new_state == TaskState::PLACING);
    publishInferenceEnable(should_enable);

    // Update goal when transitioning to navigation
    if (new_state == TaskState::NAVIGATING_TO_PICK ||
        new_state == TaskState::NAVIGATING_TO_PLACE) {
        publishCurrentGoal();
    }

    // Clear task on terminal states
    if (new_state == TaskState::COMPLETED ||
        new_state == TaskState::FAILED ||
        new_state == TaskState::CANCELLED) {

        publishInferenceEnable(false);
    }
}

void TaskManagerNode::statusTimerCallback() {
    warehouser_msgs::msg::TaskStatus status_msg;
    status_msg.state = TaskStateMachine::stateToString(state_machine_.getState());

    auto task = state_machine_.getTask();
    if (task) {
        status_msg.task_id = task->task_id;
        status_msg.intent = task->intent;
        status_msg.target_color = task->target_color;
    }

    status_pub_->publish(status_msg);
}

void TaskManagerNode::publishCurrentGoal() {
    auto task = state_machine_.getTask();
    if (!task) return;

    warehouser_msgs::msg::Goal goal_msg;

    if (state_machine_.getState() == TaskState::NAVIGATING_TO_PICK) {
        goal_msg.x = task->object_x;
        goal_msg.y = task->object_y;
        current_goal_x_ = task->object_x;
        current_goal_y_ = task->object_y;
    } else {
        goal_msg.x = task->dest_x;
        goal_msg.y = task->dest_y;
        current_goal_x_ = task->dest_x;
        current_goal_y_ = task->dest_y;
    }

    goal_msg.target_color = task->target_color;
    goal_pub_->publish(goal_msg);
}

void TaskManagerNode::publishInferenceEnable(bool enable) {
    std_msgs::msg::Bool msg;
    msg.data = enable;
    enable_pub_->publish(msg);
}

float TaskManagerNode::distanceToGoal() const {
    float dx = current_goal_x_ - robot_x_;
    float dy = current_goal_y_ - robot_y_;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace warehouser_task
