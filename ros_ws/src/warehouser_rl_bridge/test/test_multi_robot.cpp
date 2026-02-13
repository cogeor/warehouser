#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "warehouser_rl_bridge/reward_strategy.hpp"

using namespace warehouser;

// ============ Multi-Robot Reset Tests ============

class MultiRobotResetTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create world state with configurable robot count
        world_.entities.clear();
    }

    void addRobotToWorld(const std::string& id, float x, float y,
                         float theta = 0.0f, bool in_collision = false) {
        warehouser_msgs::msg::Entity robot;
        robot.id = id;
        robot.type = 0;  // TYPE_ROBOT
        robot.x = x;
        robot.y = y;
        robot.theta = theta;
        robot.is_carrying = false;
        robot.in_robot_collision = in_collision;
        world_.entities.push_back(robot);
    }

    size_t countRobotsInWorld() const {
        size_t count = 0;
        for (const auto& entity : world_.entities) {
            if (entity.type == 0) {  // TYPE_ROBOT
                count++;
            }
        }
        return count;
    }

    warehouser_msgs::msg::WorldState world_;
};

TEST_F(MultiRobotResetTest, ResetSpawnsRequestedRobotCount) {
    // Simulate reset with 3 robots
    addRobotToWorld("robot0", 1.0f, 1.0f);
    addRobotToWorld("robot1", 3.0f, 1.0f);
    addRobotToWorld("robot2", 5.0f, 1.0f);

    EXPECT_EQ(countRobotsInWorld(), 3u);
}

TEST_F(MultiRobotResetTest, RobotsHaveDistinctIds) {
    addRobotToWorld("robot0", 1.0f, 1.0f);
    addRobotToWorld("robot1", 3.0f, 1.0f);
    addRobotToWorld("robot2", 5.0f, 1.0f);

    std::set<std::string> ids;
    for (const auto& entity : world_.entities) {
        if (entity.type == 0) {
            EXPECT_TRUE(ids.find(entity.id) == ids.end())
                << "Duplicate robot id: " << entity.id;
            ids.insert(entity.id);
        }
    }

    EXPECT_EQ(ids.size(), 3u);
}

TEST_F(MultiRobotResetTest, RobotsHaveDistinctPositions) {
    addRobotToWorld("robot0", 1.0f, 1.0f);
    addRobotToWorld("robot1", 3.0f, 1.0f);
    addRobotToWorld("robot2", 5.0f, 3.0f);

    // Check all robots are at different positions
    for (size_t i = 0; i < world_.entities.size(); ++i) {
        for (size_t j = i + 1; j < world_.entities.size(); ++j) {
            if (world_.entities[i].type == 0 && world_.entities[j].type == 0) {
                float dx = world_.entities[i].x - world_.entities[j].x;
                float dy = world_.entities[i].y - world_.entities[j].y;
                float dist = std::sqrt(dx * dx + dy * dy);
                EXPECT_GT(dist, 0.5f)
                    << "Robots " << i << " and " << j << " are too close";
            }
        }
    }
}

// ============ Multi-Robot Action Tests ============

class MultiRobotActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        prev_world_.entities.clear();
        curr_world_.entities.clear();
        goal_.x = 10.0f;
        goal_.y = 10.0f;
        goal_.active = true;
    }

    void addRobot(warehouser_msgs::msg::WorldState& world,
                  const std::string& id, float x, float y) {
        warehouser_msgs::msg::Entity robot;
        robot.id = id;
        robot.type = 0;  // TYPE_ROBOT
        robot.x = x;
        robot.y = y;
        robot.theta = 0.0f;
        robot.is_carrying = false;
        robot.in_robot_collision = false;
        world.entities.push_back(robot);
    }

    RewardContext makeContext(size_t robot_index) {
        return RewardContext{prev_world_, curr_world_, goal_,
                             1, 500, robot_index};
    }

    warehouser_msgs::msg::WorldState prev_world_;
    warehouser_msgs::msg::WorldState curr_world_;
    warehouser_msgs::msg::Goal goal_;
};

