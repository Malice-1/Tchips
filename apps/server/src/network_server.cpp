#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <random>
#include <steam/isteamnetworkingutils.h>
#include <tchips/server/network_server.hpp>

namespace {
struct MessageRelease {
    void operator()(SteamNetworkingMessage_t *message) const {
        if (message)
            message->Release();
    }
};
} // namespace
namespace tchips::server {
std::expected<void, std::string> NetworkServer::listen(std::uint16_t port) {
    if (listener_ != 0)
        return std::unexpected("server already listening");
    group_ = SteamNetworkingSockets()->CreatePollGroup();
    if (group_ == 0)
        return std::unexpected("cannot create receive poll group");
    std::array<SteamNetworkingConfigValue_t, 8> options;
    options[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                      reinterpret_cast<void *>(callback));
    options[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, reinterpret_cast<std::intptr_t>(this));
    options[2].SetInt32(k_ESteamNetworkingConfig_TimeoutConnected, 3000);
    options[3].SetInt32(k_ESteamNetworkingConfig_RecvMaxMessageSize,
                        static_cast<int>(wire::max_client_bytes));
    options[4].SetInt32(k_ESteamNetworkingConfig_RecvBufferSize, 128 * 1024);
    options[5].SetInt32(k_ESteamNetworkingConfig_RecvBufferMessages, 256);
    options[6].SetInt32(k_ESteamNetworkingConfig_SendBufferSize, 256 * 1024);
    options[7].SetInt32(k_ESteamNetworkingConfig_NagleTime, 0);
    const bool automatic = port == 0;
    const auto start = automatic ? static_cast<std::uint16_t>(35000 + std::random_device{}() % 20000) : port;
    for (unsigned attempt = 0; attempt < (automatic ? 16U : 1U); ++attempt) {
        SteamNetworkingIPAddr address;
        address.Clear();
        address.m_port = static_cast<std::uint16_t>(start + attempt);
        listener_ = SteamNetworkingSockets()->CreateListenSocketIP(address, static_cast<int>(options.size()),
                                                                   options.data());
        if (listener_ != 0) {
            port_ = address.m_port;
            break;
        }
    }
    if (listener_ == 0) {
        SteamNetworkingSockets()->DestroyPollGroup(group_);
        group_ = 0;
        return std::unexpected("cannot listen on UDP port");
    }
    return {};
}
NetworkServer::~NetworkServer() {
    shutdown();
}
void NetworkServer::callback(SteamNetConnectionStatusChangedCallback_t *info) {
    const auto data = SteamNetworkingSockets()->GetConnectionUserData(info->m_hConn);
    if (data == 0 || data == -1)
        return;
    reinterpret_cast<NetworkServer *>(static_cast<std::intptr_t>(data))->status(info);
}
void NetworkServer::status(SteamNetConnectionStatusChangedCallback_t *info) {
    auto *sockets = SteamNetworkingSockets();
    switch (info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_Connecting:
        if (peers_.size() >= world_.config().max_players + 16U) {
            sockets->SetConnectionUserData(info->m_hConn, 0);
            sockets->CloseConnection(info->m_hConn, 1005, "server full", false);
            return;
        }
        if (sockets->AcceptConnection(info->m_hConn) != k_EResultOK ||
            !sockets->SetConnectionPollGroup(info->m_hConn, group_)) {
            sockets->SetConnectionUserData(info->m_hConn, 0);
            sockets->CloseConnection(info->m_hConn, 1003, "accept failed", false);
            return;
        }
        peers_.try_emplace(info->m_hConn);
        break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        drop(info->m_hConn, wire::pb::DisconnectReason::NORMAL, false);
        break;
    default:
        break;
    }
}
bool NetworkServer::send(HSteamNetConnection connection, const wire::pb::Envelope &message, bool reliable) {
    const auto bytes = message.SerializeAsString();
    if (bytes.size() > wire::max_server_bytes)
        return false;
    const auto result = SteamNetworkingSockets()->SendMessageToConnection(
        connection, bytes.data(), static_cast<uint32>(bytes.size()),
        reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_UnreliableNoNagle, nullptr);
    return result == k_EResultOK;
}
void NetworkServer::drop(HSteamNetConnection connection, wire::pb::DisconnectReason::Code reason,
                         bool notify) {
    const auto it = peers_.find(connection);
    if (it == peers_.end())
        return;
    if (notify) {
        auto message = wire::envelope(it->second.session);
        message.mutable_disconnect()->set_code(reason);
        (void)send(connection, message, true);
    }
    if (it->second.session != 0) {
        world_.disconnect(it->second.session);
        std::cout << "[server] player disconnected, active=" << world_.player_count() << '\n';
    }
    SteamNetworkingSockets()->SetConnectionUserData(connection, 0);
    SteamNetworkingSockets()->CloseConnection(connection, 1000 + static_cast<int>(reason), "session closed",
                                              notify);
    peers_.erase(it);
}
void NetworkServer::receive(HSteamNetConnection connection, const wire::pb::Envelope &message,
                            bool reliable) {
    auto it = peers_.find(connection);
    if (it == peers_.end())
        return;
    auto &peer = it->second;
    if (peer.session == 0) {
        if (!reliable || !message.has_handshake() || message.session_id() != 0) {
            drop(connection, wire::pb::DisconnectReason::INVALID_MESSAGE, true);
            return;
        }
        if (message.handshake().map_fingerprint() != world_.map().fingerprint()) {
            drop(connection, wire::pb::DisconnectReason::MAP_MISMATCH, true);
            return;
        }
        const auto session = next_session_++;
        auto player = world_.connect(session);
        if (!player) {
            drop(connection, wire::pb::DisconnectReason::SERVER_FULL, true);
            return;
        }
        peer.session = session;
        auto accepted = wire::envelope(session);
        auto *body = accepted.mutable_accepted();
        body->set_session_id(session);
        wire::write(*body->mutable_player(), *player);
        body->set_tick_rate(world_.config().movement.tick_rate);
        body->set_speed(world_.config().movement.speed);
        body->set_half_extent(world_.config().movement.half_extent);
        body->set_map_fingerprint(world_.map().fingerprint());
        body->set_server_tick(world_.tick());
        if (!send(connection, accepted, true)) {
            drop(connection, wire::pb::DisconnectReason::TIMEOUT, false);
            return;
        }
        std::cout << "[server] player accepted, active=" << world_.player_count() << '\n';
        return;
    }
    if (message.session_id() != peer.session) {
        drop(connection, wire::pb::DisconnectReason::INVALID_MESSAGE, true);
        return;
    }
    if (message.has_disconnect()) {
        drop(connection, wire::pb::DisconnectReason::NORMAL, false);
        return;
    }
    if (!message.has_inputs()) {
        drop(connection, wire::pb::DisconnectReason::INVALID_MESSAGE, true);
        return;
    }
    for (const auto &input : message.inputs().commands()) {
        const auto result = world_.submit(peer.session, wire::read(input));
        if (result == InputResult::invalid || result == InputResult::out_of_window ||
            result == InputResult::unknown_session) {
            drop(connection, wire::pb::DisconnectReason::INVALID_MESSAGE, true);
            return;
        }
    }
}
void NetworkServer::pump() {
    SteamNetworkingSockets()->RunCallbacks();
    for (unsigned budget = 0; budget < 512 && group_ != 0; ++budget) {
        SteamNetworkingMessage_t *raw = nullptr;
        const auto count = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(group_, &raw, 1);
        if (count <= 0)
            break;
        const std::unique_ptr<SteamNetworkingMessage_t, MessageRelease> message{raw};
        auto it = peers_.find(raw->m_conn);
        if (it == peers_.end())
            continue;
        auto &peer = it->second;
        const auto now = Clock::now();
        peer.tokens =
            std::min(120.0, peer.tokens + std::chrono::duration<double>(now - peer.bucket_time).count() * 90);
        peer.bucket_time = now;
        if (peer.tokens < 1) {
            drop(raw->m_conn, wire::pb::DisconnectReason::RATE_LIMIT, true);
            continue;
        }
        peer.tokens -= 1;
        const auto parsed = wire::decode(
            {static_cast<const std::byte *>(raw->m_pData), static_cast<std::size_t>(raw->m_cbSize)}, true);
        if (!parsed) {
            std::cerr << "[server] rejected network message: " << parsed.error() << '\n';
            drop(raw->m_conn,
                 parsed.error() == "protocol version" ? wire::pb::DisconnectReason::VERSION_MISMATCH
                                                      : wire::pb::DisconnectReason::INVALID_MESSAGE,
                 true);
            continue;
        }
        peer.last_message = now;
        receive(raw->m_conn, *parsed, (raw->m_nFlags & k_nSteamNetworkingSend_Reliable) != 0);
    }
    const auto now = Clock::now();
    std::vector<HSteamNetConnection> stale;
    for (const auto &[connection, peer] : peers_)
        if (now - peer.last_message > std::chrono::seconds(5))
            stale.push_back(connection);
    for (auto connection : stale)
        drop(connection, wire::pb::DisconnectReason::TIMEOUT, true);
}
void NetworkServer::publish() {
    std::vector<HSteamNetConnection> failed;
    for (auto &[connection, peer] : peers_) {
        if (peer.session == 0)
            continue;
        const auto states = world_.visible(peer.session);
        std::set<contracts::EntityId> current;
        bool ok = true;
        for (const auto &state : states) {
            current.insert(state.id);
            if (!peer.visible.contains(state.id)) {
                auto message = wire::envelope(peer.session);
                auto *spawn = message.mutable_spawn();
                spawn->set_tick(world_.tick());
                wire::write(*spawn->mutable_entity(), state);
                ok = send(connection, message, true) && ok;
            }
        }
        for (const auto id : peer.visible)
            if (!current.contains(id)) {
                auto message = wire::envelope(peer.session);
                message.mutable_despawn()->set_tick(world_.tick());
                message.mutable_despawn()->set_entity_id(id);
                ok = send(connection, message, true) && ok;
            }
        peer.visible = std::move(current);
        auto message = wire::envelope(peer.session);
        auto *snapshot = message.mutable_snapshot();
        snapshot->set_tick(world_.tick());
        snapshot->set_acknowledged_input(world_.acknowledged(peer.session));
        for (const auto &state : states)
            wire::write(*snapshot->add_entities(), state);
        if (!send(connection, message, false))
            ok = false;
        if (!ok)
            failed.push_back(connection);
    }
    for (auto connection : failed)
        drop(connection, wire::pb::DisconnectReason::TIMEOUT, true);
}
void NetworkServer::shutdown() {
    if (listener_ == 0 && group_ == 0)
        return;
    while (!peers_.empty())
        drop(peers_.begin()->first, wire::pb::DisconnectReason::SERVER_STOPPING, true);
    if (listener_ != 0)
        SteamNetworkingSockets()->CloseListenSocket(listener_);
    if (group_ != 0)
        SteamNetworkingSockets()->DestroyPollGroup(group_);
    listener_ = 0;
    group_ = 0;
}
} // namespace tchips::server
