#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <tchips/client/prediction.hpp>
#include <tchips/server/world.hpp>

namespace {
using namespace tchips;
geometry::WorldMap test_map(std::string_view obstacles = "0") {
    std::istringstream input{"TCHIPS_MAP 1 size 3000 3000 tile 64 spawn 100 100 obstacles " +
                             std::string(obstacles)};
    auto map = geometry::WorldMap::parse(input);
    if (!map)
        throw std::runtime_error(map.error());
    return std::move(*map);
}
} // namespace
TEST(AuthoritativeWorld, TwoPlayersUniqueEntitiesAndNoGhostOnReconnect) {
    server::World world{test_map()};
    auto a = world.connect(1), b = world.connect(2);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_NE(a->id, b->id);
    EXPECT_EQ(world.player_count(), 2);
    EXPECT_EQ(world.visible(1).size(), 2);
    world.disconnect(1);
    EXPECT_FALSE(world.player(1));
    EXPECT_EQ(world.visible(2).size(), 1);
    auto c = world.connect(3);
    ASSERT_TRUE(c);
    EXPECT_NE(c->id, a->id);
    EXPECT_EQ(world.player_count(), 2);
}
TEST(AuthoritativeWorld, InvalidInputsCannotChangeState) {
    server::World world{test_map()};
    const auto initial = world.connect(1);
    ASSERT_TRUE(initial);
    for (const auto command :
         std::vector<contracts::InputCommand>{{0, {1, 0}, 0},
                                              {1, {2, 0}, 0},
                                              {1, {0, -2}, 0},
                                              {1, {std::numeric_limits<double>::quiet_NaN(), 0}, 0},
                                              {1, {1, 0}, std::numeric_limits<double>::infinity()}})
        EXPECT_EQ(world.submit(1, command), server::InputResult::invalid);
    EXPECT_EQ(world.submit(1, {std::numeric_limits<std::uint64_t>::max(), {1, 0}, 0}),
              server::InputResult::out_of_window);
    world.step();
    EXPECT_DOUBLE_EQ(world.player(1)->position.x, initial->position.x);
    EXPECT_EQ(world.acknowledged(1), 0);
}
TEST(AuthoritativeWorld, SpeedAndDiagonalAreBoundedAtThirtyTicks) {
    server::World world{test_map()};
    const auto a = world.connect(1), b = world.connect(2);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    for (std::uint64_t seq = 1; seq <= 30; ++seq) {
        EXPECT_EQ(world.submit(1, {seq, {1, 0}, 0}), server::InputResult::accepted);
        EXPECT_EQ(world.submit(2, {seq, {1, 1}, 0}), server::InputResult::accepted);
        world.step();
    }
    EXPECT_NEAR(world.player(1)->position.x - a->position.x, 240, 1e-8);
    const auto p = world.player(2)->position;
    EXPECT_NEAR(std::hypot(p.x - b->position.x, p.y - b->position.y), 240, 1e-8);
}
TEST(AuthoritativeWorld, FloodProcessesAtMostOneCommandPerTick) {
    server::World world{test_map()};
    auto start = world.connect(1);
    ASSERT_TRUE(start);
    for (std::uint64_t i = 1; i <= 120; ++i)
        EXPECT_EQ(world.submit(1, {i, {1, 0}, 0}), server::InputResult::accepted);
    EXPECT_EQ(world.submit(1, {121, {1, 0}, 0}), server::InputResult::out_of_window);
    world.step();
    EXPECT_DOUBLE_EQ(world.player(1)->position.x, start->position.x + 8);
    EXPECT_EQ(world.acknowledged(1), 1);
}
TEST(AuthoritativeWorld, DuplicatesAndReorderingDoNotAddMovement) {
    server::World world{test_map()};
    auto start = world.connect(1);
    ASSERT_TRUE(start);
    EXPECT_EQ(world.submit(1, {2, {1, 0}, 0}), server::InputResult::accepted);
    EXPECT_EQ(world.submit(1, {1, {1, 0}, 0}), server::InputResult::accepted);
    EXPECT_EQ(world.submit(1, {1, {-1, 0}, 0}), server::InputResult::duplicate);
    world.step();
    world.step();
    EXPECT_EQ(world.acknowledged(1), 2);
    EXPECT_EQ(world.submit(1, {1, {1, 0}, 0}), server::InputResult::duplicate);
    for (int i = 0; i < 30; ++i)
        world.step();
    EXPECT_DOUBLE_EQ(world.player(1)->position.x, start->position.x + 16);
}
TEST(AuthoritativeWorld, LostGapEventuallySkippedWithoutSpeedBonus) {
    server::World world{test_map()};
    auto start = world.connect(1);
    ASSERT_TRUE(start);
    EXPECT_EQ(world.submit(1, {4, {1, 0}, 0}), server::InputResult::accepted);
    for (int i = 0; i < 3; ++i)
        world.step();
    EXPECT_EQ(world.acknowledged(1), 0);
    world.step();
    EXPECT_EQ(world.acknowledged(1), 4);
    EXPECT_DOUBLE_EQ(world.player(1)->position.x, start->position.x + 8);
    EXPECT_EQ(world.submit(1, {3, {1, 0}, 0}), server::InputResult::duplicate);
}
TEST(AuthoritativeWorld, ServerAppliesWallAndVisibility) {
    server::World world{test_map("1 200 50 1 500")};
    ASSERT_TRUE(world.connect(1));
    for (std::uint64_t seq = 1; seq <= 60; ++seq) {
        (void)world.submit(1, {seq, {1, 0}, 0});
        world.step();
    }
    EXPECT_DOUBLE_EQ(world.player(1)->position.x, 188);
    server::WorldConfig config;
    config.visibility_radius = 64;
    config.max_players = 2;
    server::World small{test_map(), config};
    ASSERT_TRUE(small.connect(1));
    ASSERT_TRUE(small.connect(2));
    EXPECT_FALSE(small.connect(3));
    for (std::uint64_t i = 1; i <= 20; ++i) {
        (void)small.submit(2, {i, {0, 1}, 0});
        small.step();
    }
    EXPECT_EQ(small.visible(1).size(), 1);
}
TEST(GeometryContract, FingerprintIsStableAcrossWhitespaceAndChangesWithGeometry) {
    auto a = test_map();
    std::istringstream input{"TCHIPS_MAP 1\nsize 3000 3000\ntile 64\nspawn 100 100\nobstacles 0\n"};
    auto b = geometry::WorldMap::parse(input);
    ASSERT_TRUE(b);
    EXPECT_EQ(a.fingerprint(), b->fingerprint());
    EXPECT_NE(a.fingerprint(), test_map("1 200 200 20 20").fingerprint());
}
TEST(Prediction, RejectsImpossibleAcknowledgementAndReplaysOnlyPending) {
    auto map = test_map();
    client::Prediction prediction{map};
    prediction.begin({1, {100, 100}, 0}, {});
    ASSERT_TRUE(prediction.predict({1, 0}, 0));
    ASSERT_TRUE(prediction.predict({1, 0}, 0));
    EXPECT_FALSE(prediction.reconcile({1, {100, 100}, 0}, 3));
    EXPECT_TRUE(prediction.reconcile({1, {108, 100}, 0}, 1));
    EXPECT_DOUBLE_EQ(prediction.state().position.x, 116);
    EXPECT_EQ(prediction.pending_count(), 1);
    EXPECT_FALSE(prediction.reconcile({1, {108, 100}, 0}, 0));
}
TEST(Prediction, ConvergesUnderLatencyLossDuplicatesAndOutOfOrderSnapshots) {
    for (const auto latency : {0U, 3U, 6U}) {
        auto map = test_map("1 900 50 4 1500");
        server::World world{map};
        auto initial = world.connect(1);
        ASSERT_TRUE(initial);
        client::Prediction prediction{map};
        prediction.begin(*initial, {});
        struct Packet {
            unsigned due;
            std::vector<contracts::InputCommand> batch;
        };
        struct Snapshot {
            unsigned due;
            std::uint64_t tick, ack;
            contracts::EntityState self;
        };
        std::vector<Packet> packets;
        std::vector<Snapshot> snapshots;
        std::uint64_t last_snapshot = 0;
        for (unsigned tick = 1; tick <= 240; ++tick) {
            ASSERT_TRUE(prediction.predict(tick < 100 ? math::WorldVector{1, 0} : math::WorldVector{}, 0));
            if (tick % 7 != 0) {
                const auto batch = prediction.redundant_batch();
                packets.push_back({tick + latency + (tick % 11 == 0 ? 2U : 0U), batch});
                if (tick % 13 == 0)
                    packets.push_back({tick + latency + 1, batch});
            }
            for (auto it = packets.begin(); it != packets.end();)
                if (it->due <= tick) {
                    for (const auto &command : it->batch) {
                        const auto result = world.submit(1, command);
                        EXPECT_TRUE(result == server::InputResult::accepted ||
                                    result == server::InputResult::duplicate);
                    }
                    it = packets.erase(it);
                } else
                    ++it;
            world.step();
            if (tick % 5 != 0)
                snapshots.push_back({tick + latency + (tick % 17 == 0 ? 5U : 0U), world.tick(),
                                     world.acknowledged(1), *world.player(1)});
            for (auto it = snapshots.begin(); it != snapshots.end();)
                if (it->due <= tick) {
                    if (it->tick > last_snapshot) {
                        ASSERT_TRUE(prediction.reconcile(it->self, it->ack));
                        last_snapshot = it->tick;
                    }
                    it = snapshots.erase(it);
                } else
                    ++it;
        }
        EXPECT_NEAR(prediction.state().position.x, world.player(1)->position.x, 1e-8) << latency;
        EXPECT_NEAR(prediction.authoritative_distance(), 0, 1e-8);
        EXPECT_LE(world.player(1)->position.x, 888);
        EXPECT_GT(world.acknowledged(1), 200);
    }
}
TEST(Interpolation, ClampsAndWrapsAnglesWithoutExtrapolation) {
    client::RemoteInterpolator view;
    view.push(10, {1, {100, 100}, 3.1});
    view.push(20, {1, {200, 100}, -3.1});
    view.push(15, {1, {999, 100}, 0});
    auto middle = view.sample(15);
    EXPECT_DOUBLE_EQ(middle.position.x, 150);
    EXPECT_NEAR(middle.angle, std::acos(-1.0), 1e-9);
    EXPECT_DOUBLE_EQ(view.sample(0).position.x, 100);
    EXPECT_DOUBLE_EQ(view.sample(999).position.x, 200);
}
