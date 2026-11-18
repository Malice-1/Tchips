#include <tchips/client/input_manager.hpp>
#include <tchips/client/application.hpp>
#include <gtest/gtest.h>
#include <array>

namespace {
void key(tchips::client::InputManager& input,SDL_Scancode code,bool down,bool repeat=false) {SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;event.key.scancode=code;event.key.repeat=repeat;input.handle(event);}
}
TEST(Input, KeyReleaseAndFocusLossStopMovement) {
    tchips::client::InputManager input;key(input,SDL_SCANCODE_W,true);key(input,SDL_SCANCODE_D,true);
    EXPECT_DOUBLE_EQ(input.movement().x,1);EXPECT_DOUBLE_EQ(input.movement().y,-1);
    key(input,SDL_SCANCODE_D,false);EXPECT_DOUBLE_EQ(input.movement().x,0);
    SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;input.handle(lost);EXPECT_DOUBLE_EQ(input.movement().y,0);
    lost.type=SDL_EVENT_WINDOW_FOCUS_GAINED;input.handle(lost);EXPECT_DOUBLE_EQ(input.movement().y,0);
}
TEST(Input, FullscreenToggleIsOneShotAndRepeatIsIgnored) {
    tchips::client::InputManager input;key(input,SDL_SCANCODE_F11,true);EXPECT_TRUE(input.consume_fullscreen_toggle());EXPECT_FALSE(input.consume_fullscreen_toggle());
    key(input,SDL_SCANCODE_F11,true,true);EXPECT_FALSE(input.consume_fullscreen_toggle());key(input,SDL_SCANCODE_ESCAPE,true);EXPECT_TRUE(input.quit_requested());
}
TEST(Input, MouseUsesLogicalWindowCoordinates) {
    tchips::client::InputManager input;SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=123.5F;motion.motion.y=245.5F;input.handle(motion);
    EXPECT_DOUBLE_EQ(input.mouse().x,123.5);EXPECT_DOUBLE_EQ(input.mouse().y,245.5);
}
TEST(ClientConfig, ParsesMapFullscreenAndBoundedFrames) {
    const std::array<std::string_view,5> arguments{"--map","custom.map","--fullscreen","--frames","3"};
    const auto config=tchips::client::Application::parse_arguments(arguments);ASSERT_TRUE(config);EXPECT_TRUE(config->fullscreen);EXPECT_EQ(config->max_frames,3);EXPECT_EQ(config->map_path,"custom.map");
    const std::array<std::string_view,1> missing{"--map"};EXPECT_FALSE(tchips::client::Application::parse_arguments(missing));
    const std::array<std::string_view,3> conflict{"--smoke-test","--frames","3"};EXPECT_FALSE(tchips::client::Application::parse_arguments(conflict));
}
