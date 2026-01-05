#include "warehouser_simulation/entity.hpp"

namespace warehouser {

warehouser_msgs::msg::Entity Entity::toMsg() const {
    warehouser_msgs::msg::Entity msg;
    msg.id = id;
    msg.type = static_cast<uint8_t>(getType());
    msg.x = x;
    msg.y = y;
    return msg;
}

}  // namespace warehouser