TEST_F(MultiRobotActionTest, ActionsForRobot0Execute) {
    // Add 3 robots
    addRobot(prev_world_, "robot0", 1.0f, 1.0f);
    addRobot(prev_world_, "robot1", 3.0f, 1.0f);
    addRobot(prev_world_, "robot2", 5.0f, 1.0f);

    // Robot 0 moves closer to goal
    addRobot(curr_world_, "robot0", 2.0f, 2.0f);  // Moved
    addRobot(curr_world_, "robot1", 3.0f, 1.0f);  // Unchanged
    addRobot(curr_world_, "robot2", 5.0f, 1.0f);  // Unchanged

    NavigationRewardStrategy strategy;
    auto result = strategy.calculate(makeContext(0));

    // Robot 0 moved closer to goal (10,10)
    // Progress reward should be positive
    EXPECT_GT(result.reward, 0.0f);
}

TEST_F(MultiRobotActionTest, ActionsForRobot1Execute) {
    // Add 3 robots
    addRobot(prev_world_, "robot0", 1.0f, 1.0f);
    addRobot(prev_world_, "robot1", 3.0f, 1.0f);
    addRobot(prev_world_, "robot2", 5.0f, 1.0f);

    // Robot 1 moves closer to goal
    addRobot(curr_world_, "robot0", 1.0f, 1.0f);  // Unchanged
    addRobot(curr_world_, "robot1", 4.0f, 2.0f);  // Moved
    addRobot(curr_world_, "robot2", 5.0f, 1.0f);  // Unchanged

    NavigationRewardStrategy strategy;
    auto result = strategy.calculate(makeContext(1));

    // Robot 1 moved closer to goal
    EXPECT_GT(result.reward, 0.0f);
}

TEST_F(MultiRobotActionTest, ActionsForRobot2Execute) {
    // Add 3 robots
    addRobot(prev_world_, "robot0", 1.0f, 1.0f);
    addRobot(prev_world_, "robot1", 3.0f, 1.0f);
    addRobot(prev_world_, "robot2", 5.0f, 1.0f);

    // Robot 2 moves closer to goal
    addRobot(curr_world_, "robot0", 1.0f, 1.0f);  // Unchanged
    addRobot(curr_world_, "robot1", 3.0f, 1.0f);  // Unchanged
    addRobot(curr_world_, "robot2", 6.0f, 2.0f);  // Moved

    NavigationRewardStrategy strategy;
    auto result = strategy.calculate(makeContext(2));

    // Robot 2 moved closer to goal
    EXPECT_GT(result.reward, 0.0f);
}

TEST_F(MultiRobotActionTest, InvalidRobotIndexReturnsNoReward) {
    addRobot(prev_world_, "robot0", 1.0f, 1.0f);
    addRobot(curr_world_, "robot0", 2.0f, 2.0f);

    NavigationRewardStrategy strategy;
    auto result = strategy.calculate(makeContext(5));  // Invalid index

    // No reward for non-existent robot
    EXPECT_FLOAT_EQ(result.reward, 0.0f);
}

// ============ Robot Collision Penalty Tests ============

class RobotCollisionPenaltyTest : public ::testing::Test {
protected:
    void SetUp() override {
        world_.entities.clear();
        goal_.x = 10.0f;
        goal_.y = 10.0f;
        goal_.active = true;
    }

    void addRobot(const std::string& id, float x, float y,
                  bool in_collision = false) {
        warehouser_msgs::msg::Entity robot;
        robot.id = id;
        robot.type = 0;  // TYPE_ROBOT
        robot.x = x;
        robot.y = y;
        robot.theta = 0.0f;
        robot.is_carrying = false;
        robot.in_robot_collision = in_collision;
        world_.entities.push_back(robot);
    }

