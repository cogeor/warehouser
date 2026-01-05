/**
 * @file test_simulation_node.cpp
 * @brief Integration tests for the simulation node.
 *
 * These tests require the simulation node to be running.
 * Run with: ros2 launch warehouser_simulation simulation.launch.py
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/empty.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>

#include "warehouser_msgs/msg/world_state.hpp"
#include "warehouser_msgs/srv/sim_step.hpp"

using namespace std::chrono_literals;

class SimulationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        rclcpp::init(0, nullptr);
        node_ = std::make_shared<rclcpp::Node>("simulation_integration_test");

        // Create subscribers
        state_sub_ = node_->create_subscription<warehouser_msgs::msg::WorldState>(
            "/world/state", 10,
            [this](const warehouser_msgs::msg::WorldState::SharedPtr msg) {
                last_state_ = *msg;
                state_received_ = true;
            });

        // Create publishers
        cmd_pub_ = node_->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
        move_pub_ = node_->create_publisher<std_msgs::msg::String>("/sim/move_entity", 10);
        pick_pub_ = node_->create_publisher<std_msgs::msg::Empty>("/sim/pick", 10);
        unpick_pub_ = node_->create_publisher<std_msgs::msg::Empty>("/sim/unpick", 10);

        // Create service clients
        start_client_ = node_->create_client<std_srvs::srv::Trigger>("/sim/start");
        pause_client_ = node_->create_client<std_srvs::srv::Trigger>("/sim/pause");
        reset_client_ = node_->create_client<std_srvs::srv::Trigger>("/sim/reset");
        step_client_ = node_->create_client<warehouser_msgs::srv::SimStep>("/sim/step");
    }

    void TearDown() override {
        node_.reset();
        rclcpp::shutdown();
    }

    bool waitForService(rclcpp::ClientBase::SharedPtr client, std::chrono::seconds timeout = 5s) {
        return client->wait_for_service(timeout);
    }

    bool waitForState(std::chrono::seconds timeout = 5s) {
        state_received_ = false;
        auto start = std::chrono::steady_clock::now();
        while (!state_received_) {
            rclcpp::spin_some(node_);
            std::this_thread::sleep_for(10ms);
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
        }
        return true;
    }

    void spinFor(std::chrono::milliseconds duration) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < duration) {
            rclcpp::spin_some(node_);
            std::this_thread::sleep_for(10ms);
        }
    }

    const warehouser_msgs::msg::Entity* findRobot() const {
        for (const auto& entity : last_state_.entities) {
            if (entity.type == 0) {  // TYPE_ROBOT
                return &entity;
            }
        }
        return nullptr;
    }

    const warehouser_msgs::msg::Entity* findEntity(const std::string& id) const {
        for (const auto& entity : last_state_.entities) {
            if (entity.id == id) {
                return &entity;
            }
        }
        return nullptr;
    }

    rclcpp::Node::SharedPtr node_;

    // Subscribers
    rclcpp::Subscription<warehouser_msgs::msg::WorldState>::SharedPtr state_sub_;

    // Publishers
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr move_pub_;
    rclcpp::Publisher<std_msgs::msg::Empty>::SharedPtr pick_pub_;
    rclcpp::Publisher<std_msgs::msg::Empty>::SharedPtr unpick_pub_;

    // Service clients
    rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr start_client_;
    rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr pause_client_;
    rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr reset_client_;
    rclcpp::Client<warehouser_msgs::srv::SimStep>::SharedPtr step_client_;

    warehouser_msgs::msg::WorldState last_state_;
    bool state_received_ = false;
};

TEST_F(SimulationIntegrationTest, ServicesAvailable) {
    EXPECT_TRUE(waitForService(start_client_));
    EXPECT_TRUE(waitForService(pause_client_));
    EXPECT_TRUE(waitForService(reset_client_));
    EXPECT_TRUE(waitForService(step_client_));
}

TEST_F(SimulationIntegrationTest, ReceivesWorldState) {
    ASSERT_TRUE(waitForService(start_client_));

    // Start simulation
    auto request = std::make_shared<std_srvs::srv::Trigger::Request>();
    auto future = start_client_->async_send_request(request);
    rclcpp::spin_until_future_complete(node_, future, 5s);

    // Should receive world state
    ASSERT_TRUE(waitForState());
    EXPECT_FALSE(last_state_.entities.empty());
}

TEST_F(SimulationIntegrationTest, WorldStateContainsRobot) {
    ASSERT_TRUE(waitForService(start_client_));

    auto request = std::make_shared<std_srvs::srv::Trigger::Request>();
    auto future = start_client_->async_send_request(request);
    rclcpp::spin_until_future_complete(node_, future, 5s);

    ASSERT_TRUE(waitForState());

    const auto* robot = findRobot();
    ASSERT_NE(robot, nullptr);
    EXPECT_EQ(robot->id, "robot");
}

TEST_F(SimulationIntegrationTest, StartAndPauseWork) {
    ASSERT_TRUE(waitForService(start_client_));
    ASSERT_TRUE(waitForService(pause_client_));

    // Start
    auto start_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    auto start_future = start_client_->async_send_request(start_req);
    rclcpp::spin_until_future_complete(node_, start_future, 5s);
    EXPECT_TRUE(start_future.get()->success);

    waitForState();
    EXPECT_TRUE(last_state_.running);

    // Pause
    auto pause_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    auto pause_future = pause_client_->async_send_request(pause_req);
    rclcpp::spin_until_future_complete(node_, pause_future, 5s);
    EXPECT_TRUE(pause_future.get()->success);

    waitForState();
    EXPECT_FALSE(last_state_.running);
}

TEST_F(SimulationIntegrationTest, ResetRestoresInitialState) {
    ASSERT_TRUE(waitForService(start_client_));
    ASSERT_TRUE(waitForService(reset_client_));

    // Start and move robot
    auto start_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    start_client_->async_send_request(start_req);
    spinFor(100ms);

    // Send velocity command
    geometry_msgs::msg::Twist cmd;
    cmd.linear.x = 1.0;
    cmd_pub_->publish(cmd);
    spinFor(500ms);

    waitForState();
    const auto* robot_moved = findRobot();
    ASSERT_NE(robot_moved, nullptr);
    float moved_x = robot_moved->x;

    // Reset
    auto reset_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    auto reset_future = reset_client_->async_send_request(reset_req);
    rclcpp::spin_until_future_complete(node_, reset_future, 5s);
    EXPECT_TRUE(reset_future.get()->success);

    waitForState();
    const auto* robot_reset = findRobot();
    ASSERT_NE(robot_reset, nullptr);

    // Robot should be back at spawn (approximately 1.0)
    EXPECT_NE(robot_reset->x, moved_x);
    EXPECT_NEAR(robot_reset->x, 1.0f, 0.5f);
}

TEST_F(SimulationIntegrationTest, CmdVelMovesRobot) {
    ASSERT_TRUE(waitForService(start_client_));

    // Start simulation
    auto start_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    start_client_->async_send_request(start_req);
    spinFor(100ms);

    waitForState();
    const auto* robot_before = findRobot();
    ASSERT_NE(robot_before, nullptr);
    float x_before = robot_before->x;

    // Send forward velocity
    geometry_msgs::msg::Twist cmd;
    cmd.linear.x = 1.0;
    cmd.angular.z = 0.0;
    cmd_pub_->publish(cmd);

    // Wait for robot to move
    spinFor(1000ms);
    waitForState();

    const auto* robot_after = findRobot();
    ASSERT_NE(robot_after, nullptr);

    // Robot should have moved forward
    EXPECT_GT(robot_after->x, x_before);
}

TEST_F(SimulationIntegrationTest, MoveEntityUpdatesPosition) {
    ASSERT_TRUE(waitForService(start_client_));

    auto start_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    start_client_->async_send_request(start_req);
    spinFor(100ms);

    // Move red_1 object to new position
    std_msgs::msg::String move_msg;
    move_msg.data = R"({"id": "red_1", "x": 7.0, "y": 7.0})";
    move_pub_->publish(move_msg);

    spinFor(200ms);
    waitForState();

    const auto* obj = findEntity("red_1");
    ASSERT_NE(obj, nullptr);
    EXPECT_NEAR(obj->x, 7.0f, 0.1f);
    EXPECT_NEAR(obj->y, 7.0f, 0.1f);
}

TEST_F(SimulationIntegrationTest, SimStepAdvancesTime) {
    ASSERT_TRUE(waitForService(step_client_));
    ASSERT_TRUE(waitForService(reset_client_));

    // Reset first
    auto reset_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    auto reset_future = reset_client_->async_send_request(reset_req);
    rclcpp::spin_until_future_complete(node_, reset_future, 5s);

    waitForState();
    float time_before = last_state_.sim_time;

    // Step simulation
    auto step_req = std::make_shared<warehouser_msgs::srv::SimStep::Request>();
    step_req->num_ticks = 10;
    auto step_future = step_client_->async_send_request(step_req);
    rclcpp::spin_until_future_complete(node_, step_future, 5s);

    auto result = step_future.get();
    EXPECT_TRUE(result->success);
    EXPECT_GT(result->elapsed_sim_time, 0.0f);
    EXPECT_GT(result->state.sim_time, time_before);
}

TEST_F(SimulationIntegrationTest, PickAndUnpickObject) {
    ASSERT_TRUE(waitForService(start_client_));
    ASSERT_TRUE(waitForService(reset_client_));

    // Reset
    auto reset_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    reset_client_->async_send_request(reset_req);
    spinFor(100ms);

    // Start
    auto start_req = std::make_shared<std_srvs::srv::Trigger::Request>();
    start_client_->async_send_request(start_req);
    spinFor(100ms);

    // Move robot to object position
    std_msgs::msg::String move_msg;
    move_msg.data = R"({"id": "robot", "x": 3.0, "y": 2.0})";
    move_pub_->publish(move_msg);
    spinFor(100ms);

    // Pick
    pick_pub_->publish(std_msgs::msg::Empty());
    spinFor(200ms);

    waitForState();
    const auto* robot_carrying = findRobot();
    ASSERT_NE(robot_carrying, nullptr);
    EXPECT_TRUE(robot_carrying->is_carrying);

    // Unpick
    unpick_pub_->publish(std_msgs::msg::Empty());
    spinFor(200ms);

    waitForState();
    const auto* robot_not_carrying = findRobot();
    ASSERT_NE(robot_not_carrying, nullptr);
    EXPECT_FALSE(robot_not_carrying->is_carrying);
}

// Main function for running tests
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
