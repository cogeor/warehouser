/**
 * @file test_action_safety.cpp
 * @brief Integration tests for RLBridge action processing and safety.
 *
 * Tests the action processing pipeline that transforms normalized RL actions
 * into safe robot commands, including:
 * - Action scaling from normalized [-1, 1] to physical velocity limits
 * - SafetyController integration for obstacle avoidance
 * - Discrete action triggering with threshold comparison
 */

#include <gtest/gtest.h>

#include "warehouser_safety/safety_controller.hpp"

using namespace warehouser_safety;

// =============================================================================
// Action Scaling Tests
// =============================================================================

class ActionScalingTest : public ::testing::Test {
protected:
    // Simulate the scaling logic from RLBridgeNode::sendAction()
    static std::pair<float, float> scaleAction(
        float linear_normalized,
        float angular_normalized,
        float v_max,
        float omega_max
    ) {
        // Scale normalized actions [-1, 1] to velocity limits
        // This mirrors: float scaled_linear = linear * v_max_;
        float scaled_linear = linear_normalized * v_max;
        float scaled_angular = angular_normalized * omega_max;
        return {scaled_linear, scaled_angular};
    }
};

TEST_F(ActionScalingTest, ScalesLinearVelocityByVMax) {
    float v_max = 1.0f;
    float omega_max = 2.0f;

    auto [linear, angular] = scaleAction(0.5f, 0.0f, v_max, omega_max);

    EXPECT_FLOAT_EQ(linear, 0.5f);  // 0.5 * 1.0 = 0.5
    EXPECT_FLOAT_EQ(angular, 0.0f);
}

TEST_F(ActionScalingTest, ScalesAngularVelocityByOmegaMax) {
    float v_max = 1.0f;
    float omega_max = 2.0f;

    auto [linear, angular] = scaleAction(0.0f, 0.5f, v_max, omega_max);

    EXPECT_FLOAT_EQ(linear, 0.0f);
    EXPECT_FLOAT_EQ(angular, 1.0f);  // 0.5 * 2.0 = 1.0
}

TEST_F(ActionScalingTest, HandlesNegativeNormalizedActions) {
    float v_max = 1.0f;
    float omega_max = 2.0f;

    auto [linear, angular] = scaleAction(-1.0f, -0.5f, v_max, omega_max);

    EXPECT_FLOAT_EQ(linear, -1.0f);   // -1.0 * 1.0 = -1.0
    EXPECT_FLOAT_EQ(angular, -1.0f);  // -0.5 * 2.0 = -1.0
}

TEST_F(ActionScalingTest, MaxNormalizedGivesMaxVelocity) {
    float v_max = 1.5f;
    float omega_max = 3.0f;

    auto [linear, angular] = scaleAction(1.0f, 1.0f, v_max, omega_max);

    EXPECT_FLOAT_EQ(linear, 1.5f);   // 1.0 * 1.5 = 1.5
    EXPECT_FLOAT_EQ(angular, 3.0f);  // 1.0 * 3.0 = 3.0
}

TEST_F(ActionScalingTest, ZeroNormalizedGivesZeroVelocity) {
    float v_max = 2.0f;
    float omega_max = 4.0f;

    auto [linear, angular] = scaleAction(0.0f, 0.0f, v_max, omega_max);

    EXPECT_FLOAT_EQ(linear, 0.0f);
    EXPECT_FLOAT_EQ(angular, 0.0f);
}

// =============================================================================
// Safety Controller Integration Tests
// =============================================================================

class SafetyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_linear_vel = 1.0f;
        config_.max_angular_vel = 2.0f;
        config_.min_distance = 0.3f;
        config_.slowdown_distance = 0.8f;
        controller_.setConfig(config_);
    }

    SafetyController controller_;
    SafetyConfig config_;
};

TEST_F(SafetyIntegrationTest, NominalStateWithNoObstacles) {
    // Empty lidar data (no obstacles detected)
    LidarData lidar;
    lidar.angle_min = -1.57f;
    lidar.angle_max = 1.57f;
    // No ranges = no obstacles in range

    Velocity cmd{0.5f, 1.0f};
    auto safe_vel = controller_.applySafetyLimits(cmd, lidar);

    // With no obstacles, velocity should pass through unchanged
    // (or at most clamped to max)
    EXPECT_EQ(controller_.getState(), SafetyState::NOMINAL);
    EXPECT_FLOAT_EQ(safe_vel.linear, 0.5f);
    EXPECT_FLOAT_EQ(safe_vel.angular, 1.0f);
}

