#pragma once

#include <functional>
#include <optional>
#include <string>

namespace warehouser_task {

enum class TaskState {
    IDLE,
    NAVIGATING_TO_PICK,
    PICKING,
    NAVIGATING_TO_PLACE,
    PLACING,
    COMPLETED,
    FAILED,
    CANCELLED
};

enum class TaskEvent {
    COMMAND_RECEIVED,
    REACHED_OBJECT,
    PICK_SUCCESS,
    PICK_FAILED,
    REACHED_DESTINATION,
    PLACE_SUCCESS,
    PLACE_FAILED,
    TIMEOUT,
    CANCEL_REQUESTED,
    COLLISION
};

struct Task {
    std::string task_id;
    std::string intent;  // "pick", "navigate", "pick_and_place"

    // Target object
    std::string target_object_id;
    std::string target_color;
    float object_x{0.0f};
    float object_y{0.0f};
    float pickup_radius{0.5f};

    // Destination (for pick_and_place)
    float dest_x{0.0f};
    float dest_y{0.0f};
    float place_radius{0.5f};

    // Failure info
    std::string failure_reason;
};

class TaskStateMachine {
public:
    using StateChangeCallback = std::function<void(TaskState, TaskState)>;

    TaskStateMachine() = default;

    void handleEvent(TaskEvent event);

    void setTask(const Task& task);
    void clearTask();

    [[nodiscard]] TaskState getState() const noexcept { return state_; }
    [[nodiscard]] const std::optional<Task>& getTask() const noexcept { return task_; }

    void setStateChangeCallback(StateChangeCallback callback) {
        on_state_change_ = std::move(callback);
    }

    [[nodiscard]] static const char* stateToString(TaskState state);
    [[nodiscard]] static const char* eventToString(TaskEvent event);

private:
    void transitionTo(TaskState new_state);
    void handleIdle(TaskEvent event);
    void handleNavigatingToPick(TaskEvent event);
    void handlePicking(TaskEvent event);
    void handleNavigatingToPlace(TaskEvent event);
    void handlePlacing(TaskEvent event);

    TaskState state_{TaskState::IDLE};
    std::optional<Task> task_;
    StateChangeCallback on_state_change_;
};

}  // namespace warehouser_task
