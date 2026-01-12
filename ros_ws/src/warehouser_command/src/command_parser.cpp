#include "warehouser_command/command_parser.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace warehouser_command {

namespace {

std::string trim(const std::string& str) {
    const auto start = str.find_first_not_of(" \t\n\r\"");
    if (start == std::string::npos) return "";
    const auto end = str.find_last_not_of(" \t\n\r\"");
    return str.substr(start, end - start + 1);
}

std::string removeQuotes(const std::string& str) {
    std::string s = trim(str);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

}  // namespace

std::optional<float> CommandParser::parseFloat(const std::string& value) {
    std::string v = trim(value);
    float result{};
    auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), result);
    if (ec == std::errc()) {
        return result;
    }
    return std::nullopt;
}

std::expected<std::map<std::string, std::string>, std::string>
CommandParser::parseSimpleJson(const std::string& json_str) {
    // Simple JSON parser for flat objects
    // Supports: {"key": "value", "key2": 123}

    std::map<std::string, std::string> result;

    std::string str = json_str;

    // Remove outer braces
    auto start = str.find('{');
    auto end = str.rfind('}');
    if (start == std::string::npos || end == std::string::npos || start >= end) {
        return std::unexpected("Invalid JSON: missing braces");
    }
    str = str.substr(start + 1, end - start - 1);

    // Parse key-value pairs
    size_t pos = 0;
    while (pos < str.size()) {
        // Find key
        auto key_start = str.find('"', pos);
        if (key_start == std::string::npos) break;

        auto key_end = str.find('"', key_start + 1);
        if (key_end == std::string::npos) {
            return std::unexpected("Invalid JSON: unclosed key quote");
        }

        std::string key = str.substr(key_start + 1, key_end - key_start - 1);

        // Find colon
        auto colon = str.find(':', key_end);
        if (colon == std::string::npos) {
            return std::unexpected("Invalid JSON: missing colon after key");
        }

        // Find value
        pos = colon + 1;
        while (pos < str.size() && std::isspace(str[pos])) pos++;

        std::string value;

        if (pos < str.size() && str[pos] == '"') {
            // String value
            auto val_start = pos + 1;
            auto val_end = str.find('"', val_start);
            if (val_end == std::string::npos) {
                return std::unexpected("Invalid JSON: unclosed value quote");
            }
            value = str.substr(val_start, val_end - val_start);
            pos = val_end + 1;
        } else if (pos < str.size() && str[pos] == '{') {
            // Nested object - find matching brace
            int depth = 1;
            auto obj_start = pos;
            pos++;
            while (pos < str.size() && depth > 0) {
                if (str[pos] == '{') depth++;
                else if (str[pos] == '}') depth--;
                pos++;
            }
            value = str.substr(obj_start, pos - obj_start);
        } else {
            // Number or literal
            auto val_start = pos;
            while (pos < str.size() && str[pos] != ',' && str[pos] != '}') {
                pos++;
            }
            value = trim(str.substr(val_start, pos - val_start));
        }

        result[key] = value;

        // Skip comma
        while (pos < str.size() && (str[pos] == ',' || std::isspace(str[pos]))) {
            pos++;
        }
    }

    return result;
}

std::expected<Command, std::string> CommandParser::parse(const std::string& json_str) {
    auto parsed = parseSimpleJson(json_str);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }

    const auto& fields = *parsed;
    Command cmd;

    // Required: action
    if (auto it = fields.find("action"); it != fields.end()) {
        cmd.action = it->second;
    } else {
        return std::unexpected("Missing required field: action");
    }

    // Optional: target
    if (auto it = fields.find("target"); it != fields.end()) {
        cmd.target = it->second;
    }

    // Optional: destination (nested object)
    if (auto it = fields.find("destination"); it != fields.end()) {
        auto dest = parseSimpleJson(it->second);
        if (dest) {
            if (auto x_it = dest->find("x"); x_it != dest->end()) {
                cmd.dest_x = parseFloat(x_it->second);
            }
            if (auto y_it = dest->find("y"); y_it != dest->end()) {
                cmd.dest_y = parseFloat(y_it->second);
            }
            if (auto zone_it = dest->find("zone"); zone_it != dest->end()) {
                cmd.dest_zone = zone_it->second;
            }
        }
    }

    return cmd;
}

}  // namespace warehouser_command
