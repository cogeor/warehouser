from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    pkg_dir = get_package_share_directory('warehouser_inference')
    config_file = os.path.join(pkg_dir, 'config', 'inference_params.yaml')

    return LaunchDescription([
        DeclareLaunchArgument(
            'model_path',
            default_value='',
            description='Path to ONNX model file'
        ),

        Node(
            package='warehouser_inference',
            executable='inference_node',
            name='inference',
            parameters=[
                config_file,
                {'default_model_path': LaunchConfiguration('model_path')}
            ],
            output='screen',
        ),
    ])
