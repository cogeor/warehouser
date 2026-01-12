#include <gtest/gtest.h>
#include "warehouser_task/task_state_machine.hpp"

using namespace warehouser_task;

class TaskStateMachineTest : public ::testing::Test {
protected:
    TaskStateMachine sm_;

    void setPickTask() {
        Task task;
        task.task_id = "test_1";
        task.intent = "pick";
        task.object_x = 5.0f;
        task.object_y = 5.0f;
        sm_.setTask(task);
    }

    void setPickAndPlaceTask() {
        Task task;
        task.task_id = "test_2";
        task.intent = "pick_and_place";
        task.object_x = 5.0f;
        task.object_y = 5.0f;
        task.dest_x = 8.0f;
        task.dest_y = 8.0f;
        sm_.setTask(task);
    }

    void setNavigateTask() {
        Task task;
        task.task_id = "test_3";
        task.intent = "navigate";
        task.dest_x = 5.0f;
        task.dest_y = 5.0f;
        sm_.setTask(task);
    }
};

TEST_F(TaskStateMachineTest, InitialStateIsIdle) {
    EXPECT_EQ(sm_.getState(), TaskState::IDLE);
}

TEST_F(TaskStateMachineTest, CommandStartsPick) {
    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    EXPECT_EQ(sm_.getState(), TaskState::NAVIGATING_TO_PICK);
}

TEST_F(TaskStateMachineTest, ReachObjectTransitionsToPicking) {
    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    sm_.handleEvent(TaskEvent::REACHED_OBJECT);
    EXPECT_EQ(sm_.getState(), TaskState::PICKING);
}

TEST_F(TaskStateMachineTest, PickSuccessCompletesPickTask) {
    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    sm_.handleEvent(TaskEvent::REACHED_OBJECT);
    sm_.handleEvent(TaskEvent::PICK_SUCCESS);
    EXPECT_EQ(sm_.getState(), TaskState::COMPLETED);
}

TEST_F(TaskStateMachineTest, PickSuccessContinuesToPlace) {
    setPickAndPlaceTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    sm_.handleEvent(TaskEvent::REACHED_OBJECT);
    sm_.handleEvent(TaskEvent::PICK_SUCCESS);
    EXPECT_EQ(sm_.getState(), TaskState::NAVIGATING_TO_PLACE);
}

TEST_F(TaskStateMachineTest, FullPickAndPlaceFlow) {
    setPickAndPlaceTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    EXPECT_EQ(sm_.getState(), TaskState::NAVIGATING_TO_PICK);

    sm_.handleEvent(TaskEvent::REACHED_OBJECT);
    EXPECT_EQ(sm_.getState(), TaskState::PICKING);

    sm_.handleEvent(TaskEvent::PICK_SUCCESS);
    EXPECT_EQ(sm_.getState(), TaskState::NAVIGATING_TO_PLACE);

    sm_.handleEvent(TaskEvent::REACHED_DESTINATION);
    EXPECT_EQ(sm_.getState(), TaskState::PLACING);

    sm_.handleEvent(TaskEvent::PLACE_SUCCESS);
    EXPECT_EQ(sm_.getState(), TaskState::COMPLETED);
}

TEST_F(TaskStateMachineTest, NavigateTaskSkipsPick) {
    setNavigateTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    EXPECT_EQ(sm_.getState(), TaskState::NAVIGATING_TO_PLACE);

    sm_.handleEvent(TaskEvent::REACHED_DESTINATION);
    EXPECT_EQ(sm_.getState(), TaskState::COMPLETED);
}

TEST_F(TaskStateMachineTest, CancelFromNavigating) {
    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    sm_.handleEvent(TaskEvent::CANCEL_REQUESTED);
    EXPECT_EQ(sm_.getState(), TaskState::CANCELLED);
}

TEST_F(TaskStateMachineTest, TimeoutCausesFailure) {
    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    sm_.handleEvent(TaskEvent::TIMEOUT);
    EXPECT_EQ(sm_.getState(), TaskState::FAILED);
    EXPECT_FALSE(sm_.getTask()->failure_reason.empty());
}

TEST_F(TaskStateMachineTest, CollisionCausesFailure) {
    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    sm_.handleEvent(TaskEvent::COLLISION);
    EXPECT_EQ(sm_.getState(), TaskState::FAILED);
}

TEST_F(TaskStateMachineTest, PickFailedCausesFailure) {
    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);
    sm_.handleEvent(TaskEvent::REACHED_OBJECT);
    sm_.handleEvent(TaskEvent::PICK_FAILED);
    EXPECT_EQ(sm_.getState(), TaskState::FAILED);
}

TEST_F(TaskStateMachineTest, StateChangeCallbackFires) {
    bool callback_fired = false;
    TaskState captured_old, captured_new;

    sm_.setStateChangeCallback([&](TaskState old_s, TaskState new_s) {
        callback_fired = true;
        captured_old = old_s;
        captured_new = new_s;
    });

    setPickTask();
    sm_.handleEvent(TaskEvent::COMMAND_RECEIVED);

    EXPECT_TRUE(callback_fired);
    EXPECT_EQ(captured_old, TaskState::IDLE);
    EXPECT_EQ(captured_new, TaskState::NAVIGATING_TO_PICK);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
