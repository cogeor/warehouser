#include <rclcpp/rclcpp.hpp>
#include "warehouser_task/task_manager_node.hpp"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<warehouser_task::TaskManagerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
