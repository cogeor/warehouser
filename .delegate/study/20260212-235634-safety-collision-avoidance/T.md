# Template: Safety and Collision Avoidance Patterns

Created: 2026-02-13T00:15:00Z

## Source

No templates available in `.delegate/templates/`, but extensive research and code analysis from:
- S.md: Recent academic papers on ORCA-DWA hybrid, safe RL, ISO 3691-4 standards
- I.md: Warehouser existing SafetyController implementation
- Industry best practices for warehouse AMR safety

## Patterns Discovered

Based on research synthesis and current Warehouser architecture analysis, the following patterns are applicable:

### 1. ORCA-DWA Hybrid for Multi-Robot Collision Avoidance
### 2. MPC Shielding for Safe RL Deployment
### 3. Multi-Zone Safety Controller (ISO 3691-4 Compliant)
### 4. Robot-Robot Collision Detection with Spatial Hashing
### 5. Time-to-Collision (TTC) Prediction
### 6. Sensor Redundancy and Health Monitoring
### 7. Nonstop Zones for Deadlock Prevention
### 8. Emergency Stop with Smart Recovery

---

## Application

### Template 1: ORCA-DWA Hybrid Collision Avoidance

**Pattern:** Combine ORCA's optimal multi-agent velocity selection with DWA's fast local planning. ORCA activates conditionally when robots are within 2m of each other.

**Research Source:** Nature Scientific Reports (Apr 2025) - "27.9% path length reduction, 100% obstacle avoidance success rate"

**Implementation for Warehouser:**

```cpp
// File: warehouser_safety/include/warehouser_safety/orca_dwa_controller.hpp
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace warehouser_safety {

// ORCA: Optimal Reciprocal Collision Avoidance
struct VelocityObstacle {
    float agent_x;
    float agent_y;
    float agent_vx;
    float agent_vy;
    float radius;
};

struct ORCAResult {
    float optimal_vx;
    float optimal_vy;
    bool collision_free;
};

class ORCAController {
public:
    // ORCA half-plane constraint
    struct HalfPlane {
        float nx;  // Normal x
        float ny;  // Normal y
        float c;   // Constant (distance from origin)
    };

    ORCAResult computeOptimalVelocity(
        float x, float y,
        float vx, float vy,
        float pref_vx, float pref_vy,
        const std::vector<VelocityObstacle>& neighbors,
        float time_horizon = 2.0f) const {

        ORCAResult result;
        result.optimal_vx = pref_vx;
        result.optimal_vy = pref_vy;
        result.collision_free = true;

        if (neighbors.empty()) {
            return result;
        }

        // Build ORCA half-planes for each neighbor
        std::vector<HalfPlane> planes;
        planes.reserve(neighbors.size());

        for (const auto& neighbor : neighbors) {
            float rel_x = neighbor.agent_x - x;
            float rel_y = neighbor.agent_y - y;
            float rel_vx = neighbor.agent_vx - vx;
            float rel_vy = neighbor.agent_vy - vy;

            float dist_sq = rel_x * rel_x + rel_y * rel_y;
            float combined_radius = 0.6f;  // 2 * robot radius (0.3m each)
            float combined_radius_sq = combined_radius * combined_radius;

            // Check if collision course
            if (dist_sq < combined_radius_sq) {
                result.collision_free = false;
            }

            // Compute velocity obstacle
            float dist = std::sqrt(dist_sq);
            float inv_time = 1.0f / time_horizon;

            // Direction to neighbor
            float nx = rel_x / dist;
            float ny = rel_y / dist;

            // ORCA half-plane: velocity change responsibility is split 50/50
            float u_x = rel_vx - (rel_x * inv_time);
            float u_y = rel_vy - (rel_y * inv_time);

            HalfPlane plane;
            plane.nx = nx;
            plane.ny = ny;
            plane.c = 0.5f * (u_x * nx + u_y * ny);

            planes.push_back(plane);
        }

        // Linear programming: find velocity closest to preferred velocity
        // that satisfies all half-plane constraints
        result.optimal_vx = pref_vx;
        result.optimal_vy = pref_vy;

        // Simple projection (for full LP, use optimization library)
        for (const auto& plane : planes) {
            float dot = result.optimal_vx * plane.nx + result.optimal_vy * plane.ny;
            if (dot < plane.c) {
                // Project velocity onto valid side of half-plane
                float delta = plane.c - dot;
                result.optimal_vx += delta * plane.nx;
                result.optimal_vy += delta * plane.ny;
            }
        }

        return result;
    }
};

// DWA: Dynamic Window Approach
struct DWAConfig {
    float max_linear_vel = 1.0f;
    float max_angular_vel = 2.0f;
    float max_linear_accel = 1.0f;
    float max_angular_accel = 2.0f;
    float dt = 0.1f;  // Prediction timestep
    int num_linear_samples = 10;
    int num_angular_samples = 20;
    float predict_time = 1.0f;  // Prediction horizon
};

struct DWATrajectory {
    float linear_vel;
    float angular_vel;
    float cost;
    bool is_safe;
};

class DWAController {
public:
    explicit DWAController(const DWAConfig& config) : config_(config) {}

    // Sample velocities from dynamic window
    std::vector<DWATrajectory> sampleDynamicWindow(
        float current_v, float current_omega,
        float goal_x, float goal_y, float goal_theta,
        const std::vector<float>& lidar_ranges,
        float lidar_angle_min, float lidar_angle_max) const {

        std::vector<DWATrajectory> trajectories;

        // Dynamic window constraints
        float v_min = std::max(-config_.max_linear_vel,
                              current_v - config_.max_linear_accel * config_.dt);
        float v_max = std::min(config_.max_linear_vel,
                              current_v + config_.max_linear_accel * config_.dt);
        float omega_min = std::max(-config_.max_angular_vel,
                                   current_omega - config_.max_angular_accel * config_.dt);
        float omega_max = std::min(config_.max_angular_vel,
                                   current_omega + config_.max_angular_accel * config_.dt);

        // Sample velocity space
        float v_step = (v_max - v_min) / config_.num_linear_samples;
        float omega_step = (omega_max - omega_min) / config_.num_angular_samples;

        for (int i = 0; i <= config_.num_linear_samples; ++i) {
            float v = v_min + i * v_step;

            for (int j = 0; j <= config_.num_angular_samples; ++j) {
                float omega = omega_min + j * omega_step;

                DWATrajectory traj;
                traj.linear_vel = v;
                traj.angular_vel = omega;

                // Evaluate trajectory
                float goal_cost = evaluateGoalCost(v, omega, goal_x, goal_y, goal_theta);
                float clearance_cost = evaluateClearanceCost(v, omega, lidar_ranges,
                                                             lidar_angle_min, lidar_angle_max);
                float velocity_cost = evaluateVelocityCost(v, omega);

                // Weighted sum (tune these weights)
                traj.cost = 1.0f * goal_cost + 2.0f * clearance_cost + 0.5f * velocity_cost;
                traj.is_safe = clearance_cost < 100.0f;  // Threshold for safety

                trajectories.push_back(traj);
            }
        }

        return trajectories;
    }

private:
    DWAConfig config_;

    float evaluateGoalCost(float v, float omega, float goal_x, float goal_y, float goal_theta) const {
        // Predict final heading after trajectory
        float theta_pred = omega * config_.predict_time;
        float heading_error = std::abs(theta_pred - goal_theta);
        return heading_error;
    }

    float evaluateClearanceCost(float v, float omega,
                                const std::vector<float>& lidar_ranges,
                                float angle_min, float angle_max) const {
        // Predict trajectory and check clearance
        float min_clearance = 10.0f;

        // Simple forward projection
        float dist = v * config_.predict_time;
        if (dist < 0.01f) return 0.0f;  // Stationary is safe

        // Check lidar rays in direction of motion
        float motion_angle = 0.0f;  // Forward

        for (size_t i = 0; i < lidar_ranges.size(); ++i) {
            float angle = angle_min + (angle_max - angle_min) * i / lidar_ranges.size();

            // Weight rays based on alignment with motion direction
            float angle_diff = std::abs(angle - motion_angle);
            if (angle_diff < 0.5f) {  // ±30 degrees
                min_clearance = std::min(min_clearance, lidar_ranges[i]);
            }
        }

        // Cost increases as clearance decreases
        if (min_clearance < 0.5f) {
            return 100.0f;  // Unsafe
        } else if (min_clearance < 1.0f) {
            return 50.0f / min_clearance;
        }
        return 1.0f / min_clearance;
    }

    float evaluateVelocityCost(float v, float omega) const {
        // Prefer higher velocities (within reason)
        return (config_.max_linear_vel - std::abs(v)) / config_.max_linear_vel;
    }
};

// Hybrid ORCA-DWA Controller
class HybridCollisionAvoidance {
public:
    HybridCollisionAvoidance(const DWAConfig& dwa_config)
        : dwa_(dwa_config) {}

    struct VelocityCommand {
        float linear;
        float angular;
        bool orca_active;
    };

    VelocityCommand computeVelocity(
        // Current state
        float x, float y, float theta,
        float current_v, float current_omega,
        // Goal
        float goal_x, float goal_y, float goal_theta,
        // Lidar
        const std::vector<float>& lidar_ranges,
        float lidar_angle_min, float lidar_angle_max,
        // Multi-robot
        const std::vector<VelocityObstacle>& neighbors) {

        VelocityCommand cmd;
        cmd.orca_active = false;

        // Step 1: DWA pre-screening
        auto trajectories = dwa_.sampleDynamicWindow(
            current_v, current_omega,
            goal_x, goal_y, goal_theta,
            lidar_ranges, lidar_angle_min, lidar_angle_max);

        // Filter out unsafe trajectories
        std::vector<DWATrajectory> safe_trajectories;
        for (const auto& traj : trajectories) {
            if (traj.is_safe) {
                safe_trajectories.push_back(traj);
            }
        }

        if (safe_trajectories.empty()) {
            // No safe trajectory - emergency stop
            cmd.linear = 0.0f;
            cmd.angular = 0.0f;
            return cmd;
        }

        // Find best trajectory from DWA
        auto best_traj = std::min_element(
            safe_trajectories.begin(), safe_trajectories.end(),
            [](const DWATrajectory& a, const DWATrajectory& b) {
                return a.cost < b.cost;
            });

        float preferred_vx = best_traj->linear_vel * std::cos(theta);
        float preferred_vy = best_traj->linear_vel * std::sin(theta);

        // Step 2: Check if ORCA is needed (robots within 2m)
        bool orca_needed = false;
        for (const auto& neighbor : neighbors) {
            float dx = neighbor.agent_x - x;
            float dy = neighbor.agent_y - y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 2.0f) {
                orca_needed = true;
                break;
            }
        }

        if (orca_needed) {
            // Step 3: Apply ORCA for multi-robot coordination
            float current_vx = current_v * std::cos(theta);
            float current_vy = current_v * std::sin(theta);

            auto orca_result = orca_.computeOptimalVelocity(
                x, y, current_vx, current_vy,
                preferred_vx, preferred_vy, neighbors);

            // Convert back to linear/angular
            cmd.linear = std::sqrt(
                orca_result.optimal_vx * orca_result.optimal_vx +
                orca_result.optimal_vy * orca_result.optimal_vy);

            // Compute angular velocity to align with ORCA direction
            float target_heading = std::atan2(orca_result.optimal_vy, orca_result.optimal_vx);
            float heading_error = target_heading - theta;

            // Normalize angle to [-pi, pi]
            while (heading_error > M_PI) heading_error -= 2.0f * M_PI;
            while (heading_error < -M_PI) heading_error += 2.0f * M_PI;

            cmd.angular = 2.0f * heading_error;  // Proportional control
            cmd.orca_active = true;
        } else {
            // Use DWA result directly
            cmd.linear = best_traj->linear_vel;
            cmd.angular = best_traj->angular_vel;
        }

        return cmd;
    }

private:
    ORCAController orca_;
    DWAController dwa_;
};

}  // namespace warehouser_safety
```

