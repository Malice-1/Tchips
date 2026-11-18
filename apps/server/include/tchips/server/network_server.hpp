#pragma once
#include <chrono>
#include <expected>
#include <map>
#include <set>
#include <steam/steamnetworkingsockets.h>
#include <string>
#include <tchips/protocol/codec.hpp>
#include <tchips/server/world.hpp>

namespace tchips::server {
// Single application thread; SDK initialization outlives this adapter.
class NetworkServer {
  public:
    explicit NetworkServer(World &world) : world_(world) {}
    ~NetworkServer();
    NetworkServer(const NetworkServer &) = delete;
    NetworkServer &operator=(const NetworkServer &) = delete;
    [[nodiscard]] std::expected<void, std::string> listen(std::uint16_t port);
    void pump();
    void publish();
    void shutdown();
    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
    [[nodiscard]] std::size_t peer_count() const noexcept { return peers_.size(); }

  private:
    using Clock = std::chrono::steady_clock;
    struct Peer {
        contracts::SessionId session{};
        Clock::time_point opened{Clock::now()}, last_message{Clock::now()}, bucket_time{Clock::now()};
        double tokens{120};
        std::set<contracts::EntityId> visible;
    };
    World &world_;
    HSteamListenSocket listener_{};
    HSteamNetPollGroup group_{};
    std::uint16_t port_{};
    std::map<HSteamNetConnection, Peer> peers_;
    contracts::SessionId next_session_{1};
    static void callback(SteamNetConnectionStatusChangedCallback_t *info);
    void status(SteamNetConnectionStatusChangedCallback_t *info);
    void receive(HSteamNetConnection connection, const wire::pb::Envelope &message, bool reliable);
    void drop(HSteamNetConnection connection, wire::pb::DisconnectReason::Code reason, bool notify);
    [[nodiscard]] bool send(HSteamNetConnection connection, const wire::pb::Envelope &message, bool reliable);
};
} // namespace tchips::server
