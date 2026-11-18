#pragma once
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace tchips::server {
inline constexpr std::uint32_t tick_rate = 30;
inline constexpr auto fixed_step = std::chrono::nanoseconds{1'000'000'000 / tick_rate};
struct Config {
    std::uint64_t max_ticks = 0; // zero means run until interrupted
    bool sleep = true;
    bool help = false;
    std::uint16_t port = 27020;
    std::filesystem::path map_path;
    int lag_ms = 0;
    float loss_percent = 0;
};
[[nodiscard]] std::expected<Config, std::string> parse_arguments(std::span<const std::string_view> arguments);
} // namespace tchips::server
