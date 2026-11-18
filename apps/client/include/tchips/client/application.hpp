#pragma once
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace tchips::client {
struct ApplicationConfig {
    std::uint64_t max_frames{};
    bool check_sdl{}, fullscreen{}, smoke_test{}, help{};
    bool offline{}, network_smoke{};
    std::string server_address{"127.0.0.1:27020"};
    int lag_ms{};
    float loss_percent{};
    std::filesystem::path map_path;
};
class Application {
  public:
    [[nodiscard]] static std::expected<ApplicationConfig, std::string>
    parse_arguments(std::span<const std::string_view> arguments);
    [[nodiscard]] int run(const ApplicationConfig &config);
};
} // namespace tchips::client
