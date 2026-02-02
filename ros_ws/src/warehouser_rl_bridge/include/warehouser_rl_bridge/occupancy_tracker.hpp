#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace warehouser {

/// Configuration for occupancy tracking
struct OccupancyConfig {
    float world_width = 10.0f;   // Width of world in meters
    float world_height = 10.0f;  // Height of world in meters
    float cell_size = 0.5f;      // Cell size in meters
};

/// Tracks visited cells for exploration rewards.
/// Uses a 2D grid discretization of the world.
class OccupancyTracker {
public:
    explicit OccupancyTracker(const OccupancyConfig& config = {});

    /// Mark cell at world position as visited
    /// @param x World X coordinate
    /// @param y World Y coordinate
    /// @return true if this is a NEW visit (first time visiting this cell)
    bool markVisited(float x, float y);

    /// Check if cell at position was ever visited
    bool isVisited(float x, float y) const;

    /// Get visit count for cell at position
    int visitCount(float x, float y) const;

    /// Get coverage percentage [0, 1]
    /// @return Fraction of cells that have been visited at least once
    float coverage() const;

    /// Reset all visit counts to zero
    void reset();

    /// Get grid dimensions
    size_t gridWidth() const { return grid_width_; }
    size_t gridHeight() const { return grid_height_; }

    /// Get total cell count
    size_t totalCells() const { return visit_counts_.size(); }

    /// Get number of visited cells
    size_t visitedCells() const;

    /// Get config
    const OccupancyConfig& config() const { return config_; }

private:
    OccupancyConfig config_;
    size_t grid_width_;
    size_t grid_height_;
    std::vector<int> visit_counts_;  // Flattened grid [y * width + x]

    /// Convert world position to grid cell coordinates
    /// Clamps to valid grid bounds
    std::pair<size_t, size_t> worldToCell(float x, float y) const;

    /// Get flat index from cell coordinates
    size_t cellIndex(size_t cx, size_t cy) const;
};

}  // namespace warehouser
