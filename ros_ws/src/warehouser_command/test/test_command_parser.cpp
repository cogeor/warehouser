#include <gtest/gtest.h>
#include "warehouser_command/command_parser.hpp"
#include "warehouser_command/object_resolver.hpp"

using namespace warehouser_command;

class CommandParserTest : public ::testing::Test {
protected:
    CommandParser parser_;
};

TEST_F(CommandParserTest, ParsesSimplePickCommand) {
    auto result = parser_.parse(R"({"action": "pick", "target": "red"})");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->action, "pick");
    EXPECT_EQ(result->target, "red");
}

TEST_F(CommandParserTest, ParsesGotoWithCoordinates) {
    auto result = parser_.parse(R"({"action": "goto", "destination": {"x": 5.0, "y": 3.0}})");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->action, "goto");
    ASSERT_TRUE(result->dest_x.has_value());
    ASSERT_TRUE(result->dest_y.has_value());
    EXPECT_FLOAT_EQ(*result->dest_x, 5.0f);
    EXPECT_FLOAT_EQ(*result->dest_y, 3.0f);
}

TEST_F(CommandParserTest, ParsesGotoWithZone) {
    auto result = parser_.parse(R"({"action": "goto", "destination": {"zone": "station_a"}})");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->action, "goto");
    ASSERT_TRUE(result->dest_zone.has_value());
    EXPECT_EQ(*result->dest_zone, "station_a");
}

TEST_F(CommandParserTest, FailsOnMissingAction) {
    auto result = parser_.parse(R"({"target": "red"})");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("action"), std::string::npos);
}

TEST_F(CommandParserTest, FailsOnInvalidJson) {
    auto result = parser_.parse("not valid json");
    ASSERT_FALSE(result.has_value());
}

TEST_F(CommandParserTest, FailsOnEmptyJson) {
    auto result = parser_.parse("{}");
    ASSERT_FALSE(result.has_value());
}

class ObjectResolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        objects_ = {
            {"red_1", "red", 3.0f, 2.0f, false},
            {"red_2", "red", 7.0f, 5.0f, false},
            {"green_1", "green", 5.0f, 5.0f, false},
            {"blue_1", "blue", 2.0f, 8.0f, true},  // picked
        };
        resolver_.updateObjects(objects_);
        resolver_.updateRobot({1.0f, 1.0f});
    }

    std::vector<ObjectInfo> objects_;
    ObjectResolver resolver_;
};

TEST_F(ObjectResolverTest, ResolvesClosestByColor) {
    auto result = resolver_.resolveByColor("red");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, "red_1");  // Closer to robot at (1,1)
}

TEST_F(ObjectResolverTest, SkipsPickedObjects) {
    auto result = resolver_.resolveByColor("blue");
    EXPECT_FALSE(result.has_value());  // blue_1 is picked
}

TEST_F(ObjectResolverTest, ResolvesById) {
    auto result = resolver_.resolveById("green_1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, "green_1");
}

TEST_F(ObjectResolverTest, ReturnsNulloptForUnknownColor) {
    auto result = resolver_.resolveByColor("purple");
    EXPECT_FALSE(result.has_value());
}

class ZoneResolverTest : public ::testing::Test {
protected:
    ZoneResolver resolver_;
};

TEST_F(ZoneResolverTest, ResolvesDefaultZones) {
    auto result = resolver_.resolve("station_a");
    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(result->first, 8.0f);
    EXPECT_FLOAT_EQ(result->second, 8.0f);
}

TEST_F(ZoneResolverTest, AddsCustomZone) {
    resolver_.addZone("custom", 4.0f, 4.0f);
    auto result = resolver_.resolve("custom");
    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(result->first, 4.0f);
}

TEST_F(ZoneResolverTest, ReturnsNulloptForUnknownZone) {
    auto result = resolver_.resolve("nonexistent");
    EXPECT_FALSE(result.has_value());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
