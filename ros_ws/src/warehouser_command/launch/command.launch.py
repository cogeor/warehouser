from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    pkg_dir = get_package_share_directory('warehouser_command')
    config_file = os.path.join(pkg_dir, 'config', 'zones.yaml')

    return LaunchDescription([
        Node(
            package='warehouser_command',
            executable='command_node',
            name='command',
            parameters=[config_file],
            output='screen',
        ),
    ])
