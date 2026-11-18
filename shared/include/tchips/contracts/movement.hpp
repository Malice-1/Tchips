#pragma once
#include <cstdint>
#include <tchips/math/geometry.hpp>

namespace tchips::contracts {
using SessionId = std::uint64_t;
using EntityId = std::uint64_t;
struct MovementRules {
    std::uint32_t tick_rate{30};
    double speed{240};
    double half_extent{12};
};
struct InputCommand {
    std::uint64_t sequence{};
    math::WorldVector direction;
    double angle{};
};
struct EntityState {
    EntityId id{};
    math::WorldPosition position;
    double angle{};
};
inline constexpr unsigned input_window = 120;
inline constexpr unsigned input_redundancy = 16;
inline constexpr unsigned max_visible_entities = 128;
} // namespace tchips::contracts