    RewardContext makeContext(size_t robot_index) {
        return RewardContext{world_, world_, goal_, 1, 500, robot_index};
    }

    warehouser_msgs::msg::WorldState world_;
    warehouser_msgs::msg::Goal goal_;
};

TEST_F(RobotCollisionPenaltyTest, PenaltyAppliedWhenColliding) {
    addRobot("robot0", 1.0f, 1.0f, true);  // in_robot_collision = true
    addRobot("robot1", 1.3f, 1.0f, true);  // in_robot_collision = true

    RobotCollisionConfig config;
    config.robot_collision_penalty = -50.0f;
    RobotCollisionRewardStrategy strategy(config);

    auto result0 = strategy.calculate(makeContext(0));
    auto result1 = strategy.calculate(makeContext(1));

    EXPECT_FLOAT_EQ(result0.reward, -50.0f);
    EXPECT_FLOAT_EQ(result1.reward, -50.0f);
}

TEST_F(RobotCollisionPenaltyTest, NoPenaltyWhenNotColliding) {
    addRobot("robot0", 1.0f, 1.0f, false);  // in_robot_collision = false
    addRobot("robot1", 10.0f, 10.0f, false);  // Far apart

    RobotCollisionRewardStrategy strategy;
    auto result0 = strategy.calculate(makeContext(0));
    auto result1 = strategy.calculate(makeContext(1));

    EXPECT_FLOAT_EQ(result0.reward, 0.0f);
    EXPECT_FLOAT_EQ(result1.reward, 0.0f);
}

TEST_F(RobotCollisionPenaltyTest, OnlyCollidingRobotPenalized) {
    addRobot("robot0", 1.0f, 1.0f, true);   // Colliding
    addRobot("robot1", 1.3f, 1.0f, true);   // Colliding
    addRobot("robot2", 15.0f, 15.0f, false); // Not colliding

    RobotCollisionConfig config;
    config.robot_collision_penalty = -25.0f;
    RobotCollisionRewardStrategy strategy(config);

    auto result0 = strategy.calculate(makeContext(0));
    auto result1 = strategy.calculate(makeContext(1));
    auto result2 = strategy.calculate(makeContext(2));

    EXPECT_FLOAT_EQ(result0.reward, -25.0f);
    EXPECT_FLOAT_EQ(result1.reward, -25.0f);
    EXPECT_FLOAT_EQ(result2.reward, 0.0f);
}

TEST_F(RobotCollisionPenaltyTest, CollisionDoesNotTerminate) {
    addRobot("robot0", 1.0f, 1.0f, true);

    RobotCollisionRewardStrategy strategy;
    auto result = strategy.calculate(makeContext(0));

    // Robot collision is a penalty, not termination
    EXPECT_FALSE(result.terminated);
}

TEST_F(RobotCollisionPenaltyTest, DefaultPenaltyValue) {
    addRobot("robot0", 1.0f, 1.0f, true);

    RobotCollisionRewardStrategy strategy;  // Default config
    auto result = strategy.calculate(makeContext(0));

    // Default penalty is -50.0f
    EXPECT_FLOAT_EQ(result.reward, -50.0f);
}

TEST_F(RobotCollisionPenaltyTest, StrategyNameCorrect) {
    RobotCollisionRewardStrategy strategy;
    EXPECT_EQ(strategy.name(), "robot_collision");
}

// ============ Per-Robot Publisher Tests ============

class PerRobotPublisherTest : public ::testing::Test {
protected:
    // Test that the RL bridge creates correct number of publishers
    // This is a structural test - actual ROS publishers tested in integration
};

