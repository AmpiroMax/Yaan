/*
Created: 09:08:2026 - 00:06:00
Last updated: 09:08:2026 - 00:06:00
Module: engine/platform/render
File: engine/platform/render/interfaces/IRenderer.h

Responsibility:
- The platform rendering contract (Rule 0). Everything the engine and game may ask
  of a renderer goes through this interface; bgfx lives only behind it.

Key items:
- IRenderer: init/shutdown, frame, resource creation, submission, screenshot capture.
- RendererInitParams: window handle + framebuffer size + low-res internal target (Q9).
- MeshHandle / TextureHandle / ProgramHandle: opaque POD handles (0 = invalid).
- Vertex: the fixed vertex layout for stage 2 (position, normal, uv, color).

Dependencies:
- Uses: C++ stdlib, glm (project-wide math vocabulary, Rule 2). Nothing else.
- Used by: engine/render (primary consumer), engine/editor, engine/app, tests
  (null backend), the screenshot tour.

Notes:
- FROZEN CONTRACT (Rule 26): authored by the lead (Q55). Changes only through a
  group sync — message the lead, do not edit.
- Deliberately thin: materials, culling, batching, lighting and the camera live in
  engine/render on top of this. The backend owns the low-res internal target and
  the integer upscale (Q9); consumers never see it.
- Skinned meshes (ozz skinning matrices) are a stage-3 extension; the contract will
  gain create_skinned_mesh + submit overload at the next sync, not ad-hoc.
- Null backend: all methods succeed, handles are valid-but-inert, save_screenshot
  writes nothing and returns false.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add bgfx types, includes, or assumptions to this header.
*/
/*
UPD:
- 09:08:2026 - 00:06:00: Initial frozen contract for stage 2 (skeleton walk).
*/

#pragma once

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <span>
#include <string>
#include <string_view>

namespace dfn::platform {

// Opaque resource handles. id == 0 means "invalid / none".
struct MeshHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct TextureHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};
struct ProgramHandle {
    uint32_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};

enum class TextureFormat : uint8_t {
    RGBA8,   // 4x uint8, sRGB sampling decided by the backend
    R8,      // single channel (masks, heightmap debug views)
};

// Fixed vertex layout for stage 2. Extended (skinning) variants arrive via a
// contract sync, not by mutating this struct.
struct Vertex {
    glm::vec3 position;   // meters, world or model space depending on submit
    glm::vec3 normal;     // unit
    glm::vec2 uv;         // 0..1
    uint32_t color_rgba;  // 0xAABBGGRR packed, white = 0xFFFFFFFF
};

struct RendererInitParams {
    void* native_window_handle = nullptr; // backend-specific (from IWindow)
    uint32_t framebuffer_width = 0;       // real window framebuffer, pixels
    uint32_t framebuffer_height = 0;
    uint32_t internal_width = 0;          // low-res internal target (Q9); the
    uint32_t internal_height = 0;         // backend integer-upscales to the framebuffer
    bool vsync = true;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Lifecycle ----------------------------------------------------------------
    [[nodiscard]] virtual bool init(const RendererInitParams& params) = 0;
    virtual void shutdown() = 0;
    virtual void resize(uint32_t framebuffer_width, uint32_t framebuffer_height) = 0;

    // Frame --------------------------------------------------------------------
    // view/proj are the interpolated camera matrices for this render frame (Rule 12:
    // simulation is fixed-step; interpolation happens in engine/render, not here).
    virtual void begin_frame(const glm::mat4& view, const glm::mat4& proj) = 0;
    virtual void end_frame() = 0;

    // Resources ----------------------------------------------------------------
    [[nodiscard]] virtual MeshHandle create_mesh(std::span<const Vertex> vertices,
                                                 std::span<const uint32_t> indices) = 0;
    virtual void destroy_mesh(MeshHandle mesh) = 0;

    [[nodiscard]] virtual TextureHandle create_texture(uint32_t width, uint32_t height,
                                                       TextureFormat format,
                                                       std::span<const uint8_t> pixels) = 0;
    virtual void destroy_texture(TextureHandle texture) = 0;

    // Loads a compiled shader pair by logical name (e.g. "terrain", "unlit").
    // Compiled artifacts are produced by the CMake shaderc step (Q50); the backend
    // resolves the per-API binary. Names, not paths — consumers never know the layout.
    [[nodiscard]] virtual ProgramHandle load_program(std::string_view name) = 0;
    virtual void destroy_program(ProgramHandle program) = 0;

    // Submission ---------------------------------------------------------------
    // Queues one draw for the current frame. texture may be invalid (untextured).
    virtual void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                        TextureHandle texture = {}) = 0;

    // Immediate debug line in world space, lives one frame. No-op in release builds.
    virtual void debug_line(const glm::vec3& from, const glm::vec3& to,
                            uint32_t color_rgba) = 0;

    // Tooling ------------------------------------------------------------------
    // Captures the final upscaled framebuffer to a PNG after the current end_frame.
    // Backbone of the screenshot tour (Rule 27). Null backend returns false.
    virtual bool save_screenshot(const std::string& path) = 0;

    // Hot-reloads shader programs from the compiled artifacts on disk (Q50).
    // Debug-build convenience; a backend may implement it as a no-op.
    virtual void reload_shaders() = 0;
};

} // namespace dfn::platform
