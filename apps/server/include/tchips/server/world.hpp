#pragma once
#include <array>
#include <entt/entity/registry.hpp>
#include <map>
#include <optional>
#include <tchips/contracts/movement.hpp>
#include <tchips/geometry/collision_system.hpp>
#include <vector>

namespace tchips::server {
struct WorldConfig {
    contracts::MovementRules movement;
    unsigned max_players{128};
    double visibility_radius{1024};
    unsigned gap_grace_ticks{3};
};
enum class InputResult { accepted, duplicate, invalid, out_of_window, unknown_session };
class World {
  public:
    explicit World(geometry::WorldMap map, WorldConfig config = {});
    [[nodiscard]] std::optional<contracts::EntityState> connect(contracts::SessionId session);
    void disconnect(contracts::SessionId session);
    [[nodiscard]] InputResult submit(contracts::SessionId session, const contracts::InputCommand &input);
    void step();
    [[nodiscard]] std::optional<contracts::EntityState> player(contracts::SessionId session) const;
    [[nodiscard]] std::vector<contracts::EntityState> visible(contracts::SessionId observer) const;
    [[nodiscard]] std::uint64_t acknowledged(contracts::SessionId session) const;
    [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
    [[nodiscard]] std::size_t player_count() const noexcept { return sessions_.size(); }
    [[nodiscard]] const geometry::WorldMap &map() const noexcept { return map_; }
    [[nodiscard]] const WorldConfig &config() const noexcept { return config_; }

  private:
    struct Player {
        contracts::EntityState state;
        std::uint64_t ack{};
        std::array<std::optional<contracts::InputCommand>, 128> pending;
        unsigned gap_wait{};
    };
    geometry::WorldMap map_;
    WorldConfig config_;
    entt::registry entities_;
    std::map<contracts::SessionId, entt::entity> sessions_;
    contracts::EntityId next_entity_{1};
    std::uint64_t tick_{};
};
} // namespace tchips::server
