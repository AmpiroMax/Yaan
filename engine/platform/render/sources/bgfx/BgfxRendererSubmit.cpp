/*
Created: 10:08:2026 - 01:47:53
Last updated: 10:08:2026 - 01:47:53
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxRendererSubmit.cpp

Responsibility:
- BgfxRenderer::submit — the one draw entry point: sun-shadow caster pass
  (with the light-volume cull), carried-light cube-face caster passes (sphere
  + per-face cull), and the scene draw with its per-draw params. One of four
  translation units over BgfxRendererImpl.h (Rule 21 split); lifecycle is
  BgfxRenderer.cpp, frame path BgfxRendererFrame.cpp, handle bookkeeping
  BgfxRendererResources.cpp.

Key items:
- BgfxRenderer::submit (DrawParams form; the four-argument convenience
  overload lives in IRenderer and forwards here).

Dependencies:
- Uses: BgfxRendererImpl.h, bgfx, glm.
- Used by: dfn_platform_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The sun caster cull here is NOT RenderSystem::visible_or_casting: that one
  decides whether to submit AT ALL (and deliberately keeps off-screen casters
  inside the volume); this one decides whether a submitted draw also needs a
  depth pass. Both sides were A/B-verified with a non-vacuous control.
*/
/*
UPD:
- 10:08:2026 - 01:47:53: Created in the Rule 21 split of BgfxRenderer.cpp.
  submit moved verbatim; no behaviour change.
*/

#include "engine/platform/render/sources/bgfx/BgfxRendererImpl.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>

namespace dfn::platform {

void BgfxRenderer::submit(MeshHandle mesh, ProgramHandle program,
                          const glm::mat4& transform, TextureHandle texture,
                          const DrawParams& params_in) {
    Impl& im = *impl_;
    if (!im.initialized || !im.in_frame) {
        return;
    }
    const auto mesh_it = im.meshes.find(mesh.id);
    const auto prog_it = im.programs.find(program.id);
    if (mesh_it == im.meshes.end() || prog_it == im.programs.end()) {
        return;
    }
    const bool is_transparent = im.transparent.contains(program.id);

    // Shadow caster pass (в1): every opaque submit also renders depth into the
    // sun's shadow view — terrain, trees, houses, scatter all cast. Skipped
    // when the sun is below the shadow threshold (night) or resources failed.
    const bool is_cutout = im.cutout.contains(program.id);
    // World-space bounding sphere of this draw, used by both shadow passes.
    // The mesh sphere is model space, so it needs the transform's largest axis
    // scale — uniform scaling is the norm here and taking the max is the safe
    // direction for a cull.
    const glm::vec3 world_center =
        glm::vec3(transform * glm::vec4(mesh_it->second.center, 1.0f));
    const float world_radius =
        mesh_it->second.radius
        * std::max({glm::length(glm::vec3(transform[0])),
                    glm::length(glm::vec3(transform[1])),
                    glm::length(glm::vec3(transform[2]))});

    // CASTER CULL AGAINST THE SUN VOLUME. The shadow map is an eye-centred
    // ortho box of half extent SHADOW_HALF_EXTENT_M and depth half
    // SHADOW_DEPTH_HALF_M; anything wholly outside it cannot darken a single
    // texel, so its depth draw is pure cost. This is not the same test as
    // RenderSystem::visible_or_casting — that one decides whether to submit at
    // all (and deliberately KEEPS off-screen casters that are inside this
    // volume). This one decides whether a submitted draw also needs a depth
    // pass, which is the only reason keeping those casters costs anything.
    bool casts_into_sun_map = im.shadow_active;
    if (casts_into_sun_map) {
        const glm::vec3 ls = glm::vec3(im.shadow_view * glm::vec4(world_center, 1.0f));
        const float r = world_radius;
        casts_into_sun_map = std::fabs(ls.x) <= SHADOW_HALF_EXTENT_M + r
                          && std::fabs(ls.y) <= SHADOW_HALF_EXTENT_M + r
                          && std::fabs(ls.z) <= SHADOW_DEPTH_HALF_M + r;
    }
    if (casts_into_sun_map && !is_transparent && !im.non_casting.contains(program.id)) {
        // Cutout casters (leaf cards) punch their mask through the depth map
        // and sway with the SAME wind as the visible geometry — otherwise the
        // canopy casts a solid rectangle, or its shadow stands still while the
        // leaves move. Everything else keeps the cheap depth-only program, so
        // only foliage pays for the discard.
        bgfx::ProgramHandle caster = im.shadow_program;
        if (is_cutout && bgfx::isValid(im.shadow_cutout_program)) {
            caster = im.shadow_cutout_program;
            const auto shadow_tex = im.textures.find(texture.id);
            if (shadow_tex != im.textures.end()) {
                bgfx::setTexture(0, im.s_tex_color, shadow_tex->second,
                                 BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
                                     | BGFX_SAMPLER_MIP_POINT);
            }
        }
        bgfx::setTransform(glm::value_ptr(transform));
        bgfx::setVertexBuffer(0, mesh_it->second.vb);
        bgfx::setIndexBuffer(mesh_it->second.ib);
        bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
        bgfx::submit(VIEW_SHADOW, caster);
    }

    // Carried-light cube faces. Cutout casters are deliberately skipped: a leaf
    // card would punch its RECTANGLE into torchlight, and a canopy that casts
    // nothing under a torch is far less wrong than a canopy that casts boxes.
    if (im.shadow_light_count > 0 && !is_transparent && !is_cutout) {
        const glm::vec3 wcenter = world_center;
        const float wradius = world_radius;
        for (uint32_t li = 0; li < im.shadow_light_count; ++li) {
            const PointLight& light = im.lights[li];
            const glm::vec3 to_center = wcenter - light.position;
            const float reach = light.radius_m + wradius;
            // THE CULL that makes this affordable: only casters whose sphere
            // meets the light's sphere are drawn at all. In a tunnel that is
            // the resident chunk plus a handful of props.
            if (glm::dot(to_center, to_center) > reach * reach) {
                continue;
            }
            const float caster_params[4] = {light.position.x, light.position.y,
                                            light.position.z, light.radius_m};
            for (uint32_t f = 0; f < POINT_SHADOW_FACES; ++f) {
                // Per-face frustum reject: each face is a 90-degree pyramid, so
                // its four side planes have inward normals normalize(fwd +/-
                // right) and normalize(fwd +/- up). A 256 m terrain chunk still
                // hits 2-3 faces, but a prop usually hits one — that is the
                // difference between 6 draws per caster and ~1.5.
                const glm::vec3 fwd = POINT_SHADOW_FACE_DIR[f];
                const glm::vec3 up = POINT_SHADOW_FACE_UP[f];
                const glm::vec3 right = glm::cross(fwd, up);
                const glm::vec3 planes[4] = {fwd + right, fwd - right, fwd + up,
                                             fwd - up};
                bool outside = false;
                for (const glm::vec3& p : planes) {
                    if (glm::dot(to_center, glm::normalize(p)) < -wradius) {
                        outside = true;
                        break;
                    }
                }
                if (outside) {
                    continue;
                }
                bgfx::setUniform(im.u_point_caster, caster_params);
                bgfx::setTransform(glm::value_ptr(transform));
                bgfx::setVertexBuffer(0, mesh_it->second.vb);
                bgfx::setIndexBuffer(mesh_it->second.ib);
                bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z
                               | BGFX_STATE_DEPTH_TEST_LESS);
                bgfx::submit(static_cast<bgfx::ViewId>(
                                 VIEW_POINT_SHADOW_FIRST
                                 + li * POINT_SHADOW_FACES + f),
                             im.point_shadow_program);
            }
        }
    }

