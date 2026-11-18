#pragma once
#include <cstdint>
#include <expected>
#include <filesystem>
#include <istream>
#include <string>
#include <tchips/math/geometry.hpp>
#include <vector>

namespace tchips::geometry {
class WorldMap {
  public:
    static constexpr std::size_t max_obstacles = 512;
    static constexpr double max_dimension = 100'000;
    [[nodiscard]] static std::expected<WorldMap, std::string> load(const std::filesystem::path &path);
    [[nodiscard]] static std::expected<WorldMap, std::string> parse(std::istream &input);
    [[nodiscard]] double width() const noexcept { return width_; }
    [[nodiscard]] double height() const noexcept { return height_; }
    [[nodiscard]] double tile_size() const noexcept { return tile_size_; }
    [[nodiscard]] math::WorldPosition spawn() const noexcept { return spawn_; }
    [[nodiscard]] const std::vector<math::WorldRect> &obstacles() const noexcept { return obstacles_; }
    [[nodiscard]] std::uint64_t fingerprint() const noexcept;

  private:
    double width_{}, height_{}, tile_size_{};
    math::WorldPosition spawn_;
    std::vector<math::WorldRect> obstacles_;
};
} // namespace tchips::geometry
