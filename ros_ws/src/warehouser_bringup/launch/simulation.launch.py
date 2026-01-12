"""Launch simulation nodes only (for training)."""

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    bringup_dir = get_package_share_directory('warehouser_bringup')
    world_config = os.path.join(bringup_dir, 'config', 'world.yaml')

    return LaunchDescription([
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

        # RL Bridge node
        Node(
            package='warehouser_rl_bridge',
            executable='rl_bridge_node',
            name='rl_bridge',
            output='screen',
        ),
    ])
