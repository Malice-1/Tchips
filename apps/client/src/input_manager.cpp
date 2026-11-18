#include <tchips/client/input_manager.hpp>
#include <utility>

namespace tchips::client {
void InputManager::handle(const SDL_Event& event) noexcept {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) quit_ = true;
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.scancode == SDL_SCANCODE_ESCAPE) quit_ = true;
        if (event.key.scancode == SDL_SCANCODE_F11) toggle_fullscreen_ = true;
    }
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        const auto pressed = event.type == SDL_EVENT_KEY_DOWN;
        switch (event.key.scancode) {
            case SDL_SCANCODE_W: up_ = pressed; break;
            case SDL_SCANCODE_S: down_ = pressed; break;
            case SDL_SCANCODE_A: left_ = pressed; break;
            case SDL_SCANCODE_D: right_ = pressed; break;
            default: break;
        }
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION) mouse_ = {event.motion.x, event.motion.y};
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED) { focused_ = false; up_=down_=left_=right_=false; }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) focused_ = true;
}
math::WorldVector InputManager::movement() const noexcept {
    if (!focused_) return {};
    return {static_cast<double>(right_) - static_cast<double>(left_), static_cast<double>(down_) - static_cast<double>(up_)};
}
math::ScreenPosition InputManager::mouse() const noexcept {
    return mouse_;
}
bool InputManager::consume_fullscreen_toggle() noexcept { return std::exchange(toggle_fullscreen_, false); }
} // namespace tchips::client
