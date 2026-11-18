#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <tchips/server/world.hpp>

namespace tchips::server {
World::World(geometry::WorldMap map, WorldConfig config) : map_(std::move(map)), config_(config) {
    const auto &m = config.movement;
    if (m.tick_rate != 30 || !std::isfinite(m.speed) || m.speed <= 0 || m.speed > 1000 ||
        !std::isfinite(m.half_extent) || m.half_extent <= 0 || m.half_extent > 64 ||
        config.max_players == 0 || config.max_players > contracts::max_visible_entities ||
        !std::isfinite(config.visibility_radius) || config.visibility_radius <= 0 ||
        config.gap_grace_ticks > 30 || !geometry::CollisionSystem::fits(map_, map_.spawn(), m.half_extent))
        throw std::invalid_argument("invalid authoritative world configuration/spawn");
}
std::optional<contracts::EntityState> World::connect(contracts::SessionId session) {
    if (session == 0 || sessions_.contains(session) || sessions_.size() >= config_.max_players ||
        next_entity_ == std::numeric_limits<std::uint64_t>::max())
        return std::nullopt;
    auto position = map_.spawn();
    for (unsigned i = 0; i < 64; ++i) {
        const auto slot = (static_cast<unsigned>(next_entity_ % 64) + 63 + i) % 64;
        math::WorldPosition candidate{position.x + (slot / 8) * 32.0, position.y + (slot % 8) * 32.0};
        if (geometry::CollisionSystem::fits(map_, candidate, config_.movement.half_extent)) {
            position = candidate;
            break;
        }
    }
    const auto entity = entities_.create();
    Player player;
    player.state = {next_entity_++, position, 0};
    entities_.emplace<Player>(entity, std::move(player));
    sessions_.emplace(session, entity);
    return entities_.get<Player>(entity).state;
}
void World::disconnect(contracts::SessionId session) {
    if (const auto it = sessions_.find(session); it != sessions_.end()) {
        entities_.destroy(it->second);
        sessions_.erase(it);
    }
}
InputResult World::submit(contracts::SessionId session, const contracts::InputCommand &input) {
    const auto it = sessions_.find(session);
    if (it == sessions_.end())
        return InputResult::unknown_session;
    if (input.sequence == 0 || !std::isfinite(input.direction.x) || !std::isfinite(input.direction.y) ||
        std::abs(input.direction.x) > 1 || std::abs(input.direction.y) > 1 || !std::isfinite(input.angle) ||
        std::abs(input.angle) > std::numbers::pi)
        return InputResult::invalid;
    auto &player = entities_.get<Player>(it->second);
    if (input.sequence <= player.ack)
        return InputResult::duplicate;
    if (input.sequence - player.ack > contracts::input_window)
        return InputResult::out_of_window;
    auto &slot = player.pending[input.sequence % player.pending.size()];
    if (slot)
        return InputResult::duplicate;
    slot = input;
    return InputResult::accepted;
}
void World::step() {
    ++tick_;
    for (auto [session, entity] : sessions_) {
        (void)session;
        auto &p = entities_.get<Player>(entity);
        auto &expected = p.pending[(p.ack + 1) % p.pending.size()];
        std::optional<contracts::InputCommand> command;
        if (expected && expected->sequence == p.ack + 1) {
            command = expected;
            expected.reset();
            p.gap_wait = 0;
        } else {
            std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
            for (const auto &entry : p.pending)
                if (entry && entry->sequence > p.ack)
                    next = std::min(next, entry->sequence);
            if (next != std::numeric_limits<std::uint64_t>::max() && ++p.gap_wait > config_.gap_grace_ticks) {
                auto &available = p.pending[next % p.pending.size()];
                command = available;
                available.reset();
                p.gap_wait = 0;
            }
        }
        if (!command)
            continue;
        const auto direction = math::normalized_input(command->direction);
        const auto distance = config_.movement.speed / config_.movement.tick_rate;
        p.state.position = geometry::CollisionSystem::move(map_, p.state.position,
                                                           {direction.x * distance, direction.y * distance},
                                                           config_.movement.half_extent);
        p.state.angle = command->angle;
        p.ack = command->sequence;
    }
}
std::optional<contracts::EntityState> World::player(contracts::SessionId session) const {
    const auto it = sessions_.find(session);
    if (it == sessions_.end())
        return std::nullopt;
    return entities_.get<Player>(it->second).state;
}
std::uint64_t World::acknowledged(contracts::SessionId session) const {
    const auto it = sessions_.find(session);
    return it == sessions_.end() ? 0 : entities_.get<Player>(it->second).ack;
}
std::vector<contracts::EntityState> World::visible(contracts::SessionId observer) const {
    std::vector<contracts::EntityState> result;
    const auto self = player(observer);
    if (!self)
        return result;
    result.reserve(sessions_.size());
    for (const auto &[session, entity] : sessions_) {
        (void)session;
        const auto &state = entities_.get<Player>(entity).state;
        if (state.id == self->id ||
            std::hypot(state.position.x - self->position.x, state.position.y - self->position.y) <=
                config_.visibility_radius)
            result.push_back(state);
    }
    return result;
}
} // namespace tchips::server
