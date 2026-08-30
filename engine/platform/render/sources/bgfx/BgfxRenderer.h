/*
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
- View layout: 0 = sun shadow map (depth only, opaque casters), 1 = scene into
  the internal target, 2 = backbuffer clear (letterbox black), 3 =
  point-sampled integer-upscale quad.
- save_screenshot schedules the capture into the NEXT end_frame (bgfx captures
  during frame processing); the PNG lands on disk during that frame. The Tour
  renders flush frames after scheduling, so callers need no extra handling.
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
    [[nodiscard]] MeshHandle create_skinned_mesh(std::span<const SkinnedVertex> vertices,
                                                 std::span<const uint32_t> indices) override;

    [[nodiscard]] TextureHandle create_texture(uint32_t width, uint32_t height,
                                               TextureFormat format,
                                               std::span<const uint8_t> pixels) override;
    void destroy_texture(TextureHandle texture) override;

    [[nodiscard]] ProgramHandle load_program(std::string_view name) override;
    void destroy_program(ProgramHandle program) override;

    // The five-argument form is the contract's pure virtual since the
    // DrawParams sync; the four-argument convenience overload lives in
    // IRenderer and forwards here with defaults.
    using IRenderer::submit;
    void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                TextureHandle texture, const DrawParams& params) override;
    using IRenderer::submit_skinned;
    void submit_skinned(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                        std::span<const glm::mat4> bone_palette, TextureHandle texture,
                        const DrawParams& params) override;
    void debug_line(const glm::vec3& from, const glm::vec3& to,
                    uint32_t color_rgba) override;

    bool save_screenshot(const std::string& path) override;
    void reload_shaders() override; // debug no-op this stage (embedded shaders)

    void set_wireframe(bool enabled) override;
    void set_debug_lines(bool enabled) override;
    void set_present_rect_norm(float x, float y, float w, float h) override;
    [[nodiscard]] const RenderFrameStats& frame_stats() const override;
    [[nodiscard]] const RenderPick& center_pick() const override;
    [[nodiscard]] uint32_t native_texture_handle(TextureHandle handle) const override;

private:
    struct Impl; // all bgfx types live here (.cpp only)
    std::unique_ptr<Impl> impl_;
};

} // namespace dfn::platform
