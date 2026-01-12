#include "warehouser_command/object_resolver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace warehouser_command {

namespace {

float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

void ObjectResolver::updateObjects(const std::vector<ObjectInfo>& objects) {
    objects_ = objects;
}

void ObjectResolver::updateRobot(const RobotInfo& robot) {
    robot_ = robot;
}

std::optional<ObjectInfo> ObjectResolver::resolveByColor(const std::string& color) const {
    std::vector<ObjectInfo> matches;

    for (const auto& obj : objects_) {
        if (obj.color == color && !obj.is_picked) {
            matches.push_back(obj);
        }
    }

    if (matches.empty()) {
        return std::nullopt;
    }

    // Find closest to robot
    auto closest = std::min_element(matches.begin(), matches.end(),
        [this](const ObjectInfo& a, const ObjectInfo& b) {
            return distance(robot_.x, robot_.y, a.x, a.y) <
                   distance(robot_.x, robot_.y, b.x, b.y);
        });

    return *closest;
}

std::optional<ObjectInfo> ObjectResolver::resolveById(const std::string& id) const {
    for (const auto& obj : objects_) {
        if (obj.id == id) {
            return obj;
        }
    }
    return std::nullopt;
}

ZoneResolver::ZoneResolver() {
    // Default zones
    zones_["station_a"] = {8.0f, 8.0f};
    zones_["station_b"] = {2.0f, 8.0f};
    zones_["charging"] = {1.0f, 1.0f};
    zones_["drop_zone"] = {8.0f, 1.0f};
}

std::optional<std::pair<float, float>> ZoneResolver::resolve(const std::string& zone) const {
    auto it = zones_.find(zone);
    if (it != zones_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ZoneResolver::addZone(const std::string& name, float x, float y) {
    zones_[name] = {x, y};
}

}  // namespace warehouser_command
