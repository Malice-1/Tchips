#include <tchips/client/asset_manager.hpp>
#include <array>
#include <fstream>

namespace tchips::client {
std::expected<SpriteAsset, std::string> AssetManager::load_character() const {
    const auto path = root_ / "sprites/temporary.sprite";
    std::error_code error;
    const auto bytes = std::filesystem::file_size(path, error);
    if (error || bytes > 4096) return std::unexpected("sprite missing, unreadable or larger than 4 KiB: " + path.string());
    std::ifstream file(path);
    if (!file) return std::unexpected("cannot open sprite: " + path.string());
    return parse_sprite(file);
}
std::expected<SpriteAsset, std::string> AssetManager::parse_sprite(std::istream& input) {
    std::string header;
    int version = 0;
    SpriteAsset sprite;
    if (!(input >> header >> version >> sprite.width >> sprite.height) || header != "TCHIPS_SPRITE" || version != 1 || sprite.width == 0 || sprite.height == 0 || sprite.width > 32 || sprite.height > 32) return std::unexpected("invalid sprite header/version/dimensions");
    const std::array<Color, 5> palette{{{1.0F, 0.78F, 0.32F}, {0.34F, 0.88F, 0.77F}, {0.12F, 0.46F, 0.48F}, {0.04F, 0.13F, 0.18F}, {0.95F, 0.99F, 0.94F}}};
    sprite.pixels.reserve(sprite.width * sprite.height);
    std::string row;
    for (std::uint32_t y = 0; y < sprite.height; ++y) {
        if (!(input >> row) || row.size() != sprite.width) return std::unexpected("invalid sprite row");
        for (std::uint32_t x = 0; x < sprite.width; ++x) {
            const auto symbol = row[x];
            if (symbol == '.') continue;
            if (symbol < 'a' || symbol > 'e') return std::unexpected("unknown sprite palette symbol");
            sprite.pixels.push_back({x, y, palette[static_cast<std::size_t>(symbol - 'a')]});
        }
    }
    if (input >> row) return std::unexpected("trailing sprite data");
    if (sprite.pixels.empty() || input.bad()) return std::unexpected("empty or unreadable sprite");
    return sprite;
}
} // namespace tchips::client
