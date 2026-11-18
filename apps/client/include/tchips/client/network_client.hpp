#pragma once
#include <algorithm>
#include <chrono>
#include <expected>
#include <map>
#include <steam/steamnetworkingsockets.h>
#include <string>
#include <tchips/client/prediction.hpp>
#include <tchips/protocol/codec.hpp>

namespace tchips::client {
enum class ConnectionState { disconnected, connecting, handshake, active, rejected };
class NetworkClient {
  public:
    explicit NetworkClient(const geometry::WorldMap &map) : map_(map), prediction_(map) {}
    ~NetworkClient();
    NetworkClient(const NetworkClient &) = delete;
    NetworkClient &operator=(const NetworkClient &) = delete;
    [[nodiscard]] std::expected<void, std::string> connect(std::string address);
    void update(double elapsed, math::WorldVector direction, double angle);
    void pump();
    void close();
    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] bool active() const noexcept { return state_ == ConnectionState::active; }
    [[nodiscard]] const Prediction &prediction() const noexcept { return prediction_; }
    [[nodiscard]] contracts::SessionId session_id() const noexcept { return session_; }
    [[nodiscard]] std::vector<contracts::EntityState> remote_entities() const;
    [[nodiscard]] std::size_t visible_count() const noexcept { return active() ? remotes_.size() + 1 : 0; }
    [[nodiscard]] int ping_ms() const;
    [[nodiscard]] double alpha() const noexcept {
        return std::clamp(accumulator_ * rules_.tick_rate, 0.0, 1.0);
    }
    [[nodiscard]] std::string status_text() const;

  private:
    using Clock = std::chrono::steady_clock;
    const geometry::WorldMap &map_;
    Prediction prediction_;
    contracts::MovementRules rules_;
    HSteamNetConnection connection_{};
    ConnectionState state_{ConnectionState::disconnected};
    contracts::SessionId session_{};
    std::uint64_t last_tick_{};
    std::map<contracts::EntityId, RemoteInterpolator> remotes_;
    std::map<contracts::EntityId, std::uint64_t> despawn_ticks_;
    std::string address_;
    Clock::time_point attempt_time_{}, snapshot_time_{}, retry_time_{};
    bool stopped_{true}, retry_allowed_{true};
    double accumulator_{};
    static void callback(SteamNetConnectionStatusChangedCallback_t *info);
    void status(SteamNetConnectionStatusChangedCallback_t *info);
    void receive(const wire::pb::Envelope &message);
    [[nodiscard]] bool send(const wire::pb::Envelope &message, bool reliable);
    void lost(bool permanent = false);
    [[nodiscard]] bool valid_entity(const contracts::EntityState &state) const;
};
} // namespace tchips::client
