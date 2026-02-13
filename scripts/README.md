# Demo Scripts

## Quick Start (Docker)

Build and run the simulation in Docker:

```bash
# Build the demo image
docker build -f Dockerfile.demo -t warehouser-demo .

# Run with rosbridge exposed on port 9090
docker run -it --rm -p 9090:9090 warehouser-demo
```

Then start the frontend:

```bash
cd web_frontend
npm install
npm run dev
# Open http://localhost:5173
```

## Predetermined Policy Demo

A simple demo that shows the robot picking up objects and delivering them
to the drop zone - no machine learning required.

### Prerequisites

1. ROS2 Jazzy installed and sourced
2. Warehouser packages built: `cd ros_ws && colcon build`

### Running

Terminal 1 - Launch simulation:
```bash
cd ros_ws
source install/setup.bash
ros2 launch warehouser_bringup demo.launch.py
```

Terminal 2 - Run predetermined policy:
```bash
cd scripts
source ../ros_ws/install/setup.bash
python demo_predetermined.py
```

Terminal 3 - View in browser:
```bash
cd web_frontend
npm run dev
# Open http://localhost:5173
```

### What It Does

The predetermined policy:
1. Finds the closest object in the world
2. Navigates to it using proportional control
3. Picks it up when close enough
4. Navigates to the drop zone at (8, 8)
5. Drops the object
6. Repeats until all objects are delivered

## Training a Policy

See `training/training/scripts/train.py` for the full training pipeline.

Quick start:
```bash
cd training

# Train for 100k timesteps
python -m training.scripts.train --total-timesteps 100000

# Export to ONNX for inference
python -m training.scripts.export_onnx --model checkpoints/final_model.zip

# Load in inference node
ros2 service call /model/load warehouser_msgs/srv/LoadModel "{model_path: 'model.onnx'}"
```

## Pipeline Status

| Component | Status |
|-----------|--------|
| Simulation (ROS2) | Ready |
| Observations | Ready (V1, V2, V3) |
| RL Bridge | Ready |
| Training (SB3) | Ready |
| VecNormalize | Ready |
| Action Wrappers | Ready |
| ONNX Export | Ready |
| Frontend | Ready |
