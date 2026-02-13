#include "warehouser_simulation/robot.hpp"

#include "warehouser_simulation/pickable_object.hpp"

namespace warehouser {

bool Robot::tryPick(PickableObject& obj) {
    // Can't pick if already carrying
    if (is_carrying) {
        return false;
    }

    // Can't pick if object already picked
    if (obj.is_picked) {
        return false;
    }

    // Check distance
    float dist = distance(x, y, obj.x, obj.y);
    if (dist > obj.pickup_radius) {
        return false;
    }

    // Pick up the object
    is_carrying = true;
    carried_object_id = obj.id;
    obj.is_picked = true;
    return true;
}

void Robot::unpick(PickableObject& obj) {
    // Verify we're carrying this object
    if (!is_carrying || carried_object_id != obj.id) {
        return;
    }

    // Drop at robot position
    is_carrying = false;
    carried_object_id.clear();
    obj.is_picked = false;
    obj.x = x;
    obj.y = y;
}

warehouser_msgs::msg::Entity Robot::toMsg() const {
    auto msg = Entity::toMsg();
    msg.theta = theta;
    msg.v = v;
    msg.omega = omega;
    msg.is_carrying = is_carrying;
    msg.carried_object_id = carried_object_id;
    msg.in_robot_collision = in_robot_collision;
    return msg;
}

}  // namespace warehouser
