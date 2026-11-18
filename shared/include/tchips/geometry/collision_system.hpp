#pragma once
#include <tchips/geometry/world_map.hpp>

namespace tchips::geometry {
class CollisionSystem {
  public:
    [[nodiscard]] static bool fits(const WorldMap &map, math::WorldPosition position,
                                   double half_extent) noexcept;
    // Swept axis resolution: no tunnelling even when the desired delta is large.
    [[nodiscard]] static math::WorldPosition move(const WorldMap &map, math::WorldPosition start,
                                                  math::WorldVector delta, double half_extent) noexcept;
};
} // namespace tchips::geometry
