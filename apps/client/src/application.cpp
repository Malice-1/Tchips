#include <SDL3/SDL.h>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <tchips/client/application.hpp>
#include <tchips/client/asset_manager.hpp>
#include <tchips/client/input_manager.hpp>
#include <tchips/client/renderer.hpp>
#include <tchips/contracts/version.hpp>
#ifdef TCHIPS_NETWORK_ENABLED
#include <steam/isteamnetworkingutils.h>
#include <tchips/client/network_client.hpp>
#endif
#ifdef TCHIPS_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#endif

namespace {
struct SdlLifetime {
    ~SdlLifetime() { SDL_Quit(); }
};
struct CursorLifetime {
    ~CursorLifetime() { SDL_ShowCursor(); }
};
#ifdef TCHIPS_NETWORK_ENABLED
struct NetworkLifetime {
    bool active{};
    ~NetworkLifetime() {
        if (active)
            GameNetworkingSockets_Kill();
    }
};
#endif
int report(const std::string &error) {
    std::cerr << "[client] " << error << '\n';
    return 1;
}
int sdl_error(const char *stage) {
    return report(std::string(stage) + ": " + SDL_GetError());
}
bool push_key(SDL_Scancode code, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = code;
    event.key.down = down;
    return SDL_PushEvent(&event);
}
} // namespace
namespace tchips::client {
std::expected<ApplicationConfig, std::string>
Application::parse_arguments(std::span<const std::string_view> args) {
    ApplicationConfig config;
#ifndef TCHIPS_NETWORK_ENABLED
    config.offline = true;
#endif
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help")
            config.help = true;
        else if (args[i] == "--check-sdl")
            config.check_sdl = true;
        else if (args[i] == "--fullscreen")
            config.fullscreen = true;
        else if (args[i] == "--smoke-test")
            config.smoke_test = true;
        else if (args[i] == "--offline")
            config.offline = true;
        else if (args[i] == "--network-smoke")
            config.network_smoke = true;
        else if (args[i] == "--connect") {
            if (++i == args.size() || args[i].empty() || args[i].starts_with("--"))
                return std::unexpected("--connect requires IP:port");
            config.server_address = std::string(args[i]);
        } else if (args[i] == "--lag-ms" || args[i] == "--loss-percent") {
            const auto option = args[i];
            if (++i == args.size())
                return std::unexpected("missing network simulation value");
            const auto value = args[i];
            unsigned parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                return std::unexpected("invalid network simulation value");
            if (option == "--lag-ms") {
                if (parsed > 1000)
                    return std::unexpected("lag must be 0..1000 ms");
                config.lag_ms = static_cast<int>(parsed);
            } else {
                if (parsed > 50)
                    return std::unexpected("loss must be 0..50 percent");
                config.loss_percent = static_cast<float>(parsed);
            }
        } else if (args[i] == "--map") {
            if (!config.map_path.empty() || ++i == args.size() || args[i].empty() ||
                args[i].starts_with("--"))
                return std::unexpected("--map requires one file path");
            config.map_path = std::string(args[i]);
        } else if (args[i] == "--frames") {
            if (config.max_frames != 0 || ++i == args.size())
                return std::unexpected("--frames requires one positive integer");
            const auto value = args[i];
            const auto result = std::from_chars(value.data(), value.data() + value.size(), config.max_frames);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                config.max_frames == 0)
                return std::unexpected("--frames requires one positive integer");
        } else
            return std::unexpected("unknown argument: " + std::string(args[i]));
    }
    if (config.smoke_test &&
        (config.max_frames != 0 || config.check_sdl || config.fullscreen || !config.map_path.empty()))
        return std::unexpected("--smoke-test uses the default map/window and its own bounded scenario");
    if (config.network_smoke &&
        (config.offline || config.smoke_test || config.max_frames != 0 || config.check_sdl))
        return std::unexpected("--network-smoke requires an online, unbounded client");
    return config;
}
int Application::run(const ApplicationConfig &config) {
    if (config.help) {
        std::cout
            << "tchips_client [--connect IP:port] [--offline] [--lag-ms N] [--loss-percent N] "
               "[--network-smoke] [--frames N] [--map PATH] [--fullscreen] [--check-sdl] [--smoke-test]\n";
        return 0;
    }
    if (!SDL_Init(config.check_sdl ? SDL_INIT_EVENTS : SDL_INIT_VIDEO))
        return sdl_error("SDL_Init");
    const SdlLifetime sdl;
    std::cout << "[client] SDL=" << SDL_GetVersion() << ", protocol=" << tchips::contracts::protocol_version
              << '\n';
    if (config.check_sdl)
        return 0;
    const auto *base = SDL_GetBasePath();
    if (!base)
        return sdl_error("SDL_GetBasePath");
    const AssetManager assets{std::filesystem::path(base) / "assets"};
    auto map_result = WorldMap::load(config.map_path.empty() ? assets.default_map_path() : config.map_path);
    if (!map_result)
        return report(map_result.error());
    const auto map = std::move(*map_result);
    auto sprite = assets.load_character();
    if (!sprite)
        return report(sprite.error());
    const CharacterBody body;
    if (!CollisionSystem::fits(map, map.spawn(), body.half_extent))
        return report("spawn does not fit the character collision box");
    [[maybe_unused]] const bool offline = config.offline || config.smoke_test;
#ifdef TCHIPS_NETWORK_ENABLED
    NetworkLifetime network_lifetime;
    if (!offline) {
        SteamNetworkingErrMsg error;
        if (!GameNetworkingSockets_Init(nullptr, error))
            return report(std::string("network init: ") + error);
        network_lifetime.active = true;
        if (!SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send,
                                                               config.lag_ms) ||
            !SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send,
                                                               config.loss_percent))
            return report("invalid SDK network simulation configuration");
    }
