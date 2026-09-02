/*
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
- TRANSPARENT_PROGRAMS / CUTOUT_PROGRAMS / NON_CASTING_PROGRAMS (logical
  name -> render state).

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

#include "engine/platform/render/sources/bgfx/BgfxRendererImpl.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace dfn::platform {

namespace {

// Logical program names rendered with alpha blend + read-only depth (the
// name -> render-state convention acknowledged at the stage-3 sync).
constexpr const char* TRANSPARENT_PROGRAMS[] = {"water", "overlay"};
// Alpha-CUTOUT programs: opaque state (depth write, no sorting) but their
// shadow depth must come from the mask, not from the card's rectangle.
// "skinned_cutout" (волна «части персонажа»): те же шейдеры, что у "skinned",
// но состояние выреза — покрытие по альфе на MSAA и каст тени с маской.
constexpr const char* CUTOUT_PROGRAMS[] = {"foliage", "skinned_cutout"};
// Programs that are drawn but must NOT write the sun shadow map.
//
// The path ribbon (§8.1) is the first, and the reason is the feather, not the
// cost. Its outer band is a DISCARD — an ordered dither that thins the surface
// out into the ground — while the cheap depth-only caster program has no
// discard at all. Left casting, a path whose visible edge dissolves would lay a
// hard-edged full-width dark strip beside itself at low sun: the shadow of a
// surface that is not there. It lies on the ground it would darken, so it has
// nothing to cast in the first place.
constexpr const char* NON_CASTING_PROGRAMS[] = {"path"};

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
    // Triangle count for the В28 frame-stats / pick hooks. The scene meshes are
    // indexed triangle lists, so index_count / 3 is exact.
    res.tri_count = static_cast<uint32_t>(indices.size() / 3);
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

MeshHandle BgfxRenderer::create_skinned_mesh(std::span<const SkinnedVertex> vertices,
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
        bgfx::createVertexBuffer(vmem, im.skinned_layout),
        bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32),
        glm::vec3(0.0f),
        0.0f,
    };
    // THE SPHERE IS THE BIND POSE'S, AND THAT IS A KNOWN LOOSENESS, NAMED HERE
    // RATHER THAN DISCOVERED LATER. Culling, the pick ray and both shadow
    // culls read this sphere through `transform`; a posed body reaches outside
    // its bind-pose sphere (an arm raised over the head is the easy case), so
    // the radius is padded by half again. Padding is the SAFE direction for
    // every one of those consumers -- a slightly too big sphere costs a draw,
    // a slightly too small one deletes a limb from a shadow.
    {
        glm::vec3 lo = vertices[0].position;
        glm::vec3 hi = lo;
        for (const SkinnedVertex& v : vertices) {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }
        res.center = (lo + hi) * 0.5f;
        res.radius = glm::length(hi - res.center) * 1.5f;
    }
    res.tri_count = static_cast<uint32_t>(indices.size() / 3);
    if (!bgfx::isValid(res.vb) || !bgfx::isValid(res.ib)) {
        if (bgfx::isValid(res.vb)) {
            bgfx::destroy(res.vb);
        }
        if (bgfx::isValid(res.ib)) {
            bgfx::destroy(res.ib);
        }
        std::fprintf(stderr, "[render] skinned mesh upload FAILED (bgfx handle "
                             "pool exhausted); it draws NOTHING\n");
        return {};
    }
    const uint32_t id = im.next_id++;
    im.meshes.emplace(id, res);
    im.skinned_meshes.insert(id);
    ++im.meshes_created;
    im.peak_meshes = std::max(im.peak_meshes,
                              static_cast<uint32_t>(im.meshes.size()));
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
        im.skinned_meshes.erase(mesh.id);
        ++im.meshes_destroyed;
    }
}

/// ONE MIP LEVEL FROM THE ONE ABOVE IT, 2x2 box. `channels` is 4 (RGBA8) or
/// 1 (R8). For RGBA the RGB average is ALPHA-WEIGHTED: a straight box filter
/// pulls the colour of fully transparent texels (usually black) into a leaf's
/// colour and a distant crown darkens as it recedes -- a look change nobody
/// asked for. On an opaque sheet (alpha 255 everywhere, a skin) the weighting
/// is the identity, so one builder serves both.
static void downsample_level(const std::vector<uint8_t>& src, uint32_t sw, uint32_t sh,
                      uint32_t channels, std::vector<uint8_t>& dst, uint32_t& dw,
                      uint32_t& dh) {
    dw = sw > 1 ? sw / 2 : 1;
    dh = sh > 1 ? sh / 2 : 1;
    dst.assign(static_cast<std::size_t>(dw) * dh * channels, 0);
    for (uint32_t y = 0; y < dh; ++y) {
        for (uint32_t x = 0; x < dw; ++x) {
            const uint32_t x0 = x * 2;
            const uint32_t y0 = y * 2;
            const uint32_t x1 = sw > 1 ? x0 + 1 : x0;
            const uint32_t y1 = sh > 1 ? y0 + 1 : y0;
            const uint32_t xs[2] = {x0, x1};
            const uint32_t ys[2] = {y0, y1};
            const std::size_t d = (static_cast<std::size_t>(y) * dw + x) * channels;
            if (channels == 1) {
                uint32_t sum = 0;
                for (uint32_t yi = 0; yi < 2; ++yi) {
                    for (uint32_t xi = 0; xi < 2; ++xi) {
                        sum += src[static_cast<std::size_t>(ys[yi]) * sw + xs[xi]];
                    }
                }
                dst[d] = static_cast<uint8_t>((sum + 2) / 4);
                continue;
            }
            uint32_t asum = 0;
            uint32_t csum[3] = {0, 0, 0};
            for (uint32_t yi = 0; yi < 2; ++yi) {
                for (uint32_t xi = 0; xi < 2; ++xi) {
                    const std::size_t o =
                        (static_cast<std::size_t>(ys[yi]) * sw + xs[xi]) * 4;
                    const uint32_t a = src[o + 3];
                    asum += a;
                    for (int k = 0; k < 3; ++k) {
                        csum[k] += static_cast<uint32_t>(src[o + k]) * a;
                    }
                }
            }
            for (int k = 0; k < 3; ++k) {
                dst[d + static_cast<std::size_t>(k)] =
                    asum > 0 ? static_cast<uint8_t>((csum[k] + asum / 2) / asum)
                             : src[(static_cast<std::size_t>(y0) * sw + x0) * 4
                                   + static_cast<std::size_t>(k)];
            }
            dst[d + 3] = static_cast<uint8_t>((asum + 2) / 4);
        }
    }
}

TextureHandle BgfxRenderer::create_texture(uint32_t width, uint32_t height,
                                           TextureFormat format,
                                           std::span<const uint8_t> pixels,
                                           const TextureParams& params) {
    Impl& im = *impl_;
    if (!im.initialized || width == 0 || height == 0 || pixels.empty()) {
        return {};
    }
    const bgfx::TextureFormat::Enum fmt = format == TextureFormat::RGBA8
                                              ? bgfx::TextureFormat::RGBA8
                                              : bgfx::TextureFormat::R8;
    const uint32_t channels = format == TextureFormat::RGBA8 ? 4u : 1u;
    const std::size_t level0_bytes = static_cast<std::size_t>(width) * height * channels;
    if (pixels.size_bytes() < level0_bytes) {
        return {};
    }
    // MASK TEXTURES GET A MIP CHAIN; EVERYTHING ELSE STAYS EXACTLY AS IT WAS.
    //
    // The discriminator is the DATA, not a flag on the interface: an RGBA8
    // texture that contains a single pixel with alpha < 255 is a cutout mask
    // (leaf card), and nothing else in this project has one. That matters
    // because the terrain ATLAS must never be mipped — a mip level averages
    // across the cell borders and every cell bleeds into its neighbour, which
    // is the reason the material sampler is point/point/point in the first
    // place. Keeping the test on alpha means the atlas, the bark and the path
    // textures take this change as a no-op and cannot regress.
    //
    // WHY A MASK NEEDS ONE — this is the treeline half of the running-shimmer
    // fix. A leaf mask at 70 m is minified ~30:1, so one screen pixel covers
    // ~900 mask texels and the point sampler picks ONE of them. Move the eye
    // 0.05 m (one 120 fps frame at RUN_SPEED) and it picks a different one:
    // the pixel flips leaf/sky at full contrast. Averaging is not a cosmetic
    // preference here, it is the only way a fraction of a pixel of motion can
    // produce a fraction of a colour change. MSAA cannot reach this at all —
    // the edge is inside the texture fetch, not on a triangle — which is
    // exactly what the measurement said: MSAA 4x took the treeline from
    // 0.095 % to 0.080 % and MSAA 8x took it no further.
    //
    // GATED ON MSAA so that DFN_MSAA=0 is a BIT-EXACT control arm and not
    // merely "the old look, roughly" (Rule 30). Without the gate the mask
    // would carry a mip chain that the single-sample path samples through the
    // old 0.5 cutout — averaged alpha against a hard threshold is the classic
    // distant-canopy dissolve, i.e. the off switch would ship a second,
    // different defect.
    //
    // THE SECOND WAY IN IS THE CALLER'S WORD (TextureParams::mip_chain, skin
    // wave 02.09). A skin is opaque, so the alpha test cannot see it, and it
    // is minified exactly like a mask at distance: a 2K sheet on a body 70 m
    // away is ~40 texels per pixel, and the point sampler picking one of them
    // per frame is the same shimmer the treeline had. Not gated on MSAA:
    // there is no cutout threshold on this path for a mip chain to dissolve
    // through, so the chain is right at any sample count.
    bool has_alpha = false;
    if (im.internal_samples > 1 && fmt == bgfx::TextureFormat::RGBA8) {
        for (std::size_t i = 3; i < level0_bytes; i += 4) {
            if (pixels[i] < 255) {
                has_alpha = true;
                break;
            }
        }
    }
    const bool build_chain = has_alpha || params.mip_chain;

    if (!build_chain) {
        const bgfx::Memory* mem =
            bgfx::copy(pixels.data(), static_cast<uint32_t>(pixels.size_bytes()));
        const bgfx::TextureHandle tex = bgfx::createTexture2D(
            static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
            fmt, BGFX_TEXTURE_NONE, mem);
        if (!bgfx::isValid(tex)) {
            return {};
        }
        const uint32_t id = im.next_id++;
        im.textures.emplace(id, tex);
        return TextureHandle{id};
    }

    const bgfx::TextureHandle tex = bgfx::createTexture2D(
        static_cast<uint16_t>(width), static_cast<uint16_t>(height), true, 1, fmt,
        BGFX_TEXTURE_NONE, nullptr);
    if (!bgfx::isValid(tex)) {
        return {};
    }
    {
        const bgfx::Memory* mem =
            bgfx::copy(pixels.data(), static_cast<uint32_t>(level0_bytes));
        bgfx::updateTexture2D(tex, 0, 0, 0, 0, static_cast<uint16_t>(width),
                              static_cast<uint16_t>(height), mem);
    }
    std::vector<uint8_t> src(pixels.begin(),
                             pixels.begin() + static_cast<std::ptrdiff_t>(level0_bytes));
    std::vector<uint8_t> dst;
    uint32_t sw = width;
    uint32_t sh = height;
    uint8_t level = 1;
    while (sw > 1 || sh > 1) {
        uint32_t dw = 1;
        uint32_t dh = 1;
        downsample_level(src, sw, sh, channels, dst, dw, dh);
        const bgfx::Memory* mem =
            bgfx::copy(dst.data(), static_cast<uint32_t>(dst.size()));
        bgfx::updateTexture2D(tex, 0, level, 0, 0, static_cast<uint16_t>(dw),
                              static_cast<uint16_t>(dh), mem);
        src.swap(dst);
        sw = dw;
        sh = dh;
        ++level;
    }
    const uint32_t id = im.next_id++;
    im.textures.emplace(id, tex);
    if (has_alpha) {
        im.mipped_textures.insert(id);
    }
    if (params.mip_chain) {
        im.filtered_textures.insert(id);
        ++im.filtered_textures_created;
    }
    return TextureHandle{id};
}

void BgfxRenderer::destroy_texture(TextureHandle texture) {
    Impl& im = *impl_;
    const auto it = im.textures.find(texture.id);
    if (it != im.textures.end()) {
        bgfx::destroy(it->second);
        im.textures.erase(it);
        im.mipped_textures.erase(texture.id);
        im.filtered_textures.erase(texture.id);
    }
}

uint32_t BgfxRenderer::native_texture_handle(TextureHandle texture) const {
    // The editor's ImGui bridge asks this so a picture the render system owns
    // (an offscreen thumbnail target) can be shown in a panel. It answers with
    // the bgfx index and nothing else — no ownership, no lifetime, no promise
    // that it stays valid past a destroy_texture.
    const Impl& im = *impl_;
    const auto it = im.textures.find(texture.id);
    if (it == im.textures.end() || !bgfx::isValid(it->second)) {
        return 0xFFFFFFFFu;
    }
    return it->second.idx;
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
    for (const char* non_casting_name : NON_CASTING_PROGRAMS) {
        if (name == non_casting_name) {
            im.non_casting.emplace(id, true);
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
