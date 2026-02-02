#include "warehouser_observations/observations_node.hpp"

#include <chrono>

namespace warehouser {

ObservationsNode::ObservationsNode(const rclcpp::NodeOptions& options)
    : Node("observations", options) {
    // Declare parameters
    int version = declare_parameter("version", 1);
    float world_size = declare_parameter("world_size", 10.0);

    int lidar_num_rays = declare_parameter("lidar_num_rays", 60);
    float lidar_fov = declare_parameter("lidar_fov", 3.14159265);
    float lidar_max_range = declare_parameter("lidar_max_range", 10.0);
    float lidar_min_range = declare_parameter("lidar_min_range", 0.1);

    float obs_rate = declare_parameter("obs_rate", 20.0);
    float lidar_rate = declare_parameter("lidar_rate", 10.0);
    odom_rate_ = declare_parameter("odom_rate", 50.0);
    bool odom_add_noise = declare_parameter("odom_add_noise", false);

    // Initialize observation builder
    ObservationConfig obs_config;
    obs_config.version = static_cast<ObservationVersion>(version);
    obs_config.world_size = static_cast<float>(world_size);
    builder_ = ObservationBuilder(obs_config);

    // Initialize lidar simulator
    LidarConfig lidar_config;
    lidar_config.num_rays = lidar_num_rays;
    lidar_config.fov = static_cast<float>(lidar_fov);
    lidar_config.max_range = static_cast<float>(lidar_max_range);
    lidar_config.min_range = static_cast<float>(lidar_min_range);
    lidar_ = LidarSimulator(lidar_config);

    // Initialize odometry simulator
    OdometryConfig odom_config;
    odom_config.add_noise = odom_add_noise;
    odom_ = OdometrySimulator(odom_config);

    // Create subscribers
    world_sub_ = create_subscription<warehouser_msgs::msg::WorldState>(
        "/world/state", 10,
        std::bind(&ObservationsNode::worldStateCallback, this,
                  std::placeholders::_1));

    goal_sub_ = create_subscription<warehouser_msgs::msg::Goal>(
        "/task/goal", 10,
        std::bind(&ObservationsNode::goalCallback, this, std::placeholders::_1));

    // Create publishers
    obs_pub_ =
        create_publisher<warehouser_msgs::msg::Observation>("/observations", 10);
    lidar_pub_ = create_publisher<warehouser_msgs::msg::LidarDebug>(
        "/observations/lidar_debug", 10);
    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("/odom", 10);

    // Create service
    get_obs_srv_ = create_service<warehouser_msgs::srv::GetObservation>(
        "/observations/get",
        std::bind(&ObservationsNode::handleGetObservation, this,
                  std::placeholders::_1, std::placeholders::_2));

    // Create timers
    auto obs_period = std::chrono::duration<double>(1.0 / obs_rate);
    obs_timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(obs_period),
        std::bind(&ObservationsNode::publishObservation, this));

    auto lidar_period = std::chrono::duration<double>(1.0 / lidar_rate);
    lidar_timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(lidar_period),
        std::bind(&ObservationsNode::publishLidarDebug, this));

    auto odom_period = std::chrono::duration<double>(1.0 / odom_rate_);
    odom_timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(odom_period),
        std::bind(&ObservationsNode::publishOdometry, this));

    RCLCPP_INFO(get_logger(),
                "Observations node initialized (version=%d, obs_rate=%.1f Hz, "
                "lidar_rate=%.1f Hz, odom_rate=%.1f Hz)",
                version, obs_rate, lidar_rate, odom_rate_);
}

void ObservationsNode::worldStateCallback(
    const warehouser_msgs::msg::WorldState::SharedPtr msg) {
    last_world_ = *msg;
    world_received_ = true;
}

void ObservationsNode::goalCallback(
    const warehouser_msgs::msg::Goal::SharedPtr msg) {
    last_goal_ = *msg;
}

void ObservationsNode::publishObservation() {
    if (!world_received_) {
        return;
    }

    auto obs = builder_.build(last_world_, last_goal_);
    obs_pub_->publish(obs);
}

void ObservationsNode::publishLidarDebug() {
    if (!world_received_) {
        return;
    }

    const auto* robot = findRobot();
    if (!robot) {
        return;
    }

    auto msg =
        lidar_.buildDebugMsg(robot->x, robot->y, robot->theta, last_world_);
    lidar_pub_->publish(msg);
}

void ObservationsNode::publishOdometry() {
    if (!world_received_) {
        return;
    }

    const auto* robot = findRobot();
    if (!robot) {
        return;
    }

    // Compute odometry from current robot pose
    SensorPose pose{robot->x, robot->y, robot->theta};
    float dt = 1.0f / odom_rate_;
    auto odom_reading = odom_.computeOdometry(pose, dt);

    // Build nav_msgs::Odometry message
    nav_msgs::msg::Odometry msg;
    msg.header.stamp = now();
    msg.header.frame_id = "odom";
    msg.child_frame_id = "base_link";

    // Set current pose (absolute position)
    msg.pose.pose.position.x = robot->x;
    msg.pose.pose.position.y = robot->y;
    msg.pose.pose.position.z = 0.0;

    // Convert theta to quaternion (rotation around Z axis)
    msg.pose.pose.orientation.x = 0.0;
    msg.pose.pose.orientation.y = 0.0;
    msg.pose.pose.orientation.z = std::sin(robot->theta / 2.0f);
    msg.pose.pose.orientation.w = std::cos(robot->theta / 2.0f);

    // Set twist (velocities in robot frame)
    if (dt > 0.0f) {
        // Convert world-frame deltas to robot-frame velocities
        float cos_theta = std::cos(robot->theta);
        float sin_theta = std::sin(robot->theta);
        // Transform to robot frame
        float vx_robot = cos_theta * odom_reading.dx + sin_theta * odom_reading.dy;
        float vy_robot = -sin_theta * odom_reading.dx + cos_theta * odom_reading.dy;

        msg.twist.twist.linear.x = vx_robot / dt;
        msg.twist.twist.linear.y = vy_robot / dt;
        msg.twist.twist.angular.z = odom_reading.dtheta / dt;
    }

    // Set covariance (6x6 diagonal, row-major)
    for (int i = 0; i < 36; ++i) {
        msg.pose.covariance[i] = 0.0;
        msg.twist.covariance[i] = 0.0;
    }
    // Diagonal elements from OdometryReading covariance
    msg.pose.covariance[0] = odom_reading.covariance[0];   // x
    msg.pose.covariance[7] = odom_reading.covariance[1];   // y
    msg.pose.covariance[35] = odom_reading.covariance[5];  // yaw
    msg.twist.covariance[0] = odom_reading.covariance[0];
    msg.twist.covariance[7] = odom_reading.covariance[1];
    msg.twist.covariance[35] = odom_reading.covariance[5];

    odom_pub_->publish(msg);
}

void ObservationsNode::handleGetObservation(
    const warehouser_msgs::srv::GetObservation::Request::SharedPtr /*request*/,
    warehouser_msgs::srv::GetObservation::Response::SharedPtr response) {
    response->observation = builder_.build(last_world_, last_goal_);
}

const warehouser_msgs::msg::Entity* ObservationsNode::findRobot() const {
    for (const auto& entity : last_world_.entities) {
        if (entity.type == 0) {  // TYPE_ROBOT = 0
            return &entity;
        }
    }
    return nullptr;
}

}  // namespace warehouser

// Main entry point
int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<warehouser::ObservationsNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
