#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <tchips/geometry/world_map.hpp>

namespace tchips::geometry {
std::uint64_t WorldMap::fingerprint() const noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto append = [&](std::uint64_t bits) {
        for (unsigned i = 0; i < 8; ++i) {
            hash ^= (bits >> (i * 8)) & 255U;
            hash *= 1099511628211ULL;
        }
    };
    for (const auto value : {width_, height_, tile_size_, spawn_.x, spawn_.y})
        append(std::bit_cast<std::uint64_t>(value));
    append(obstacles_.size());
    for (const auto &o : obstacles_)
        for (const auto value : {o.x, o.y, o.width, o.height})
            append(std::bit_cast<std::uint64_t>(value));
    return hash;
}
std::expected<WorldMap, std::string> WorldMap::load(const std::filesystem::path &path) {
    std::error_code error;
    const auto bytes = std::filesystem::file_size(path, error);
    if (error || bytes > 128 * 1024)
        return std::unexpected("map missing, unreadable or larger than 128 KiB: " + path.string());
    std::ifstream file(path);
    if (!file)
        return std::unexpected("cannot open map: " + path.string());
    return parse(file);
}
std::expected<WorldMap, std::string> WorldMap::parse(std::istream &input) {
    WorldMap map;
    std::string token;
    int version = 0;
    if (!(input >> token >> version) || token != "TCHIPS_MAP" || version != 1)
        return std::unexpected("unsupported map header/version");
    if (!(input >> token >> map.width_ >> map.height_) || token != "size")
        return std::unexpected("expected map size");
    if (!std::isfinite(map.width_) || !std::isfinite(map.height_) || map.width_ <= 0 || map.height_ <= 0 ||
        map.width_ > max_dimension || map.height_ > max_dimension)
        return std::unexpected("map dimensions out of range");
    if (!(input >> token >> map.tile_size_) || token != "tile" || !std::isfinite(map.tile_size_) ||
        map.tile_size_ < 16 || map.tile_size_ > 512)
        return std::unexpected("invalid tile size (16..512)");
    if (!(input >> token >> map.spawn_.x >> map.spawn_.y) || token != "spawn" || !math::finite(map.spawn_) ||
        map.spawn_.x < 0 || map.spawn_.y < 0 || map.spawn_.x > map.width_ || map.spawn_.y > map.height_)
        return std::unexpected("invalid spawn");
    std::int64_t count = 0;
    if (!(input >> token >> count) || token != "obstacles" || count < 0 ||
        count > static_cast<std::int64_t>(max_obstacles))
        return std::unexpected("invalid obstacle count");
    map.obstacles_.reserve(static_cast<std::size_t>(count));
    for (std::int64_t i = 0; i < count; ++i) {
        math::WorldRect o;
        if (!(input >> o.x >> o.y >> o.width >> o.height) || !std::isfinite(o.x) || !std::isfinite(o.y) ||
            !std::isfinite(o.width) || !std::isfinite(o.height) || o.x < 0 || o.y < 0 || o.width <= 0 ||
            o.height <= 0 || o.x + o.width > map.width_ || o.y + o.height > map.height_)
            return std::unexpected("invalid obstacle " + std::to_string(i));
        if (map.spawn_.x >= o.x && map.spawn_.x <= o.x + o.width && map.spawn_.y >= o.y &&
            map.spawn_.y <= o.y + o.height)
            return std::unexpected("spawn intersects an obstacle");
        map.obstacles_.push_back(o);
    }
    if (input >> token)
        return std::unexpected("unexpected trailing map data");
    if (input.bad())
        return std::unexpected("map read error");
    return map;
}
} // namespace tchips::geometry
