#pragma once
#include <tchips/client/collision_system.hpp>
#include <entt/entity/registry.hpp>

namespace tchips::client {
struct CharacterTransform {
    math::WorldPosition position, previous;
    double angle{}; // radians, +X is zero, +Y points down
};
struct CharacterBody { double half_extent{12}; double speed{240}; }; // world units and units/s
struct MovementConfig { double fixed_step{1.0 / 60}; double max_frame_delta{0.25}; unsigned max_steps{8}; };
class MovementSystem {
public:
    explicit MovementSystem(MovementConfig config = {});
    unsigned advance(entt::registry& registry, const WorldMap& map, math::WorldVector input, double elapsed_seconds);
    static void aim(CharacterTransform& transform, math::WorldPosition cursor) noexcept;
    [[nodiscard]] double alpha() const noexcept;
private:
    MovementConfig config_;
    double accumulator_{};
};
} // namespace tchips::client
