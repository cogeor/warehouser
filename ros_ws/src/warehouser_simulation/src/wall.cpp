#include "warehouser_simulation/wall.hpp"

namespace warehouser {

warehouser_msgs::msg::Entity Wall::toMsg() const {
    auto msg = Entity::toMsg();
    msg.width = width;
    msg.height = height;
    return msg;
}

}  // namespace warehouser