    bgfx::setTransform(glm::value_ptr(transform));
    bgfx::setVertexBuffer(0, mesh_it->second.vb);
    bgfx::setIndexBuffer(mesh_it->second.ib);

    // u_params: x = texture bound, y = fade (screen-door dither below 1),
    // z = highlight, w = reserved. Per DRAW, unlike u_envParams which is per
    // frame — that split is the whole reason the DrawParams sync happened.
    float params[4] = {0.0f, params_in.fade, params_in.highlight, params_in.aux0};
    const auto tex_it = im.textures.find(texture.id);
    if (tex_it != im.textures.end()) {
        // Point-sampled material textures: Daggerfall crunch, and no bleed
        // across the terrain atlas cells (stage-3 sync, informational item).
        bgfx::setTexture(0, im.s_tex_color, tex_it->second,
                         BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
                             | BGFX_SAMPLER_MIP_POINT);
        params[0] = 1.0f;
    }
    bgfx::setUniform(im.u_params, params);
    if (bgfx::isValid(im.shadow_map)) {
        // Compare-sampler flags come from the texture creation; stage 1 is the
        // dfn_shadow.sh contract (unused by shaders that do not sample it).
        bgfx::setTexture(1, im.s_shadow_map, im.shadow_map);
    }
    if (bgfx::isValid(im.point_shadow_atlas)) {
        // Stage 2 is the dfn_pointshadow.sh contract. Bound unconditionally:
        // Metal wants a real texture behind any sampler the program declares,
        // and u_pointShadowParams.x = 0 is what actually turns the lookup off.
        bgfx::setTexture(2, im.s_point_shadow, im.point_shadow_atlas);
    }

    // Culling deliberately off this stage (see header notice). Transparent
    // programs ("water"): alpha blend, depth read-only, drawn after opaques
    // by the caller (sequential view).
    const uint64_t state =
        is_transparent
            ? (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
               | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA)
            : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
               | BGFX_STATE_DEPTH_TEST_GREATER);
    bgfx::setState(state);
    bgfx::submit(VIEW_SCENE, prog_it->second);
}

} // namespace dfn::platform
