#include "warehouser_simulation/zone.hpp"

namespace warehouser {

warehouser_msgs::msg::Entity Zone::toMsg() const {
    auto msg = Entity::toMsg();
    msg.zone_name = zone_name;
    msg.radius = radius;
    return msg;
}

}  // namespace warehouser
