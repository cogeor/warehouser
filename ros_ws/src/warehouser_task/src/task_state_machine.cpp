#include "warehouser_task/task_state_machine.hpp"

namespace warehouser_task {

const char* TaskStateMachine::stateToString(TaskState state) {
    switch (state) {
        case TaskState::IDLE: return "IDLE";
        case TaskState::NAVIGATING_TO_PICK: return "NAVIGATING_TO_PICK";
        case TaskState::PICKING: return "PICKING";
        case TaskState::NAVIGATING_TO_PLACE: return "NAVIGATING_TO_PLACE";
        case TaskState::PLACING: return "PLACING";
        case TaskState::COMPLETED: return "COMPLETED";
        case TaskState::FAILED: return "FAILED";
        case TaskState::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

const char* TaskStateMachine::eventToString(TaskEvent event) {
    switch (event) {
        case TaskEvent::COMMAND_RECEIVED: return "COMMAND_RECEIVED";
        case TaskEvent::REACHED_OBJECT: return "REACHED_OBJECT";
        case TaskEvent::PICK_SUCCESS: return "PICK_SUCCESS";
        case TaskEvent::PICK_FAILED: return "PICK_FAILED";
        case TaskEvent::REACHED_DESTINATION: return "REACHED_DESTINATION";
        case TaskEvent::PLACE_SUCCESS: return "PLACE_SUCCESS";
        case TaskEvent::PLACE_FAILED: return "PLACE_FAILED";
        case TaskEvent::TIMEOUT: return "TIMEOUT";
        case TaskEvent::CANCEL_REQUESTED: return "CANCEL_REQUESTED";
        case TaskEvent::COLLISION: return "COLLISION";
    }
    return "UNKNOWN";
}

void TaskStateMachine::transitionTo(TaskState new_state) {
    if (state_ != new_state) {
        TaskState old_state = state_;
        state_ = new_state;
        if (on_state_change_) {
            on_state_change_(old_state, new_state);
        }
    }
}

void TaskStateMachine::setTask(const Task& task) {
    task_ = task;
}

void TaskStateMachine::clearTask() {
    task_.reset();
    transitionTo(TaskState::IDLE);
}

void TaskStateMachine::handleEvent(TaskEvent event) {
    // Global cancel handling
    if (event == TaskEvent::CANCEL_REQUESTED) {
        if (state_ != TaskState::IDLE &&
            state_ != TaskState::COMPLETED &&
            state_ != TaskState::FAILED &&
            state_ != TaskState::CANCELLED) {
            if (task_) {
                task_->failure_reason = "Cancelled by user";
            }
            transitionTo(TaskState::CANCELLED);
            return;
        }
    }

    // State-specific handling
    switch (state_) {
        case TaskState::IDLE:
            handleIdle(event);
            break;
        case TaskState::NAVIGATING_TO_PICK:
            handleNavigatingToPick(event);
            break;
        case TaskState::PICKING:
            handlePicking(event);
            break;
        case TaskState::NAVIGATING_TO_PLACE:
            handleNavigatingToPlace(event);
            break;
        case TaskState::PLACING:
            handlePlacing(event);
            break;
        default:
            // Terminal states - no transitions
            break;
    }
}

void TaskStateMachine::handleIdle(TaskEvent event) {
    if (event == TaskEvent::COMMAND_RECEIVED && task_) {
        if (task_->intent == "navigate") {
            // Just navigation - go to destination
            transitionTo(TaskState::NAVIGATING_TO_PLACE);
        } else {
            // Pick or pick_and_place - go to object first
            transitionTo(TaskState::NAVIGATING_TO_PICK);
        }
    }
}

void TaskStateMachine::handleNavigatingToPick(TaskEvent event) {
    switch (event) {
        case TaskEvent::REACHED_OBJECT:
            transitionTo(TaskState::PICKING);
            break;
        case TaskEvent::TIMEOUT:
            if (task_) task_->failure_reason = "Timeout reaching object";
            transitionTo(TaskState::FAILED);
            break;
        case TaskEvent::COLLISION:
            if (task_) task_->failure_reason = "Collision during navigation";
            transitionTo(TaskState::FAILED);
            break;
        default:
            break;
    }
}

void TaskStateMachine::handlePicking(TaskEvent event) {
    switch (event) {
        case TaskEvent::PICK_SUCCESS:
            if (task_ && task_->intent == "pick_and_place") {
                transitionTo(TaskState::NAVIGATING_TO_PLACE);
            } else {
                // Just pick - we're done
                transitionTo(TaskState::COMPLETED);
            }
            break;
        case TaskEvent::PICK_FAILED:
            if (task_) task_->failure_reason = "Pick action failed";
            transitionTo(TaskState::FAILED);
            break;
        default:
            break;
    }
}

void TaskStateMachine::handleNavigatingToPlace(TaskEvent event) {
    switch (event) {
        case TaskEvent::REACHED_DESTINATION:
            if (task_ && task_->intent == "navigate") {
                // Just navigation - we're done
                transitionTo(TaskState::COMPLETED);
            } else {
                transitionTo(TaskState::PLACING);
            }
            break;
        case TaskEvent::TIMEOUT:
            if (task_) task_->failure_reason = "Timeout reaching destination";
            transitionTo(TaskState::FAILED);
            break;
        case TaskEvent::COLLISION:
            if (task_) task_->failure_reason = "Collision during navigation";
            transitionTo(TaskState::FAILED);
            break;
        default:
            break;
    }
}

void TaskStateMachine::handlePlacing(TaskEvent event) {
    switch (event) {
        case TaskEvent::PLACE_SUCCESS:
            transitionTo(TaskState::COMPLETED);
            break;
        case TaskEvent::PLACE_FAILED:
            if (task_) task_->failure_reason = "Place action failed";
            transitionTo(TaskState::FAILED);
            break;
        default:
            break;
    }
}

}  // namespace warehouser_task
