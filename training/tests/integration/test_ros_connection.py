"""Integration tests for ROS2 connection.

These tests require the simulation, observations, and rl_bridge nodes to be running:
    ros2 launch warehouser_bringup bringup.launch.py
"""

import pytest

# Skip all tests if ROS2 is not available
pytest.importorskip("rclpy")


class TestROSConnection:
    """Test basic ROS2 connectivity."""

    @pytest.fixture
    def ros_context(self):
        """Initialize ROS2 context for tests."""
        import rclpy
        from rclpy.node import Node

        rclpy.init()
        node = Node("test_ros_connection")
        yield node
        node.destroy_node()
        rclpy.shutdown()

    def test_rl_reset_service_exists(self, ros_context) -> None:
        """Test that /rl/reset service is available."""
        from warehouser_msgs.srv import RLReset

        client = ros_context.create_client(RLReset, "/rl/reset")
        assert client.wait_for_service(timeout_sec=5.0), "RLReset service not available"

    def test_rl_step_service_exists(self, ros_context) -> None:
        """Test that /rl/step service is available."""
        from warehouser_msgs.srv import RLStep

        client = ros_context.create_client(RLStep, "/rl/step")
        assert client.wait_for_service(timeout_sec=5.0), "RLStep service not available"

    def test_world_state_topic_exists(self, ros_context) -> None:
        """Test that /world/state topic is publishing."""
        from warehouser_msgs.msg import WorldState

        received = []

        def callback(msg: WorldState) -> None:
            received.append(msg)

        sub = ros_context.create_subscription(WorldState, "/world/state", callback, 10)

        # Spin for a bit to receive messages
        import rclpy

        start_time = ros_context.get_clock().now()
        timeout = 5.0  # seconds

        while len(received) == 0:
            rclpy.spin_once(ros_context, timeout_sec=0.1)
            elapsed = (ros_context.get_clock().now() - start_time).nanoseconds / 1e9
            if elapsed > timeout:
                break

        assert len(received) > 0, "No WorldState messages received"
        assert len(received[0].entities) > 0, "WorldState has no entities"

    def test_observations_topic_exists(self, ros_context) -> None:
        """Test that /observations topic is publishing."""
        from warehouser_msgs.msg import Observation

        received = []

        def callback(msg: Observation) -> None:
            received.append(msg)

        sub = ros_context.create_subscription(Observation, "/observations", callback, 10)

        import rclpy

        start_time = ros_context.get_clock().now()
        timeout = 5.0

        while len(received) == 0:
            rclpy.spin_once(ros_context, timeout_sec=0.1)
            elapsed = (ros_context.get_clock().now() - start_time).nanoseconds / 1e9
            if elapsed > timeout:
                break

        assert len(received) > 0, "No Observation messages received"
        assert len(received[0].data) == 8, f"Expected 8 observation dims, got {len(received[0].data)}"


class TestRLServices:
    """Test RL service functionality."""

    @pytest.fixture
    def ros_context(self):
        """Initialize ROS2 context for tests."""
        import rclpy
        from rclpy.node import Node

        rclpy.init()
        node = Node("test_rl_services")
        yield node
        node.destroy_node()
        rclpy.shutdown()

    def test_reset_returns_observation(self, ros_context) -> None:
        """Test that RLReset returns a valid observation."""
        import rclpy
        from warehouser_msgs.srv import RLReset

        client = ros_context.create_client(RLReset, "/rl/reset")
        assert client.wait_for_service(timeout_sec=5.0)

        request = RLReset.Request()
        request.seed = 42

        future = client.call_async(request)
        rclpy.spin_until_future_complete(ros_context, future, timeout_sec=10.0)

        response = future.result()
        assert response is not None, "No response from RLReset"
        assert response.success, "RLReset failed"
        assert len(response.observation.data) == 8, "Wrong observation dimension"

    def test_step_returns_reward(self, ros_context) -> None:
        """Test that RLStep returns reward and observation."""
        import rclpy
        from warehouser_msgs.srv import RLReset, RLStep

        # First reset
        reset_client = ros_context.create_client(RLReset, "/rl/reset")
        assert reset_client.wait_for_service(timeout_sec=5.0)

        reset_request = RLReset.Request()
        reset_future = reset_client.call_async(reset_request)
        rclpy.spin_until_future_complete(ros_context, reset_future, timeout_sec=10.0)

        # Then step
        step_client = ros_context.create_client(RLStep, "/rl/step")
        assert step_client.wait_for_service(timeout_sec=5.0)

        step_request = RLStep.Request()
        step_request.action_linear = 0.5
        step_request.action_angular = 0.0
        step_request.action_pick = 0.0
        step_request.action_place = 0.0
        step_request.num_steps = 1

        step_future = step_client.call_async(step_request)
        rclpy.spin_until_future_complete(ros_context, step_future, timeout_sec=10.0)

        response = step_future.result()
        assert response is not None, "No response from RLStep"
        assert len(response.observation.data) == 8, "Wrong observation dimension"
        # Check reward is a valid number (not NaN)
        assert response.reward == response.reward, "Reward is NaN"

    def test_multiple_steps(self, ros_context) -> None:
        """Test multiple consecutive steps."""
        import rclpy
        from warehouser_msgs.srv import RLReset, RLStep

        # Reset
        reset_client = ros_context.create_client(RLReset, "/rl/reset")
        reset_client.wait_for_service(timeout_sec=5.0)
        reset_future = reset_client.call_async(RLReset.Request())
        rclpy.spin_until_future_complete(ros_context, reset_future, timeout_sec=10.0)

        # Multiple steps
        step_client = ros_context.create_client(RLStep, "/rl/step")
        step_client.wait_for_service(timeout_sec=5.0)

        positions_x = []
        for i in range(10):
            request = RLStep.Request()
            request.action_linear = 1.0
            request.action_angular = 0.0
            request.num_steps = 1

            future = step_client.call_async(request)
            rclpy.spin_until_future_complete(ros_context, future, timeout_sec=5.0)
            response = future.result()

            if response:
                positions_x.append(response.observation.data[0])

        # Robot should have moved forward (x increasing)
        assert len(positions_x) == 10, "Did not get all step responses"
        assert positions_x[-1] > positions_x[0], "Robot did not move forward"
