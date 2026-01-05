#include <gtest/gtest.h>

#include "warehouser_simulation/entity.hpp"
#include "warehouser_simulation/pickable_object.hpp"
#include "warehouser_simulation/wall.hpp"
#include "warehouser_simulation/zone.hpp"

using namespace warehouser;

class EntityTest : public ::testing::Test {};

TEST_F(EntityTest, NormalizeAnglePositive) {
    // Angles within range should stay the same
    EXPECT_NEAR(normalizeAngle(0.0f), 0.0f, 0.001f);
    EXPECT_NEAR(normalizeAngle(1.0f), 1.0f, 0.001f);
    EXPECT_NEAR(normalizeAngle(-1.0f), -1.0f, 0.001f);
}

TEST_F(EntityTest, NormalizeAngleWrap) {
    // Angles outside [-π, π] should wrap
    float pi = 3.14159265f;
    EXPECT_NEAR(normalizeAngle(2 * pi), 0.0f, 0.01f);
    EXPECT_NEAR(normalizeAngle(3 * pi), -pi, 0.01f);
    EXPECT_NEAR(normalizeAngle(-3 * pi), pi, 0.01f);
}

TEST_F(EntityTest, DistanceCalculation) {
    EXPECT_NEAR(distance(0.0f, 0.0f, 3.0f, 4.0f), 5.0f, 0.001f);
    EXPECT_NEAR(distance(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0.001f);
    EXPECT_NEAR(distance(-1.0f, -1.0f, 1.0f, 1.0f), 2.828f, 0.01f);
}

TEST_F(EntityTest, WallContainsPoint) {
    Wall wall("wall1", 0.0f, 0.0f, 2.0f, 3.0f);

    // Points inside
    EXPECT_TRUE(wall.contains(1.0f, 1.0f));
    EXPECT_TRUE(wall.contains(0.0f, 0.0f));
    EXPECT_TRUE(wall.contains(2.0f, 3.0f));

    // Points outside
    EXPECT_FALSE(wall.contains(-0.1f, 1.0f));
    EXPECT_FALSE(wall.contains(2.1f, 1.0f));
    EXPECT_FALSE(wall.contains(1.0f, 3.1f));
}

TEST_F(EntityTest, ZoneContainsPoint) {
    Zone zone("zone1", 5.0f, 5.0f, "test_zone", 1.0f);

    // Points inside
    EXPECT_TRUE(zone.contains(5.0f, 5.0f));
    EXPECT_TRUE(zone.contains(5.5f, 5.0f));
    EXPECT_TRUE(zone.contains(5.0f, 5.9f));

    // Points outside
    EXPECT_FALSE(zone.contains(6.5f, 5.0f));
    EXPECT_FALSE(zone.contains(5.0f, 6.5f));
}

TEST_F(EntityTest, PickableObjectContainsPoint) {
    PickableObject obj("obj1", 3.0f, 3.0f, "red");
    obj.pickup_radius = 0.5f;

    // Points within pickup range
    EXPECT_TRUE(obj.contains(3.0f, 3.0f));
    EXPECT_TRUE(obj.contains(3.3f, 3.0f));

    // Points outside pickup range
    EXPECT_FALSE(obj.contains(4.0f, 3.0f));
}

TEST_F(EntityTest, EntityTypeCorrect) {
    PickableObject obj("obj", 0.0f, 0.0f, "red");
    Wall wall("wall", 0.0f, 0.0f, 1.0f, 1.0f);
    Zone zone("zone", 0.0f, 0.0f, "test", 1.0f);

    EXPECT_EQ(obj.getType(), EntityType::Object);
    EXPECT_EQ(wall.getType(), EntityType::Wall);
    EXPECT_EQ(zone.getType(), EntityType::Zone);
}

TEST_F(EntityTest, PickableObjectToMsg) {
    PickableObject obj("red_1", 3.0f, 2.0f, "red");
    obj.pickup_radius = 0.5f;
    obj.is_picked = true;

    auto msg = obj.toMsg();

    EXPECT_EQ(msg.id, "red_1");
    EXPECT_EQ(msg.type, static_cast<uint8_t>(EntityType::Object));
    EXPECT_FLOAT_EQ(msg.x, 3.0f);
    EXPECT_FLOAT_EQ(msg.y, 2.0f);
    EXPECT_EQ(msg.color, "red");
    EXPECT_FLOAT_EQ(msg.pickup_radius, 0.5f);
    EXPECT_TRUE(msg.is_picked);
}
