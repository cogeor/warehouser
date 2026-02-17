#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
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
    std::string version;
    std::string export_timestamp;
    int64_t obs_dim{8};
    int64_t action_dim{4};
    bool loaded{false};
};

// Simple Result type for C++23 std::expected compatibility
template<typename T>
struct Result {
    std::variant<T, std::string> value;

    [[nodiscard]] bool has_value() const { return std::holds_alternative<T>(value); }
    [[nodiscard]] explicit operator bool() const { return has_value(); }
    [[nodiscard]] T& operator*() { return std::get<T>(value); }
    [[nodiscard]] const T& operator*() const { return std::get<T>(value); }
    [[nodiscard]] T* operator->() { return &std::get<T>(value); }
    [[nodiscard]] const T* operator->() const { return &std::get<T>(value); }
    [[nodiscard]] const std::string& error() const { return std::get<std::string>(value); }

    static Result success(T val) { return Result{std::move(val)}; }
    static Result failure(std::string err) { return Result{std::move(err)}; }
};

template<>
struct Result<void> {
    std::optional<std::string> error_;

    [[nodiscard]] bool has_value() const { return !error_.has_value(); }
    [[nodiscard]] explicit operator bool() const { return has_value(); }
    [[nodiscard]] const std::string& error() const { return *error_; }

    static Result success() { return Result{}; }
    static Result failure(std::string err) { return Result{std::move(err)}; }
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

    [[nodiscard]] Result<void> loadModel(const std::string& model_path);

    [[nodiscard]] Result<Action> infer(const std::vector<float>& observation);

    [[nodiscard]] bool isLoaded() const noexcept;

    [[nodiscard]] const ModelInfo& getModelInfo() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace warehouser_inference
