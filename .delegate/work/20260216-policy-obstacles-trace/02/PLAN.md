# Loop 02: Add obstacles to simulation and enlarge world

## Overview

This loop enlarges the simulation world from 10x10 to 20x20 meters and adds interior obstacles. Obstacles are implemented as Wall entities since they have identical behavior: axis-aligned rectangles that block robot movement and lidar rays. The existing frontend already renders walls, so obstacles will automatically appear.

## Analysis

### Key Finding: Wall Reuse Strategy

After analyzing the codebase, obstacles can be implemented by reusing the existing Wall entity type:

1. **Collision detection**: `WorldManager::checkCollision()` iterates over `walls_` vector
2. **Lidar raycast**: `LidarSimulator::checkWallCollision()` checks `entity.type == 2` (TYPE_WALL)
3. **Frontend rendering**: `RosDataBridge` maps type 2 to 'wall', `CanvasWalls` renders all walls

This means adding interior obstacles as Wall entities requires zero changes to collision detection, lidar simulation, or frontend rendering - only adding new Wall instances in `createDefaultWorld()`.

### World Layout

New 20x20 world with obstacles forming navigation challenges:

```
+--------------------+
|                    |
|    [OBS1]          |
|                    |
|         [OBS2]     |
|  R                 |
|    [OBS3]    [OBS4]|
|                    |
|                    |
|              [DROP]|
+--------------------+
```

- R: Robot spawn (2, 2)
- OBS1-4: Interior obstacles (2x2 or 1x3 boxes)
- DROP: Drop zone (moved to larger coordinates)
- Objects: Repositioned for larger world

## Tasks

### Task 1: Update world dimensions in configuration

**Goal:** Increase world size from 10x10 to 20x20 meters

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `ros_ws/src/warehouser_simulation/config/simulation_params.yaml` |
| MODIFY | `ros_ws/src/warehouser_bringup/config/simulation_params.yaml` |

**Steps:**
1. Change `world_width: 10.0` to `world_width: 20.0`
2. Change `world_height: 10.0` to `world_height: 20.0`

**simulation_params.yaml (both files):**
```yaml
simulation:
  ros__parameters:
    dt: 0.02
    world_width: 20.0
    world_height: 20.0
```

**Verify:** Configuration loads correctly (verified by subsequent simulation start)

---

### Task 2: Update WorldManager::createDefaultWorld() for larger world

**Goal:** Reposition entities and add interior obstacles

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `ros_ws/src/warehouser_simulation/src/world_manager.cpp` |

**Steps:**
1. Update robot spawn position (2, 2) - further from walls
2. Update pickable object positions spread across larger world
3. Add 4 interior obstacle walls using existing Wall class
4. Update drop zone position for larger world (16, 16)

**Code changes in `createDefaultWorld()`:**

```cpp
void WorldManager::createDefaultWorld() {
    // Spawn single robot at (2, 2) - offset from corner
    spawnRobot("robot", 2.0f, 2.0f, 0.0f);

    // Create pickable objects spread across larger world
    auto red = std::make_unique<PickableObject>("red_1", 5.0f, 8.0f, "red");
    initial_object_positions_.emplace_back("red_1", std::make_pair(5.0f, 8.0f));
    objects_.push_back(std::move(red));

    auto green = std::make_unique<PickableObject>("green_1", 12.0f, 5.0f, "green");
    initial_object_positions_.emplace_back("green_1", std::make_pair(12.0f, 5.0f));
    objects_.push_back(std::move(green));

    auto blue = std::make_unique<PickableObject>("blue_1", 15.0f, 12.0f, "blue");
    initial_object_positions_.emplace_back("blue_1", std::make_pair(15.0f, 12.0f));
    objects_.push_back(std::move(blue));

    // Create boundary walls (thin walls around the perimeter)
    walls_.push_back(std::make_unique<Wall>("wall_bottom", 0.0f, 0.0f,
                                             config_.width, 0.1f));
    walls_.push_back(std::make_unique<Wall>("wall_top", 0.0f,
                                             config_.height - 0.1f,
                                             config_.width, 0.1f));
    walls_.push_back(std::make_unique<Wall>("wall_left", 0.0f, 0.0f, 0.1f,
                                             config_.height));
    walls_.push_back(std::make_unique<Wall>("wall_right", config_.width - 0.1f,
                                             0.0f, 0.1f, config_.height));

    // Create interior obstacles (using Wall entities)
    // Obstacle 1: Vertical bar in upper-left quadrant
    walls_.push_back(std::make_unique<Wall>("obstacle_1", 4.0f, 12.0f, 1.0f, 4.0f));

    // Obstacle 2: Horizontal bar in center
    walls_.push_back(std::make_unique<Wall>("obstacle_2", 8.0f, 8.0f, 4.0f, 1.0f));

    // Obstacle 3: Square block in lower-center
    walls_.push_back(std::make_unique<Wall>("obstacle_3", 6.0f, 3.0f, 2.0f, 2.0f));

    // Obstacle 4: L-shaped obstacle (two walls) in right side
    walls_.push_back(std::make_unique<Wall>("obstacle_4a", 14.0f, 6.0f, 1.0f, 4.0f));
    walls_.push_back(std::make_unique<Wall>("obstacle_4b", 14.0f, 6.0f, 3.0f, 1.0f));

    // Create drop zone (moved for larger world)
    zones_.push_back(
        std::make_unique<Zone>("drop_zone", 16.0f, 16.0f, "drop_zone", 0.5f));
}
```

