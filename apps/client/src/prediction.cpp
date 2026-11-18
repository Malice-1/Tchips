#include <algorithm>
#include <cmath>
#include <numbers>
#include <tchips/client/prediction.hpp>

namespace tchips::client {
void Prediction::reset() {
    active_ = false;
    state_ = {};
    previous_ = {};
    authoritative_ = {};
    sequence_ = ack_ = 0;
    correction_ = 0;
    for (auto &slot : pending_)
        slot.reset();
}
void Prediction::begin(contracts::EntityState self, contracts::MovementRules rules) {
    reset();
    state_ = self;
    previous_ = authoritative_ = self.position;
    rules_ = rules;
    active_ = true;
}
void Prediction::apply(const contracts::InputCommand &input) {
    const auto direction = math::normalized_input(input.direction);
    const auto distance = rules_.speed / rules_.tick_rate;
    state_.position = geometry::CollisionSystem::move(
        map_, state_.position, {direction.x * distance, direction.y * distance}, rules_.half_extent);
    state_.angle = input.angle;
}
std::optional<contracts::InputCommand> Prediction::predict(math::WorldVector direction, double angle) {
    if (!active_ || pending_count() >= contracts::input_window || !std::isfinite(direction.x) ||
        !std::isfinite(direction.y) || std::abs(direction.x) > 1 || std::abs(direction.y) > 1 ||
        !std::isfinite(angle) || std::abs(angle) > std::numbers::pi)
        return std::nullopt;
    contracts::InputCommand input{++sequence_, direction, angle};
    pending_[sequence_ % pending_.size()] = input;
    previous_ = state_.position;
    apply(input);
    return input;
}
bool Prediction::reconcile(const contracts::EntityState &self, std::uint64_t ack) {
    if (!active_ || self.id != state_.id || ack < ack_ || ack > sequence_ ||
        !geometry::CollisionSystem::fits(map_, self.position, rules_.half_extent) ||
        !std::isfinite(self.angle))
        return false;
    const auto before = state_.position;
    state_ = self;
    authoritative_ = self.position;
    ack_ = ack;
    for (auto &slot : pending_)
        if (slot && slot->sequence <= ack_)
            slot.reset();
    for (auto seq = ack_ + 1; seq <= sequence_; ++seq) {
        const auto &slot = pending_[seq % pending_.size()];
        if (!slot || slot->sequence != seq)
            return false;
        apply(*slot);
    }
    const auto dx = state_.position.x - before.x, dy = state_.position.y - before.y;
    correction_ = std::hypot(dx, dy);
    previous_.x += dx;
    previous_.y += dy;
    return true;
}
std::vector<contracts::InputCommand> Prediction::redundant_batch() const {
    std::vector<contracts::InputCommand> result;
    result.reserve(contracts::input_redundancy);
    const auto start = std::max(
        ack_ + 1, sequence_ > contracts::input_redundancy ? sequence_ - contracts::input_redundancy + 1 : 1);
    for (auto seq = start; seq <= sequence_; ++seq) {
        const auto &slot = pending_[seq % pending_.size()];
        if (slot && slot->sequence == seq)
            result.push_back(*slot);
    }
    return result;
}
double Prediction::authoritative_distance() const noexcept {
    return std::hypot(state_.position.x - authoritative_.x, state_.position.y - authoritative_.y);
}
void RemoteInterpolator::push(std::uint64_t tick, contracts::EntityState state) {
    if (!samples_.empty() && tick <= samples_.back().first)
        return;
    samples_.emplace_back(tick, state);
    if (samples_.size() > 32)
        samples_.pop_front();
}
contracts::EntityState RemoteInterpolator::sample(double tick) const {
    if (samples_.empty())
        return {};
    if (tick <= static_cast<double>(samples_.front().first))
        return samples_.front().second;
    for (std::size_t i = 1; i < samples_.size(); ++i) {
        const auto &a = samples_[i - 1];
        const auto &b = samples_[i];
        if (tick <= static_cast<double>(b.first)) {
            const auto alpha = std::clamp(
                (tick - static_cast<double>(a.first)) / static_cast<double>(b.first - a.first), 0.0, 1.0);
            auto result = b.second;
            result.position = math::interpolate(a.second.position, b.second.position, alpha);
            const auto delta = std::atan2(std::sin(b.second.angle - a.second.angle),
                                          std::cos(b.second.angle - a.second.angle));
            result.angle = a.second.angle + delta * alpha;
            return result;
        }
    }
    return samples_.back().second;
}
} // namespace tchips::client
