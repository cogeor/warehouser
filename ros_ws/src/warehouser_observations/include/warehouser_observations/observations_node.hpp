#pragma once

#include <chrono>
#include <memory>
#include <mutex>

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/lidar_debug.hpp"
#include "warehouser_msgs/msg/observation.hpp"
#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_msgs/srv/get_observation.hpp"
#include "warehouser_observations/lidar_simulator.hpp"
#include "warehouser_observations/observation_builder.hpp"
#include "warehouser_observations/odometry_simulator.hpp"

namespace warehouser {

/// ROS2 node that builds observations from world state and publishes them.
/// Also generates debug lidar output for visualization.
class ObservationsNode : public rclcpp::Node {
public:
    explicit ObservationsNode(
        const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
    // Subscriber callbacks
    void worldStateCallback(
        const warehouser_msgs::msg::WorldState::SharedPtr msg);
    void goalCallback(const warehouser_msgs::msg::Goal::SharedPtr msg);

    // Timer callbacks
    void publishObservation();
    void publishLidarDebug();
    void publishOdometry();

    // Service callback
    void handleGetObservation(
        const warehouser_msgs::srv::GetObservation::Request::SharedPtr request,
        warehouser_msgs::srv::GetObservation::Response::SharedPtr response);

    // Find robot in current world state
    const warehouser_msgs::msg::Entity* findRobot() const;

    // Core components
    ObservationBuilder builder_;
    LidarSimulator lidar_;
    OdometrySimulator odom_;

    // Cached state (updated by subscribers)
    warehouser_msgs::msg::WorldState last_world_;
    warehouser_msgs::msg::Goal last_goal_;
    bool world_received_ = false;

    // ROS interfaces
    rclcpp::Subscription<warehouser_msgs::msg::WorldState>::SharedPtr world_sub_;
    rclcpp::Subscription<warehouser_msgs::msg::Goal>::SharedPtr goal_sub_;

    rclcpp::Publisher<warehouser_msgs::msg::Observation>::SharedPtr obs_pub_;
    rclcpp::Publisher<warehouser_msgs::msg::LidarDebug>::SharedPtr lidar_pub_;
    rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;

    rclcpp::Service<warehouser_msgs::srv::GetObservation>::SharedPtr get_obs_srv_;

    rclcpp::TimerBase::SharedPtr obs_timer_;
    rclcpp::TimerBase::SharedPtr lidar_timer_;
    rclcpp::TimerBase::SharedPtr odom_timer_;

    // Odometry timing
    float odom_rate_ = 50.0f;  // Hz
};

}  // namespace warehouser