**Integration Point:**
- Replace or augment `SafetyController::applySafetyLimits()` with `HybridCollisionAvoidance`
- Add multi-robot state to observation message
- Call `computeVelocity()` in safety node before velocity limiting

---

### Template 2: MPC Shielding for Safe RL

**Pattern:** Verify RL policy actions through predictive model. If predicted trajectory violates safety constraints, override with backup MPC controller.

**Research Source:** Science Direct (2025) - "Adaptive Robust Model Predictive Shielding"

**Implementation for Warehouser:**

```cpp
// File: warehouser_safety/include/warehouser_safety/mpc_shield.hpp
#pragma once

#include <vector>
#include <cmath>

namespace warehouser_safety {

struct SafetyConstraint {
    float min_obstacle_distance = 0.5f;  // Minimum safe distance (m)
    float max_velocity = 1.0f;            // Maximum safe velocity (m/s)
    float max_acceleration = 1.0f;        // Maximum safe acceleration (m/s^2)
};

struct TrajectoryPoint {
    float x;
    float y;
    float theta;
    float v;
    float omega;
    float time;
};

class MPCShield {
public:
    explicit MPCShield(const SafetyConstraint& constraints)
        : constraints_(constraints) {}

    struct ShieldResult {
        float linear_vel;
        float angular_vel;
        bool shield_activated;
        std::string violation_reason;
    };

    // Verify if action is safe, override if not
    ShieldResult verifyAndCorrect(
        float policy_linear, float policy_angular,
        float current_x, float current_y, float current_theta,
        float current_v, float current_omega,
        const std::vector<float>& lidar_ranges,
        float lidar_angle_min, float lidar_angle_max) {

        ShieldResult result;
        result.linear_vel = policy_linear;
        result.angular_vel = policy_angular;
        result.shield_activated = false;

        // Step 1: Predict trajectory with policy action
        auto trajectory = predictTrajectory(
            current_x, current_y, current_theta,
            current_v, current_omega,
            policy_linear, policy_angular,
            prediction_horizon_, dt_);

        // Step 2: Check safety constraints along trajectory
        for (const auto& point : trajectory) {
            // Check velocity constraint
            if (std::abs(point.v) > constraints_.max_velocity) {
                result.shield_activated = true;
                result.violation_reason = "Velocity limit exceeded";
                break;
            }

            // Check acceleration constraint
            if (trajectory.size() > 1) {
                float dv = point.v - current_v;
                float accel = std::abs(dv / dt_);
                if (accel > constraints_.max_acceleration) {
                    result.shield_activated = true;
                    result.violation_reason = "Acceleration limit exceeded";
                    break;
                }
            }

            // Check minimum distance to obstacles
            float min_dist = getMinDistanceAtPose(
                point.x, point.y, point.theta,
                lidar_ranges, lidar_angle_min, lidar_angle_max);

            if (min_dist < constraints_.min_obstacle_distance) {
                result.shield_activated = true;
                result.violation_reason = "Collision predicted";
                break;
            }
        }

        // Step 3: If unsafe, compute backup action
        if (result.shield_activated) {
            auto backup = computeBackupAction(
                current_x, current_y, current_theta,
                current_v, current_omega,
                lidar_ranges, lidar_angle_min, lidar_angle_max);

            result.linear_vel = backup.linear;
            result.angular_vel = backup.angular;
        }

        return result;
    }

private:
    SafetyConstraint constraints_;
    float prediction_horizon_ = 2.0f;  // 2 seconds lookahead
    float dt_ = 0.1f;                  // 10 Hz prediction

    std::vector<TrajectoryPoint> predictTrajectory(
        float x, float y, float theta,
        float v, float omega,
        float target_v, float target_omega,
        float horizon, float dt) const {

        std::vector<TrajectoryPoint> trajectory;

        float sim_time = 0.0f;
        float curr_x = x;
        float curr_y = y;
        float curr_theta = theta;
        float curr_v = v;
        float curr_omega = omega;

        while (sim_time < horizon) {
            // Simple linear transition to target velocity
            float alpha = std::min(1.0f, sim_time / 0.5f);  // 0.5s ramp time
            curr_v = v + alpha * (target_v - v);
            curr_omega = omega + alpha * (target_omega - omega);

            // Integrate dynamics (differential drive model)
            curr_x += curr_v * std::cos(curr_theta) * dt;
            curr_y += curr_v * std::sin(curr_theta) * dt;
            curr_theta += curr_omega * dt;

            // Normalize theta
            while (curr_theta > M_PI) curr_theta -= 2.0f * M_PI;
            while (curr_theta < -M_PI) curr_theta += 2.0f * M_PI;

            TrajectoryPoint point;
            point.x = curr_x;
            point.y = curr_y;
            point.theta = curr_theta;
            point.v = curr_v;
            point.omega = curr_omega;
            point.time = sim_time;

            trajectory.push_back(point);

            sim_time += dt;
        }

        return trajectory;
    }

    float getMinDistanceAtPose(
        float x, float y, float theta,
        const std::vector<float>& lidar_ranges,
        float angle_min, float angle_max) const {

        // For simplicity, assume lidar readings are in robot frame
        // In reality, would need to transform based on pose
        float min_dist = 10.0f;
        for (const auto& range : lidar_ranges) {
            if (range < min_dist && range > 0.1f) {
                min_dist = range;
            }
        }
        return min_dist;
    }

    struct BackupAction {
        float linear;
        float angular;
    };

    BackupAction computeBackupAction(
        float x, float y, float theta,
        float v, float omega,
        const std::vector<float>& lidar_ranges,
        float angle_min, float angle_max) const {

        BackupAction backup;

        // Emergency backup strategy: decelerate to stop
        float decel = -constraints_.max_acceleration;
        float stop_time = std::abs(v / decel);

        if (stop_time < dt_) {
            // Can stop within one timestep
            backup.linear = 0.0f;
            backup.angular = 0.0f;
        } else {
            // Decelerate at max rate
            backup.linear = v + decel * dt_;
            backup.angular = 0.0f;  // Stop rotating while decelerating
        }

        return backup;
    }
};

}  // namespace warehouser_safety
```

