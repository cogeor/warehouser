#pragma once

#include <chrono>
#include <random>
#include <vector>

namespace warehouser {

/// Configuration for Gaussian noise generation with optional dropout
struct NoiseConfig {
    float mean = 0.0f;            // Noise mean (bias)
    float stddev = 0.0f;          // Standard deviation
    float dropout_prob = 0.0f;    // Probability of dropout [0, 1]
    float dropout_value = 0.0f;   // Value to use on dropout
    bool enabled = false;         // Master enable flag
};

/// Gaussian noise model with optional dropout.
///
/// Used for domain randomization in sim-to-real transfer.
/// Supports reproducibility via seed setting.
class NoiseModel {
public:
    explicit NoiseModel(const NoiseConfig& config = {});

    /// Apply noise to a single value
    /// @param value Input value
    /// @return Noisy value (or dropout_value if dropout occurs)
    float apply(float value);

    /// Apply noise to a vector of values (in-place modification)
    /// @param values Vector of values to add noise to
    void applyVector(std::vector<float>& values);

    /// Check if dropout should occur for this sample
    /// @return true if dropout should occur
    bool shouldDropout();

    /// Set random seed for reproducibility
    /// @param seed Random seed value
    void setSeed(unsigned int seed);

    /// Update configuration
    /// @param config New configuration
    void setConfig(const NoiseConfig& config);

    /// Get current configuration
    const NoiseConfig& config() const { return config_; }

    /// Check if noise is enabled
    bool isEnabled() const { return config_.enabled; }

private:
    NoiseConfig config_;
    std::mt19937 rng_;
    std::normal_distribution<float> gaussian_;
    std::uniform_real_distribution<float> uniform_{0.0f, 1.0f};

    void updateDistributions();
};

// ============ Per-Sensor Noise Configurations ============

/// Lidar-specific noise configuration
struct LidarNoiseConfig {
    float range_stddev = 0.02f;    // 2cm range noise standard deviation
    float dropout_prob = 0.01f;    // 1% dropout probability
    bool enabled = false;          // Enable noise (domain randomization)
};

/// Odometry-specific noise configuration (proportional to motion)
struct OdomNoiseConfig {
    float linear_stddev = 0.01f;   // Linear noise as fraction of distance
    float angular_stddev = 0.02f;  // Angular noise as fraction of rotation
    bool enabled = false;          // Enable noise
};

}  // namespace warehouser
