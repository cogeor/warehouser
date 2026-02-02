#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "warehouser_observations/noise_model.hpp"

using namespace warehouser;

// ============ NoiseModel Tests ============

TEST(NoiseModel, DefaultConfigDisabled) {
    NoiseModel noise;
    EXPECT_FALSE(noise.isEnabled());
}

TEST(NoiseModel, DisabledReturnsOriginal) {
    NoiseConfig config;
    config.enabled = false;
    config.stddev = 1.0f;
    NoiseModel noise(config);

    EXPECT_FLOAT_EQ(noise.apply(5.0f), 5.0f);
    EXPECT_FLOAT_EQ(noise.apply(-3.0f), -3.0f);
    EXPECT_FLOAT_EQ(noise.apply(0.0f), 0.0f);
}

TEST(NoiseModel, EnabledAddsNoise) {
    NoiseConfig config;
    config.enabled = true;
    config.stddev = 1.0f;
    config.mean = 0.0f;
    NoiseModel noise(config);
    noise.setSeed(42);

    // With noise enabled, values should (usually) differ from input
    // Run multiple times to avoid false negative from rare exact match
    bool any_different = false;
    for (int i = 0; i < 100; ++i) {
        float result = noise.apply(5.0f);
        if (result != 5.0f) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST(NoiseModel, GaussianStatistics) {
    NoiseConfig config;
    config.enabled = true;
    config.mean = 0.0f;
    config.stddev = 1.0f;
    NoiseModel noise(config);
    noise.setSeed(42);

    // Collect samples
    std::vector<float> samples;
    samples.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        samples.push_back(noise.apply(0.0f));
    }

    // Calculate mean
    float mean = std::accumulate(samples.begin(), samples.end(), 0.0f) /
                 static_cast<float>(samples.size());
    EXPECT_NEAR(mean, 0.0f, 0.05f);  // Within 5% of expected

    // Calculate stddev
    float variance = 0.0f;
    for (float s : samples) {
        variance += (s - mean) * (s - mean);
    }
    float stddev = std::sqrt(variance / static_cast<float>(samples.size()));
    EXPECT_NEAR(stddev, 1.0f, 0.1f);  // Within 10%
}

TEST(NoiseModel, NonZeroMean) {
    NoiseConfig config;
    config.enabled = true;
    config.mean = 5.0f;
    config.stddev = 0.5f;
    NoiseModel noise(config);
    noise.setSeed(123);

    std::vector<float> samples;
    samples.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        samples.push_back(noise.apply(0.0f));
    }

    float mean = std::accumulate(samples.begin(), samples.end(), 0.0f) /
                 static_cast<float>(samples.size());
    EXPECT_NEAR(mean, 5.0f, 0.1f);
}

TEST(NoiseModel, DropoutWorks) {
    NoiseConfig config;
    config.enabled = true;
    config.stddev = 0.0f;  // No Gaussian noise
    config.dropout_prob = 0.5f;  // 50% for easy testing
    config.dropout_value = -1.0f;
    NoiseModel noise(config);
    noise.setSeed(42);

    int dropout_count = 0;
    for (int i = 0; i < 1000; ++i) {
        if (noise.apply(10.0f) == -1.0f) {
            dropout_count++;
        }
    }

    // Should be roughly 50% with some tolerance
    EXPECT_GT(dropout_count, 400);
    EXPECT_LT(dropout_count, 600);
}

TEST(NoiseModel, ShouldDropoutFrequency) {
    NoiseConfig config;
    config.enabled = true;
    config.dropout_prob = 0.1f;  // 10%
    NoiseModel noise(config);
    noise.setSeed(42);

    int dropout_count = 0;
    for (int i = 0; i < 10000; ++i) {
        if (noise.shouldDropout()) {
            dropout_count++;
        }
    }

    // Should be roughly 10%
    EXPECT_NEAR(dropout_count, 1000, 100);
}

