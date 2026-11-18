#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <set>
#include <steam/isteamnetworkingutils.h>
#include <tchips/client/network_client.hpp>

namespace {
struct MessageRelease {
    void operator()(SteamNetworkingMessage_t *message) const {
        if (message)
            message->Release();
    }
};
} // namespace
namespace tchips::client {
NetworkClient::~NetworkClient() {
    close();
}
std::expected<void, std::string> NetworkClient::connect(std::string address) {
    SteamNetworkingIPAddr target;
    target.Clear();
    if (!target.ParseString(address.c_str()) || target.m_port == 0)
        return std::unexpected("expected numeric IP:port");
    if (connection_ != 0)
        close();
    address_ = std::move(address);
    stopped_ = false;
    retry_allowed_ = true;
    session_ = 0;
    last_tick_ = 0;
    accumulator_ = 0;
    prediction_.reset();
    remotes_.clear();
    despawn_ticks_.clear();
    std::array<SteamNetworkingConfigValue_t, 8> options;
    options[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                      reinterpret_cast<void *>(callback));
    options[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, reinterpret_cast<std::intptr_t>(this));
    options[2].SetInt32(k_ESteamNetworkingConfig_TimeoutInitial, 3000);
    options[3].SetInt32(k_ESteamNetworkingConfig_TimeoutConnected, 3000);
    options[4].SetInt32(k_ESteamNetworkingConfig_RecvMaxMessageSize,
                        static_cast<int>(wire::max_server_bytes));
    options[5].SetInt32(k_ESteamNetworkingConfig_RecvBufferSize, 256 * 1024);
    options[6].SetInt32(k_ESteamNetworkingConfig_RecvBufferMessages, 256);
    options[7].SetInt32(k_ESteamNetworkingConfig_NagleTime, 0);
    connection_ = SteamNetworkingSockets()->ConnectByIPAddress(target, static_cast<int>(options.size()),
                                                               options.data());
    attempt_time_ = Clock::now();
    state_ = ConnectionState::connecting;
    if (connection_ == 0) {
        lost();
        return std::unexpected("cannot create connection");
    }
    return {};
}
void NetworkClient::callback(SteamNetConnectionStatusChangedCallback_t *info) {
    const auto data = SteamNetworkingSockets()->GetConnectionUserData(info->m_hConn);
    if (data == 0 || data == -1)
        return;
    reinterpret_cast<NetworkClient *>(static_cast<std::intptr_t>(data))->status(info);
}
void NetworkClient::status(SteamNetConnectionStatusChangedCallback_t *info) {
    if (info->m_hConn != connection_)
        return;
    if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
        state_ = ConnectionState::handshake;
        auto hello = wire::envelope();
        hello.mutable_handshake()->set_map_fingerprint(map_.fingerprint());
        if (!send(hello, true))
            lost();
    } else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer ||
               info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
        const auto code = info->m_info.m_eEndReason;
        lost(code == 1001 || code == 1002 || code == 1003);
    }
}
bool NetworkClient::send(const wire::pb::Envelope &message, bool reliable) {
    const auto bytes = message.SerializeAsString();
    if (connection_ == 0 || bytes.size() > wire::max_client_bytes)
        return false;
    return SteamNetworkingSockets()->SendMessageToConnection(
               connection_, bytes.data(), static_cast<uint32>(bytes.size()),
               reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_UnreliableNoNagle,
               nullptr) == k_EResultOK;
}
bool NetworkClient::valid_entity(const contracts::EntityState &entity) const {
    return entity.id != 0 && std::isfinite(entity.angle) && std::abs(entity.angle) <= std::numbers::pi &&
           geometry::CollisionSystem::fits(map_, entity.position, rules_.half_extent);
}
void NetworkClient::receive(const wire::pb::Envelope &message) {
    if (message.has_disconnect()) {
        const auto code = message.disconnect().code();
        lost(code == wire::pb::DisconnectReason::MAP_MISMATCH ||
             code == wire::pb::DisconnectReason::VERSION_MISMATCH ||
             code == wire::pb::DisconnectReason::INVALID_MESSAGE);
        return;
    }
    if (state_ == ConnectionState::handshake && message.has_snapshot())
        return; // Unreliable traffic can overtake reliable acceptance.
    if (state_ == ConnectionState::handshake && message.has_accepted()) {
        const auto &accepted = message.accepted();
        rules_ = {accepted.tick_rate(), accepted.speed(), accepted.half_extent()};
        if (accepted.session_id() == 0 || message.session_id() != accepted.session_id() ||
            rules_.tick_rate != 30 || !std::isfinite(rules_.speed) || rules_.speed <= 0 ||
            rules_.speed > 1000 || !std::isfinite(rules_.half_extent) || rules_.half_extent <= 0 ||
            rules_.half_extent > 64 || accepted.map_fingerprint() != map_.fingerprint() ||
            !accepted.has_player() || !valid_entity(wire::read(accepted.player()))) {
            lost(true);
            return;
        }
        session_ = accepted.session_id();
        last_tick_ = accepted.server_tick();
        prediction_.begin(wire::read(accepted.player()), rules_);
        state_ = ConnectionState::active;
        snapshot_time_ = Clock::now();
        return;
    }
    if (!active() || message.session_id() != session_) {
        lost(true);
        return;
    }
    if (message.has_snapshot()) {
        const auto &snapshot = message.snapshot();
        if (snapshot.tick() <= last_tick_)
            return;
        std::set<contracts::EntityId> present;
        std::set<contracts::EntityId> seen;
        std::optional<contracts::EntityState> self;
        for (const auto &raw : snapshot.entities()) {
            auto entity = wire::read(raw);
            if (!valid_entity(entity) || !seen.insert(entity.id).second) {
                lost(true);
                return;
            }
            present.insert(entity.id);
            if (entity.id == prediction_.state().id)
                self = entity;
            const auto removed = despawn_ticks_.find(entity.id);
            if (removed != despawn_ticks_.end() && snapshot.tick() <= removed->second)
                present.erase(entity.id);
        }
        if (!self || !prediction_.reconcile(*self, snapshot.acknowledged_input())) {
            lost(true);
            return;
        }
        for (const auto &raw : snapshot.entities()) {
            auto entity = wire::read(raw);
            if (entity.id != self->id && present.contains(entity.id))
                remotes_[entity.id].push(snapshot.tick(), entity);
        }
        for (auto it = remotes_.begin(); it != remotes_.end();)
            if (!present.contains(it->first) && it->second.latest_tick() <= snapshot.tick())
                it = remotes_.erase(it);
            else
                ++it;
        last_tick_ = snapshot.tick();
        for (auto it = despawn_ticks_.begin(); it != despawn_ticks_.end();)
            if (it->second < last_tick_)
                it = despawn_ticks_.erase(it);
            else
                ++it;
        snapshot_time_ = Clock::now();
    } else if (message.has_spawn()) {
        const auto &spawn = message.spawn();
        if (spawn.tick() <= last_tick_)
            return;
        const auto entity = wire::read(spawn.entity());
        if (!valid_entity(entity)) {
            lost(true);
            return;
        }
        const auto removed = despawn_ticks_.find(entity.id);
        if (removed != despawn_ticks_.end() && removed->second >= spawn.tick())
            return;
        despawn_ticks_.erase(entity.id);
        if (entity.id != prediction_.state().id && !remotes_.contains(entity.id) &&
            remotes_.size() >= contracts::max_visible_entities - 1) {
            lost(true);
            return;
        }
        if (entity.id != prediction_.state().id)
            remotes_[entity.id].push(spawn.tick(), entity);
    } else if (message.has_despawn()) {
        if (message.despawn().tick() > last_tick_) {
            const auto current = remotes_.find(message.despawn().entity_id());
            if (current != remotes_.end() && current->second.latest_tick() > message.despawn().tick())
                return;
            remotes_.erase(message.despawn().entity_id());
            despawn_ticks_[message.despawn().entity_id()] = message.despawn().tick();
            if (despawn_ticks_.size() > 256) {
                lost();
                return;
            }
        }
    } else
        lost(true);
}
void NetworkClient::pump() {
    SteamNetworkingSockets()->RunCallbacks();
    for (unsigned budget = 0; budget < 256 && connection_ != 0; ++budget) {
        SteamNetworkingMessage_t *raw = nullptr;
        const auto count = SteamNetworkingSockets()->ReceiveMessagesOnConnection(connection_, &raw, 1);
        if (count <= 0)
            break;
        const std::unique_ptr<SteamNetworkingMessage_t, MessageRelease> message{raw};
        auto parsed = wire::decode(
            {static_cast<const std::byte *>(raw->m_pData), static_cast<std::size_t>(raw->m_cbSize)}, false);
        if (!parsed) {
            lost(true);
            break;
        }
        const bool reliable = (raw->m_nFlags & k_nSteamNetworkingSend_Reliable) != 0;
        if (!parsed->has_snapshot() && !reliable) {
            lost(true);
            break;
        }
        receive(*parsed);
    }
    const auto now = Clock::now();
    if ((state_ == ConnectionState::connecting || state_ == ConnectionState::handshake) &&
        now - attempt_time_ > std::chrono::seconds(5))
        lost();
    if (active() && now - snapshot_time_ > std::chrono::seconds(3))
        lost();
    if (!stopped_ && retry_allowed_ && state_ == ConnectionState::disconnected && now >= retry_time_) {
        const auto target = address_;
        (void)connect(target);
    }
}
void NetworkClient::update(double elapsed, math::WorldVector direction, double angle) {
    pump();
    if (!active())
        return;
    if (!std::isfinite(elapsed) || elapsed < 0)
        return;
    accumulator_ += std::min(elapsed, 0.25);
    unsigned steps = 0;
    const auto step = 1.0 / rules_.tick_rate;
    while (accumulator_ + 1e-12 >= step && steps < 4) {
        accumulator_ = std::max(0.0, accumulator_ - step);
        ++steps;
        auto input = prediction_.predict(direction, angle);
        if (!input) {
            lost();
            return;
        }
        auto message = wire::envelope(session_);
        for (const auto &command : prediction_.redundant_batch())
            wire::write(*message.mutable_inputs()->add_commands(), command);
        if (!send(message, false)) {
            lost();
            return;
        }
    }
    if (steps == 4 && accumulator_ >= step)
        accumulator_ = std::fmod(accumulator_, step);
}
void NetworkClient::lost(bool permanent) {
    if (connection_ != 0) {
        SteamNetworkingSockets()->SetConnectionUserData(connection_, 0);
        SteamNetworkingSockets()->CloseConnection(connection_, 1000, "connection reset", false);
        connection_ = 0;
    }
    prediction_.reset();
    remotes_.clear();
    despawn_ticks_.clear();
    session_ = 0;
    accumulator_ = 0;
    retry_allowed_ = !permanent;
    state_ = permanent ? ConnectionState::rejected : ConnectionState::disconnected;
    retry_time_ = Clock::now() + std::chrono::seconds(1);
}
void NetworkClient::close() {
    stopped_ = true;
    if (connection_ != 0 && active()) {
        auto message = wire::envelope(session_);
        message.mutable_disconnect()->set_code(wire::pb::DisconnectReason::NORMAL);
        (void)send(message, true);
    }
    if (connection_ != 0) {
        SteamNetworkingSockets()->SetConnectionUserData(connection_, 0);
        SteamNetworkingSockets()->CloseConnection(connection_, 1000, "client leaving", true);
        connection_ = 0;
    }
    lost();
    stopped_ = true;
}
std::vector<contracts::EntityState> NetworkClient::remote_entities() const {
    std::vector<contracts::EntityState> result;
    result.reserve(remotes_.size());
    const auto tick =
        static_cast<double>(last_tick_) +
        std::chrono::duration<double>(Clock::now() - snapshot_time_).count() * rules_.tick_rate - 3.0;
    for (const auto &[id, interpolator] : remotes_) {
        (void)id;
        result.push_back(interpolator.sample(tick));
    }
    return result;
}
int NetworkClient::ping_ms() const {
    SteamNetConnectionRealTimeStatus_t status{};
    if (connection_ == 0 || SteamNetworkingSockets()->GetConnectionRealTimeStatus(connection_, &status, 0,
                                                                                  nullptr) != k_EResultOK)
        return -1;
    return status.m_nPing;
}
std::string NetworkClient::status_text() const {
    switch (state_) {
    case ConnectionState::active:
        return "EN LIGNE";
    case ConnectionState::connecting:
        return "CONNEXION";
    case ConnectionState::handshake:
        return "NEGOCIATION";
    case ConnectionState::rejected:
        return "SERVEUR INCOMPATIBLE";
    default:
        return "RECONNEXION";
    }
}
} // namespace tchips::client
