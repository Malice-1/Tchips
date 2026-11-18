#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <tchips/client/font5x7.hpp>
#include <tchips/client/renderer.hpp>

// Unmodified precompiled color shaders from SDL 3.4.20, Zlib license in shaders/.
namespace {
#include "cube.frag.dxil.h"
#include "cube.frag.msl.h"
#include "cube.frag.spv.h"
#include "cube.vert.dxil.h"
#include "cube.vert.msl.h"
#include "cube.vert.spv.h"
std::string gpu_error(const char *stage) {
    return std::string(stage) + ": " + SDL_GetError();
}
struct ShaderDeleter {
    SDL_GPUDevice *device;
    void operator()(SDL_GPUShader *s) const {
        if (s)
            SDL_ReleaseGPUShader(device, s);
    }
};
using Shader = std::unique_ptr<SDL_GPUShader, ShaderDeleter>;
Shader load_shader(SDL_GPUDevice *device, bool vertex) {
    SDL_GPUShaderCreateInfo info{};
    info.stage = vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_uniform_buffers = vertex ? 1U : 0U;
    const auto formats = SDL_GetGPUShaderFormats(device);
    info.entrypoint = "main";
    if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = vertex ? cube_vert_dxil : cube_frag_dxil;
        info.code_size = vertex ? cube_vert_dxil_len : cube_frag_dxil_len;
    } else if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = vertex ? cube_vert_spv : cube_frag_spv;
        info.code_size = vertex ? cube_vert_spv_len : cube_frag_spv_len;
    } else {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.code = vertex ? cube_vert_msl : cube_frag_msl;
        info.code_size = vertex ? cube_vert_msl_len : cube_frag_msl_len;
        info.entrypoint = "main0";
    }
    return Shader{SDL_CreateGPUShader(device, &info), ShaderDeleter{device}};
}
} // namespace

