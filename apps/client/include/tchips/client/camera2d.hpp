#pragma once
#include <tchips/math/geometry.hpp>

namespace tchips::client {
class Camera2D {
public:
    void set_viewport(math::Viewport viewport) noexcept;
    void follow(math::WorldPosition focus, double world_width, double world_height) noexcept;
    [[nodiscard]] math::ScreenPosition world_to_screen(math::WorldPosition point) const noexcept;
    [[nodiscard]] math::WorldPosition screen_to_world(math::ScreenPosition point) const noexcept;
    [[nodiscard]] math::Viewport viewport() const noexcept { return viewport_; }
    [[nodiscard]] math::WorldPosition center() const noexcept { return center_; }
private:
    math::Viewport viewport_;
    math::WorldPosition center_;
};
} // namespace tchips::client