#endif
    const std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{
        SDL_CreateWindow("Tchips - Phase 01 | WASD - souris - F11 plein ecran - Echap quitter", 1280, 720,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY),
        SDL_DestroyWindow};
    if (!window)
        return sdl_error("SDL_CreateWindow");
    if (!SDL_SetWindowMinimumSize(window.get(), 480, 320))
        return sdl_error("SDL_SetWindowMinimumSize");
    if (config.fullscreen && !SDL_SetWindowFullscreen(window.get(), true))
        return sdl_error("SDL_SetWindowFullscreen");
    auto renderer_result = Renderer::create(window.get());
    if (!renderer_result)
        return report(renderer_result.error());
    auto renderer = std::move(*renderer_result);
    std::cout << "[client] GPU=" << renderer->backend() << ", map=" << map.width() << 'x' << map.height()
              << ", obstacles=" << map.obstacles().size() << '\n'
              << std::flush;
    if (!SDL_HideCursor())
        return sdl_error("SDL_HideCursor");
    const CursorLifetime cursor_lifetime;
    entt::registry registry;
    const auto player = registry.create();
    registry.emplace<CharacterTransform>(player, map.spawn(), map.spawn(), 0.0);
    registry.emplace<CharacterBody>(player, body);
    float mouse_x = 0, mouse_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    InputManager inputs{{mouse_x, mouse_y}};
    Camera2D camera;
    MovementSystem movement;
    std::string hud;
    std::vector<contracts::EntityState> remote_players;
#ifdef TCHIPS_NETWORK_ENABLED
    std::unique_ptr<NetworkClient> network;
    if (!offline) {
        network = std::make_unique<NetworkClient>(map);
        if (auto connected = network->connect(config.server_address); !connected)
            return report(connected.error());
    }
    const auto network_smoke_started = std::chrono::steady_clock::now();
    std::size_t max_visible = 0;
    bool remote_moved = false;
    std::map<contracts::EntityId, math::WorldPosition> first_remote_positions;
    auto hud_time = std::chrono::steady_clock::time_point{};
    auto previous_state = ConnectionState::disconnected;
#endif
    auto last = std::chrono::steady_clock::now();
    std::uint64_t frame = 0;
    bool smoke_wall = false, smoke_resized = false, smoke_fullscreen = false, smoke_windowed = false;
    while (!inputs.quit_requested() && (config.max_frames == 0 || frame < config.max_frames)) {
#ifdef TCHIPS_TRACY_ENABLED
        ZoneScopedN("client phase01 frame");
#endif
        if (config.smoke_test) {
            // Diagnostic inputs traverse the normal SDL event queue/InputManager.
            if (frame == 0) {
                if (!push_key(SDL_SCANCODE_D, true))
                    return sdl_error("smoke key");
                SDL_WarpMouseInWindow(window.get(), 700, 200);
            }
            if (frame == 65) {
                const auto p = registry.get<CharacterTransform>(player).position;
                smoke_wall = std::abs(p.x - 436) < 0.01;
                if (!smoke_wall)
                    return report("smoke: did not stop at the obstacle");
                if (!push_key(SDL_SCANCODE_D, false) || !push_key(SDL_SCANCODE_S, true))
                    return sdl_error("smoke key");
            }
            if (frame == 100) {
                if (!SDL_SetWindowSize(window.get(), 1024, 640))
                    return sdl_error("smoke resize");
            }
            if (frame == 115 || frame == 125) {
                if (!push_key(SDL_SCANCODE_F11, true))
                    return sdl_error("smoke fullscreen");
            }
            if (frame == 135) {
                if (!push_key(SDL_SCANCODE_S, true))
                    return sdl_error("smoke resume");
            }
            if (frame == 120)
                smoke_fullscreen = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) != 0;
            if (frame == 130) {
                smoke_windowed = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) == 0;
                int width = 0, height = 0;
                SDL_GetWindowSize(window.get(), &width, &height);
                smoke_resized = width == 1024 && height == 640;
            }
            if (frame == 150) {
                const auto &transform = registry.get<CharacterTransform>(player);
                if (!smoke_wall || !smoke_fullscreen || !smoke_windowed || !smoke_resized ||
                    transform.position.y < 420 || !std::isfinite(transform.angle) ||
                    !CollisionSystem::fits(map, transform.position, body.half_extent))
                    return report("smoke: movement/window invariants failed");
                SDL_Event quit{};
                quit.type = SDL_EVENT_QUIT;
                if (!SDL_PushEvent(&quit))
                    return sdl_error("smoke quit");
            }
            if (frame > 180)
                return report("smoke timeout");
        }
        SDL_Event event;
        while (SDL_PollEvent(&event))
            inputs.handle(event);
        if (inputs.quit_requested())
            break;
        if (inputs.consume_fullscreen_toggle()) {
            const bool fullscreen = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) != 0;
            if (!SDL_SetWindowFullscreen(window.get(), !fullscreen))
                return sdl_error("SDL_SetWindowFullscreen");
        }
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double>(now - last).count();
        last = now;
        auto &transform = registry.get<CharacterTransform>(player);
        double render_alpha = movement.alpha();