**Verify:** `colcon build` succeeds

---

### Task 3: Update unit tests for new world layout

**Goal:** Fix tests that assume old 10x10 world or old entity positions

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `ros_ws/src/warehouser_simulation/test/test_world_manager.cpp` |

**Steps:**
1. Update `RobotSpawnPosition` test to expect (2, 2)
2. Update `FindObjectByColorReturnsClosest` test for new object positions
3. Update `CheckCollisionWithWalls` test for new wall positions
4. Add test verifying obstacles are included in walls vector
5. Update any tests that move robot toward old wall positions

**Key test updates:**

```cpp
TEST_F(WorldManagerTest, RobotSpawnPosition) {
    auto* robot = world_->robot();
    EXPECT_NEAR(robot->x, 2.0f, 0.01f);  // Changed from 1.0f
    EXPECT_NEAR(robot->y, 2.0f, 0.01f);  // Changed from 1.0f
    EXPECT_NEAR(robot->theta, 0.0f, 0.01f);
}

TEST_F(WorldManagerTest, HasObstacles) {
    // Should have 4 boundary walls + 5 obstacle walls = 9 total
    EXPECT_GE(world_->walls().size(), 9u);
}

TEST_F(WorldManagerTest, ObstacleBlocksMovement) {
    world_->start();
    auto* robot = world_->robot();

    // Position robot next to obstacle_3 (at 6,3 size 2x2)
    robot->x = 5.5f;
    robot->y = 4.0f;
    robot->setCommand(1.0f, 0.0f);  // Move toward obstacle

    float prev_x = robot->x;
    world_->step(0.5f);

    // Robot should be stopped by obstacle
    EXPECT_NEAR(robot->x, prev_x, 0.01f);
}

TEST_F(WorldManagerTest, ResetRestoresInitialState) {
    // Update expected position to (2, 2)
    world_->start();
    auto* robot = world_->robot();
    robot->setCommand(1.0f, 0.0f);
    world_->step(5.0f);

    world_->reset();

    EXPECT_NEAR(robot->x, 2.0f, 0.01f);  // Changed from 1.0f
    EXPECT_NEAR(robot->y, 2.0f, 0.01f);  // Changed from 1.0f
    EXPECT_FLOAT_EQ(world_->simTime(), 0.0f);
    EXPECT_FALSE(world_->isRunning());
}
```

**Verify:** `colcon test --packages-select warehouser_simulation`

---

### Task 4: Update WorldConfig default values

**Goal:** Update default WorldConfig dimensions to match new world size

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `ros_ws/src/warehouser_simulation/include/warehouser_simulation/world_manager.hpp` |

**Steps:**
1. Change default width from 10.0f to 20.0f
2. Change default height from 10.0f to 20.0f

**Code change:**
```cpp
/// Configuration for the world (dimensions only)
struct WorldConfig {
    float width = 20.0f;   // Changed from 10.0f
    float height = 20.0f;  // Changed from 10.0f
};
```

**Verify:** Default-constructed WorldManager uses 20x20 dimensions

---

### Task 5: Verify lidar interaction with obstacles

**Goal:** Confirm lidar raycast detects obstacles (no code changes expected)

**Files:**
| Action | Path |
|--------|------|
| VERIFY | `ros_ws/src/warehouser_observations/src/lidar_simulator.cpp` |

**Steps:**
1. Review `checkWallCollision()` - confirms it checks `entity.type == 2` (TYPE_WALL)
2. Since obstacles are Wall entities with type=2, lidar will automatically detect them
3. No code changes required

**Verification:** The existing code handles obstacles automatically:
```cpp
bool LidarSimulator::checkWallCollision(
    float px, float py, const warehouser_msgs::msg::WorldState& world) const {
    for (const auto& entity : world.entities) {
        if (entity.type == 2) {  // TYPE_WALL = 2 - includes obstacles
            if (px >= entity.x && px <= entity.x + entity.width &&
                py >= entity.y && py <= entity.y + entity.height) {
                return true;
            }
        }
    }
    return false;
}
```

**Verify:** Run simulation and observe lidar detecting obstacles in frontend

---

### Task 6: Full integration test

**Goal:** Verify complete system works with larger world and obstacles

**Files:**
| Action | Path |
|--------|------|
| VERIFY | (manual testing) |

**Steps:**
1. Build: `cd ros_ws && colcon build`
2. Test: `colcon test --packages-select warehouser_simulation`
3. Run simulation: `ros2 run warehouser_simulation simulation_node`
4. Verify in frontend:
   - World is larger (20x20)
   - Obstacles appear as walls
   - Robot cannot pass through obstacles
   - Lidar rays stop at obstacles
   - Keyboard navigation works around obstacles

**Verify:** All tests pass, visual verification in frontend

## Acceptance Criteria

- [ ] World dimensions increased to 20x20 in both YAML config files
- [ ] WorldConfig default values updated to 20x20
- [ ] Robot spawn position updated to (2, 2)
- [ ] 5 interior obstacle walls added to createDefaultWorld()
- [ ] Pickable objects repositioned for larger world
- [ ] Drop zone repositioned for larger world
- [ ] Unit tests updated and passing
- [ ] Obstacles block robot movement
- [ ] Obstacles appear in lidar scans
- [ ] Obstacles render in frontend (as walls)
- [ ] `colcon build` succeeds
- [ ] `colcon test --packages-select warehouser_simulation` passes
