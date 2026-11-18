#include <charconv>
#include <cmath>
#include <tchips/server/config.hpp>

namespace tchips::server {
std::expected<Config, std::string> parse_arguments(std::span<const std::string_view> arguments) {
    Config config;
    bool ticks_seen = false;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto argument = arguments[i];
        if (argument == "--help") {
            config.help = true;
        } else if (argument == "--no-sleep") {
            config.sleep = false;
        } else if (argument == "--ticks") {
            if (ticks_seen || ++i == arguments.size())
                return std::unexpected("--ticks requires one positive integer");
            ticks_seen = true;
            const auto value = arguments[i];
            const auto result = std::from_chars(value.data(), value.data() + value.size(), config.max_ticks);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                config.max_ticks == 0)
                return std::unexpected("--ticks requires one positive integer");
        } else if (argument == "--map") {
            if (++i == arguments.size() || arguments[i].empty() || arguments[i].starts_with("--") ||
                !config.map_path.empty())
                return std::unexpected("--map requires one path");
            config.map_path = std::string(arguments[i]);
        } else if (argument == "--port" || argument == "--lag-ms" || argument == "--loss-percent") {
            if (++i == arguments.size())
                return std::unexpected("missing network option value");
            const auto value = arguments[i];
            unsigned parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                return std::unexpected("invalid network option value");
            if (argument == "--port") {
                if (parsed > 65535)
                    return std::unexpected("invalid UDP port");
                config.port = static_cast<std::uint16_t>(parsed);
            } else if (argument == "--lag-ms") {
                if (parsed > 1000)
                    return std::unexpected("lag must be 0..1000 ms");
                config.lag_ms = static_cast<int>(parsed);
            } else {
                if (parsed > 50)
                    return std::unexpected("loss must be 0..50 percent");
                config.loss_percent = static_cast<float>(parsed);
            }
        } else
            return std::unexpected("unknown argument: " + std::string(argument));
    }
    if (!config.sleep && !ticks_seen && !config.help)
        return std::unexpected("--no-sleep requires --ticks to bound the diagnostic run");
    return config;
}
} // namespace tchips::server
