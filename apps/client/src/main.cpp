#include <SDL3/SDL_main.h>
#include <exception>
#include <iostream>
#include <string_view>
#include <tchips/client/application.hpp>
#include <vector>

int main(int argc, char **argv) {
    try {
        std::vector<std::string_view> arguments;
        for (int i = 1; i < argc; ++i)
            arguments.emplace_back(argv[i]);
        const auto config = tchips::client::Application::parse_arguments(arguments);
        if (!config) {
            std::cerr << "[client] " << config.error() << "; use --help\n";
            return 2;
        }
        return tchips::client::Application{}.run(*config);
    } catch (const std::exception &error) {
        std::cerr << "[client] fatal: " << error.what() << '\n';
        return 1;
    }
}
