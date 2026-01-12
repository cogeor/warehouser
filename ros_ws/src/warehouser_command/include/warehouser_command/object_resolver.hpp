#pragma once

#include <optional>
#include <string>
#include <vector>
#include <map>

namespace warehouser_command {

struct ObjectInfo {
    std::string id;
    std::string color;
    float x{0.0f};
    float y{0.0f};
    bool is_picked{false};
};

struct RobotInfo {
    float x{0.0f};
    float y{0.0f};
};

class ObjectResolver {
public:
    void updateObjects(const std::vector<ObjectInfo>& objects);
    void updateRobot(const RobotInfo& robot);

    [[nodiscard]] std::optional<ObjectInfo> resolveByColor(const std::string& color) const;
    [[nodiscard]] std::optional<ObjectInfo> resolveById(const std::string& id) const;

private:
    std::vector<ObjectInfo> objects_;
    RobotInfo robot_;
};

class ZoneResolver {
public:
    ZoneResolver();

    [[nodiscard]] std::optional<std::pair<float, float>> resolve(const std::string& zone) const;

    void addZone(const std::string& name, float x, float y);

private:
    std::map<std::string, std::pair<float, float>> zones_;
};

}  // namespace warehouser_command
