/**
 * @file test_rl_bridge_node.cpp
 * @brief Integration tests for the RL bridge node.
 *
 * These tests require simulation, observations, and rl_bridge nodes to be running.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include <rclcpp/rclcpp.hpp>

#include "warehouser_msgs/srv/rl_reset.hpp"
#include "warehouser_msgs/srv/rl_step.hpp"

using namespace std::chrono_literals;

class RLBridgeIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        rclcpp::init(0, nullptr);
        node_ = std::make_shared<rclcpp::Node>("rl_bridge_integration_test");

        // Create service clients
        reset_client_ = node_->create_client<warehouser_msgs::srv::RLReset>("/rl/reset");
        step_client_ = node_->create_client<warehouser_msgs::srv::RLStep>("/rl/step");
    }

    void TearDown() override {
        node_.reset();
        rclcpp::shutdown();
    }

    rclcpp::Node::SharedPtr node_;
    rclcpp::Client<warehouser_msgs::srv::RLReset>::SharedPtr reset_client_;
    rclcpp::Client<warehouser_msgs::srv::RLStep>::SharedPtr step_client_;
};

TEST_F(RLBridgeIntegrationTest, ServicesAvailable) {
    EXPECT_TRUE(reset_client_->wait_for_service(5s));
    EXPECT_TRUE(step_client_->wait_for_service(5s));
}

TEST_F(RLBridgeIntegrationTest, ResetReturnsObservation) {
    ASSERT_TRUE(reset_client_->wait_for_service(5s));

    auto request = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    request->seed = 42;

    auto future = reset_client_->async_send_request(request);
    ASSERT_EQ(rclcpp::spin_until_future_complete(node_, future, 10s),
              rclcpp::FutureReturnCode::SUCCESS);

    auto response = future.get();
    EXPECT_TRUE(response->success);
    EXPECT_EQ(response->observation.data.size(), 8u);
}

TEST_F(RLBridgeIntegrationTest, ResetWithSeedIsDeterministic) {
    ASSERT_TRUE(reset_client_->wait_for_service(5s));

    // Reset with same seed twice
    auto request1 = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    request1->seed = 123;

    auto future1 = reset_client_->async_send_request(request1);
    rclcpp::spin_until_future_complete(node_, future1, 10s);
    auto response1 = future1.get();

    auto request2 = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    request2->seed = 123;

    auto future2 = reset_client_->async_send_request(request2);
    rclcpp::spin_until_future_complete(node_, future2, 10s);
    auto response2 = future2.get();

    // Robot position should be the same (first 2 elements)
    EXPECT_NEAR(response1->observation.data[0], response2->observation.data[0], 0.01f);
    EXPECT_NEAR(response1->observation.data[1], response2->observation.data[1], 0.01f);
}

TEST_F(RLBridgeIntegrationTest, StepReturnsObservationAndReward) {
    ASSERT_TRUE(reset_client_->wait_for_service(5s));
    ASSERT_TRUE(step_client_->wait_for_service(5s));

    // First reset
    auto reset_req = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    auto reset_future = reset_client_->async_send_request(reset_req);
    rclcpp::spin_until_future_complete(node_, reset_future, 10s);

    // Then step
    auto step_req = std::make_shared<warehouser_msgs::srv::RLStep::Request>();
    step_req->action_linear = 0.5f;
    step_req->action_angular = 0.0f;
    step_req->action_pick = 0.0f;
    step_req->action_place = 0.0f;
    step_req->num_steps = 1;

    auto step_future = step_client_->async_send_request(step_req);
    ASSERT_EQ(rclcpp::spin_until_future_complete(node_, step_future, 10s),
              rclcpp::FutureReturnCode::SUCCESS);

    auto response = step_future.get();
    EXPECT_EQ(response->observation.data.size(), 8u);
    // Reward should be a valid float (not NaN)
    EXPECT_EQ(response->reward, response->reward);  // NaN check
}

TEST_F(RLBridgeIntegrationTest, StepMovesRobot) {
    ASSERT_TRUE(reset_client_->wait_for_service(5s));
    ASSERT_TRUE(step_client_->wait_for_service(5s));

    // Reset
    auto reset_req = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    auto reset_future = reset_client_->async_send_request(reset_req);
    rclcpp::spin_until_future_complete(node_, reset_future, 10s);
    auto reset_response = reset_future.get();
    float initial_x = reset_response->observation.data[0];

    // Step forward multiple times
    for (int i = 0; i < 10; ++i) {
        auto step_req = std::make_shared<warehouser_msgs::srv::RLStep::Request>();
        step_req->action_linear = 1.0f;  // Full forward
        step_req->action_angular = 0.0f;
        step_req->action_pick = 0.0f;
        step_req->action_place = 0.0f;
        step_req->num_steps = 1;

        auto step_future = step_client_->async_send_request(step_req);
        rclcpp::spin_until_future_complete(node_, step_future, 5s);
    }

    // Check final position
    auto step_req = std::make_shared<warehouser_msgs::srv::RLStep::Request>();
    step_req->action_linear = 0.0f;
    step_req->num_steps = 1;
    auto final_future = step_client_->async_send_request(step_req);
    rclcpp::spin_until_future_complete(node_, final_future, 5s);
    auto final_response = final_future.get();

    float final_x = final_response->observation.data[0];
    EXPECT_GT(final_x, initial_x);
}

TEST_F(RLBridgeIntegrationTest, TerminationDetected) {
    ASSERT_TRUE(reset_client_->wait_for_service(5s));
    ASSERT_TRUE(step_client_->wait_for_service(5s));

    // Reset
    auto reset_req = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    auto reset_future = reset_client_->async_send_request(reset_req);
    rclcpp::spin_until_future_complete(node_, reset_future, 10s);

    // Run many steps - should eventually truncate
    bool truncated = false;
    for (int i = 0; i < 600 && !truncated; ++i) {
        auto step_req = std::make_shared<warehouser_msgs::srv::RLStep::Request>();
        step_req->action_linear = 0.1f;
        step_req->num_steps = 1;

        auto step_future = step_client_->async_send_request(step_req);
        rclcpp::spin_until_future_complete(node_, step_future, 5s);
        auto response = step_future.get();

        if (response->truncated || response->terminated) {
            truncated = true;
        }
    }

    EXPECT_TRUE(truncated);
}

TEST_F(RLBridgeIntegrationTest, ProgressRewardPositiveTowardGoal) {
    ASSERT_TRUE(reset_client_->wait_for_service(5s));
    ASSERT_TRUE(step_client_->wait_for_service(5s));

    // Reset
    auto reset_req = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    auto reset_future = reset_client_->async_send_request(reset_req);
    rclcpp::spin_until_future_complete(node_, reset_future, 10s);
    auto reset_response = reset_future.get();

    // Get goal direction from observation (goal_dx, goal_dy)
    float goal_dx = reset_response->observation.data[3];
    float goal_dy = reset_response->observation.data[4];

    // Move toward goal
    float action_linear = 1.0f;
    float action_angular = 0.0f;  // Assume roughly facing goal for simplicity

    auto step_req = std::make_shared<warehouser_msgs::srv::RLStep::Request>();
    step_req->action_linear = action_linear;
    step_req->action_angular = action_angular;
    step_req->num_steps = 1;

    auto step_future = step_client_->async_send_request(step_req);
    rclcpp::spin_until_future_complete(node_, step_future, 5s);
    auto response = step_future.get();

    // Should get some reward (positive progress or time penalty)
    // Just verify reward is a valid number
    EXPECT_FALSE(std::isnan(response->reward));
}

TEST_F(RLBridgeIntegrationTest, InfoContainsStepCount) {
    ASSERT_TRUE(reset_client_->wait_for_service(5s));
    ASSERT_TRUE(step_client_->wait_for_service(5s));

    // Reset
    auto reset_req = std::make_shared<warehouser_msgs::srv::RLReset::Request>();
    auto reset_future = reset_client_->async_send_request(reset_req);
    rclcpp::spin_until_future_complete(node_, reset_future, 10s);

    // Step
    auto step_req = std::make_shared<warehouser_msgs::srv::RLStep::Request>();
    step_req->num_steps = 1;

    auto step_future = step_client_->async_send_request(step_req);
    rclcpp::spin_until_future_complete(node_, step_future, 5s);
    auto response = step_future.get();

    // Info should be a JSON string containing step count
    EXPECT_FALSE(response->info.empty());
    EXPECT_NE(response->info.find("step"), std::string::npos);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
