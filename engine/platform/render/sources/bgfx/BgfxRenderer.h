/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 10:58:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxRenderer.h

Responsibility:
- bgfx backend of the frozen IRenderer contract: Metal on macOS, low-res
  internal target with integer upscale (Q9), embedded compiled shaders,
  screenshot capture, one-frame debug lines.

Key items:
- BgfxRenderer: IRenderer implementation; pimpl keeps bgfx types out of this
  header (only the .cpp includes bgfx — Rule 1 hygiene even inside the backend).

Dependencies:
- Uses: IRenderer interface; bgfx/bimg/bx in the .cpp only.
- Used by: engine/app via create_bgfx_renderer.

Notes:
- Single-threaded bgfx (renderFrame before init): the whole engine loop is one
  thread this stage.
- View layout: 0 = scene into the internal target, 1 = backbuffer clear
  (letterbox black), 2 = point-sampled integer-upscale quad.
- save_screenshot schedules the capture into the NEXT end_frame (bgfx captures
  during frame processing); the PNG lands on disk during that frame. The Tour
  renders flush frames after scheduling, so callers need no extra handling.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 10:58:00: Stage 3 — set_environment (contract sync 10:48), sky
  pass, palette post (Q9b), water transparency, point-sampled textures.
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"

#include <memory>

namespace dfn::platform {

class BgfxRenderer final : public IRenderer {
public:
    BgfxRenderer();
    ~BgfxRenderer() override;
    BgfxRenderer(const BgfxRenderer&) = delete;
    BgfxRenderer& operator=(const BgfxRenderer&) = delete;

    [[nodiscard]] bool init(const RendererInitParams& params) override;
    void shutdown() override;
    void resize(uint32_t framebuffer_width, uint32_t framebuffer_height) override;

    void begin_frame(const glm::mat4& view, const glm::mat4& proj) override;
    void end_frame() override;
    void set_environment(const RenderEnvironment& env) override;

    [[nodiscard]] MeshHandle create_mesh(std::span<const Vertex> vertices,
                                         std::span<const uint32_t> indices) override;
    void destroy_mesh(MeshHandle mesh) override;

    [[nodiscard]] TextureHandle create_texture(uint32_t width, uint32_t height,
                                               TextureFormat format,
                                               std::span<const uint8_t> pixels) override;
    void destroy_texture(TextureHandle texture) override;

    [[nodiscard]] ProgramHandle load_program(std::string_view name) override;
    void destroy_program(ProgramHandle program) override;

    void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                TextureHandle texture = {}) override;
    void debug_line(const glm::vec3& from, const glm::vec3& to,
                    uint32_t color_rgba) override;

    bool save_screenshot(const std::string& path) override;
    void reload_shaders() override; // debug no-op this stage (embedded shaders)

private:
    struct Impl; // all bgfx types live here (.cpp only)
    std::unique_ptr<Impl> impl_;
};

} // namespace dfn::platform
