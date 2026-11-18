#include <tchips/client/camera2d.hpp>
#include <algorithm>
#include <cmath>

namespace tchips::client {
void Camera2D::set_viewport(math::Viewport viewport) noexcept {
    if (std::isfinite(viewport.width) && std::isfinite(viewport.height) && viewport.width > 0 && viewport.height > 0) viewport_ = viewport;
}
void Camera2D::follow(math::WorldPosition focus, double width, double height) noexcept {
    if (!math::finite(focus) || !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return;
    center_.x = width <= viewport_.width ? width / 2 : std::clamp(focus.x, viewport_.width / 2, width - viewport_.width / 2);
    center_.y = height <= viewport_.height ? height / 2 : std::clamp(focus.y, viewport_.height / 2, height - viewport_.height / 2);
}
math::ScreenPosition Camera2D::world_to_screen(math::WorldPosition point) const noexcept {
    return {point.x - center_.x + viewport_.width / 2, point.y - center_.y + viewport_.height / 2};
}
math::WorldPosition Camera2D::screen_to_world(math::ScreenPosition point) const noexcept {
    return {point.x + center_.x - viewport_.width / 2, point.y + center_.y - viewport_.height / 2};
}
} // namespace tchips::client
