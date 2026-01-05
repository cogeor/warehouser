/**
 * @file test_observations_node.cpp
 * @brief Integration tests for the observations node.
 *
 * These tests require the simulation and observations nodes to be running.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

#include <rclcpp/rclcpp.hpp>

#include "warehouser_msgs/msg/goal.hpp"
#include "warehouser_msgs/msg/lidar_debug.hpp"
#include "warehouser_msgs/msg/observation.hpp"
#include "warehouser_msgs/srv/get_observation.hpp"

using namespace std::chrono_literals;

class ObservationsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        rclcpp::init(0, nullptr);
        node_ = std::make_shared<rclcpp::Node>("observations_integration_test");

        // Create subscribers
        obs_sub_ = node_->create_subscription<warehouser_msgs::msg::Observation>(
            "/observations", 10,
            [this](const warehouser_msgs::msg::Observation::SharedPtr msg) {
                last_obs_ = *msg;
                obs_received_ = true;
            });

        lidar_sub_ = node_->create_subscription<warehouser_msgs::msg::LidarDebug>(
            "/observations/lidar_debug", 10,
            [this](const warehouser_msgs::msg::LidarDebug::SharedPtr msg) {
                last_lidar_ = *msg;
                lidar_received_ = true;
            });

        // Create publisher for goal
        goal_pub_ = node_->create_publisher<warehouser_msgs::msg::Goal>("/task/goal", 10);

        // Create service client
        get_obs_client_ =
            node_->create_client<warehouser_msgs::srv::GetObservation>("/observations/get");
    }

    void TearDown() override {
        node_.reset();
        rclcpp::shutdown();
    }

    bool waitForObservation(std::chrono::milliseconds timeout = 5000ms) {
        obs_received_ = false;
        auto start = std::chrono::steady_clock::now();
        while (!obs_received_) {
            rclcpp::spin_some(node_);
            std::this_thread::sleep_for(10ms);
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
        }
        return true;
    }

    bool waitForLidar(std::chrono::milliseconds timeout = 5000ms) {
        lidar_received_ = false;
        auto start = std::chrono::steady_clock::now();
        while (!lidar_received_) {
            rclcpp::spin_some(node_);
            std::this_thread::sleep_for(10ms);
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
        }
        return true;
    }

    void publishGoal(float x, float y, const std::string& color = "") {
        warehouser_msgs::msg::Goal goal;
        goal.x = x;
        goal.y = y;
        goal.target_color = color;
        goal.active = true;
        goal_pub_->publish(goal);
    }

    rclcpp::Node::SharedPtr node_;

    rclcpp::Subscription<warehouser_msgs::msg::Observation>::SharedPtr obs_sub_;
    rclcpp::Subscription<warehouser_msgs::msg::LidarDebug>::SharedPtr lidar_sub_;
    rclcpp::Publisher<warehouser_msgs::msg::Goal>::SharedPtr goal_pub_;
    rclcpp::Client<warehouser_msgs::srv::GetObservation>::SharedPtr get_obs_client_;

    warehouser_msgs::msg::Observation last_obs_;
    warehouser_msgs::msg::LidarDebug last_lidar_;
    bool obs_received_ = false;
    bool lidar_received_ = false;
};

TEST_F(ObservationsIntegrationTest, ReceivesObservations) {
    ASSERT_TRUE(waitForObservation());
    EXPECT_EQ(last_obs_.version, 1);  // V1 observation
}

TEST_F(ObservationsIntegrationTest, ObservationHasCorrectDimensions) {
    ASSERT_TRUE(waitForObservation());
    EXPECT_EQ(last_obs_.data.size(), 8u);  // V1 has 8 dimensions
}

TEST_F(ObservationsIntegrationTest, ReceivesLidarDebug) {
    ASSERT_TRUE(waitForLidar());
    EXPECT_FALSE(last_lidar_.ranges.empty());
}

TEST_F(ObservationsIntegrationTest, LidarDebugHas60Rays) {
    ASSERT_TRUE(waitForLidar());
    EXPECT_EQ(last_lidar_.ranges.size(), 60u);
}

TEST_F(ObservationsIntegrationTest, LidarDebugHasValidAngles) {
    ASSERT_TRUE(waitForLidar());

    // FOV should be ~π (180 degrees)
    float fov = last_lidar_.angle_max - last_lidar_.angle_min;
    EXPECT_NEAR(fov, 3.14159f, 0.1f);
}

TEST_F(ObservationsIntegrationTest, LidarRangesWithinBounds) {
    ASSERT_TRUE(waitForLidar());

    for (float range : last_lidar_.ranges) {
        EXPECT_GE(range, last_lidar_.range_min);
        EXPECT_LE(range, last_lidar_.range_max);
    }
}

TEST_F(ObservationsIntegrationTest, GetObservationServiceWorks) {
    ASSERT_TRUE(get_obs_client_->wait_for_service(5s));

    auto request = std::make_shared<warehouser_msgs::srv::GetObservation::Request>();
    auto future = get_obs_client_->async_send_request(request);

    ASSERT_EQ(rclcpp::spin_until_future_complete(node_, future, 5s),
              rclcpp::FutureReturnCode::SUCCESS);

    auto response = future.get();
    EXPECT_EQ(response->observation.version, 1);
    EXPECT_EQ(response->observation.data.size(), 8u);
}

TEST_F(ObservationsIntegrationTest, ObservationIncludesRobotPosition) {
    ASSERT_TRUE(waitForObservation());

    // Robot position (first 3 elements)
    float robot_x = last_obs_.data[0];
    float robot_y = last_obs_.data[1];
    float robot_theta = last_obs_.data[2];

    // Should be reasonable values
    EXPECT_GE(robot_x, 0.0f);
    EXPECT_LE(robot_x, 10.0f);
    EXPECT_GE(robot_y, 0.0f);
    EXPECT_LE(robot_y, 10.0f);
    EXPECT_GE(robot_theta, -M_PI);
    EXPECT_LE(robot_theta, M_PI);
}

TEST_F(ObservationsIntegrationTest, GoalAffectsObservation) {
    // Set a goal
    publishGoal(8.0f, 8.0f);
    std::this_thread::sleep_for(200ms);

    ASSERT_TRUE(waitForObservation());

    // Goal distance should be non-zero
    float goal_dist = last_obs_.data[5];
    EXPECT_GT(goal_dist, 0.0f);
}

TEST_F(ObservationsIntegrationTest, CarryingFlagInObservation) {
    ASSERT_TRUE(waitForObservation());

    // is_carrying flag (index 7)
    float is_carrying = last_obs_.data[7];
    EXPECT_TRUE(is_carrying == 0.0f || is_carrying == 1.0f);
}

TEST_F(ObservationsIntegrationTest, LidarIncludesRobotPose) {
    ASSERT_TRUE(waitForLidar());

    // Lidar message should include robot pose
    EXPECT_GE(last_lidar_.robot_x, 0.0f);
    EXPECT_LE(last_lidar_.robot_x, 10.0f);
    EXPECT_GE(last_lidar_.robot_y, 0.0f);
    EXPECT_LE(last_lidar_.robot_y, 10.0f);
}

TEST_F(ObservationsIntegrationTest, ObservationsPublishedAtRate) {
    // Wait for multiple observations
    int count = 0;
    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < 1s) {
        if (waitForObservation(100ms)) {
            count++;
        }
    }

    // Should receive ~20 observations per second (20 Hz)
    EXPECT_GE(count, 15);
    EXPECT_LE(count, 25);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
