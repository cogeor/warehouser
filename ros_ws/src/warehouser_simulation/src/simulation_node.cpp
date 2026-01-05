#include "warehouser_simulation/simulation_node.hpp"

#include <format>

// Simple JSON parsing for move_entity commands
// Format: {"id": "entity_id", "x": 5.0, "y": 3.0}
namespace {

struct MoveCommand {
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    bool valid = false;
};

MoveCommand parseMoveCommand(const std::string& json) {
    MoveCommand cmd;

    // Very simple JSON parsing - find "id", "x", "y" values
    // For production, use a proper JSON library like nlohmann/json

    auto findValue = [&json](const std::string& key) -> std::string {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";

        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";

        // Skip whitespace
        pos++;
        while (pos < json.size() && std::isspace(json[pos])) pos++;

        if (pos >= json.size()) return "";

        // Check if string value
        if (json[pos] == '"') {
            auto end = json.find('"', pos + 1);
            if (end == std::string::npos) return "";
            return json.substr(pos + 1, end - pos - 1);
        }

        // Numeric value
        auto end = json.find_first_of(",}", pos);
        if (end == std::string::npos) end = json.size();
        return json.substr(pos, end - pos);
    };

    cmd.id = findValue("id");
    auto x_str = findValue("x");
    auto y_str = findValue("y");

    if (!cmd.id.empty() && !x_str.empty() && !y_str.empty()) {
        try {
            cmd.x = std::stof(x_str);
            cmd.y = std::stof(y_str);
            cmd.valid = true;
        } catch (...) {
            cmd.valid = false;
        }
    }

    return cmd;
}

}  // namespace

namespace warehouser {

SimulationNode::SimulationNode(const rclcpp::NodeOptions& options)
    : Node("simulation", options) {
    // Declare parameters
    dt_ = declare_parameter("dt", 0.02);
    auto world_width = declare_parameter("world_width", 10.0);
    auto world_height = declare_parameter("world_height", 10.0);
    auto robot_spawn = declare_parameter<std::vector<double>>(
        "robot_spawn", {1.0, 1.0, 0.0});
    auto config_file = declare_parameter("config", "");

    // Initialize world
    WorldConfig config;
    config.width = static_cast<float>(world_width);
    config.height = static_cast<float>(world_height);
    if (robot_spawn.size() >= 3) {
        config.robot_spawn = {static_cast<float>(robot_spawn[0]),
                              static_cast<float>(robot_spawn[1]),
                              static_cast<float>(robot_spawn[2])};
    }

    world_ = WorldManager(config);

    // Load config if provided
    if (!config_file.empty()) {
        auto result = world_.loadConfig(config_file);
        if (!result) {
            RCLCPP_ERROR(get_logger(), "Failed to load config: %s",
                         result.error().c_str());
        }
    } else {
        // Load default world
        world_.loadConfig("");
    }

    // Create subscribers
    cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "/cmd_vel", 10,
        std::bind(&SimulationNode::cmdVelCallback, this, std::placeholders::_1));

    move_sub_ = create_subscription<std_msgs::msg::String>(
        "/sim/move_entity", 10,
        std::bind(&SimulationNode::moveEntityCallback, this,
                  std::placeholders::_1));

    pick_sub_ = create_subscription<std_msgs::msg::Empty>(
        "/sim/pick", 10,
        std::bind(&SimulationNode::pickCallback, this, std::placeholders::_1));

    unpick_sub_ = create_subscription<std_msgs::msg::Empty>(
        "/sim/unpick", 10,
        std::bind(&SimulationNode::unpickCallback, this, std::placeholders::_1));

    // Create publishers
    state_pub_ = create_publisher<warehouser_msgs::msg::WorldState>(
        "/world/state", 10);
    clock_pub_ = create_publisher<rosgraph_msgs::msg::Clock>("/clock", 10);

    // Create services
    start_srv_ = create_service<std_srvs::srv::Trigger>(
        "/sim/start",
        std::bind(&SimulationNode::handleStart, this, std::placeholders::_1,
                  std::placeholders::_2));

    pause_srv_ = create_service<std_srvs::srv::Trigger>(
        "/sim/pause",
        std::bind(&SimulationNode::handlePause, this, std::placeholders::_1,
                  std::placeholders::_2));

    reset_srv_ = create_service<std_srvs::srv::Trigger>(
        "/sim/reset",
        std::bind(&SimulationNode::handleReset, this, std::placeholders::_1,
                  std::placeholders::_2));

