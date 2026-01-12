from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    pkg_dir = get_package_share_directory('warehouser_task')
    config_file = os.path.join(pkg_dir, 'config', 'task_manager_params.yaml')

    return LaunchDescription([
        Node(
            package='warehouser_task',
            executable='task_manager_node',
            name='task_manager',
            parameters=[config_file],
            output='screen',
        ),
    ])
