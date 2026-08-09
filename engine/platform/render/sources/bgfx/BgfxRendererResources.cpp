/*
Created: 10:08:2026 - 01:47:53
Last updated: 10:08:2026 - 01:47:53
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxRendererResources.cpp

Responsibility:
- Handle bookkeeping of the bgfx backend: mesh / texture / program create and
  destroy, including the GPU buffer budget accounting (the one-mesh-per-puddle
  crash's legacy). One of four translation units over BgfxRendererImpl.h
  (Rule 21 split); lifecycle is BgfxRenderer.cpp, frame path
  BgfxRendererFrame.cpp, draws BgfxRendererSubmit.cpp.

Key items:
- BgfxRenderer::create_mesh / destroy_mesh / create_texture / destroy_texture /
  load_program / destroy_program.
- TRANSPARENT_PROGRAMS / CUTOUT_PROGRAMS (logical name -> render state).

Dependencies:
- Uses: BgfxRendererImpl.h, bgfx.
- Used by: dfn_platform_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER STORE THE RESULT OF A FAILED CREATE (crash fix 09:08:2026): bgfx
  returns BGFX_INVALID_HANDLE on pool exhaustion and destroying it in release
  walks off an array at shutdown. Every create is validated, every destroy
  guarded by bgfx::isValid.
*/
/*
UPD:
- 10:08:2026 - 01:47:53: Created in the Rule 21 split of BgfxRenderer.cpp.
  Handle bookkeeping moved verbatim; no behaviour change.
*/

#include "engine/platform/render/sources/bgfx/BgfxRendererImpl.h"

#include <algorithm>
#include <cstdio>

namespace dfn::platform {

namespace {

// Logical program names rendered with alpha blend + read-only depth (the
// name -> render-state convention acknowledged at the stage-3 sync).
constexpr const char* TRANSPARENT_PROGRAMS[] = {"water", "overlay"};
// Alpha-CUTOUT programs: opaque state (depth write, no sorting) but their
// shadow depth must come from the mask, not from the card's rectangle.
constexpr const char* CUTOUT_PROGRAMS[] = {"foliage"};

} // namespace

MeshHandle BgfxRenderer::create_mesh(std::span<const Vertex> vertices,
                                     std::span<const uint32_t> indices) {
    Impl& im = *impl_;
    if (!im.initialized || vertices.empty() || indices.empty()) {
        return {};
    }
    const bgfx::Memory* vmem = bgfx::copy(
        vertices.data(), static_cast<uint32_t>(vertices.size_bytes()));
    const bgfx::Memory* imem = bgfx::copy(
        indices.data(), static_cast<uint32_t>(indices.size_bytes()));
    Impl::MeshRes res{
        bgfx::createVertexBuffer(vmem, im.mesh_layout),
        bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32),
        glm::vec3(0.0f),
        0.0f,
    };
    // Bounding sphere from the AABB centre: one pass, no iterative refinement.
    // Slightly loose for long thin meshes, which is the safe direction for a
    // cull.
    {
        glm::vec3 lo = vertices[0].position;
        glm::vec3 hi = lo;
        for (const Vertex& v : vertices) {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }
        res.center = (lo + hi) * 0.5f;
        res.radius = glm::length(hi - res.center);
    }
    // NEVER STORE THE RESULT OF A FAILED CREATE. bgfx returns
    // BGFX_INVALID_HANDLE (idx 0xFFFF) when its handle pool is exhausted; that
    // value used to be stored in the mesh record and handed back as a VALID
    // engine handle, because MeshHandle's validity is "id != 0" and the id is
    // ours, not bgfx's. The consequence was a SEGFAULT AT EXIT inside
    // bgfx::freeAllHandles -> VertexLayoutRef::release, which indexes
    // m_vertexBufferRef[0xFFFF] and walks off the array. Nothing between the
    // failed create and the crash mentioned either.
    if (!bgfx::isValid(res.vb) || !bgfx::isValid(res.ib)) {
        if (bgfx::isValid(res.vb)) {
            bgfx::destroy(res.vb);
        }
        if (bgfx::isValid(res.ib)) {
            bgfx::destroy(res.ib);
        }
        std::fprintf(stderr,
                     "[render] GPU BUFFER CREATE FAILED — bgfx handle pool "
                     "exhausted (limit %d vertex / %d index buffers). Live "
                     "meshes %zu, peak %u, created %llu, destroyed %llu. This "
                     "mesh DRAWS NOTHING; the caller must treat the invalid "
                     "handle as a failure, not as an empty mesh.\n",
                     BGFX_MESH_HANDLE_BUDGET, BGFX_MESH_HANDLE_BUDGET,
                     im.meshes.size(), im.peak_meshes,
                     static_cast<unsigned long long>(im.meshes_created),
                     static_cast<unsigned long long>(im.meshes_destroyed));
        return {};
    }
    const uint32_t id = im.next_id++;
    im.meshes.emplace(id, res);
    ++im.meshes_created;
    im.peak_meshes = std::max(im.peak_meshes,
                              static_cast<uint32_t>(im.meshes.size()));
    // One warning at three quarters of the budget, so the cliff is reported
    // BEFORE it is walked off rather than after.
    if (!im.mesh_budget_warned
        && im.meshes.size() > static_cast<size_t>(BGFX_MESH_HANDLE_BUDGET) * 3 / 4) {
        im.mesh_budget_warned = true;
        std::fprintf(stderr,
                     "[render] GPU BUFFER BUDGET WARNING: %zu live meshes of %d. "
                     "Past the limit every further create fails silently and the "
                     "world stops drawing. Cost must scale with SCREEN area, not "
                     "world area.\n",
                     im.meshes.size(), BGFX_MESH_HANDLE_BUDGET);
    }
    return MeshHandle{id};
}