namespace tchips::client {
std::expected<std::unique_ptr<Renderer>, std::string> Renderer::create(SDL_Window *window) {
    // Private construction ensures partially acquired resources roll back via RAII.
    auto renderer = std::unique_ptr<Renderer>{new Renderer};
    renderer->window_ = window;
    renderer->device_.reset(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, false, nullptr));
    if (!renderer->device_)
        return std::unexpected(gpu_error("SDL_CreateGPUDevice"));
    auto *device = renderer->device_.get();
    if (!SDL_ClaimWindowForGPUDevice(device, window))
        return std::unexpected(gpu_error("SDL_ClaimWindowForGPUDevice"));
    renderer->claimed_ = true;
    auto vertex = load_shader(device, true);
    if (!vertex)
        return std::unexpected(gpu_error("vertex shader"));
    auto fragment = load_shader(device, false);
    if (!fragment)
        return std::unexpected(gpu_error("fragment shader"));
    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(Vertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    std::array<SDL_GPUVertexAttribute, 2> attributes{};
    attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0};
    attributes[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, r)};
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    SDL_GPUGraphicsPipelineCreateInfo pipeline{};
    pipeline.vertex_shader = vertex.get();
    pipeline.fragment_shader = fragment.get();
    pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline.vertex_input_state = {&buffer, 1, attributes.data(), static_cast<Uint32>(attributes.size())};
    pipeline.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline.target_info.color_target_descriptions = &target;
    pipeline.target_info.num_color_targets = 1;
    renderer->pipeline_ = Pipeline{SDL_CreateGPUGraphicsPipeline(device, &pipeline), {device}};
    if (!renderer->pipeline_)
        return std::unexpected(gpu_error("SDL_CreateGPUGraphicsPipeline"));
    SDL_GPUBufferCreateInfo vb{};
    vb.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb.size = static_cast<Uint32>(max_vertices * sizeof(Vertex));
    renderer->vertices_buffer_ = Buffer{SDL_CreateGPUBuffer(device, &vb), {device}};
    if (!renderer->vertices_buffer_)
        return std::unexpected(gpu_error("SDL_CreateGPUBuffer"));
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = vb.size;
    renderer->transfer_ = Transfer{SDL_CreateGPUTransferBuffer(device, &tb), {device}};
    if (!renderer->transfer_)
        return std::unexpected(gpu_error("SDL_CreateGPUTransferBuffer"));
    renderer->vertices_.reserve(max_vertices);
    return renderer;
}
Renderer::~Renderer() {
    if (device_) {
        SDL_WaitForGPUIdle(device_.get());
        transfer_.reset();
        vertices_buffer_.reset();
        pipeline_.reset();
        if (claimed_)
            SDL_ReleaseWindowFromGPUDevice(device_.get(), window_);
    }
}
std::string Renderer::backend() const {
    return SDL_GetGPUDeviceDriver(device_.get());
}
std::expected<void, std::string> Renderer::wait_idle() {
    if (!SDL_WaitForGPUIdle(device_.get()))
        return std::unexpected(gpu_error("SDL_WaitForGPUIdle"));
    return {};
}
void Renderer::triangle(math::ScreenPosition a, math::ScreenPosition b, math::ScreenPosition c, Color color) {
    if (vertices_.size() + 3 > max_vertices) {
        overflow_ = true;
        return;
    }
    for (const auto p : {a, b, c})
        vertices_.push_back({static_cast<float>(2 * p.x / viewport_.width - 1),
                             static_cast<float>(1 - 2 * p.y / viewport_.height), 0, color.r, color.g,
                             color.b});
}
void Renderer::rectangle(math::ScreenPosition p, double width, double height, Color color) {
    triangle(p, {p.x + width, p.y}, {p.x + width, p.y + height}, color);
    triangle(p, {p.x + width, p.y + height}, {p.x, p.y + height}, color);
}
void Renderer::world_rectangle(const Camera2D &camera, math::WorldRect rect, Color color) {
    const auto p = camera.world_to_screen({rect.x, rect.y});
    if (p.x + rect.width < 0 || p.y + rect.height < 0 || p.x > viewport_.width || p.y > viewport_.height)
        return;
    rectangle(p, rect.width, rect.height, color);
}
void Renderer::character(const Camera2D &camera, const SpriteAsset &sprite, math::WorldPosition position,
                         double angle, bool remote) {
    const auto center = camera.world_to_screen(position);
    if (center.x < -40 || center.y < -40 || center.x > viewport_.width + 40 ||
        center.y > viewport_.height + 40)
        return;
    rectangle({center.x - 12, center.y + 7}, 24, 7, {0.04F, 0.10F, 0.13F});
    const auto cosine = std::cos(angle), sine = std::sin(angle);
    const auto rotate = [&](double x, double y) -> math::ScreenPosition {
        return {center.x + x * cosine - y * sine, center.y + x * sine + y * cosine};
    };
    constexpr double pixel_size = 1.4;
    for (const auto &pixel : sprite.pixels) {
        const auto x = (pixel.x - static_cast<double>(sprite.width) / 2) * pixel_size,
                   y = (pixel.y - static_cast<double>(sprite.height) / 2) * pixel_size;
        const auto a = rotate(x, y), b = rotate(x + pixel_size, y),
                   c = rotate(x + pixel_size, y + pixel_size), d = rotate(x, y + pixel_size);
        const auto color = remote ? Color{pixel.color.b, pixel.color.r * 0.8F, pixel.color.g} : pixel.color;
        triangle(a, b, c, color);
        triangle(a, c, d, color);
    }
    triangle(rotate(24, 0), rotate(16, -4), rotate(16, 4),
             remote ? Color{0.9F, 0.65F, 1} : Color{1, 0.78F, 0.32F});
}
void Renderer::diagnostic_text(std::string_view text) {
    if (text.empty())
        return;
    const auto columns = std::max(1.0, std::floor((viewport_.width - 36) / 12));
    const auto lines = std::ceil(static_cast<double>(std::min<std::size_t>(100, text.size())) / columns);
    rectangle({10, 10}, std::min(viewport_.width - 20, static_cast<double>(text.size()) * 12 + 16),
              lines * 18 + 10, {0.025F, 0.045F, 0.06F});
    double x = 18, y = 17;
    for (const auto symbol : text.substr(0, 100)) {
        if (x + 10 > viewport_.width - 18) {
            x = 18;
            y += 18;
        }
        for (const auto &glyph : diagnostic_font)
            if (glyph.character == symbol) {
                for (unsigned row = 0; row < 7; ++row)
                    for (unsigned col = 0; col < 5; ++col)
                        if ((glyph.rows[row] & (1U << (4 - col))) != 0)
                            rectangle({x + col * 2, y + row * 2}, 2, 2, {0.76F, 0.93F, 0.87F});
                break;
            }
        x += 12;
    }
}
std::expected<void, std::string> Renderer::draw(const WorldMap &map, const Camera2D &camera,
                                                const SpriteAsset &sprite, const CharacterTransform &player,
                                                double alpha, math::ScreenPosition cursor,
                                                std::span<const contracts::EntityState> remotes,
                                                std::string_view hud) {
    vertices_.clear();
    overflow_ = false;
    viewport_ = camera.viewport();
    const auto top_left = camera.screen_to_world({0, 0});
    // Bound decorative tiles at extreme resolutions; this never changes collision geometry.
    const auto tile =
        map.tile_size() *
        std::max(1.0, std::ceil(std::max(viewport_.width, viewport_.height) / (map.tile_size() * 64)));
    const auto left = std::max(0, static_cast<int>(std::floor(top_left.x / tile)));
    const auto top = std::max(0, static_cast<int>(std::floor(top_left.y / tile)));
    const auto right =
        static_cast<int>(std::ceil(std::min(map.width(), top_left.x + viewport_.width) / tile));
    const auto bottom =
        static_cast<int>(std::ceil(std::min(map.height(), top_left.y + viewport_.height) / tile));
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x) {
            const Color color = (x + y) % 2 == 0 ? Color{0.12F, 0.22F, 0.24F} : Color{0.13F, 0.24F, 0.25F};
            world_rectangle(camera,
                            {x * tile, y * tile, std::min(tile, map.width() - x * tile),
                             std::min(tile, map.height() - y * tile)},
                            color);
            if ((x * 13 + y * 7) % 11 == 0)
                world_rectangle(camera, {x * tile + 12, y * tile + 12, 3, 3}, {0.23F, 0.36F, 0.31F});
        }
    for (const auto &o : map.obstacles()) {
        world_rectangle(camera, {o.x + 6, o.y + 8, o.width, o.height}, {0.055F, 0.11F, 0.14F});
        world_rectangle(camera, o, {0.27F, 0.36F, 0.43F});
        world_rectangle(camera, {o.x + 3, o.y + 3, o.width - 6, 6}, {0.43F, 0.53F, 0.58F});
        world_rectangle(camera, {o.x + 3, o.y + o.height - 8, o.width - 6, 5}, {0.17F, 0.24F, 0.31F});
    }
    for (const auto &remote : remotes)
        character(camera, sprite, remote.position, remote.angle, true);
    character(camera, sprite, math::interpolate(player.previous, player.position, alpha), player.angle,
              false);
    const Color reticle{0.75F, 0.91F, 0.87F};
    rectangle({cursor.x - 7, cursor.y - 1}, 4, 2, reticle);
    rectangle({cursor.x + 3, cursor.y - 1}, 4, 2, reticle);
    rectangle({cursor.x - 1, cursor.y - 7}, 2, 4, reticle);
    rectangle({cursor.x - 1, cursor.y + 3}, 2, 4, reticle);
    diagnostic_text(hud);
    if (overflow_)
        return std::unexpected("scene exceeds bounded vertex capacity");

    auto *device = device_.get();
    auto *commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
        return std::unexpected(gpu_error("SDL_AcquireGPUCommandBuffer"));
    SDL_GPUTexture *target = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window_, &target, nullptr, nullptr)) {
        const auto error = gpu_error("acquire swapchain");
        SDL_CancelGPUCommandBuffer(commands);
        return std::unexpected(error);
    }
    if (!target) {
        if (!SDL_SubmitGPUCommandBuffer(commands))
            return std::unexpected(gpu_error("submit minimized frame"));
        SDL_Delay(10);
        return {};
    }
    const auto bytes = static_cast<Uint32>(vertices_.size() * sizeof(Vertex));
    auto *mapped = SDL_MapGPUTransferBuffer(device, transfer_.get(), true);
    if (!mapped) {
        const auto error = gpu_error("SDL_MapGPUTransferBuffer");
        SDL_SubmitGPUCommandBuffer(commands);
        return std::unexpected(error);
    }
    std::memcpy(mapped, vertices_.data(), bytes);
    SDL_UnmapGPUTransferBuffer(device, transfer_.get());
    auto *copy = SDL_BeginGPUCopyPass(commands);
    if (!copy) {
        const auto error = gpu_error("SDL_BeginGPUCopyPass");
        SDL_SubmitGPUCommandBuffer(commands);
        return std::unexpected(error);
    }
    SDL_GPUTransferBufferLocation source{transfer_.get(), 0};
    SDL_GPUBufferRegion destination{vertices_buffer_.get(), 0, bytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUColorTargetInfo color{};
    color.texture = target;
    color.clear_color = {0.035F, 0.065F, 0.09F, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    auto *pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
    if (!pass) {
        const auto error = gpu_error("SDL_BeginGPURenderPass");
        SDL_SubmitGPUCommandBuffer(commands);
        return std::unexpected(error);
    }
    SDL_BindGPUGraphicsPipeline(pass, pipeline_.get());
    const std::array<float, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    SDL_PushGPUVertexUniformData(commands, 0, identity.data(), sizeof(identity));
    SDL_GPUBufferBinding binding{vertices_buffer_.get(), 0};
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass, static_cast<Uint32>(vertices_.size()), 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    if (!SDL_SubmitGPUCommandBuffer(commands))
        return std::unexpected(gpu_error("SDL_SubmitGPUCommandBuffer"));
    return {};
}
} // namespace tchips::client
