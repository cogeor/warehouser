# Observation.msg
# Observation vector for policy input

# Version 1: Position-based (8 dims)
# [robot_x, robot_y, robot_theta, goal_dx, goal_dy, goal_dist, goal_heading, is_carrying]
#
# Version 2: Lidar-based (63 dims) - future
# [lidar_ranges(60), goal_bearing, goal_dist, is_carrying]

int32 version
float32[] data