TEST(NoiseModel, ZeroDropoutProbNeverDrops) {
    NoiseConfig config;
    config.enabled = true;
    config.dropout_prob = 0.0f;
    NoiseModel noise(config);

    for (int i = 0; i < 1000; ++i) {
        EXPECT_FALSE(noise.shouldDropout());
    }
}

TEST(NoiseModel, SeedReproducibility) {
    NoiseConfig config;
    config.enabled = true;
    config.stddev = 1.0f;

    NoiseModel noise1(config);
    NoiseModel noise2(config);
    noise1.setSeed(42);
    noise2.setSeed(42);

    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(noise1.apply(0.0f), noise2.apply(0.0f));
    }
}

TEST(NoiseModel, DifferentSeedsDifferentResults) {
    NoiseConfig config;
    config.enabled = true;
    config.stddev = 1.0f;

    NoiseModel noise1(config);
    NoiseModel noise2(config);
    noise1.setSeed(42);
    noise2.setSeed(123);

    bool any_different = false;
    for (int i = 0; i < 100; ++i) {
        if (noise1.apply(0.0f) != noise2.apply(0.0f)) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST(NoiseModel, ApplyVectorWhenDisabled) {
    NoiseConfig config;
    config.enabled = false;
    config.stddev = 1.0f;
    NoiseModel noise(config);

    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> original = values;

    noise.applyVector(values);

    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_FLOAT_EQ(values[i], original[i]);
    }
}

TEST(NoiseModel, ApplyVectorWhenEnabled) {
    NoiseConfig config;
    config.enabled = true;
    config.stddev = 0.5f;
    NoiseModel noise(config);
    noise.setSeed(42);

    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> original = values;

    noise.applyVector(values);

    // At least some values should differ
    bool any_different = false;
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] != original[i]) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST(NoiseModel, SetConfigUpdatesNoise) {
    NoiseModel noise;
    EXPECT_FALSE(noise.isEnabled());

    NoiseConfig config;
    config.enabled = true;
    config.stddev = 1.0f;
    noise.setConfig(config);

    EXPECT_TRUE(noise.isEnabled());
    EXPECT_FLOAT_EQ(noise.config().stddev, 1.0f);
}

// ============ Per-Sensor Config Tests ============

TEST(LidarNoiseConfig, DefaultValues) {
    LidarNoiseConfig config;
    EXPECT_FLOAT_EQ(config.range_stddev, 0.02f);
    EXPECT_FLOAT_EQ(config.dropout_prob, 0.01f);
    EXPECT_FALSE(config.enabled);
}

TEST(OdomNoiseConfig, DefaultValues) {
    OdomNoiseConfig config;
    EXPECT_FLOAT_EQ(config.linear_stddev, 0.01f);
    EXPECT_FLOAT_EQ(config.angular_stddev, 0.02f);
    EXPECT_FALSE(config.enabled);
}

// ============ Integration Tests ============

TEST(NoiseIntegration, LidarStyleUsage) {
    // Simulate lidar noise application pattern
    LidarNoiseConfig lidar_config;
    lidar_config.enabled = true;
    lidar_config.range_stddev = 0.02f;
    lidar_config.dropout_prob = 0.01f;

    NoiseConfig noise_cfg;
    noise_cfg.enabled = lidar_config.enabled;
    noise_cfg.stddev = lidar_config.range_stddev;
    noise_cfg.dropout_prob = lidar_config.dropout_prob;
    noise_cfg.dropout_value = 10.0f;  // max_range

    NoiseModel noise(noise_cfg);
    noise.setSeed(42);

    // Simulate 60 lidar rays
    std::vector<float> ranges(60, 5.0f);  // All at 5 meters
    noise.applyVector(ranges);

    // Count dropouts and non-dropouts
    int dropout_count = 0;
    for (float r : ranges) {
        if (r == 10.0f) {
            dropout_count++;
        }
    }

    // With 1% dropout, expect 0-2 dropouts in 60 rays
    EXPECT_LE(dropout_count, 5);  // Allow some variance
}