    step_srv_ = create_service<warehouser_msgs::srv::SimStep>(
        "/sim/step",
        std::bind(&SimulationNode::handleStep, this, std::placeholders::_1,
                  std::placeholders::_2));

    // Create timer for simulation loop
    auto period = std::chrono::duration<double>(dt_);
    timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&SimulationNode::tick, this));

    RCLCPP_INFO(get_logger(), "Simulation node initialized (dt=%.3f)", dt_);
}

void SimulationNode::tick() {
    // Step simulation
    world_.step(dt_);

    // Publish world state
    state_pub_->publish(world_.toMsg());

    // Publish simulation clock
    rosgraph_msgs::msg::Clock clock_msg;
    clock_msg.clock =
        rclcpp::Time(static_cast<int64_t>(world_.simTime() * 1e9));
    clock_pub_->publish(clock_msg);
}

void SimulationNode::cmdVelCallback(
    const geometry_msgs::msg::Twist::SharedPtr msg) {
    if (auto* robot = world_.robot()) {
        robot->setCommand(static_cast<float>(msg->linear.x),
                          static_cast<float>(msg->angular.z));
    }
}

void SimulationNode::moveEntityCallback(
    const std_msgs::msg::String::SharedPtr msg) {
    auto cmd = parseMoveCommand(msg->data);
    if (!cmd.valid) {
        RCLCPP_WARN(get_logger(), "Invalid move_entity command: %s",
                    msg->data.c_str());
        return;
    }

    auto result = world_.moveEntity(cmd.id, cmd.x, cmd.y);
    if (!result) {
        RCLCPP_WARN(get_logger(), "Failed to move entity: %s",
                    result.error().c_str());
    }
}

void SimulationNode::pickCallback(const std_msgs::msg::Empty::SharedPtr /*msg*/) {
    auto* robot = world_.robot();
    if (!robot || robot->is_carrying) {
        return;
    }

    // Try to pick the closest object within range
    for (auto& obj : world_.objects()) {
        if (!obj->is_picked && robot->tryPick(*obj)) {
            RCLCPP_INFO(get_logger(), "Robot picked up %s", obj->id.c_str());
            break;
        }
    }
}

void SimulationNode::unpickCallback(
    const std_msgs::msg::Empty::SharedPtr /*msg*/) {
    auto* robot = world_.robot();
    if (!robot || !robot->is_carrying) {
        return;
    }

    // Find and drop the carried object
    if (auto* obj = world_.findObject(robot->carried_object_id)) {
        robot->unpick(*obj);
        RCLCPP_INFO(get_logger(), "Robot dropped %s", obj->id.c_str());
    }
}

void SimulationNode::handleStart(
    const std_srvs::srv::Trigger::Request::SharedPtr /*request*/,
    std_srvs::srv::Trigger::Response::SharedPtr response) {
    world_.start();
    response->success = true;
    response->message = "Simulation started";
    RCLCPP_INFO(get_logger(), "Simulation started");
}

void SimulationNode::handlePause(
    const std_srvs::srv::Trigger::Request::SharedPtr /*request*/,
    std_srvs::srv::Trigger::Response::SharedPtr response) {
    world_.pause();
    response->success = true;
    response->message = "Simulation paused";
    RCLCPP_INFO(get_logger(), "Simulation paused");
}

void SimulationNode::handleReset(
    const std_srvs::srv::Trigger::Request::SharedPtr /*request*/,
    std_srvs::srv::Trigger::Response::SharedPtr response) {
    world_.reset();
    response->success = true;
    response->message = "Simulation reset";
    RCLCPP_INFO(get_logger(), "Simulation reset");
}

void SimulationNode::handleStep(
    const warehouser_msgs::srv::SimStep::Request::SharedPtr request,
    warehouser_msgs::srv::SimStep::Response::SharedPtr response) {
    float elapsed = 0.0f;
    int ticks = request->num_ticks > 0 ? request->num_ticks : 1;

    // Temporarily enable simulation for stepping
    bool was_running = world_.isRunning();
    world_.start();

    for (int i = 0; i < ticks; ++i) {
        world_.step(dt_);
        elapsed += dt_;
    }

    // Restore running state
    if (!was_running) {
        world_.pause();
    }

    response->success = true;
    response->elapsed_sim_time = elapsed;
    response->state = world_.toMsg();
}

}  // namespace warehouser

// Main entry point
int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<warehouser::SimulationNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