#ifdef TCHIPS_NETWORK_ENABLED
        if (network) {
            const auto mouse_world = camera.screen_to_world(inputs.mouse());
            const auto angle =
                std::atan2(mouse_world.y - transform.position.y, mouse_world.x - transform.position.x);
            const auto smoke_age = std::chrono::duration<double>(now - network_smoke_started).count();
            network->update(elapsed,
                            config.network_smoke
                                ? (smoke_age < 2.5 ? math::WorldVector{1, 0} : math::WorldVector{})
                                : inputs.movement(),
                            config.network_smoke ? 0 : angle);
            if (network->state() != previous_state) {
                previous_state = network->state();
                std::cout << "[client] network=" << network->status_text() << '\n' << std::flush;
            }
            if (network->active()) {
                const auto &prediction = network->prediction();
                transform.position = prediction.state().position;
                transform.previous = prediction.previous();
                transform.angle = prediction.state().angle;
            }
            render_alpha = network->alpha();
            remote_players = network->remote_entities();
            max_visible = std::max(max_visible, network->visible_count());
            for (const auto &remote : remote_players) {
                const auto [entry, added] = first_remote_positions.try_emplace(remote.id, remote.position);
                if (!added &&
                    std::hypot(remote.position.x - entry->second.x, remote.position.y - entry->second.y) > 4)
                    remote_moved = true;
            }
            if (now - hud_time > std::chrono::milliseconds(250)) {
                std::ostringstream line;
                const auto ping = network->ping_ms();
                line << network->status_text() << " PING " << (ping >= 0 ? std::to_string(ping) : "--")
                     << " MS CORR " << std::fixed << std::setprecision(1)
                     << network->prediction().correction() << " U FILE "
                     << network->prediction().pending_count() << " JOUEURS " << network->visible_count();
                hud = line.str();
                hud_time = now;
                if (!SDL_SetWindowTitle(window.get(), ("Tchips | " + hud + " | WASD / F11 / ESC").c_str()))
                    return sdl_error("SDL_SetWindowTitle");
            }
            if (config.network_smoke && smoke_age > 8) {
                if (!network->active() || max_visible < 2 || !remote_moved ||
                    network->prediction().state().position.x <= map.spawn().x + 4 ||
                    network->prediction().authoritative_distance() > 0.5)
                    return report("network smoke did not converge with two moving players");
                std::cout << "[client] network smoke passed: visible=" << max_visible
                          << ", remote-motion=yes, gap=" << network->prediction().authoritative_distance()
                          << ", ping=" << network->ping_ms() << '\n';
                break;
            }
        } else
#endif
        {
            movement.advance(registry, map, inputs.movement(), config.smoke_test ? 1.0 / 60 : elapsed);
            render_alpha = movement.alpha();
        }
        int width = 0, height = 0;
        if (!SDL_GetWindowSize(window.get(), &width, &height))
            return sdl_error("SDL_GetWindowSize");
        if (width <= 0 || height <= 0) {
            SDL_Delay(10);
            continue;
        }
        camera.set_viewport({static_cast<double>(width), static_cast<double>(height)});
        camera.follow(math::interpolate(transform.previous, transform.position, render_alpha), map.width(),
                      map.height());
        MovementSystem::aim(transform, camera.screen_to_world(inputs.mouse()));
        auto rendered = renderer->draw(map, camera, *sprite, transform, render_alpha, inputs.mouse(),
                                       remote_players, hud);
        if (!rendered)
            return report(rendered.error());
        ++frame;
#ifdef TCHIPS_TRACY_ENABLED
        FrameMark;
#endif
    }
    if (auto idle = renderer->wait_idle(); !idle)
        return report(idle.error());
    registry.clear();
    if (config.smoke_test)
        std::cout << "[client] smoke passed: input, collision, resize, fullscreen, camera, quit-event\n";
    std::cout << "[client] stopped, frames=" << frame << '\n';
    return 0;
}
} // namespace tchips::client
