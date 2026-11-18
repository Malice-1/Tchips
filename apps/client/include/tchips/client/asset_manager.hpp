#pragma once
#include <tchips/client/world_map.hpp>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace tchips::client {
struct Color { float r{}, g{}, b{}; };
struct SpritePixel { std::uint32_t x{}, y{}; Color color; };
struct SpriteAsset { std::uint32_t width{}, height{}; std::vector<SpritePixel> pixels; };
class AssetManager {
public:
    explicit AssetManager(std::filesystem::path root) : root_(std::move(root)) {}
    [[nodiscard]] std::filesystem::path default_map_path() const { return root_ / "maps/prototype.map"; }
    [[nodiscard]] std::expected<SpriteAsset, std::string> load_character() const;
    [[nodiscard]] static std::expected<SpriteAsset, std::string> parse_sprite(std::istream& input);
private:
    std::filesystem::path root_;
};
} // namespace tchips::client
