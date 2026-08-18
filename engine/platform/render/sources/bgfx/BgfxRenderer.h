/*
Created: 09:08:2026 - 00:45:00
Last updated: 18:08:2026 - 12:51:47
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
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 10:58:00: Stage 3 — set_environment (contract sync 10:48), sky
  pass, palette post (Q9b), water transparency, point-sampled textures.
- 09:08:2026 - 14:11:37: Dynamic sun shadows (в1): shadow view 0, view layout
  renumbered.
- 09:08:2026 - 21:02:17: DrawParams sync: submit takes per-draw params (fade
  drives the screen-door dither, highlight reserved for sim's hover).
- 14:08:2026 - 16:35:53: В28 debug/editor hooks: set_wireframe / frame_stats /
  center_pick overrides for the new IRenderer contract (definitions in
  BgfxRendererFrame.cpp; accumulation in BgfxRendererSubmit.cpp).
- 17:08:2026 - 18:29:30: set_debug_lines.
- 17:08:2026 - 19:17:13: native_texture_handle — объявление переопределения нового добавленного метода IRenderer.
- 18:08:2026 - 12:51:47: set_present_rect_norm — объявление нового пункта контракта IRenderer.
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

    // The five-argument form is the contract's pure virtual since the
    // DrawParams sync; the four-argument convenience overload lives in
    // IRenderer and forwards here with defaults.
    using IRenderer::submit;
    void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                TextureHandle texture, const DrawParams& params) override;
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
