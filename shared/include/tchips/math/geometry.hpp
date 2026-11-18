#pragma once
#include <cmath>

namespace tchips::math {
// World units, logical window pixels, and world units/second are distinct.
struct WorldPosition { double x{}, y{}; };
struct ScreenPosition { double x{}, y{}; };
struct WorldVector { double x{}, y{}; };
struct Viewport { double width{1}, height{1}; };
struct WorldRect { double x{}, y{}, width{}, height{}; };
[[nodiscard]] inline bool finite(WorldPosition p) noexcept { return std::isfinite(p.x) && std::isfinite(p.y); }
[[nodiscard]] inline WorldVector normalized_input(WorldVector direction) noexcept {
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y)) return {};
    const auto length = std::hypot(direction.x, direction.y);
    if (length > 1.0) return {direction.x / length, direction.y / length};
    return direction;
}
[[nodiscard]] inline WorldPosition interpolate(WorldPosition a, WorldPosition b, double alpha) noexcept {
    return {a.x + (b.x - a.x) * alpha, a.y + (b.y - a.y) * alpha};
}
} // namespace tchips::math
