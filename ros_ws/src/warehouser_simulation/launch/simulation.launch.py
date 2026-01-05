"""Launch file for the simulation node."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    # Get package share directory
    pkg_dir = get_package_share_directory('warehouser_simulation')
    default_params_file = os.path.join(pkg_dir, 'config', 'simulation_params.yaml')

    # Declare arguments
    params_file_arg = DeclareLaunchArgument(
        'params_file',
        default_value=default_params_file,
        description='Path to the parameter file'
    )

    # Simulation node
    simulation_node = Node(
        package='warehouser_simulation',
        executable='simulation_node',
        name='simulation',
        parameters=[LaunchConfiguration('params_file')],
        output='screen',
        emulate_tty=True,
    )

    return LaunchDescription([
        params_file_arg,
        simulation_node,
    ])
