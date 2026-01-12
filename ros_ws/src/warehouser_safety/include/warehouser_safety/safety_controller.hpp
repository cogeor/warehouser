#pragma once

#include <vector>

namespace warehouser_safety {

enum class SafetyState {
    NOMINAL,
    SLOWDOWN,
    EMERGENCY,
    STOPPED
};

struct SafetyConfig {
    float min_distance{0.3f};
    float slowdown_distance{0.8f};
    float max_linear_vel{1.0f};
    float max_angular_vel{2.0f};
};

struct Velocity {
    float linear{0.0f};
    float angular{0.0f};
};

struct LidarData {
    std::vector<float> ranges;
    float angle_min{-1.57f};  // -90 degrees
    float angle_max{1.57f};   // +90 degrees
    float range_min{0.1f};
    float range_max{10.0f};
};

class SafetyController {
public:
    explicit SafetyController(const SafetyConfig& config = SafetyConfig{});

    Velocity applySafetyLimits(const Velocity& cmd_raw, const LidarData& lidar);

    [[nodiscard]] SafetyState getState() const noexcept { return state_; }

    [[nodiscard]] float getMinObstacleDistance() const noexcept { return last_min_distance_; }

    void setConfig(const SafetyConfig& config) { config_ = config; }

private:
    float getMinDistance(const LidarData& lidar, float angle_min, float angle_max) const;
    float computeScale(float distance) const;

    SafetyConfig config_;
    SafetyState state_{SafetyState::NOMINAL};
    float last_min_distance_{10.0f};
};

}  // namespace warehouser_safety
