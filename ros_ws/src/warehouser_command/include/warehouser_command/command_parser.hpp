#pragma once

#include <expected>
#include <optional>
#include <string>
#include <map>

namespace warehouser_command {

struct Command {
    std::string action;  // "pick", "place", "goto"
    std::string target;  // color, zone, or object_id
    std::optional<float> dest_x;
    std::optional<float> dest_y;
    std::optional<std::string> dest_zone;
};

class CommandParser {
public:
    [[nodiscard]] std::expected<Command, std::string> parse(const std::string& json_str);

private:
    [[nodiscard]] std::expected<std::map<std::string, std::string>, std::string>
    parseSimpleJson(const std::string& json_str);

    [[nodiscard]] std::optional<float> parseFloat(const std::string& value);
};

}  // namespace warehouser_command