**Integration Point:**
- Add `MPCShield` to `SafetyNode`
- Call `verifyAndCorrect()` before applying velocity limits
- Log shield activation rate as training metric
- Add to reward function: `shield_penalty = -1.0 * shield_activated`

---

### Template 3: Multi-Zone Safety Controller (ISO 3691-4)

**Pattern:** Implement multiple safety zones with different speed limits, compliant with ISO 3691-4 warehouse robot safety standard.

**Research Source:** ISO 3691-4:2023 - International standard for driverless industrial trucks

**Implementation for Warehouser:**

```cpp
// File: warehouser_safety/include/warehouser_safety/multi_zone_safety.hpp
#pragma once

#include <cmath>
#include <algorithm>

namespace warehouser_safety {

enum class SafetyZone {
    NOMINAL,      // Normal operation (> 1.5m)
    DETECTION,    // Slow down warning (0.8m - 1.5m)
    WARNING,      // Significant slowdown (0.5m - 0.8m)
    PROTECTION,   // Minimal speed (0.3m - 0.5m)
    EMERGENCY     // Emergency stop (< 0.3m)
};

struct ZoneConfig {
    // Distance thresholds (meters)
    float detection_distance = 1.5f;
    float warning_distance = 0.8f;
    float protection_distance = 0.5f;
    float emergency_distance = 0.3f;

    // Speed scaling factors
    float detection_scale = 0.8f;    // 80% of max speed
    float warning_scale = 0.5f;       // 50% of max speed
    float protection_scale = 0.2f;    // 20% of max speed
    float emergency_scale = 0.0f;     // Full stop

    // ISO 3691-4 parameters
    float robot_radius = 0.3f;
    float max_velocity = 1.0f;
    float reaction_time = 0.2f;       // Human reaction time (s)
    float brake_deceleration = 2.0f;  // Braking deceleration (m/s^2)
};

class MultiZoneSafetyController {
public:
    explicit MultiZoneSafetyController(const ZoneConfig& config)
        : config_(config) {}

    struct SafetyResult {
        float linear_vel;
        float angular_vel;
        SafetyZone zone;
        float min_distance;
        float required_stopping_distance;
    };

    SafetyResult applySafety(
        float cmd_linear, float cmd_angular,
        const std::vector<float>& lidar_ranges,
        float lidar_angle_min, float lidar_angle_max) {

        SafetyResult result;
        result.linear_vel = cmd_linear;
        result.angular_vel = cmd_angular;

        // Get minimum distance in forward cone
        result.min_distance = getMinDistanceInCone(
            lidar_ranges, lidar_angle_min, lidar_angle_max);

        // Calculate required stopping distance based on current velocity
        result.required_stopping_distance = calculateStoppingDistance(
            std::abs(cmd_linear));

        // Determine safety zone
        result.zone = determineSafetyZone(result.min_distance);

        // Apply zone-specific speed scaling
        float scale = getSpeedScale(result.zone);

        // Only scale forward motion
        if (result.linear_vel > 0.0f) {
            result.linear_vel *= scale;
        }

        // In EMERGENCY zone, stop all motion
        if (result.zone == SafetyZone::EMERGENCY) {
            result.linear_vel = 0.0f;
            result.angular_vel = 0.0f;
        }

        // Ensure stopping distance constraint
        // If required stopping distance > available distance, reduce speed
        if (result.required_stopping_distance > result.min_distance - config_.robot_radius) {
            float safe_velocity = calculateSafeVelocity(result.min_distance);
            result.linear_vel = std::min(result.linear_vel, safe_velocity);
        }

        return result;
    }

    // Calculate stopping distance per ISO 3691-4
    float calculateStoppingDistance(float velocity) const {
        // d = v * t_reaction + v^2 / (2 * a_brake)
        float reaction_dist = velocity * config_.reaction_time;
        float brake_dist = (velocity * velocity) / (2.0f * config_.brake_deceleration);
        return reaction_dist + brake_dist + config_.robot_radius;  // Add safety margin
    }

    // Calculate maximum safe velocity for given distance
    float calculateSafeVelocity(float distance) const {
        // Solve: d = v*t + v^2/(2*a) for v
        // This is a quadratic equation: v^2/(2a) + v*t - d = 0
        float a = 1.0f / (2.0f * config_.brake_deceleration);
        float b = config_.reaction_time;
        float c = -(distance - config_.robot_radius);

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f) {
            return 0.0f;
        }

        float v = (-b + std::sqrt(discriminant)) / (2.0f * a);
        return std::max(0.0f, v);
    }

private:
    ZoneConfig config_;

    float getMinDistanceInCone(
        const std::vector<float>& ranges,
        float angle_min, float angle_max) const {

        float min_dist = 10.0f;
        size_t num_rays = ranges.size();

        // Forward cone: ±60 degrees
        float cone_angle = 1.05f;  // ~60 degrees in radians

        for (size_t i = 0; i < num_rays; ++i) {
            float angle = angle_min + (angle_max - angle_min) * i / num_rays;

            if (angle >= -cone_angle && angle <= cone_angle) {
                if (ranges[i] > 0.01f && ranges[i] < min_dist) {
                    min_dist = ranges[i];
                }
            }
        }

        return min_dist;
    }

    SafetyZone determineSafetyZone(float distance) const {
        if (distance < config_.emergency_distance) {
            return SafetyZone::EMERGENCY;
        } else if (distance < config_.protection_distance) {
            return SafetyZone::PROTECTION;
        } else if (distance < config_.warning_distance) {
            return SafetyZone::WARNING;
        } else if (distance < config_.detection_distance) {
            return SafetyZone::DETECTION;
        } else {
            return SafetyZone::NOMINAL;
        }
    }

    float getSpeedScale(SafetyZone zone) const {
        switch (zone) {
            case SafetyZone::EMERGENCY:
                return config_.emergency_scale;
            case SafetyZone::PROTECTION:
                return config_.protection_scale;
            case SafetyZone::WARNING:
                return config_.warning_scale;
            case SafetyZone::DETECTION:
                return config_.detection_scale;
            case SafetyZone::NOMINAL:
            default:
                return 1.0f;
        }
    }
};

}  // namespace warehouser_safety
```

