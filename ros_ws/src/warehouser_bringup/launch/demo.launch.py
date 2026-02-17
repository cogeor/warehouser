"""Demo launch: full system with optional trained model.

If a model exists at /ros_ws/models/policy_latest.onnx, it will be loaded
automatically. Otherwise, the inference node uses stub behavior.
"""

from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    bringup_dir = get_package_share_directory('warehouser_bringup')
    sim_params = os.path.join(bringup_dir, 'config', 'simulation_params.yaml')
    world_config = os.path.join(bringup_dir, 'config', 'world.yaml')

    # Check for trained model
    model_path = '/ros_ws/models/policy_latest.onnx'
    if not os.path.exists(model_path):
        model_path = ''  # Use stub behavior

    return LaunchDescription([
        # Simulation node
        Node(
            package='warehouser_simulation',
            executable='simulation_node',
            name='simulation',
            parameters=[sim_params, {'config': world_config}],
            output='screen',
        ),

        # Observations node
        Node(
            package='warehouser_observations',
            executable='observations_node',
            name='observations',
            output='screen',
        ),

        # Inference node (auto-loads model if present)
        Node(
            package='warehouser_inference',
            executable='inference_node',
            name='inference',
            parameters=[{'default_model_path': model_path}],
            output='screen',
        ),

        # Safety node
        Node(
            package='warehouser_safety',
            executable='safety_node',
            name='safety',
            output='screen',
        ),

        # Task manager node
        Node(
            package='warehouser_task',
            executable='task_manager_node',
            name='task_manager',
            output='screen',
        ),

        # Command node
        Node(
            package='warehouser_command',
            executable='command_node',
            name='command',
            output='screen',
        ),

        # Rosbridge for frontend
        Node(
            package='rosbridge_server',
            executable='rosbridge_websocket',
            name='rosbridge',
            parameters=[{'port': 9090}],
            output='screen',
        ),
    ])