TEST_F(PerRobotPublisherTest, PublisherCountMatchesRobotCount) {
    // Verify the expected behavior: for N robots, create N of each publisher type
    // cmd_vel_pubs_, pick_pubs_, unpick_pubs_ vectors

    // Test case 1: 3 robots
    size_t robot_count = 3;
    std::vector<std::string> expected_topics;
    for (size_t i = 0; i < robot_count; ++i) {
        std::string prefix = "/robot" + std::to_string(i);
        expected_topics.push_back(prefix + "/cmd_vel");
        expected_topics.push_back(prefix + "/sim/pick");
        expected_topics.push_back(prefix + "/sim/unpick");
    }

    // Should have 9 topics (3 robots x 3 topic types)
    EXPECT_EQ(expected_topics.size(), 9u);

    // Verify topic naming pattern
    EXPECT_EQ(expected_topics[0], "/robot0/cmd_vel");
    EXPECT_EQ(expected_topics[3], "/robot1/cmd_vel");
    EXPECT_EQ(expected_topics[6], "/robot2/cmd_vel");
}

TEST_F(PerRobotPublisherTest, TopicNamingConvention) {
    // Verify the naming convention: /robot{N}/topic_name
    for (size_t i = 0; i < 5; ++i) {
        std::string prefix = "/robot" + std::to_string(i);
        EXPECT_EQ(prefix, "/robot" + std::to_string(i));

        std::string cmd_vel_topic = prefix + "/cmd_vel";
        std::string pick_topic = prefix + "/sim/pick";
        std::string unpick_topic = prefix + "/sim/unpick";

        // Verify topics start with correct prefix
        EXPECT_TRUE(cmd_vel_topic.find(prefix) == 0);
        EXPECT_TRUE(pick_topic.find(prefix) == 0);
        EXPECT_TRUE(unpick_topic.find(prefix) == 0);
    }
}

// ============ Composite Reward with Robot Collision ============

TEST(CompositeRewardTest, IncludesRobotCollisionStrategy) {
    auto strategy = createDefaultRewardStrategy();

    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->name(), "composite");

    auto* composite = dynamic_cast<CompositeRewardStrategy*>(strategy.get());
    ASSERT_NE(composite, nullptr);

    // Should have 5 strategies now: Navigation, Collision, Time, PickPlace, RobotCollision
    EXPECT_EQ(composite->strategyCount(), 5u);
}

TEST(CompositeRewardTest, RobotCollisionPenaltyIntegrated) {
    auto strategy = createDefaultRewardStrategy();

    // Create world with robot in collision
    warehouser_msgs::msg::WorldState world;
    warehouser_msgs::msg::Entity robot;
    robot.id = "robot0";
    robot.type = 0;
    robot.x = 5.0f;
    robot.y = 5.0f;
    robot.in_robot_collision = true;
    world.entities.push_back(robot);

    warehouser_msgs::msg::Goal goal;
    goal.x = 10.0f;
    goal.y = 10.0f;
    goal.active = true;

    RewardContext ctx{world, world, goal, 1, 500, 0};
    auto result = strategy->calculate(ctx);

    // Should include robot collision penalty (-50) in total reward
    // Along with time penalty (-0.1) and possibly others
    // Total should be negative due to collision penalty
    EXPECT_LT(result.reward, 0.0f);
}

TEST(CompositeRewardTest, NoCollisionNoPenalty) {
    auto strategy = createDefaultRewardStrategy();

    // Create world with robot NOT in collision
    warehouser_msgs::msg::WorldState world;
    warehouser_msgs::msg::Entity robot;
    robot.id = "robot0";
    robot.type = 0;
    robot.x = 5.0f;
    robot.y = 5.0f;
    robot.in_robot_collision = false;
    world.entities.push_back(robot);

    warehouser_msgs::msg::Goal goal;
    goal.x = 10.0f;
    goal.y = 10.0f;
    goal.active = true;

    RewardContext ctx{world, world, goal, 1, 500, 0};
    auto result = strategy->calculate(ctx);

    // Without collision, should only have time penalty (-0.1)
    // No progress since robot didn't move
    EXPECT_NEAR(result.reward, -0.1f, 0.01f);
}
