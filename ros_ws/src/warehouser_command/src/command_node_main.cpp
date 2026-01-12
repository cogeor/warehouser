#include <rclcpp/rclcpp.hpp>
#include "warehouser_command/command_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<warehouser_command::CommandNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
