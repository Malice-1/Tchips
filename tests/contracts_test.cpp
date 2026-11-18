#include <tchips/contracts/version.hpp>
#include <entt/entt.hpp>
#include <gtest/gtest.h>

TEST(Protocol, AcceptsOnlyCurrentVersion) {
    using namespace tchips::contracts;
    EXPECT_TRUE(accepts_protocol(protocol_version));
    EXPECT_FALSE(accepts_protocol(0));
    EXPECT_FALSE(accepts_protocol(protocol_version + 1));
}
TEST(Entities, DestroyedEntityCannotRemainAlive) {
    entt::registry zone;
    const auto first = zone.create();
    ASSERT_TRUE(zone.valid(first));
    zone.destroy(first);
    EXPECT_FALSE(zone.valid(first));
    const auto replacement = zone.create();
    EXPECT_TRUE(zone.valid(replacement));
    EXPECT_FALSE(zone.valid(first));
}
