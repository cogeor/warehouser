#include <rclcpp/rclcpp.hpp>
#include "warehouser_safety/safety_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<warehouser_safety::SafetyNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
