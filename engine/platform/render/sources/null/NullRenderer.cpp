/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 21:02:17
Module: engine/platform/render
File: engine/platform/render/sources/null/NullRenderer.cpp

Responsibility:
- NullRenderer implementation + create_null_renderer factory.

Key items:
- NullRenderer methods; create_null_renderer().

Dependencies:
- Uses: NullRenderer.h, CreateNullRenderer.h.
- Used by: dfn_platform_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 10:59:00: Stage 3 — set_environment no-op.
- 09:08:2026 - 21:02:17: DrawParams sync: submit signature; params ignored.
*/

#include "engine/platform/render/sources/null/NullRenderer.h"

#include "engine/platform/render/sources/null/CreateNullRenderer.h"

#include <memory>

namespace dfn::platform {

bool NullRenderer::init(const RendererInitParams&) {
    return true; // accepts any params, including a null native handle
}

void NullRenderer::shutdown() {}

void NullRenderer::resize(uint32_t, uint32_t) {}

void NullRenderer::begin_frame(const glm::mat4&, const glm::mat4&) {
    frame_submits_ = 0;
}

void NullRenderer::end_frame() {}

void NullRenderer::set_environment(const RenderEnvironment&) {
    // Accepted and ignored (contract sync 09:08:2026 - 10:48).
}

MeshHandle NullRenderer::create_mesh(std::span<const Vertex>, std::span<const uint32_t>) {
    ++live_meshes_;
    return MeshHandle{next_id_++};
}

void NullRenderer::destroy_mesh(MeshHandle mesh) {
    if (mesh.valid() && live_meshes_ > 0) {
        --live_meshes_;
    }
}

TextureHandle NullRenderer::create_texture(uint32_t, uint32_t, TextureFormat,
                                           std::span<const uint8_t>) {
    ++live_textures_;
    return TextureHandle{next_id_++};
}

void NullRenderer::destroy_texture(TextureHandle texture) {
    if (texture.valid() && live_textures_ > 0) {
        --live_textures_;
    }
}

ProgramHandle NullRenderer::load_program(std::string_view) {
    return ProgramHandle{next_id_++}; // every logical name "loads" (Rule 3)
}

void NullRenderer::destroy_program(ProgramHandle) {}

void NullRenderer::submit(MeshHandle, ProgramHandle, const glm::mat4&, TextureHandle,
                          const DrawParams&) {
    // DrawParams accepted and ignored: a headless run has no pixels to dither.
    ++frame_submits_;
}

void NullRenderer::debug_line(const glm::vec3&, const glm::vec3&, uint32_t) {}

bool NullRenderer::save_screenshot(const std::string&) {
    return false; // frozen contract: null writes nothing and returns false
}

void NullRenderer::reload_shaders() {}

std::unique_ptr<IRenderer> create_null_renderer() {
    return std::make_unique<NullRenderer>();
}

} // namespace dfn::platform