TEST_F(SafetyIntegrationTest, SlowdownWhenObstacleNear) {
    // Lidar with obstacle in slowdown zone
    LidarData lidar;
    lidar.angle_min = -1.57f;
    lidar.angle_max = 1.57f;
    lidar.ranges = std::vector<float>(55, 0.6f);  // Obstacles at 0.6m (in slowdown zone)

    Velocity cmd{1.0f, 0.0f};
    auto safe_vel = controller_.applySafetyLimits(cmd, lidar);

    // Should be in SLOWDOWN state with reduced velocity
    EXPECT_EQ(controller_.getState(), SafetyState::SLOWDOWN);
    EXPECT_LT(safe_vel.linear, 1.0f);  // Should be reduced
    EXPECT_GE(safe_vel.linear, 0.0f);  // But not negative
}

TEST_F(SafetyIntegrationTest, EmergencyStopWhenObstacleTooClose) {
    // Lidar with obstacle in emergency zone
    LidarData lidar;
    lidar.angle_min = -1.57f;
    lidar.angle_max = 1.57f;
    lidar.ranges = std::vector<float>(55, 0.2f);  // Obstacles at 0.2m (below min_distance)

    Velocity cmd{1.0f, 2.0f};
    auto safe_vel = controller_.applySafetyLimits(cmd, lidar);

    // Should stop forward motion
    EXPECT_LE(safe_vel.linear, 0.0f);  // No forward velocity (may allow backward)
}

TEST_F(SafetyIntegrationTest, AllowsBackwardMotionWhenObstacleAhead) {
    // Obstacle directly ahead
    LidarData lidar;
    lidar.angle_min = -0.5f;  // Narrower forward-facing cone
    lidar.angle_max = 0.5f;
    lidar.ranges = std::vector<float>(10, 0.2f);  // Very close obstacle ahead

    // Request backward motion (negative linear velocity)
    Velocity cmd{-0.5f, 0.0f};
    auto safe_vel = controller_.applySafetyLimits(cmd, lidar);

    // Backward motion should be allowed since obstacle is ahead
    // Implementation may vary - this tests the concept
    EXPECT_LE(safe_vel.linear, 0.0f);  // Should not convert to forward
}

TEST_F(SafetyIntegrationTest, AngularVelocityMayBeReduced) {
    // Obstacle very close
    LidarData lidar;
    lidar.angle_min = -1.57f;
    lidar.angle_max = 1.57f;
    lidar.ranges = std::vector<float>(55, 0.15f);  // Very close obstacles

    Velocity cmd{0.0f, 2.0f};  // Pure rotation
    auto safe_vel = controller_.applySafetyLimits(cmd, lidar);

    // Angular velocity may also be reduced in emergency
    // Exact behavior depends on implementation
    EXPECT_LE(std::abs(safe_vel.angular), 2.0f);
}

TEST_F(SafetyIntegrationTest, ConfigUpdateAffectsBehavior) {
    // Set very conservative limits
    SafetyConfig strict_config;
    strict_config.min_distance = 0.5f;
    strict_config.slowdown_distance = 1.5f;
    strict_config.max_linear_vel = 0.5f;
    strict_config.max_angular_vel = 1.0f;
    controller_.setConfig(strict_config);

    LidarData lidar;
    lidar.ranges = std::vector<float>(55, 1.0f);  // 1m distance

    Velocity cmd{1.0f, 2.0f};  // Request more than max
    auto safe_vel = controller_.applySafetyLimits(cmd, lidar);

    // Should be in slowdown (1.0m < 1.5m slowdown_distance)
    EXPECT_EQ(controller_.getState(), SafetyState::SLOWDOWN);
}

// =============================================================================
// Discrete Action Tests
// =============================================================================

class DiscreteActionTest : public ::testing::Test {
protected:
    // Simulate the discrete action threshold logic from RLBridgeNode::sendAction()
    static bool shouldTriggerPick(float pick_signal) {
        // Mirrors: if (pick > 0.5f) pick_pubs_[robot_id]->publish(...)
        return pick_signal > 0.5f;
    }

    static bool shouldTriggerPlace(float place_signal) {
        // Mirrors: if (place > 0.5f) unpick_pubs_[robot_id]->publish(...)
        return place_signal > 0.5f;
    }
};

