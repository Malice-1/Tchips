#include <chrono>
#include <cmath>
#include <functional>
#include <gtest/gtest.h>
#include <sstream>
#include <steam/isteamnetworkingutils.h>
#include <tchips/client/network_client.hpp>
#include <tchips/server/network_server.hpp>
#include <thread>

namespace {
using namespace tchips;
using Clock = std::chrono::steady_clock;
geometry::WorldMap test_map() {
    auto result =
        geometry::WorldMap::load(std::filesystem::path{TCHIPS_TEST_CONTENT_DIR} / "maps/prototype.map");
    if (!result)
        throw std::runtime_error(result.error());
    return std::move(*result);
}
struct Harness {
    geometry::WorldMap map{test_map()};
    server::World world{map};
    server::NetworkServer server{world};
    client::NetworkClient a{map}, b{map};
    math::WorldVector input_a{}, input_b{};
    Clock::time_point previous{Clock::now()}, next_tick{previous};
    Harness() {
        auto result = server.listen(0);
        if (!result)
            throw std::runtime_error(result.error());
    }
    std::string address() const { return "127.0.0.1:" + std::to_string(server.port()); }
    void step() {
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        server.pump();
        a.update(elapsed, input_a, 0);
        b.update(elapsed, input_b, 0);
        if (now >= next_tick) {
            world.step();
            server.publish();
            next_tick = now + std::chrono::nanoseconds{1'000'000'000 / 30};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    bool wait(const std::function<bool()> &predicate,
              std::chrono::seconds timeout = std::chrono::seconds(5)) {
        const auto end = Clock::now() + timeout;
        while (Clock::now() < end) {
            step();
            if (predicate())
                return true;
        }
        return false;
    }
};
struct RawPeer {
    HSteamNetConnection connection{};
    ~RawPeer() {
        if (connection != 0)
            SteamNetworkingSockets()->CloseConnection(connection, 1000, "test end", false);
    }
    bool send(const wire::pb::Envelope &message, bool reliable = true) {
        const auto bytes = message.SerializeAsString();
        return SteamNetworkingSockets()->SendMessageToConnection(
                   connection, bytes.data(), static_cast<uint32>(bytes.size()),
                   reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_UnreliableNoNagle,
                   nullptr) == k_EResultOK;
    }
    std::optional<wire::pb::Envelope> receive() {
        SteamNetworkingMessage_t *message = nullptr;
        if (SteamNetworkingSockets()->ReceiveMessagesOnConnection(connection, &message, 1) <= 0)
            return {};
        const auto decoded = wire::decode(
            {static_cast<const std::byte *>(message->m_pData), static_cast<std::size_t>(message->m_cbSize)},
            false);
        message->Release();
        return decoded ? std::optional{*decoded} : std::nullopt;
    }
    bool open(Harness &harness) {
        SteamNetworkingIPAddr address;
        address.Clear();
        if (!address.ParseString(harness.address().c_str()))
            return false;
        connection = SteamNetworkingSockets()->ConnectByIPAddress(address, 0, nullptr);
        return harness.wait([&] {
            SteamNetConnectionInfo_t info{};
            return SteamNetworkingSockets()->GetConnectionInfo(connection, &info) &&
                   info.m_eState == k_ESteamNetworkingConnectionState_Connected;
        });
    }
    std::optional<wire::pb::ConnectionAccepted> accept(Harness &harness) {
        auto hello = wire::envelope();
        hello.mutable_handshake()->set_map_fingerprint(harness.map.fingerprint());
        if (!send(hello))
            return {};
        std::optional<wire::pb::ConnectionAccepted> accepted;
        if (!harness.wait([&] {
                if (auto message = receive(); message && message->has_accepted())
                    accepted = message->accepted();
                return accepted.has_value();
            }))
            return {};
        return accepted;
    }
};
class NetworkIntegration : public testing::Test {
  protected:
    static void SetUpTestSuite() {
        SteamNetworkingErrMsg error;
        ASSERT_TRUE(GameNetworkingSockets_Init(nullptr, error)) << error;
    }
    static void TearDownTestSuite() { GameNetworkingSockets_Kill(); }
    void TearDown() override {
        SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, 0);
        SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, 0);
        SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketJitter_Send_Avg,
                                                          0);
        SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketJitter_Send_Max,
                                                          0);
        SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketJitter_Send_Pct,
                                                          0);
    }
};
} // namespace
TEST_F(NetworkIntegration, TwoClientsMoveSeeEachOtherAndReconnectWithoutGhost) {
    Harness h;
    ASSERT_TRUE(h.a.connect(h.address()));
    ASSERT_TRUE(h.b.connect(h.address()));
    ASSERT_TRUE(h.wait([&] {
        return h.a.active() && h.b.active() && h.a.visible_count() == 2 && h.b.visible_count() == 2;
    }));
    const auto old_session = h.a.session_id(), old_entity = h.a.prediction().state().id;
    EXPECT_NE(old_session, h.b.session_id());
    const auto start = h.world.player(old_session)->position;
    h.input_a = {1, 0};
    ASSERT_TRUE(h.wait([&] { return h.world.player(old_session)->position.x > start.x + 40; }));
    h.input_a = {};
    ASSERT_TRUE(h.wait([&] { return h.a.prediction().authoritative_distance() < 0.01; }));
    auto remotes = h.b.remote_entities();
    ASSERT_EQ(remotes.size(), 1);
    EXPECT_EQ(remotes[0].id, old_entity);
    h.a.close();
    ASSERT_TRUE(h.wait([&] { return h.world.player_count() == 1 && h.b.visible_count() == 1; }));
    EXPECT_FALSE(h.world.player(old_session));
    ASSERT_TRUE(h.a.connect(h.address()));
    ASSERT_TRUE(h.wait([&] { return h.a.active() && h.a.visible_count() == 2 && h.b.visible_count() == 2; }));
    EXPECT_NE(h.a.session_id(), old_session);
    EXPECT_NE(h.a.prediction().state().id, old_entity);
    for (const auto &entity : h.b.remote_entities())
        EXPECT_NE(entity.id, old_entity);
}
TEST_F(NetworkIntegration, InvalidCommandAndClientPositionResultsRejected) {
    for (const bool forge_result : {false, true}) {
        Harness h;
        RawPeer raw;
        ASSERT_TRUE(raw.open(h));
        const auto accepted = raw.accept(h);
        ASSERT_TRUE(accepted);
        ASSERT_EQ(h.world.player_count(), 1);
        auto invalid = wire::envelope(accepted->session_id());
        if (forge_result) {
            auto *state = invalid.mutable_snapshot()->add_entities();
            state->set_entity_id(accepted->player().entity_id());
            state->set_x(999);
            state->set_y(999);
        } else {
            auto *input = invalid.mutable_inputs()->add_commands();
            input->set_sequence(1);
            input->set_move_x(2);
        }
        ASSERT_TRUE(raw.send(invalid));
        ASSERT_TRUE(h.wait([&] { return h.world.player_count() == 0; }));
    }
}
TEST_F(NetworkIntegration, DuplicateAndFutureInputsDoNotBypassServerBudget) {
    Harness h;
    RawPeer raw;
    ASSERT_TRUE(raw.open(h));
    auto accepted = raw.accept(h);
    ASSERT_TRUE(accepted);
    const auto start = accepted->player().x();
    auto message = wire::envelope(accepted->session_id());
    for (unsigned i = 0; i < 16; ++i) {
        auto *input = message.mutable_inputs()->add_commands();
        input->set_sequence(1);
        input->set_move_x(1);
    }
    ASSERT_TRUE(raw.send(message, false));
    ASSERT_TRUE(h.wait([&] { return h.world.acknowledged(accepted->session_id()) == 1; }));
    for (int i = 0; i < 30; ++i)
        h.step();
    EXPECT_DOUBLE_EQ(h.world.player(accepted->session_id())->position.x, start + 8);
    message.mutable_inputs()->mutable_commands(0)->set_sequence(1000000);
    ASSERT_TRUE(raw.send(message, false));
    ASSERT_TRUE(h.wait([&] { return h.world.player_count() == 0; }));
}
TEST_F(NetworkIntegration, VersionAndMapMismatchCreateNoPlayer) {
    for (const bool version : {true, false}) {
        Harness h;
        RawPeer raw;
        ASSERT_TRUE(raw.open(h));
        auto message = wire::envelope();
        message.mutable_handshake()->set_map_fingerprint(version ? h.map.fingerprint() : 0);
        if (version)
            message.set_protocol_version(1);
        ASSERT_TRUE(raw.send(message));
        bool closed = false;
        ASSERT_TRUE(h.wait([&] {
            if (auto response = raw.receive(); response && response->has_disconnect())
                closed = true;
            return closed;
        }));
        EXPECT_EQ(h.world.player_count(), 0);
    }
}
TEST_F(NetworkIntegration, LatencyLossAndJitterConvergeAfterStopping) {
    ASSERT_TRUE(
        SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, 80));
    ASSERT_TRUE(
        SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, 15));
    ASSERT_TRUE(SteamNetworkingUtils()->SetGlobalConfigValueFloat(
        k_ESteamNetworkingConfig_FakePacketJitter_Send_Avg, 10));
    ASSERT_TRUE(SteamNetworkingUtils()->SetGlobalConfigValueFloat(
        k_ESteamNetworkingConfig_FakePacketJitter_Send_Max, 30));
    ASSERT_TRUE(SteamNetworkingUtils()->SetGlobalConfigValueFloat(
        k_ESteamNetworkingConfig_FakePacketJitter_Send_Pct, 50));
    Harness h;
    ASSERT_TRUE(h.a.connect(h.address()));
    ASSERT_TRUE(h.b.connect(h.address()));
    ASSERT_TRUE(h.wait(
        [&] { return h.a.active() && h.b.active() && h.a.visible_count() == 2 && h.b.visible_count() == 2; },
        std::chrono::seconds(10)));
    const auto session = h.a.session_id();
    const auto start = h.world.player(session)->position.x;
    h.input_a = {1, 0};
    h.input_b = {1, 0};
    ASSERT_TRUE(
        h.wait([&] { return h.world.player(session) && h.world.player(session)->position.x > start + 40; },
               std::chrono::seconds(5)));
    h.input_a = {};
    h.input_b = {};
    ASSERT_TRUE(h.wait(
        [&] {
            return h.a.active() && h.b.active() && h.a.prediction().authoritative_distance() < 0.01 &&
                   h.b.prediction().authoritative_distance() < 0.01 &&
                   std::abs(h.a.prediction().state().position.x - h.world.player(session)->position.x) < 0.01;
        },
        std::chrono::seconds(5)));
    const auto end = Clock::now() + std::chrono::milliseconds(700);
    while (Clock::now() < end) {
        h.step();
        ASSERT_TRUE(h.a.active());
        EXPECT_LT(std::abs(h.a.prediction().state().position.x - h.world.player(session)->position.x), 0.01);
    }
}
TEST_F(NetworkIntegration, CompleteInterruptionCleansAndAutomaticallyReconnects) {
    Harness h;
    ASSERT_TRUE(h.a.connect(h.address()));
    ASSERT_TRUE(h.b.connect(h.address()));
    ASSERT_TRUE(h.wait([&] { return h.a.active() && h.b.active() && h.world.player_count() == 2; }));
    const auto old_a = h.a.session_id(), old_b = h.b.session_id();
    ASSERT_TRUE(
        SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, 100));
    ASSERT_TRUE(h.wait([&] { return h.world.player_count() == 0 && !h.a.active() && !h.b.active(); },
                       std::chrono::seconds(8)));
    ASSERT_TRUE(
        SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, 0));
    ASSERT_TRUE(h.wait(
        [&] { return h.a.active() && h.b.active() && h.a.visible_count() == 2 && h.b.visible_count() == 2; },
        std::chrono::seconds(8)));
    EXPECT_NE(h.a.session_id(), old_a);
    EXPECT_NE(h.b.session_id(), old_b);
    EXPECT_FALSE(h.world.player(old_a));
    EXPECT_FALSE(h.world.player(old_b));
    EXPECT_EQ(h.world.player_count(), 2);
}
TEST_F(NetworkIntegration, ServerShutdownRemovesAllPlayers) {
    Harness h;
    ASSERT_TRUE(h.a.connect(h.address()));
    ASSERT_TRUE(h.b.connect(h.address()));
    ASSERT_TRUE(h.wait([&] { return h.a.active() && h.b.active(); }));
    h.server.shutdown();
    EXPECT_EQ(h.world.player_count(), 0);
    ASSERT_TRUE(h.wait([&] { return !h.a.active() && !h.b.active(); }));
}
