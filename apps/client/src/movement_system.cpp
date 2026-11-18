#include <tchips/client/movement_system.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tchips::client {
MovementSystem::MovementSystem(MovementConfig config) : config_(config) {
    if (!std::isfinite(config.fixed_step) || config.fixed_step <= 0 || !std::isfinite(config.max_frame_delta) || config.max_frame_delta < config.fixed_step || config.max_steps == 0 || config.max_steps > 1024) throw std::invalid_argument("invalid fixed-step configuration");
}
unsigned MovementSystem::advance(entt::registry& registry, const WorldMap& map, math::WorldVector input, double elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0) return 0;
    accumulator_ += std::min(elapsed, config_.max_frame_delta);
    const auto direction = math::normalized_input(input);
    unsigned steps = 0;
    while (accumulator_ + 1e-12 >= config_.fixed_step && steps < config_.max_steps) {
        auto view = registry.view<CharacterTransform, CharacterBody>();
        for (auto [entity, transform, body] : view.each()) {
            (void)entity;
            transform.previous = transform.position;
            if (!std::isfinite(body.speed) || body.speed < 0) continue;
            const auto distance = body.speed * config_.fixed_step;
            transform.position = CollisionSystem::move(map, transform.position, {direction.x * distance, direction.y * distance}, body.half_extent);
        }
        accumulator_ = std::max(0.0, accumulator_ - config_.fixed_step);
        ++steps;
    }
    // Drop overload backlog, never process an unbounded catch-up loop.
    if (steps == config_.max_steps && accumulator_ >= config_.fixed_step) accumulator_ = std::fmod(accumulator_, config_.fixed_step);
    return steps;
}
double MovementSystem::alpha() const noexcept { return std::clamp(accumulator_ / config_.fixed_step, 0.0, 1.0); }
void MovementSystem::aim(CharacterTransform& transform, math::WorldPosition cursor) noexcept {
    if (!math::finite(cursor)) return;
    const auto dx = cursor.x - transform.position.x, dy = cursor.y - transform.position.y;
    if (std::hypot(dx, dy) > 1e-9) transform.angle = std::atan2(dy, dx);
}
} // namespace tchips::client
