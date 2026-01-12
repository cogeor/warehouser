#include <gtest/gtest.h>
#include "warehouser_inference/policy_inference.hpp"

using namespace warehouser_inference;

class PolicyInferenceTest : public ::testing::Test {
protected:
    PolicyInference policy_;
};

TEST_F(PolicyInferenceTest, InitiallyNotLoaded) {
    EXPECT_FALSE(policy_.isLoaded());
}

TEST_F(PolicyInferenceTest, InferFailsWithoutModel) {
    std::vector<float> obs(8, 0.0f);
    auto result = policy_.infer(obs);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "No model loaded");
}

TEST_F(PolicyInferenceTest, LoadNonexistentModelFails) {
    auto result = policy_.loadModel("/nonexistent/path/model.onnx");
    EXPECT_FALSE(result.has_value());
}

#ifndef ONNXRUNTIME_AVAILABLE
// Stub-only tests
TEST_F(PolicyInferenceTest, StubLoadSucceeds) {
    // Create a temporary file to simulate model
    auto result = policy_.loadModel(__FILE__);  // Use this test file as dummy
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(policy_.isLoaded());
}

TEST_F(PolicyInferenceTest, StubInferReturnsReactiveAction) {
    // Load stub model
    policy_.loadModel(__FILE__);

    // Create observation: robot at origin, goal ahead
    std::vector<float> obs = {
        0.0f,   // robot_x
        0.0f,   // robot_y
        0.0f,   // robot_theta (facing +x)
        5.0f,   // goal_dx
        0.0f,   // goal_dy
        5.0f,   // goal_dist
        0.0f,   // goal_heading (directly ahead)
        0.0f    // is_carrying
    };

    auto result = policy_.infer(obs);
    ASSERT_TRUE(result.has_value());

    auto action = *result;
    // Should move forward since goal is ahead
    EXPECT_GT(action.linear, 0.0f);
    // Should not turn much since goal is ahead
    EXPECT_NEAR(action.angular, 0.0f, 0.1f);
}

TEST_F(PolicyInferenceTest, StubTurnsTowardsGoal) {
    policy_.loadModel(__FILE__);

    // Goal to the left
    std::vector<float> obs = {
        0.0f, 0.0f, 0.0f,  // robot pose
        0.0f, 5.0f, 5.0f,  // goal relative
        1.57f,             // goal_heading (90 degrees left)
        0.0f               // is_carrying
    };

    auto result = policy_.infer(obs);
    ASSERT_TRUE(result.has_value());

    // Should turn left (positive angular)
    EXPECT_GT(result->angular, 0.0f);
}

TEST_F(PolicyInferenceTest, StubPicksWhenClose) {
    policy_.loadModel(__FILE__);

    // Very close to goal, not carrying
    std::vector<float> obs = {
        0.0f, 0.0f, 0.0f,
        0.3f, 0.0f, 0.3f,  // goal_dist < 0.5
        0.0f,
        0.0f               // not carrying
    };

    auto result = policy_.infer(obs);
    ASSERT_TRUE(result.has_value());

    EXPECT_GT(result->pick, 0.5f);
}
#endif

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