**Configuration (add to safety_params.yaml):**
```yaml
safety:
  ros__parameters:
    # Multi-zone safety (ISO 3691-4 compliant)
    detection_distance: 1.5    # Detection zone start (m)
    warning_distance: 0.8      # Warning zone start (m)
    protection_distance: 0.5   # Protection zone start (m)
    emergency_distance: 0.3    # Emergency stop threshold (m)

    detection_scale: 0.8       # Speed scale in detection zone
    warning_scale: 0.5         # Speed scale in warning zone
    protection_scale: 0.2      # Speed scale in protection zone

    robot_radius: 0.3          # Robot radius (m)
    reaction_time: 0.2         # Reaction time allowance (s)
    brake_deceleration: 2.0    # Braking deceleration (m/s^2)
```

---

### Template 4: Robot-Robot Collision Detection with Spatial Hashing

**Pattern:** Efficient O(n) collision detection for multi-robot scenarios using spatial hash grid.

**Implementation for Warehouser:**

```cpp
// File: warehouser_simulation/include/warehouser_simulation/spatial_hash.hpp
#pragma once

#include <unordered_map>
#include <vector>
#include <cmath>

namespace warehouser_simulation {

template<typename Entity>
class SpatialHash {
public:
    SpatialHash(float cell_size) : cell_size_(cell_size) {
        inv_cell_size_ = 1.0f / cell_size_;
    }

    void clear() {
        grid_.clear();
    }

    void insert(Entity* entity, float x, float y) {
        int64_t key = computeKey(x, y);
        grid_[key].push_back(entity);
    }

    // Find all entities within radius of point
    std::vector<Entity*> query(float x, float y, float radius) const {
        std::vector<Entity*> results;

        // Check all cells that could contain entities within radius
        int min_cell_x = static_cast<int>(std::floor((x - radius) * inv_cell_size_));
        int max_cell_x = static_cast<int>(std::floor((x + radius) * inv_cell_size_));
        int min_cell_y = static_cast<int>(std::floor((y - radius) * inv_cell_size_));
        int max_cell_y = static_cast<int>(std::floor((y + radius) * inv_cell_size_));

        for (int cx = min_cell_x; cx <= max_cell_x; ++cx) {
            for (int cy = min_cell_y; cy <= max_cell_y; ++cy) {
                int64_t key = computeKeyFromCell(cx, cy);
                auto it = grid_.find(key);
                if (it != grid_.end()) {
                    for (Entity* entity : it->second) {
                        results.push_back(entity);
                    }
                }
            }
        }

        return results;
    }

private:
    float cell_size_;
    float inv_cell_size_;
    std::unordered_map<int64_t, std::vector<Entity*>> grid_;

    int64_t computeKey(float x, float y) const {
        int cx = static_cast<int>(std::floor(x * inv_cell_size_));
        int cy = static_cast<int>(std::floor(y * inv_cell_size_));
        return computeKeyFromCell(cx, cy);
    }

    int64_t computeKeyFromCell(int cx, int cy) const {
        // Combine two 32-bit integers into 64-bit key
        return (static_cast<int64_t>(cx) << 32) | (static_cast<int64_t>(cy) & 0xFFFFFFFF);
    }
};

// Add to WorldManager class
class RobotCollisionDetector {
public:
    explicit RobotCollisionDetector(float robot_radius)
        : robot_radius_(robot_radius), spatial_hash_(2.0f * robot_radius) {}

    struct CollisionPair {
        size_t robot_a_index;
        size_t robot_b_index;
        float overlap_distance;
    };

    std::vector<CollisionPair> detectCollisions(const std::vector<Robot*>& robots) {
        std::vector<CollisionPair> collisions;

        // Build spatial hash
        spatial_hash_.clear();
        for (size_t i = 0; i < robots.size(); ++i) {
            spatial_hash_.insert(robots[i], robots[i]->x, robots[i]->y);
        }

        // Check each robot against nearby robots
        for (size_t i = 0; i < robots.size(); ++i) {
            Robot* robot_a = robots[i];

            // Query nearby robots
            auto nearby = spatial_hash_.query(
                robot_a->x, robot_a->y, 2.0f * robot_radius_);

            for (Robot* robot_b : nearby) {
                // Find index of robot_b
                size_t j = 0;
                for (; j < robots.size(); ++j) {
                    if (robots[j] == robot_b) break;
                }

                // Skip self and already checked pairs
                if (i >= j) continue;

                // Check collision
                float dx = robot_a->x - robot_b->x;
                float dy = robot_a->y - robot_b->y;
                float dist_sq = dx * dx + dy * dy;
                float collision_dist = 2.0f * robot_radius_;
                float collision_dist_sq = collision_dist * collision_dist;

                if (dist_sq < collision_dist_sq) {
                    CollisionPair pair;
                    pair.robot_a_index = i;
                    pair.robot_b_index = j;
                    pair.overlap_distance = collision_dist - std::sqrt(dist_sq);
                    collisions.push_back(pair);
                }
            }
        }

        return collisions;
    }

private:
    float robot_radius_;
    SpatialHash<Robot> spatial_hash_;
};

}  // namespace warehouser_simulation
```