void BgfxRenderer::destroy_mesh(MeshHandle mesh) {
    Impl& im = *impl_;
    const auto it = im.meshes.find(mesh.id);
    if (it != im.meshes.end()) {
        // Validity checked even though create_mesh now refuses to store an
        // invalid handle: bgfx's own destroy() only asserts in debug builds, so
        // in release an invalid handle here is a crash three call frames later
        // with nothing on the stack to say where it came from.
        if (bgfx::isValid(it->second.vb)) {
            bgfx::destroy(it->second.vb);
        }
        if (bgfx::isValid(it->second.ib)) {
            bgfx::destroy(it->second.ib);
        }
        im.meshes.erase(it);
        ++im.meshes_destroyed;
    }
}

TextureHandle BgfxRenderer::create_texture(uint32_t width, uint32_t height,
                                           TextureFormat format,
                                           std::span<const uint8_t> pixels) {
    Impl& im = *impl_;
    if (!im.initialized || width == 0 || height == 0 || pixels.empty()) {
        return {};
    }
    const bgfx::TextureFormat::Enum fmt = format == TextureFormat::RGBA8
                                              ? bgfx::TextureFormat::RGBA8
                                              : bgfx::TextureFormat::R8;
    const bgfx::Memory* mem =
        bgfx::copy(pixels.data(), static_cast<uint32_t>(pixels.size_bytes()));
    const bgfx::TextureHandle tex = bgfx::createTexture2D(
        static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1, fmt,
        BGFX_TEXTURE_NONE, mem);
    if (!bgfx::isValid(tex)) {
        return {};
    }
    const uint32_t id = im.next_id++;
    im.textures.emplace(id, tex);
    return TextureHandle{id};
}

void BgfxRenderer::destroy_texture(TextureHandle texture) {
    Impl& im = *impl_;
    const auto it = im.textures.find(texture.id);
    if (it != im.textures.end()) {
        bgfx::destroy(it->second);
        im.textures.erase(it);
    }
}

ProgramHandle BgfxRenderer::load_program(std::string_view name) {
    Impl& im = *impl_;
    if (!im.initialized) {
        return {};
    }
    const bgfx::ProgramHandle prog = im.make_program(std::string(name).c_str());
    if (!bgfx::isValid(prog)) {
        return {};
    }
    const uint32_t id = im.next_id++;
    im.programs.emplace(id, prog);
    for (const char* transparent_name : TRANSPARENT_PROGRAMS) {
        if (name == transparent_name) {
            im.transparent.emplace(id, true);
        }
    }
    for (const char* cutout_name : CUTOUT_PROGRAMS) {
        if (name == cutout_name) {
            im.cutout.emplace(id, true);
        }
    }
    return ProgramHandle{id};
}

void BgfxRenderer::destroy_program(ProgramHandle program) {
    Impl& im = *impl_;
    const auto it = im.programs.find(program.id);
    if (it != im.programs.end()) {
        bgfx::destroy(it->second);
        im.programs.erase(it);
    }
    im.transparent.erase(program.id);
    im.cutout.erase(program.id);
}

} // namespace dfn::platform
