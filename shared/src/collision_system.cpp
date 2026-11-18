#include <algorithm>
#include <cmath>
#include <tchips/geometry/collision_system.hpp>

namespace tchips::geometry {
bool CollisionSystem::fits(const WorldMap &map, math::WorldPosition p, double half) noexcept {
    if (!math::finite(p) || !std::isfinite(half) || half <= 0 || p.x < half || p.y < half ||
        p.x > map.width() - half || p.y > map.height() - half)
        return false;
    for (const auto &o : map.obstacles())
        if (p.x + half > o.x && p.x - half < o.x + o.width && p.y + half > o.y && p.y - half < o.y + o.height)
            return false;
    return true;
}
math::WorldPosition CollisionSystem::move(const WorldMap &map, math::WorldPosition start,
                                          math::WorldVector delta, double half) noexcept {
    if (!fits(map, start, half) || !std::isfinite(delta.x) || !std::isfinite(delta.y))
        return start;
    auto next = start;
    next.x = std::clamp(start.x + delta.x, half, map.width() - half);
    for (const auto &o : map.obstacles()) {
        if (start.y + half <= o.y || start.y - half >= o.y + o.height)
            continue;
        if (delta.x > 0 && start.x + half <= o.x && next.x + half > o.x)
            next.x = std::min(next.x, o.x - half);
        if (delta.x < 0 && start.x - half >= o.x + o.width && next.x - half < o.x + o.width)
            next.x = std::max(next.x, o.x + o.width + half);
    }
    next.y = std::clamp(start.y + delta.y, half, map.height() - half);
    for (const auto &o : map.obstacles()) {
        if (next.x + half <= o.x || next.x - half >= o.x + o.width)
            continue;
        if (delta.y > 0 && start.y + half <= o.y && next.y + half > o.y)
            next.y = std::min(next.y, o.y - half);
        if (delta.y < 0 && start.y - half >= o.y + o.height && next.y - half < o.y + o.height)
            next.y = std::max(next.y, o.y + o.height + half);
    }
    return next;
}
} // namespace tchips::geometry