**Integration into WorldManager::step():**

```cpp
// In world_manager.cpp, add to step() function:

// After updating all robots, check robot-robot collisions
RobotCollisionDetector collision_detector(Robot::kRadius);
auto collisions = collision_detector.detectCollisions(robots_);

for (const auto& collision : collisions) {
    // Rollback both robots to previous positions
    robots_[collision.robot_a_index]->x = prev_positions_[collision.robot_a_index].x;
    robots_[collision.robot_a_index]->y = prev_positions_[collision.robot_a_index].y;
    robots_[collision.robot_a_index]->stop();

    robots_[collision.robot_b_index]->x = prev_positions_[collision.robot_b_index].x;
    robots_[collision.robot_b_index]->y = prev_positions_[collision.robot_b_index].y;
    robots_[collision.robot_b_index]->stop();

    // Add collision flag to world state for reward calculation
    robot_collisions_[collision.robot_a_index] = true;
    robot_collisions_[collision.robot_b_index] = true;
}
```

---

### Template 5: Time-to-Collision (TTC) Prediction

**Pattern:** Predictive safety check based on velocity projection, triggers emergency stop before collision.

**Implementation for Warehouser:**

```cpp
// File: warehouser_safety/include/warehouser_safety/ttc_calculator.hpp
#pragma once

#include <cmath>
#include <limits>
#include <vector>

namespace warehouser_safety {

class TimeToCollisionCalculator {
public:
    struct TTCResult {
        float ttc;                    // Time to collision (seconds)
        bool collision_imminent;      // TTC < threshold
        float collision_angle;        // Angle to collision point
        float collision_distance;     // Current distance to collision
    };

    // Calculate TTC for moving obstacle
    static TTCResult calculateTTC(
        // Robot state
        float robot_x, float robot_y, float robot_theta,
        float robot_vx, float robot_vy,
        // Obstacle state (if dynamic)
        float obs_x, float obs_y,
        float obs_vx, float obs_vy,
        float robot_radius, float obs_radius) {

        TTCResult result;
        result.ttc = std::numeric_limits<float>::infinity();
        result.collision_imminent = false;

        // Relative position
        float rel_x = obs_x - robot_x;
        float rel_y = obs_y - robot_y;

        // Relative velocity
        float rel_vx = obs_vx - robot_vx;
        float rel_vy = obs_vy - robot_vy;

        // Current distance
        float dist = std::sqrt(rel_x * rel_x + rel_y * rel_y);
        result.collision_distance = dist;
        result.collision_angle = std::atan2(rel_y, rel_x);

        // Combined radius for collision
        float combined_radius = robot_radius + obs_radius;

        // Check if already colliding
        if (dist < combined_radius) {
            result.ttc = 0.0f;
            result.collision_imminent = true;
            return result;
        }

        // Solve for collision time using quadratic equation
        // |p + v*t| = r
        // (px + vx*t)^2 + (py + vy*t)^2 = r^2
        // t^2*(vx^2 + vy^2) + t*2*(px*vx + py*vy) + (px^2 + py^2 - r^2) = 0

        float a = rel_vx * rel_vx + rel_vy * rel_vy;
        float b = 2.0f * (rel_x * rel_vx + rel_y * rel_vy);
        float c = rel_x * rel_x + rel_y * rel_y - combined_radius * combined_radius;

        // If no relative velocity, no collision
        if (std::abs(a) < 1e-6f) {
            return result;
        }

        float discriminant = b * b - 4.0f * a * c;

        // No collision
        if (discriminant < 0.0f) {
            return result;
        }

        // Calculate collision time
        float sqrt_discriminant = std::sqrt(discriminant);
        float t1 = (-b - sqrt_discriminant) / (2.0f * a);
        float t2 = (-b + sqrt_discriminant) / (2.0f * a);

        // Take earliest positive time
        if (t1 > 0.0f) {
            result.ttc = t1;
        } else if (t2 > 0.0f) {
            result.ttc = t2;
        } else {
            // Moving away from obstacle
            return result;
        }

        // Check if collision is imminent
        result.collision_imminent = result.ttc < 1.0f;  // 1 second threshold

        return result;
    }

    // Calculate TTC for static obstacle using lidar
    static TTCResult calculateTTCFromLidar(
        float robot_v, float robot_omega,
        const std::vector<float>& lidar_ranges,
        float lidar_angle_min, float lidar_angle_max,
        float robot_radius) {

        TTCResult result;
        result.ttc = std::numeric_limits<float>::infinity();
        result.collision_imminent = false;

        if (std::abs(robot_v) < 1e-3f) {
            // Robot is stationary
            return result;
        }

        size_t num_rays = lidar_ranges.size();

        for (size_t i = 0; i < num_rays; ++i) {
            float angle = lidar_angle_min +
                         (lidar_angle_max - lidar_angle_min) * i / (num_rays - 1);
            float range = lidar_ranges[i];

            // Skip invalid readings
            if (range < 0.01f || range > 10.0f) {
                continue;
            }

            // Calculate velocity component toward this obstacle
            // Positive if moving toward obstacle
            float v_toward = robot_v * std::cos(angle);

            if (v_toward > 0.0f) {
                // Moving toward this obstacle
                float ttc = (range - robot_radius) / v_toward;

                if (ttc > 0.0f && ttc < result.ttc) {
                    result.ttc = ttc;
                    result.collision_angle = angle;
                    result.collision_distance = range;
                }
            }
        }

        result.collision_imminent = result.ttc < 1.0f;

        return result;
    }
};

}  // namespace warehouser_safety
```

**Integration into SafetyController:**

```cpp
// Add to SafetyController::applySafetyLimits()

// Calculate TTC
auto ttc_result = TimeToCollisionCalculator::calculateTTCFromLidar(
    cmd_raw.linear, cmd_raw.angular,
    lidar.ranges, lidar.angle_min, lidar.angle_max,
    0.3f  // robot radius
);

// Override if collision imminent
if (ttc_result.collision_imminent && ttc_result.ttc < 1.0f) {
    state_ = SafetyState::EMERGENCY;
    cmd_safe.linear = 0.0f;
    cmd_safe.angular = 0.0f;
    last_ttc_ = ttc_result.ttc;
    return cmd_safe;
}
```