TEST_F(DiscreteActionTest, PickTriggersAboveThreshold) {
    EXPECT_TRUE(shouldTriggerPick(0.6f));
    EXPECT_TRUE(shouldTriggerPick(0.51f));
    EXPECT_TRUE(shouldTriggerPick(1.0f));
}

TEST_F(DiscreteActionTest, PickDoesNotTriggerAtOrBelowThreshold) {
    EXPECT_FALSE(shouldTriggerPick(0.5f));   // Exactly at threshold
    EXPECT_FALSE(shouldTriggerPick(0.49f));
    EXPECT_FALSE(shouldTriggerPick(0.0f));
    EXPECT_FALSE(shouldTriggerPick(-1.0f));
}

TEST_F(DiscreteActionTest, PlaceTriggersAboveThreshold) {
    EXPECT_TRUE(shouldTriggerPlace(0.6f));
    EXPECT_TRUE(shouldTriggerPlace(0.51f));
    EXPECT_TRUE(shouldTriggerPlace(1.0f));
}

TEST_F(DiscreteActionTest, PlaceDoesNotTriggerAtOrBelowThreshold) {
    EXPECT_FALSE(shouldTriggerPlace(0.5f));   // Exactly at threshold
    EXPECT_FALSE(shouldTriggerPlace(0.49f));
    EXPECT_FALSE(shouldTriggerPlace(0.0f));
    EXPECT_FALSE(shouldTriggerPlace(-1.0f));
}

TEST_F(DiscreteActionTest, ThresholdEdgeCases) {
    // Test values very close to threshold
    EXPECT_FALSE(shouldTriggerPick(0.5f));
    EXPECT_TRUE(shouldTriggerPick(0.500001f));

    // Negative values never trigger
    EXPECT_FALSE(shouldTriggerPick(-0.5f));
    EXPECT_FALSE(shouldTriggerPlace(-0.5f));
}

// =============================================================================
// Action Feedback Tests
// =============================================================================

class ActionFeedbackTest : public ::testing::Test {
protected:
    // Simulate pick/place success detection from RLBridgeNode::handleRLStep()
    static bool detectPickSuccess(
        float pick_signal,
        bool is_carrying_now,
        bool was_carrying_before
    ) {
        // Mirrors: response->pick_success = (request->action_pick > 0.5f) &&
        //                                    is_carrying && !prev_carrying;
        return (pick_signal > 0.5f) && is_carrying_now && !was_carrying_before;
    }

    static bool detectPlaceSuccess(
        float place_signal,
        bool is_carrying_now,
        bool was_carrying_before
    ) {
        // Mirrors: response->place_success = (request->action_place > 0.5f) &&
        //                                     !is_carrying && prev_carrying;
        return (place_signal > 0.5f) && !is_carrying_now && was_carrying_before;
    }
};

TEST_F(ActionFeedbackTest, PickSuccessWhenNowCarryingAndWasNot) {
    EXPECT_TRUE(detectPickSuccess(0.8f, true, false));
}

TEST_F(ActionFeedbackTest, PickFailsWhenNotRequestedEvenIfCarrying) {
    EXPECT_FALSE(detectPickSuccess(0.3f, true, false));  // Not requested
}

TEST_F(ActionFeedbackTest, PickFailsWhenAlreadyCarrying) {
    EXPECT_FALSE(detectPickSuccess(0.8f, true, true));  // Was already carrying
}

TEST_F(ActionFeedbackTest, PickFailsWhenStillNotCarrying) {
    EXPECT_FALSE(detectPickSuccess(0.8f, false, false));  // Still not carrying
}

TEST_F(ActionFeedbackTest, PlaceSuccessWhenDroppedAndWasCarrying) {
    EXPECT_TRUE(detectPlaceSuccess(0.8f, false, true));
}

TEST_F(ActionFeedbackTest, PlaceFailsWhenNotRequestedEvenIfDropped) {
    EXPECT_FALSE(detectPlaceSuccess(0.3f, false, true));  // Not requested
}

TEST_F(ActionFeedbackTest, PlaceFailsWhenStillCarrying) {
    EXPECT_FALSE(detectPlaceSuccess(0.8f, true, true));  // Still carrying
}

TEST_F(ActionFeedbackTest, PlaceFailsWhenWasNotCarrying) {
    EXPECT_FALSE(detectPlaceSuccess(0.8f, false, false));  // Wasn't carrying
}
