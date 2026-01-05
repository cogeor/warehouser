# TaskStatus.msg
# Current task state

# Task states
uint8 STATE_IDLE = 0
uint8 STATE_NAVIGATING = 1
uint8 STATE_PICKING = 2
uint8 STATE_CARRYING = 3
uint8 STATE_PLACING = 4
uint8 STATE_COMPLETE = 5
uint8 STATE_FAILED = 6

uint8 state
string target_object_id
string target_color
float32 distance_to_goal
