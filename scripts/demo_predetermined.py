#!/usr/bin/env python3
"""Predetermined policy demo - no ML required.

This script demonstrates the simulation without deep learning by using
a simple hardcoded behavior:
1. Move toward closest object
2. Pick it up when close
3. Move toward drop zone
4. Drop object
5. Repeat

Usage:
    # Terminal 1: Launch simulation
    cd ros_ws && source install/setup.bash
    ros2 launch warehouser_bringup demo.launch.py

    # Terminal 2: Run this script
    cd scripts
    python demo_predetermined.py

    # Terminal 3: View in browser
    cd web_frontend && npm run dev
    # Open http://localhost:5173
"""

import math
import time
import sys

try:
    import rclpy
    from rclpy.node import Node
    from geometry_msgs.msg import Twist
    from std_msgs.msg import Empty
    from warehouser_msgs.msg import WorldState, Entity
    from std_srvs.srv import Trigger
except ImportError as e:
    print(f"Error: ROS2 packages not available: {e}")
    print("\nTo run this demo, you need:")
    print("  1. ROS2 Humble installed")
    print("  2. source ros_ws/install/setup.bash")
    print("  3. ros2 launch warehouser_bringup demo.launch.py (in another terminal)")
    sys.exit(1)


class PredeterminedPolicyNode(Node):
    """Node that runs a simple predetermined policy."""

    # Robot physical constants
    ROBOT_RADIUS = 0.3
    PICKUP_DIST = 0.5
    DROP_ZONE_DIST = 0.6

    # Control gains
    LINEAR_GAIN = 0.8
    ANGULAR_GAIN = 2.0

    # Target positions
    DROP_ZONE_X = 8.0
    DROP_ZONE_Y = 8.0

    def __init__(self):
        super().__init__("predetermined_policy")

        # State
        self.robot_x = 0.0
        self.robot_y = 0.0
        self.robot_theta = 0.0
        self.is_carrying = False
        self.objects: list[Entity] = []
        self.world_received = False

        # Publishers
        self.cmd_pub = self.create_publisher(Twist, "/robot/cmd_vel", 10)
        self.pick_pub = self.create_publisher(Empty, "/robot/sim/pick", 10)
        self.unpick_pub = self.create_publisher(Empty, "/robot/sim/unpick", 10)

        # Subscriber
        self.world_sub = self.create_subscription(
            WorldState, "/world/state", self.world_callback, 10
        )

        # Start simulation service client
        self.start_client = self.create_client(Trigger, "/sim/start")

        # Control timer (10 Hz)
        self.timer = self.create_timer(0.1, self.control_loop)

        self.get_logger().info("Predetermined policy node started")
        self.get_logger().info("Waiting for world state...")

        # Start simulation
        self._start_simulation()

    def _start_simulation(self):
        """Call /sim/start to begin the simulation."""
        if not self.start_client.wait_for_service(timeout_sec=5.0):
            self.get_logger().warn("Start service not available")
            return

        request = Trigger.Request()
        future = self.start_client.call_async(request)
        future.add_done_callback(
            lambda f: self.get_logger().info("Simulation started!")
        )

    def world_callback(self, msg: WorldState):
        """Process world state updates."""
        self.world_received = True
        self.objects = []

        for entity in msg.entities:
            if entity.type == Entity.TYPE_ROBOT:
                self.robot_x = entity.x
                self.robot_y = entity.y
                self.robot_theta = entity.theta
                self.is_carrying = entity.is_carrying
            elif entity.type == Entity.TYPE_OBJECT:
                self.objects.append(entity)

    def get_closest_object(self) -> Entity | None:
        """Find the closest pickable object."""
        if not self.objects:
            return None

        closest = None
        min_dist = float("inf")

        for obj in self.objects:
            dx = obj.x - self.robot_x
            dy = obj.y - self.robot_y
            dist = math.sqrt(dx * dx + dy * dy)
            if dist < min_dist:
                min_dist = dist
                closest = obj

        return closest

    def compute_velocity_to_target(
        self, target_x: float, target_y: float
    ) -> tuple[float, float]:
        """Compute velocity commands to reach a target.

        Returns:
            (linear_vel, angular_vel) tuple
        """
        # Compute relative position
        dx = target_x - self.robot_x
        dy = target_y - self.robot_y
        distance = math.sqrt(dx * dx + dy * dy)

        # Compute desired heading
        desired_theta = math.atan2(dy, dx)

        # Compute heading error
        theta_error = desired_theta - self.robot_theta
        # Normalize to [-pi, pi]
        while theta_error > math.pi:
            theta_error -= 2 * math.pi
        while theta_error < -math.pi:
            theta_error += 2 * math.pi

        # Proportional control
        angular_vel = self.ANGULAR_GAIN * theta_error

        # Only move forward if roughly facing target
        if abs(theta_error) < math.pi / 4:
            linear_vel = self.LINEAR_GAIN * min(distance, 1.0)
        else:
            linear_vel = 0.0

        # Clamp velocities
        linear_vel = max(-1.0, min(1.0, linear_vel))
        angular_vel = max(-2.0, min(2.0, angular_vel))

        return linear_vel, angular_vel

    def control_loop(self):
        """Main control loop - runs at 10 Hz."""
        if not self.world_received:
            return

        cmd = Twist()

        if self.is_carrying:
            # Go to drop zone
            dist_to_zone = math.sqrt(
                (self.DROP_ZONE_X - self.robot_x) ** 2 +
                (self.DROP_ZONE_Y - self.robot_y) ** 2
            )

            if dist_to_zone < self.DROP_ZONE_DIST:
                # Drop the object
                self.get_logger().info("Dropping object at drop zone!")
                self.unpick_pub.publish(Empty())
                cmd.linear.x = 0.0
                cmd.angular.z = 0.0
            else:
                # Navigate to drop zone
                linear, angular = self.compute_velocity_to_target(
                    self.DROP_ZONE_X, self.DROP_ZONE_Y
                )
                cmd.linear.x = linear
                cmd.angular.z = angular

        else:
            # Find closest object
            target = self.get_closest_object()

            if target is None:
                # No objects left, stop
                self.get_logger().info("All objects delivered! Stopping.")
                cmd.linear.x = 0.0
                cmd.angular.z = 0.0
            else:
                dist_to_obj = math.sqrt(
                    (target.x - self.robot_x) ** 2 +
                    (target.y - self.robot_y) ** 2
                )

                if dist_to_obj < self.PICKUP_DIST:
                    # Pick up the object
                    self.get_logger().info(f"Picking up object {target.id}!")
                    self.pick_pub.publish(Empty())
                    cmd.linear.x = 0.0
                    cmd.angular.z = 0.0
                else:
                    # Navigate to object
                    linear, angular = self.compute_velocity_to_target(
                        target.x, target.y
                    )
                    cmd.linear.x = linear
                    cmd.angular.z = angular

        self.cmd_pub.publish(cmd)


def main():
    print("=" * 60)
    print("WAREHOUSER - Predetermined Policy Demo")
    print("=" * 60)
    print()
    print("This demo shows the simulation without deep learning.")
    print("The robot will:")
    print("  1. Find the closest object")
    print("  2. Navigate to it and pick it up")
    print("  3. Navigate to the drop zone and drop it")
    print("  4. Repeat until all objects are delivered")
    print()
    print("Make sure the simulation is running:")
    print("  ros2 launch warehouser_bringup demo.launch.py")
    print()
    print("View in browser at http://localhost:5173")
    print("=" * 60)
    print()

    rclpy.init()
    node = PredeterminedPolicyNode()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
