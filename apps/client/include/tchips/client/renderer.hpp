#pragma once
#include <SDL3/SDL.h>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tchips/client/asset_manager.hpp>
#include <tchips/client/camera2d.hpp>
#include <tchips/client/movement_system.hpp>
#include <tchips/contracts/movement.hpp>
#include <vector>

namespace tchips::client {
class Renderer {
  public:
    [[nodiscard]] static std::expected<std::unique_ptr<Renderer>, std::string> create(SDL_Window *window);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    [[nodiscard]] std::expected<void, std::string>
    draw(const WorldMap &map, const Camera2D &camera, const SpriteAsset &sprite,
         const CharacterTransform &player, double alpha, math::ScreenPosition cursor,
         std::span<const contracts::EntityState> remotes = {}, std::string_view hud = {});
    [[nodiscard]] std::expected<void, std::string> wait_idle();
    [[nodiscard]] std::string backend() const;

  private:
    template <class T, auto Release> struct GpuDeleter {
        SDL_GPUDevice *device{};
        void operator()(T *resource) const noexcept {
            if (resource)
                Release(device, resource);
        }
    };
    using Pipeline = std::unique_ptr<SDL_GPUGraphicsPipeline,
                                     GpuDeleter<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>>;
    using Buffer = std::unique_ptr<SDL_GPUBuffer, GpuDeleter<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>>;
    using Transfer = std::unique_ptr<SDL_GPUTransferBuffer,
                                     GpuDeleter<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>>;
    Renderer() = default;
    struct Vertex {
        float x, y, z, r, g, b;
    };
    static constexpr std::size_t max_vertices = 262144;
    SDL_Window *window_{}; // non-owning; Application outlives Renderer
    std::unique_ptr<SDL_GPUDevice, decltype(&SDL_DestroyGPUDevice)> device_{nullptr, SDL_DestroyGPUDevice};
    Pipeline pipeline_{nullptr, {}};
    Buffer vertices_buffer_{nullptr, {}};
    Transfer transfer_{nullptr, {}};
    bool claimed_{};
    bool overflow_{};
    std::vector<Vertex> vertices_;
    math::Viewport viewport_;
    void triangle(math::ScreenPosition a, math::ScreenPosition b, math::ScreenPosition c, Color color);
    void rectangle(math::ScreenPosition p, double width, double height, Color color);
    void world_rectangle(const Camera2D &camera, math::WorldRect rect, Color color);
    void character(const Camera2D &camera, const SpriteAsset &sprite, math::WorldPosition position,
                   double angle, bool remote);
    void diagnostic_text(std::string_view text);
};
} // namespace tchips::client
