#pragma once
#include <SDL3/SDL.h>
#include <tchips/math/geometry.hpp>

namespace tchips::client {
class InputManager {
public:
    explicit InputManager(math::ScreenPosition initial_mouse = {}) : mouse_(initial_mouse) {}
    void handle(const SDL_Event& event) noexcept;
    [[nodiscard]] math::WorldVector movement() const noexcept;
    [[nodiscard]] math::ScreenPosition mouse() const noexcept;
    [[nodiscard]] bool quit_requested() const noexcept { return quit_; }
    [[nodiscard]] bool consume_fullscreen_toggle() noexcept;
private:
    bool quit_{}, toggle_fullscreen_{}, focused_{true};
    bool up_{}, down_{}, left_{}, right_{};
    math::ScreenPosition mouse_;
};
} // namespace tchips::client
