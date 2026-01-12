"""Launch full system: simulation + inference + task management + frontend."""

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.substitutions import LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    bringup_dir = get_package_share_directory('warehouser_bringup')
    world_config = os.path.join(bringup_dir, 'config', 'world.yaml')

    return LaunchDescription([
        DeclareLaunchArgument(
            'model_path',
            default_value='',
            description='Path to ONNX model file'
        ),

        # Simulation node
        Node(
            package='warehouser_simulation',
            executable='simulation_node',
            name='simulation',
            parameters=[world_config],
            output='screen',
        ),

        # Observations node
        Node(
            package='warehouser_observations',
            executable='observations_node',
            name='observations',
            output='screen',
        ),

        # Inference node
        Node(
            package='warehouser_inference',
            executable='inference_node',
            name='inference',
            parameters=[{
                'default_model_path': LaunchConfiguration('model_path'),
            }],
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
