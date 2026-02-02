#include "warehouser_observations/noise_model.hpp"

namespace warehouser {

NoiseModel::NoiseModel(const NoiseConfig& config)
    : config_(config),
      gaussian_(config.mean, config.stddev) {
    // Seed with time by default for non-deterministic behavior
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    rng_.seed(static_cast<unsigned int>(seed));
}

float NoiseModel::apply(float value) {
    if (!config_.enabled) {
        return value;
    }

    if (shouldDropout()) {
        return config_.dropout_value;
    }

    return value + gaussian_(rng_);
}

void NoiseModel::applyVector(std::vector<float>& values) {
    if (!config_.enabled) {
        return;
    }

    for (auto& v : values) {
        v = apply(v);
    }
}

bool NoiseModel::shouldDropout() {
    if (config_.dropout_prob <= 0.0f) {
        return false;
    }
    return uniform_(rng_) < config_.dropout_prob;
}

void NoiseModel::setSeed(unsigned int seed) {
    rng_.seed(seed);
}

void NoiseModel::setConfig(const NoiseConfig& config) {
    config_ = config;
    updateDistributions();
}

void NoiseModel::updateDistributions() {
    gaussian_ = std::normal_distribution<float>(config_.mean, config_.stddev);
}

}  // namespace warehouser
