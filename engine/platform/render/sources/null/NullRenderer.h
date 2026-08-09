/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 10:59:00
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
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 10:59:00: Stage 3 — set_environment (accepted-and-ignored per
  the contract sync 10:48).
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

    bool save_screenshot(const std::string& path) override; // always false
    void reload_shaders() override;

    // Introspection for tests (backend-local, not part of IRenderer).
    [[nodiscard]] uint32_t live_meshes() const { return live_meshes_; }
    [[nodiscard]] uint32_t live_textures() const { return live_textures_; }
    [[nodiscard]] uint32_t frame_submits() const { return frame_submits_; }

private:
    uint32_t next_id_ = 1; // shared counter; 0 stays the invalid id
    uint32_t live_meshes_ = 0;
    uint32_t live_textures_ = 0;
    uint32_t frame_submits_ = 0;
};

} // namespace dfn::platform
