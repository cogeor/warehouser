# Pipeline Analysis: Warehouser Simulation

## 1. Current Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           WAREHOUSER PIPELINE                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                  │
│  │   Frontend   │◄───│  rosbridge   │◄───│  Simulation  │                  │
│  │   (React)    │    │  (ws:9090)   │    │   (ROS2)     │                  │
│  └──────────────┘    └──────────────┘    └──────────────┘                  │
│         ▲                                       │                           │
│         │                                       ▼                           │
│         │                              ┌──────────────┐                     │
│         │                              │ Observations │                     │
│         │                              │    Node      │                     │
│         │                              └──────────────┘                     │
│         │                                       │                           │
│         │                                       ▼                           │
│         │                              ┌──────────────┐                     │
│         └──────────────────────────────│  RL Bridge   │◄─── Training       │
│                                        │    Node      │     (Python)       │
│                                        └──────────────┘                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 2. How to Run the Frontend Demo

### Prerequisites
1. **ROS2 Humble** installed and sourced
2. **Node.js 18+** for frontend
3. **rosbridge_server** package installed

### Steps to Run

**Terminal 1: Build ROS workspace**
```bash
cd ros_ws
colcon build
source install/setup.bash
```

**Terminal 2: Launch demo (simulation + rosbridge)**
```bash
cd ros_ws
source install/setup.bash
ros2 launch warehouser_bringup demo.launch.py
```

**Terminal 3: Start frontend**
```bash
cd web_frontend
npm install
npm run dev
# Open http://localhost:5173
```

### Expected Behavior
- Frontend shows 10x10 world with robot at (1,1)
- 4 objects (red, green, blue) visible
- Drop zone at (8,8)
- Click "Start" to enable simulation
- Click "Demo" to auto-cycle through picking commands

## 3. Training Pipeline Gap Analysis

### What's READY (90%)

| Component | Status | Notes |
|-----------|--------|-------|
| ROSGymEnv | ✅ Ready | Full Gymnasium wrapper with ROS2 services |
| PPO Training | ✅ Ready | train.py with SB3, callbacks, checkpointing |
| VecNormalize | ✅ Ready | Observation/reward normalization |
| Observation Builder | ✅ Ready | V1 (5-dim), V2 (63-dim lidar), V3 (multi-robot) |
| Reward System | ✅ Ready | Composite rewards, progress, collision penalties |
| Action Wrappers | ✅ Ready | Scaling, smoothing, acceleration limits |
| ONNX Export | ✅ Ready | export_onnx.py for inference |
| Safety Controller | ✅ Ready | Obstacle avoidance, emergency stop |
| Multi-Robot | ✅ Ready | Per-robot publishers, RLReset with robot_count |

### What's MISSING (10%)

| Gap | Priority | Effort | Description |
|-----|----------|--------|-------------|
| ROS2 Build on Windows | HIGH | 1-2 days | ROS2 not in PATH, need Docker or WSL |
| Integration Test | MEDIUM | 1 day | End-to-end training smoke test |
| Inference Node | LOW | Done | Stub mode works without trained model |

### Steps to First Trained Policy

1. **Setup ROS2 environment** (Docker recommended for Windows)
   ```bash
   docker pull ros:humble
   # or use WSL2 with Ubuntu 22.04
   ```

2. **Build and launch simulation**
   ```bash
   cd ros_ws && colcon build
   ros2 launch warehouser_bringup simulation.launch.py
   ```

3. **Run training** (in another terminal)
   ```bash
   cd training
   source ../ros_ws/install/setup.bash
   python -m training.scripts.train --total-timesteps 100000
   ```

4. **Export to ONNX**
   ```bash
   python -m training.scripts.export_onnx --model checkpoints/final_model.zip
   ```

5. **Load in inference node**
   ```bash
   ros2 service call /model/load warehouser_msgs/srv/LoadModel "{model_path: 'model.onnx'}"
   ```

## 4. Terrain Encoding

### Current Format: YAML Configuration

**Location:** `ros_ws/src/warehouser_bringup/config/world.yaml`

```yaml
simulation:
  ros__parameters:
    world_width: 10.0
    world_height: 10.0

    robot:
      id: "robot"
      spawn_x: 1.0
      spawn_y: 1.0

    objects:
      - id: "red_1"
        color: "red"
        x: 3.0
        y: 2.0
        pickup_radius: 0.5

    walls:
      - id: "wall_bottom"
        x: 0.0
        y: 0.0
        width: 10.0
        height: 0.1

    zones:
      - id: "drop_zone"
        x: 8.0
        y: 8.0
        radius: 0.5
```

### Industry Standards Comparison

| Standard | Format | Use Case | Our Gap |
|----------|--------|----------|---------|
| **OpenAI Gym/MuJoCo** | XML (MJCF) | Physics simulation | Different domain |
| **ROS2 Nav2** | YAML + PNG costmap | Navigation | Could adopt for occupancy |
| **Isaac Sim** | USD (Universal Scene Description) | NVIDIA robotics | Overkill for 2D |
| **Gazebo** | SDF/URDF | 3D physics | Overkill for 2D |
| **Custom 2D** | JSON/YAML | Simple environments | **We use this** |

### Recommended Terrain Enhancements

1. **Occupancy Grid Support** (for complex obstacles)
   ```yaml
   terrain:
     type: "occupancy_grid"
     resolution: 0.1  # 10cm per cell
     data: "terrain.png"  # Grayscale PNG
   ```

2. **Procedural Generation**
   ```yaml
   terrain:
     type: "procedural"
     seed: 42
     obstacle_density: 0.1
     num_objects: 10
   ```

3. **Scenario Files**
   ```yaml
   scenarios:
     - name: "warehouse_simple"
       objects: 4
       walls: "boundary_only"
     - name: "warehouse_maze"
       walls: "maze_grid"
       difficulty: "hard"
   ```

### Current Terrain Capabilities

| Feature | Status | Notes |
|---------|--------|-------|
| Static walls | ✅ | AABB rectangles |
| Pickable objects | ✅ | Circular pickup radius |
| Drop zones | ✅ | Circular zones |
| Multiple robots | ✅ | Spawn via reset |
| Procedural generation | ❌ | Not implemented |
| Occupancy grids | ❌ | Not implemented |
| Dynamic obstacles | ❌ | Not implemented |

## 5. Quick Start Demo Script

For immediate testing without ROS2, see `scripts/demo_predetermined.py`:

```python
# This script publishes directly to /robot/cmd_vel
# to make the robot move in a square pattern
```

## 6. Summary

### To See Frontend Working:
1. Need ROS2 running (Docker/WSL recommended on Windows)
2. Launch `demo.launch.py`
3. Run `npm run dev` in web_frontend
4. Should see robot and world immediately

### Time to First Trained Policy:
- **If ROS2 setup complete**: ~2 hours (training time)
- **If ROS2 not setup**: ~1 day (environment setup)

### Terrain Status:
- Current: Simple YAML config (adequate for MVP)
- Future: Consider occupancy grids for complex environments
