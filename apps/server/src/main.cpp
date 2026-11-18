#include <tchips/contracts/version.hpp>
#include <tchips/server/config.hpp>
#include <tchips/server/world.hpp>
#ifdef TCHIPS_NETWORK_ENABLED
#include <steam/isteamnetworkingutils.h>
#include <tchips/server/network_server.hpp>
#endif
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>
#ifdef TCHIPS_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#endif

namespace {
volatile std::sig_atomic_t interrupted = 0;
extern "C" void stop(int) {
    interrupted = 1;
}
#ifdef TCHIPS_NETWORK_ENABLED
struct NetworkLifetime {
    ~NetworkLifetime() { GameNetworkingSockets_Kill(); }
};
#endif
} // namespace
int main(int argc, char **argv) {
    try {
        std::vector<std::string_view> args;
        for (int i = 1; i < argc; ++i)
            args.emplace_back(argv[i]);
        const auto parsed = tchips::server::parse_arguments(args);
        if (!parsed) {
            std::cerr << "[server] " << parsed.error() << '\n';
            return 2;
        }
        const auto config = *parsed;
        if (config.help) {
            std::cout << "tchips_server [--port 27020] [--map PATH] [--lag-ms N] [--loss-percent N] [--ticks "
                         "N] [--no-sleep]\n";
            return 0;
        }
        if (std::signal(SIGINT, stop) == SIG_ERR || std::signal(SIGTERM, stop) == SIG_ERR) {
            std::cerr << "[server] signal handler failure\n";
            return 1;
        }
        // A deployed map lives beside the executable, never behind an SDL dependency.
        auto map_path = config.map_path;
        if (map_path.empty())
            map_path = std::filesystem::path(argv[0]).parent_path() / "assets/maps/prototype.map";
        auto map = tchips::geometry::WorldMap::load(map_path);
        if (!map) {
            std::cerr << "[server] " << map.error() << '\n';
            return 1;
        }
        tchips::server::World world{std::move(*map)};
#ifdef TCHIPS_NETWORK_ENABLED
        SteamNetworkingErrMsg error;
        if (!GameNetworkingSockets_Init(nullptr, error)) {
            std::cerr << "[server] network init: " << error << '\n';
            return 1;
        }
        const NetworkLifetime runtime;
        auto *utils = SteamNetworkingUtils();
        if (!utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, config.lag_ms) ||
            !utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send,
                                              config.loss_percent)) {
            std::cerr << "[server] invalid network simulation config\n";
            return 1;
        }
        tchips::server::NetworkServer network{world};
        if (auto listen = network.listen(config.port); !listen) {
            std::cerr << "[server] " << listen.error() << '\n';
            return 1;
        }
        std::cout << "[server] UDP port=" << network.port()
                  << ", protocol=" << tchips::contracts::protocol_version << ", tick_rate=30\n"
                  << std::flush;
#else
        std::cout << "[server] networking disabled, tick_rate=30\n";
#endif
        auto deadline = std::chrono::steady_clock::now();
        while (!interrupted && (config.max_ticks == 0 || world.tick() < config.max_ticks)) {
#ifdef TCHIPS_NETWORK_ENABLED
            network.pump();
#endif
            const auto now = std::chrono::steady_clock::now();
            if (!config.sleep || now >= deadline) {
#ifdef TCHIPS_TRACY_ENABLED
                ZoneScopedN("server authoritative step");
#endif
                world.step();
#ifdef TCHIPS_NETWORK_ENABLED
                network.publish();
#endif
                deadline += tchips::server::fixed_step;
                if (now > deadline + tchips::server::fixed_step)
                    deadline = now + tchips::server::fixed_step;
#ifdef TCHIPS_TRACY_ENABLED
                FrameMark;
#endif
            } else
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
#ifdef TCHIPS_NETWORK_ENABLED
        network.shutdown();
#endif
        std::cout << "[server] stopped, ticks=" << world.tick() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "[server] fatal: " << error.what() << '\n';
        return 1;
    }
}
