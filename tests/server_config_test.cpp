#include <tchips/server/config.hpp>
#include <gtest/gtest.h>
#include <array>
#include <string_view>

TEST(ServerConfig, DefaultsUseFixedClockAndInterruptibleRun) {
    const auto result = tchips::server::parse_arguments({});
    ASSERT_TRUE(result);
    EXPECT_EQ(result->max_ticks, 0);
    EXPECT_TRUE(result->sleep);
    EXPECT_EQ(tchips::server::tick_rate, 30);
    EXPECT_GT(tchips::server::fixed_step.count(), 0);
}
TEST(ServerConfig, AcceptsBoundedDiagnosticRun) {
    const std::array<std::string_view, 3> args{"--ticks", "3", "--no-sleep"};
    const auto result = tchips::server::parse_arguments(args);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->max_ticks, 3);
    EXPECT_FALSE(result->sleep);
}
TEST(ServerConfig, RejectsMalformedOrUnboundedDiagnosticArguments) {
    for (const auto value : {"0", "-1", "1.5", "3trailing", "18446744073709551616"}) {
        const std::array<std::string_view, 2> args{"--ticks", value};
        EXPECT_FALSE(tchips::server::parse_arguments(args)) << value;
    }
    const std::array<std::string_view, 1> missing{"--ticks"}, unbounded{"--no-sleep"}, unknown{"--position"};
    EXPECT_FALSE(tchips::server::parse_arguments(missing));
    EXPECT_FALSE(tchips::server::parse_arguments(unbounded));
    EXPECT_FALSE(tchips::server::parse_arguments(unknown));
}
