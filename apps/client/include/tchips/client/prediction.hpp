#pragma once
#include <array>
#include <deque>
#include <optional>
#include <tchips/contracts/movement.hpp>
#include <tchips/geometry/collision_system.hpp>
#include <vector>

namespace tchips::client {
class Prediction {
  public:
    explicit Prediction(const geometry::WorldMap &map) : map_(map) {}
    void reset();
    void begin(contracts::EntityState self, contracts::MovementRules rules);
    [[nodiscard]] std::optional<contracts::InputCommand> predict(math::WorldVector direction, double angle);
    [[nodiscard]] bool reconcile(const contracts::EntityState &self, std::uint64_t acknowledged);
    [[nodiscard]] std::vector<contracts::InputCommand> redundant_batch() const;
    [[nodiscard]] const contracts::EntityState &state() const noexcept { return state_; }
    [[nodiscard]] math::WorldPosition previous() const noexcept { return previous_; }
    [[nodiscard]] std::size_t pending_count() const noexcept {
        return static_cast<std::size_t>(sequence_ - ack_);
    }
    [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }
    [[nodiscard]] double correction() const noexcept { return correction_; }
    [[nodiscard]] double authoritative_distance() const noexcept;

  private:
    const geometry::WorldMap &map_;
    contracts::MovementRules rules_;
    contracts::EntityState state_;
    math::WorldPosition previous_, authoritative_;
    std::uint64_t sequence_{}, ack_{};
    double correction_{};
    bool active_{};
    std::array<std::optional<contracts::InputCommand>, 128> pending_;
    void apply(const contracts::InputCommand &command);
};
class RemoteInterpolator {
  public:
    void push(std::uint64_t tick, contracts::EntityState state);
    [[nodiscard]] contracts::EntityState sample(double tick) const;
    [[nodiscard]] std::uint64_t latest_tick() const noexcept {
        return samples_.empty() ? 0 : samples_.back().first;
    }

  private:
    std::deque<std::pair<std::uint64_t, contracts::EntityState>> samples_;
};
} // namespace tchips::client
