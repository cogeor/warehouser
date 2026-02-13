#include "warehouser_command/command_node.hpp"

namespace warehouser_command {

CommandNode::CommandNode(const rclcpp::NodeOptions& options)
    : Node("command", options) {

    // Subscribers
    cmd_sub_ = create_subscription<std_msgs::msg::String>(
        "/command/json", 10,
        std::bind(&CommandNode::commandCallback, this, std::placeholders::_1));

    world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
        "/world/state", 10,
        std::bind(&CommandNode::worldStateCallback, this, std::placeholders::_1));

    // Publishers
    goal_pub_ = create_publisher<warehouser_msgs::msg::Goal>("/task/goal_input", 10);
    response_pub_ = create_publisher<std_msgs::msg::String>("/command/response", 10);

    RCLCPP_INFO(get_logger(), "Command node initialized");
}

void CommandNode::commandCallback(const std_msgs::msg::String::SharedPtr msg) {
    RCLCPP_INFO(get_logger(), "Received command: %s", msg->data.c_str());

    std_msgs::msg::String response;

    auto result = parser_.parse(msg->data);
    if (!result) {
        response.data = "ERROR: " + result.error();
        response_pub_->publish(response);
        RCLCPP_WARN(get_logger(), "Parse error: %s", result.error().c_str());
        return;
    }

    try {
        executeCommand(*result);
        response.data = "OK: " + result->action;
    } catch (const std::exception& e) {
        response.data = "ERROR: " + std::string(e.what());
        RCLCPP_ERROR(get_logger(), "Execution error: %s", e.what());
    }

    response_pub_->publish(response);
}

void CommandNode::worldStateCallback(const warehouser_msgs::msg::WorldState::SharedPtr msg) {
    std::vector<ObjectInfo> objects;
    RobotInfo robot;

    for (const auto& entity : msg->entities) {
        if (entity.type == 0) {  // Robot
            robot.x = entity.x;
            robot.y = entity.y;
        } else if (entity.type == 1) {  // Object
            ObjectInfo obj;
            obj.id = entity.id;
            obj.color = entity.color;
            obj.x = entity.x;
            obj.y = entity.y;
            obj.is_picked = false;  // TODO: track from entity
            objects.push_back(obj);
        }
    }

    object_resolver_.updateObjects(objects);
    object_resolver_.updateRobot(robot);
}

void CommandNode::executeCommand(const Command& cmd) {
    warehouser_msgs::msg::Goal goal;

    if (cmd.action == "pick" || cmd.action == "pick_and_place") {
        // Resolve target object
        auto obj = object_resolver_.resolveByColor(cmd.target);
        if (!obj) {
            obj = object_resolver_.resolveById(cmd.target);
        }

        if (!obj) {
            throw std::runtime_error("No object found: " + cmd.target);
        }

        goal.x = obj->x;
        goal.y = obj->y;
        goal.target_color = obj->color;

        RCLCPP_INFO(get_logger(), "Resolved %s to object %s at (%.2f, %.2f)",
                    cmd.target.c_str(), obj->id.c_str(), obj->x, obj->y);

    } else if (cmd.action == "goto") {
        if (cmd.dest_x && cmd.dest_y) {
            goal.x = *cmd.dest_x;
            goal.y = *cmd.dest_y;
        } else if (!cmd.target.empty()) {
            auto zone = zone_resolver_.resolve(cmd.target);
            if (!zone) {
                throw std::runtime_error("Unknown zone: " + cmd.target);
            }
            goal.x = zone->first;
            goal.y = zone->second;

            RCLCPP_INFO(get_logger(), "Resolved zone %s to (%.2f, %.2f)",
                        cmd.target.c_str(), goal.x, goal.y);
        } else {
            throw std::runtime_error("Goto requires destination or zone");
        }
    } else {
        throw std::runtime_error("Unknown action: " + cmd.action);
    }

    goal_pub_->publish(goal);
    RCLCPP_INFO(get_logger(), "Published goal: (%.2f, %.2f)", goal.x, goal.y);
}

}  // namespace warehouser_command