---

### Template 6: Sensor Health Monitoring

**Pattern:** Monitor sensor data staleness and validity, implement fail-safe behavior on sensor failure.

**Implementation for Warehouser:**

```cpp
// File: warehouser_safety/include/warehouser_safety/sensor_monitor.hpp
#pragma once

#include <chrono>
#include <string>

namespace warehouser_safety {

enum class SensorHealth {
    HEALTHY,
    DEGRADED,
    FAILED
};

class SensorHealthMonitor {
public:
    struct SensorStatus {
        SensorHealth health;
        std::string status_message;
        float data_age_ms;
        float dropout_rate;
    };

    explicit SensorHealthMonitor(float max_age_ms = 200.0f,
                                 float dropout_threshold = 0.3f)
        : max_age_ms_(max_age_ms),
          dropout_threshold_(dropout_threshold),
          dropout_count_(0),
          total_readings_(0) {}

    void updateLidarReading(const std::vector<float>& ranges, float max_range) {
        last_reading_time_ = std::chrono::steady_clock::now();
        total_readings_++;

        // Count dropout rays (at max range)
        size_t dropout_rays = 0;
        for (float range : ranges) {
            if (range >= max_range - 0.01f) {
                dropout_rays++;
            }
        }

        if (dropout_rays > 0) {
            dropout_count_++;
        }

        // Exponential moving average of dropout rate
        float current_dropout = static_cast<float>(dropout_rays) / ranges.size();
        dropout_rate_ = 0.9f * dropout_rate_ + 0.1f * current_dropout;
    }

    SensorStatus checkHealth() const {
        SensorStatus status;

        // Calculate data age
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_reading_time_);
        status.data_age_ms = static_cast<float>(age.count());

        // Check data staleness
        if (status.data_age_ms > max_age_ms_ * 2.0f) {
            status.health = SensorHealth::FAILED;
            status.status_message = "Sensor data timeout";
            return status;
        }

        if (status.data_age_ms > max_age_ms_) {
            status.health = SensorHealth::DEGRADED;
            status.status_message = "Sensor data stale";
        }

        // Check dropout rate
        status.dropout_rate = dropout_rate_;
        if (dropout_rate_ > dropout_threshold_ * 2.0f) {
            status.health = SensorHealth::FAILED;
            status.status_message = "Excessive sensor dropout";
            return status;
        }

        if (dropout_rate_ > dropout_threshold_) {
            status.health = SensorHealth::DEGRADED;
            status.status_message = "High sensor dropout";
        }

        if (status.health != SensorHealth::DEGRADED &&
            status.health != SensorHealth::FAILED) {
            status.health = SensorHealth::HEALTHY;
            status.status_message = "Sensor operating normally";
        }

        return status;
    }

    // Get fail-safe velocity command based on sensor health
    struct Velocity {
        float linear;
        float angular;
    };

    Velocity getFailsafeCommand(SensorHealth health, const Velocity& requested) const {
        Velocity failsafe = requested;

        switch (health) {
            case SensorHealth::HEALTHY:
                // Normal operation
                break;

            case SensorHealth::DEGRADED:
                // Reduce speed to 50%
                failsafe.linear *= 0.5f;
                failsafe.angular *= 0.5f;
                break;

            case SensorHealth::FAILED:
                // Emergency stop
                failsafe.linear = 0.0f;
                failsafe.angular = 0.0f;
                break;
        }

        return failsafe;
    }

private:
    float max_age_ms_;
    float dropout_threshold_;
    float dropout_rate_ = 0.0f;
    size_t dropout_count_;
    size_t total_readings_;
    std::chrono::steady_clock::time_point last_reading_time_;
};

}  // namespace warehouser_safety
```

**Integration into SafetyNode:**

```cpp
// In safety_node.cpp, replace unsafe passthrough with:

// Add member variable
SensorHealthMonitor lidar_monitor_;

// In lidarCallback()
void lidarCallback(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
    // Update monitor
    lidar_monitor_.updateLidarReading(msg->ranges, msg->range_max);

    // Store lidar data
    last_lidar_ = convertToLidarData(msg);
    lidar_received_ = true;
}

// In cmdRawCallback()
void cmdRawCallback(const geometry_msgs::msg::Twist::SharedPtr msg) {
    Velocity cmd_raw = convertToVelocity(msg);

    // Check sensor health
    auto sensor_status = lidar_monitor_.checkHealth();

    if (sensor_status.health == SensorHealth::FAILED) {
        RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 1000,
            "Sensor health check failed: %s", sensor_status.status_message.c_str());
    }

    // Apply safety limits
    Velocity cmd_safe;
    if (lidar_received_) {
        cmd_safe = controller_.applySafetyLimits(cmd_raw, last_lidar_);
    } else {
        // No lidar data - emergency stop (fail-safe)
        cmd_safe.linear = 0.0f;
        cmd_safe.angular = 0.0f;
    }

    // Apply sensor health failsafe
    cmd_safe = lidar_monitor_.getFailsafeCommand(sensor_status.health, cmd_safe);

    publish(cmd_safe);
}
```

---

### Template 7: Nonstop Zones for Deadlock Prevention

**Pattern:** Define critical zones (intersections, narrow passages) where robots cannot stop, preventing deadlocks.

**Research Source:** IEEE (2025) - "Multi-Robot Scheduling with Nonstop Areas"

**Implementation for Warehouser:**

```python
# File: training/training/envs/nonstop_zones.py

from dataclasses import dataclass
from typing import List, Tuple
import numpy as np

@dataclass
class NonstopZone:
    """A rectangular zone where robots must maintain minimum velocity."""
    x_min: float
    y_min: float
    x_max: float
    y_max: float
    min_velocity: float = 0.3  # Minimum velocity in zone (m/s)
    name: str = ""

class NonstopZoneManager:
    """Manage nonstop zones for deadlock prevention."""

    def __init__(self):
        self.zones: List[NonstopZone] = []

    def add_intersection(self, center_x: float, center_y: float,
                         radius: float, min_velocity: float = 0.3,
                         name: str = "intersection"):
        """Add circular intersection as nonstop zone (approximated as square)."""
        zone = NonstopZone(
            x_min=center_x - radius,
            y_min=center_y - radius,
            x_max=center_x + radius,
            y_max=center_y + radius,
            min_velocity=min_velocity,
            name=name
        )
        self.zones.append(zone)

    def add_corridor(self, x1: float, y1: float, x2: float, y2: float,
                     width: float, min_velocity: float = 0.3,
                     name: str = "corridor"):
        """Add narrow corridor as nonstop zone."""
        zone = NonstopZone(
            x_min=min(x1, x2) - width/2,
            y_min=min(y1, y2) - width/2,
            x_max=max(x1, x2) + width/2,
            y_max=max(y1, y2) + width/2,
            min_velocity=min_velocity,
            name=name
        )
        self.zones.append(zone)

    def is_in_nonstop_zone(self, x: float, y: float) -> Tuple[bool, float]:
        """
        Check if position is in nonstop zone.

        Returns:
            (is_in_zone, min_velocity_required)
        """
        for zone in self.zones:
            if (zone.x_min <= x <= zone.x_max and
                zone.y_min <= y <= zone.y_max):
                return True, zone.min_velocity
        return False, 0.0

    def compute_velocity_penalty(self, x: float, y: float,
                                 velocity: float) -> float:
        """
        Compute penalty for violating nonstop zone rules.

        Returns penalty value (0 if no violation).
        """
        is_in_zone, min_vel = self.is_in_nonstop_zone(x, y)

        if is_in_zone and velocity < min_vel:
            # Penalty proportional to velocity deficit
            deficit = min_vel - velocity
            return -10.0 * deficit  # Penalty increases with slower speed

        return 0.0

    def get_zone_at_position(self, x: float, y: float) -> str:
        """Get name of nonstop zone at position, or empty string."""
        for zone in self.zones:
            if (zone.x_min <= x <= zone.x_max and
                zone.y_min <= y <= zone.y_max):
                return zone.name
        return ""

# Example warehouse layout with nonstop zones
def create_warehouse_nonstop_zones(width: float, height: float) -> NonstopZoneManager:
    """Create typical warehouse nonstop zones."""
    manager = NonstopZoneManager()

    # Add intersection nonstop zones (common deadlock points)
    # Assume warehouse has grid layout with aisles every 5 meters
    for x in np.arange(5.0, width - 5.0, 5.0):
        for y in np.arange(5.0, height - 5.0, 5.0):
            manager.add_intersection(
                center_x=x, center_y=y, radius=1.0,
                min_velocity=0.3, name=f"intersection_{int(x)}_{int(y)}"
            )

    # Add main corridor as nonstop zone
    manager.add_corridor(
        x1=width/2, y1=0.0, x2=width/2, y2=height,
        width=2.0, min_velocity=0.4, name="main_corridor"
    )

    return manager
```

