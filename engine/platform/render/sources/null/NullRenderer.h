/*
Module: engine/platform/render
File: engine/platform/render/sources/null/NullRenderer.h

Responsibility:
- Headless IRenderer backend (Rule 3): every method succeeds, handles are
  valid-but-inert, save_screenshot writes nothing and returns false (frozen
  contract's stated null semantics).

Key items:
- NullRenderer: full IRenderer implementation without a GPU.

Dependencies:
- Uses: IRenderer interface only.
- Used by: headless tests, CI tour smoke runs, engine/app (null mode).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Behavior must satisfy every IRenderer postcondition; a feature that crashes
  under this backend is a bug (Rule 3).
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"

namespace dfn::platform {

class NullRenderer final : public IRenderer {
public:
    [[nodiscard]] bool init(const RendererInitParams& params) override;
    void shutdown() override;
    void resize(uint32_t framebuffer_width, uint32_t framebuffer_height) override;

    void begin_frame(const glm::mat4& view, const glm::mat4& proj) override;
    void end_frame() override;
    void set_environment(const RenderEnvironment& env) override; // accepted, ignored

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

    using IRenderer::submit;
    void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                TextureHandle texture, const DrawParams& params) override;
    using IRenderer::submit_skinned;
    void submit_skinned(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                        std::span<const glm::mat4> bone_palette, TextureHandle texture,
                        const DrawParams& params) override;
    void debug_line(const glm::vec3& from, const glm::vec3& to,
                    uint32_t color_rgba) override;

    bool save_screenshot(const std::string& path) override; // always false
    void reload_shaders() override;

    // В28 debug/editor hooks: inert but valid (Rule 3). Wireframe is a no-op;
    // stats and the pick stay zeroed — a headless run draws no pixels to
    // introspect.
    void set_wireframe(bool enabled) override;
    void set_debug_lines(bool /*enabled*/) override {} // nothing draws here
    void set_present_rect_norm(float, float, float, float) override {} // ничего не выводится
    [[nodiscard]] const RenderFrameStats& frame_stats() const override;
    [[nodiscard]] const RenderPick& center_pick() const override;

    // Introspection for tests (backend-local, not part of IRenderer).
    [[nodiscard]] uint32_t live_meshes() const { return live_meshes_; }
    [[nodiscard]] uint32_t live_textures() const { return live_textures_; }
    [[nodiscard]] uint32_t frame_submits() const { return frame_submits_; }
    /// How many bones the LAST skinned submit carried. A headless run cannot
    /// look at pixels, so this is the only thing a test can ask about a
    /// palette -- and it is exactly the thing that goes wrong (an empty
    /// palette draws a bind-pose statue and looks like "skinning is broken").
    [[nodiscard]] uint32_t last_palette_bones() const { return last_palette_bones_; }
    [[nodiscard]] uint32_t skinned_submits() const { return skinned_submits_; }

private:
    uint32_t next_id_ = 1; // shared counter; 0 stays the invalid id
    uint32_t live_meshes_ = 0;
    uint32_t live_textures_ = 0;
    uint32_t frame_submits_ = 0;
    uint32_t skinned_submits_ = 0;
    uint32_t last_palette_bones_ = 0;
    RenderFrameStats frame_stats_{}; // always zero (Rule 3: inert but valid)
    RenderPick pick_{};              // always "no hit"
};

} // namespace dfn::platform
