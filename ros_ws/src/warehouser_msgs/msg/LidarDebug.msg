# LidarDebug.msg
# Simulated lidar data for visualization/debugging

float32[] ranges           # Distance to obstacle for each ray
float32 angle_min          # Start angle (radians)
float32 angle_max          # End angle (radians)
float32 range_min          # Minimum valid range
float32 range_max          # Maximum valid range

# Robot pose when scan was taken
float32 robot_x
float32 robot_y
float32 robot_theta
