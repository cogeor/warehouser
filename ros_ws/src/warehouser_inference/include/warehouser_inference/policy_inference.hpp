#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace warehouser_inference {

struct Action {
    float linear{0.0f};
    float angular{0.0f};
    float pick{0.0f};
    float place{0.0f};
};

struct ModelInfo {
    std::string path;
    int64_t obs_dim{8};
    int64_t action_dim{4};
    bool loaded{false};
};

class PolicyInference {
public:
    PolicyInference();
    ~PolicyInference();

    // Non-copyable
    PolicyInference(const PolicyInference&) = delete;
    PolicyInference& operator=(const PolicyInference&) = delete;

    // Move-only
    PolicyInference(PolicyInference&&) noexcept;
    PolicyInference& operator=(PolicyInference&&) noexcept;

    [[nodiscard]] std::expected<void, std::string> loadModel(const std::string& model_path);

    [[nodiscard]] std::expected<Action, std::string> infer(const std::vector<float>& observation);

    [[nodiscard]] bool isLoaded() const noexcept;

    [[nodiscard]] const ModelInfo& getModelInfo() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace warehouser_inference
