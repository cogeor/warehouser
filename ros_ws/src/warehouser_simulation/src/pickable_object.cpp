#include "warehouser_simulation/pickable_object.hpp"

namespace warehouser {

warehouser_msgs::msg::Entity PickableObject::toMsg() const {
    auto msg = Entity::toMsg();
    msg.color = color;
    msg.pickup_radius = pickup_radius;
    msg.is_picked = is_picked;
    return msg;
}

}  // namespace warehouser