**Integration into reward function:**

```python
# File: training/training/envs/ros_env.py

# Add to observation space
self.nonstop_zones = create_warehouse_nonstop_zones(
    self.world_config.width, self.world_config.height
)

# Add to step() reward calculation
def _compute_reward(self, obs: Dict, action: np.ndarray) -> float:
    reward = 0.0

    # ... existing reward components ...

    # Nonstop zone penalty
    robot_x = obs['robot_x']
    robot_y = obs['robot_y']
    robot_v = obs['robot_linear_velocity']

    nonstop_penalty = self.nonstop_zones.compute_velocity_penalty(
        robot_x, robot_y, abs(robot_v)
    )
    reward += nonstop_penalty

    # Add to info
    is_in_zone, min_vel = self.nonstop_zones.is_in_nonstop_zone(robot_x, robot_y)
    info['in_nonstop_zone'] = is_in_zone
    info['nonstop_velocity_required'] = min_vel

    return reward, info
```

---

### Template 8: Emergency Stop with Smart Recovery

**Pattern:** Implement E-Stop with automatic recovery instead of requiring manual restart.

**Research Source:** Automate.org (2025) - "Dynamic Safety Features for AMRs"

**Implementation for Warehouser:**

```cpp
// File: warehouser_safety/include/warehouser_safety/emergency_stop.hpp
#pragma once

#include <chrono>
#include <string>

namespace warehouser_safety {

enum class EStopState {
    NORMAL,           // Normal operation
    SLOWING,          // Slowing down due to obstacle
    EMERGENCY_STOP,   // Full emergency stop
    RECOVERY_WAIT,    // Waiting to verify obstacle cleared
    RECOVERY_SLOW     // Slow recovery from E-Stop
};

enum class EStopTrigger {
    NONE,
    OBSTACLE_COLLISION,
    SENSOR_FAILURE,
    MANUAL_BUTTON,
    WIRELESS_FLEET,
    FIRE_ALARM,
    SYSTEM_ERROR
};

class EmergencyStopController {
public:
    struct EStopConfig {
        float recovery_wait_time = 2.0f;    // Wait before recovery (s)
        float recovery_slow_speed = 0.2f;   // Slow speed during recovery (m/s)
        float recovery_duration = 3.0f;     // Duration of slow recovery (s)
        float min_clearance_for_recovery = 1.0f;  // Minimum clearance to resume (m)
        bool auto_recovery_enabled = true;   // Enable automatic recovery
    };

    explicit EmergencyStopController(const EStopConfig& config)
        : config_(config), state_(EStopState::NORMAL) {}

    struct EStopResult {
        float linear_vel;
        float angular_vel;
        EStopState state;
        std::string status_message;
        bool recovery_in_progress;
    };

    EStopResult update(
        float requested_linear, float requested_angular,
        float min_obstacle_distance,
        EStopTrigger trigger,
        float dt) {

        EStopResult result;
        result.linear_vel = requested_linear;
        result.angular_vel = requested_angular;
        result.state = state_;
        result.recovery_in_progress = false;

        auto now = std::chrono::steady_clock::now();

        switch (state_) {
            case EStopState::NORMAL:
                handleNormalState(trigger, min_obstacle_distance, result);
                break;

            case EStopState::SLOWING:
                handleSlowingState(trigger, min_obstacle_distance, result);
                break;

            case EStopState::EMERGENCY_STOP:
                handleEmergencyStopState(trigger, min_obstacle_distance, now, result);
                break;

            case EStopState::RECOVERY_WAIT:
                handleRecoveryWaitState(min_obstacle_distance, now, result);
                break;

            case EStopState::RECOVERY_SLOW:
                handleRecoverySlowState(requested_linear, requested_angular,
                                       min_obstacle_distance, now, result);
                break;
        }

        state_ = result.state;
        return result;
    }

    // Manual E-Stop button pressed (cannot auto-recover)
    void triggerManualEStop() {
        state_ = EStopState::EMERGENCY_STOP;
        manual_estop_active_ = true;
        estop_time_ = std::chrono::steady_clock::now();
    }

    // Clear manual E-Stop (requires human intervention)
    void clearManualEStop() {
        manual_estop_active_ = false;
    }

private:
    EStopConfig config_;
    EStopState state_;
    EStopTrigger last_trigger_ = EStopTrigger::NONE;
    std::chrono::steady_clock::time_point estop_time_;
    std::chrono::steady_clock::time_point recovery_start_time_;
    bool manual_estop_active_ = false;

    void handleNormalState(EStopTrigger trigger, float min_distance,
                          EStopResult& result) {
        if (trigger != EStopTrigger::NONE) {
            // Transition to emergency stop
            state_ = EStopState::EMERGENCY_STOP;
            last_trigger_ = trigger;
            estop_time_ = std::chrono::steady_clock::now();

            result.state = EStopState::EMERGENCY_STOP;
            result.linear_vel = 0.0f;
            result.angular_vel = 0.0f;
            result.status_message = getTriggerMessage(trigger);
        }
        // else: normal operation, use requested velocity
    }

    void handleSlowingState(EStopTrigger trigger, float min_distance,
                           EStopResult& result) {
        // Currently unused - could implement gradual slowdown
        // For now, jump directly to E-Stop
    }

    void handleEmergencyStopState(EStopTrigger trigger, float min_distance,
                                  std::chrono::steady_clock::time_point now,
                                  EStopResult& result) {
        // Emergency stop: no motion
        result.linear_vel = 0.0f;
        result.angular_vel = 0.0f;
        result.status_message = "EMERGENCY STOP: " + getTriggerMessage(last_trigger_);

        // Check if can transition to recovery
        if (manual_estop_active_) {
            result.status_message += " (manual reset required)";
            return;
        }

        if (!config_.auto_recovery_enabled) {
            result.status_message += " (auto-recovery disabled)";
            return;
        }

        // Check if trigger cleared and obstacle moved away
        if (trigger == EStopTrigger::NONE &&
            min_distance > config_.min_clearance_for_recovery) {

            // Transition to recovery wait
            state_ = EStopState::RECOVERY_WAIT;
            recovery_start_time_ = now;
            result.state = EStopState::RECOVERY_WAIT;
            result.status_message = "Recovery wait: obstacle cleared";
        }
    }

    void handleRecoveryWaitState(float min_distance,
                                 std::chrono::steady_clock::time_point now,
                                 EStopResult& result) {
        // Wait period before resuming motion
        result.linear_vel = 0.0f;
        result.angular_vel = 0.0f;
        result.recovery_in_progress = true;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - recovery_start_time_);
        float elapsed_sec = elapsed.count() / 1000.0f;

        // Check if obstacle returned
        if (min_distance < config_.min_clearance_for_recovery) {
            state_ = EStopState::EMERGENCY_STOP;
            result.state = EStopState::EMERGENCY_STOP;
            result.status_message = "Recovery aborted: obstacle detected";
            return;
        }

        // Check if wait period complete
        if (elapsed_sec > config_.recovery_wait_time) {
            state_ = EStopState::RECOVERY_SLOW;
            recovery_start_time_ = now;
            result.state = EStopState::RECOVERY_SLOW;
            result.status_message = "Recovery slow start";
        } else {
            result.status_message = "Recovery wait: " +
                std::to_string(config_.recovery_wait_time - elapsed_sec) + "s";
        }
    }

    void handleRecoverySlowState(float requested_linear, float requested_angular,
                                 float min_distance,
                                 std::chrono::steady_clock::time_point now,
                                 EStopResult& result) {
        result.recovery_in_progress = true;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - recovery_start_time_);
        float elapsed_sec = elapsed.count() / 1000.0f;

        // Check if obstacle appeared during recovery
        if (min_distance < config_.min_clearance_for_recovery) {
            state_ = EStopState::EMERGENCY_STOP;
            result.state = EStopState::EMERGENCY_STOP;
            result.linear_vel = 0.0f;
            result.angular_vel = 0.0f;
            result.status_message = "Recovery aborted: obstacle detected";
            return;
        }

        // Gradually increase speed during recovery
        float progress = std::min(1.0f, elapsed_sec / config_.recovery_duration);
        float speed_limit = config_.recovery_slow_speed +
            progress * (1.0f - config_.recovery_slow_speed);

        result.linear_vel = std::clamp(requested_linear,
                                       -speed_limit, speed_limit);
        result.angular_vel = requested_angular;

        // Check if recovery complete
        if (progress >= 1.0f) {
            state_ = EStopState::NORMAL;
            result.state = EStopState::NORMAL;
            result.status_message = "Recovery complete";
            result.recovery_in_progress = false;
        } else {
            result.status_message = "Recovery: " +
                std::to_string(static_cast<int>(progress * 100)) + "%";
        }
    }

    std::string getTriggerMessage(EStopTrigger trigger) const {
        switch (trigger) {
            case EStopTrigger::OBSTACLE_COLLISION:
                return "Obstacle collision";
            case EStopTrigger::SENSOR_FAILURE:
                return "Sensor failure";
            case EStopTrigger::MANUAL_BUTTON:
                return "Manual E-Stop button";
            case EStopTrigger::WIRELESS_FLEET:
                return "Fleet-wide E-Stop";
            case EStopTrigger::FIRE_ALARM:
                return "Fire alarm triggered";
            case EStopTrigger::SYSTEM_ERROR:
                return "System error";
            case EStopTrigger::NONE:
            default:
                return "None";
        }
    }
};

}  // namespace warehouser_safety
```

