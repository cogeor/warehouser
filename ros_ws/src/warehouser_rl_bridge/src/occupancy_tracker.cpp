#include "warehouser_rl_bridge/occupancy_tracker.hpp"

namespace warehouser {

OccupancyTracker::OccupancyTracker(const OccupancyConfig& config)
    : config_(config) {
    grid_width_ = static_cast<size_t>(
        std::ceil(config.world_width / config.cell_size));
    grid_height_ = static_cast<size_t>(
        std::ceil(config.world_height / config.cell_size));
    visit_counts_.resize(grid_width_ * grid_height_, 0);
}

std::pair<size_t, size_t> OccupancyTracker::worldToCell(float x, float y) const {
    // Clamp to valid world bounds
    float clamped_x = std::max(0.0f, std::min(x, config_.world_width - 0.001f));
    float clamped_y = std::max(0.0f, std::min(y, config_.world_height - 0.001f));

    size_t cx = static_cast<size_t>(clamped_x / config_.cell_size);
    size_t cy = static_cast<size_t>(clamped_y / config_.cell_size);

    // Extra safety: clamp to grid bounds
    cx = std::min(cx, grid_width_ - 1);
    cy = std::min(cy, grid_height_ - 1);

    return {cx, cy};
}

size_t OccupancyTracker::cellIndex(size_t cx, size_t cy) const {
    return cy * grid_width_ + cx;
}

bool OccupancyTracker::markVisited(float x, float y) {
    auto [cx, cy] = worldToCell(x, y);
    size_t idx = cellIndex(cx, cy);
    bool is_new = (visit_counts_[idx] == 0);
    visit_counts_[idx]++;
    return is_new;
}

bool OccupancyTracker::isVisited(float x, float y) const {
    auto [cx, cy] = worldToCell(x, y);
    return visit_counts_[cellIndex(cx, cy)] > 0;
}

int OccupancyTracker::visitCount(float x, float y) const {
    auto [cx, cy] = worldToCell(x, y);
    return visit_counts_[cellIndex(cx, cy)];
}

float OccupancyTracker::coverage() const {
    if (visit_counts_.empty()) {
        return 0.0f;
    }
    return static_cast<float>(visitedCells()) /
           static_cast<float>(visit_counts_.size());
}

size_t OccupancyTracker::visitedCells() const {
    size_t count = 0;
    for (int visits : visit_counts_) {
        if (visits > 0) {
            count++;
        }
    }
    return count;
}

void OccupancyTracker::reset() {
    std::fill(visit_counts_.begin(), visit_counts_.end(), 0);
}

}  // namespace warehouser