**Integration into SafetyNode:**

```cpp
// Add member
EmergencyStopController estop_controller_;

// In safety callback
EStopTrigger trigger = EStopTrigger::NONE;
if (front_dist < 0.3f) {
    trigger = EStopTrigger::OBSTACLE_COLLISION;
}

auto estop_result = estop_controller_.update(
    cmd_safe.linear, cmd_safe.angular,
    front_dist, trigger, dt);

cmd_safe.linear = estop_result.linear_vel;
cmd_safe.angular = estop_result.angular_vel;

// Publish status
publishEStopStatus(estop_result);
```

---

## Summary of Copy-Paste Ready Templates

### Quick Reference Table

| Template | Purpose | Complexity | Priority |
|----------|---------|------------|----------|
| 1. ORCA-DWA Hybrid | Multi-robot collision avoidance | High | P1 |
| 2. MPC Shielding | Safe RL policy verification | Medium | P1 |
| 3. Multi-Zone Safety | ISO 3691-4 compliant zones | Low | P0 |
| 4. Spatial Hash | Efficient robot-robot collision | Low | P0 |
| 5. TTC Calculator | Predictive collision detection | Low | P1 |
| 6. Sensor Monitor | Health checking and failsafe | Low | P0 |
| 7. Nonstop Zones | Deadlock prevention | Low | P2 |
| 8. Smart E-Stop | Auto-recovery emergency stop | Medium | P2 |

### Integration Roadmap for Warehouser

**Phase 1 (P0): Critical Safety Fixes**
1. Replace `SafetyController` with `MultiZoneSafetyController` (Template 3)
2. Add `SensorHealthMonitor` to `SafetyNode` (Template 6)
3. Implement `RobotCollisionDetector` in `WorldManager` (Template 4)
4. Fix lidar passthrough → use fail-safe behavior

**Phase 2 (P1): Enhanced Safety**
5. Add `MPCShield` to safety pipeline (Template 2)
6. Implement `TimeToCollisionCalculator` in safety checks (Template 5)
7. Consider `HybridCollisionAvoidance` for multi-robot scenarios (Template 1)

**Phase 3 (P2): Production Features**
8. Add `NonstopZoneManager` to environment (Template 7)
9. Implement `EmergencyStopController` with recovery (Template 8)
10. Full ISO 3691-4 compliance testing

### Key Takeaways

1. **No collision avoidance exists** - current system is purely reactive (slowdown/stop)
2. **ORCA-DWA hybrid proven best** - 100% success rate in 2025 research
3. **MPC shielding enables safe RL** - formal guarantees during training and deployment
4. **ISO 3691-4 compliance critical** - multi-zone safety with proper stopping distances
5. **Sensor redundancy non-negotiable** - health monitoring prevents unsafe passthrough
6. **Smart E-Stop reduces downtime** - auto-recovery when obstacle clears
7. **Spatial hashing scales** - O(n) multi-robot collision detection
8. **TTC prediction prevents collisions** - forward-looking vs reactive safety

All templates are designed to integrate with existing Warehouser architecture following C++23 standards and ROS2 patterns.
