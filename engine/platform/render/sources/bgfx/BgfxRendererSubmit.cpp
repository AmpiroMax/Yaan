/*
Created: 10:08:2026 - 01:47:53
Last updated: 28:08:2026 - 12:49:39
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
- 10:08:2026 - 20:01:43: The sun caster pass now obeys DrawParams::fade
  (SHADOW_CASTER_MIN_FADE). A cross-fading terrain LOD node cast SOLID depth
  at every fade value, so both levels of one patch of ground were in the
  shadow map together and the visible one was shadowed by the other.
- 10:08:2026 - 23:24:48: COVERAGE ANTIALIASING ON THE INTERNAL TARGET — the
  user's oldest complaint («при беге трясет», «всё дергает и перерисовывается
  очень рябью»). MSAA 4x on the internal colour+depth target (DFN_MSAA=0|2|4|8),
  an alpha-weighted mip chain for cutout MASKS only, and
  BGFX_STATE_BLEND_ALPHA_TO_COVERAGE on the cutout path. Measured, one 0.05 m
  stride at RUN_SPEED, DFN_WIND_FREEZE=120, 640x360, palette off, control
  0.000 %: near canopy 0.864 -> 0.621 %, treeline 0.094 -> 0.004 %. MSAA ALONE
  IS WORTH ALMOST NOTHING (0.819 / 0.080) and MSAA 8x equals 4x to three
  digits — the residual pixels are not partially covered, they are written or
  discarded, so the fix had to reach the MASK. Details and the palette-on
  numbers in docs/specs/render.md.
- 13:08:2026 - 16:10:00: Casters inside 40 m also draw into VIEW_SHADOW_NEAR.
  The cull against the near volume is what keeps this from being a second full
  shadow pass, and the cutout mask is BOUND AGAIN for it — bgfx consumes the
  pending setTexture with the preceding submit, and a leaf card that punched a
  solid rectangle into the near map would raise the 40 px reading and lower the
  8 px one, i.e. produce the exact opposite of the claim under test.
- 13:08:2026 - 22:28:39: The carried-light cube pass now honours
  DrawParams::casts_in_point_shadows (the "фонарь не заслоняет собственное
  пламя" defect: every cube texel held the light holder's own mesh at
  0.11-0.64 m and the sconce lit NOTHING). DFN_LOD_POINT_CAST=1 and
  DFN_SELF_POINT_CAST=1 are the counterfactual arms; DFN_PS_LOG=1 names every
  draw entering a cube face -- the door that found the culprit.
- 14:08:2026 - 16:35:53: В28: each scene submit accumulates scene_draws_accum /
  scene_tris_accum and runs the centre-of-screen ray against its bounding
  sphere for center_pick (nearest hit). Reuses world_center / world_radius.
- 15:08:2026 - 15:23:22: привязка DrawParams::aux_texture к стадии 4 с нейтральной подменой;
  u_params.w — признак «лист есть» (был «зарезервировано»).
- 23:08:2026 - 00:30:00: aux2_texture привязывается на стадию 5 с нейтральной подстановкой.
- 22:08:2026 - 23:48:18: aux3_texture привязывается на стадию 6 (маска троп): износ линейно, CLAMP, нейтральная подстановка.
- 23:08:2026 - 18:10:03: params.z = -1 у эмиссивного дро (fs_prop выводит альбедо без освещения).
- 28:08:2026 - 11:22:00: params.z = -(2 + сила) у ОКОННОЙ ВСТАВКИ (сила из
  DrawParams::aux1): второй род эмиссии различается величиной того же z,
  порог -1.5. Ноль в aux1 — прежние -1.0 бит-в-бит.
- 28:08:2026 - 12:49:39: u_matParams ставится на КАЖДОМ дро (зона МАТЕРИАЛЫ, 28.08).
  Безусловно, а не «если материал задан»: uniform в bgfx липкий, и дро,
  промолчавший после золотого герба, унаследовал бы его блик.
*/

#include "engine/platform/render/sources/bgfx/BgfxRendererImpl.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
    // DOMINANT-INSTANCE GATE (SHADOW_CASTER_MIN_FADE). A dissolving draw is
    // half present on screen but was fully present in the depth map, so a
    // terrain LOD cross-fade put TWO versions of the same ground in the sun
    // map at once and the visible one landed inside the other's shadow. The
    // derivation, the "at most one" proof and the accepted cost are with the
    // constant in BgfxRendererImpl.h; this is the only place that reads it,
    // and it reads DrawParams::fade rather than anything terrain-shaped so
    // the next fading caster inherits the rule (Rule 32).
    bool casts_into_sun_map =
        im.shadow_active && params_in.fade > SHADOW_CASTER_MIN_FADE;
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

        // THE NEAR CASCADE'S OWN PASS (R6b). Same caster, same state, a volume
        // 8x smaller — and that ratio is the whole cost story: the cull below
        // is what keeps this from being a second full shadow pass. At 40 m the
        // set is the chunk under the eye, its neighbours' near corners and the
        // trees you are standing in, against 320 m for the far map. The texture
        // bind is deliberately re-issued: bgfx consumes the pending setTexture
        // with the submit above, so the cutout mask must be bound again or leaf
        // cards punch solid rectangles into the map this change exists to
        // sharpen — which would raise the 40 px reading and lower the 8 px one,
        // the exact opposite of the claim.
        if (im.shadow_near_active) {
            const glm::vec3 lsn =
                glm::vec3(im.shadow_view_near * glm::vec4(world_center, 1.0f));
            const float r = world_radius;
            if (std::fabs(lsn.x) <= SHADOW_NEAR_HALF_EXTENT_M + r
                && std::fabs(lsn.y) <= SHADOW_NEAR_HALF_EXTENT_M + r
                && std::fabs(lsn.z) <= SHADOW_DEPTH_HALF_M + r) {
                if (caster.idx == im.shadow_cutout_program.idx) {
                    const auto near_tex = im.textures.find(texture.id);
                    if (near_tex != im.textures.end()) {
                        bgfx::setTexture(0, im.s_tex_color, near_tex->second,
                                         BGFX_SAMPLER_MIN_POINT
                                             | BGFX_SAMPLER_MAG_POINT
                                             | BGFX_SAMPLER_MIP_POINT);
                    }
                }
                bgfx::setTransform(glm::value_ptr(transform));
                bgfx::setVertexBuffer(0, mesh_it->second.vb);
                bgfx::setIndexBuffer(mesh_it->second.ib);
                bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
                bgfx::submit(VIEW_SHADOW_NEAR, caster);
            }
        }
    }

    // Carried-light cube faces. Cutout casters are deliberately skipped: a leaf
    // card would punch its RECTANGLE into torchlight, and a canopy that casts
    // nothing under a torch is far less wrong than a canopy that casts boxes.
    // STAND-IN geometry is skipped by the caller's own word
    // (DrawParams::casts_in_point_shadows, see IRenderer.h): the coarse LOD
    // terrain is built without the carves, and drawn into these faces it was
    // solid rock through the tunnel's air — every texel of every face held
    // "occluder at centimetres" and the sconce lit NOTHING (floor at 2.79 m
    // read 0 of 255 across the whole frame). DFN_LOD_POINT_CAST=1 is the
    // counterfactual arm: it restores the defect from this same binary, so
    // the fix's before/after is a dose pair rather than two builds (Rule 47).
    static const bool lod_point_cast = [] {
        const char* e = std::getenv("DFN_LOD_POINT_CAST");
        const bool on = e != nullptr && *e == '1';
        if (on) {
            std::fprintf(stderr, "[render] DFN_LOD_POINT_CAST=1: stand-in "
                                 "geometry casts into point shadows again "
                                 "(the defect arm)\n");
        }
        return on;
    }();
    if (im.shadow_light_count > 0 && !is_transparent && !is_cutout
        && (params_in.casts_in_point_shadows || lod_point_cast)) {
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
                // DIAGNOSTIC (DFN_PS_LOG=1): name every draw that enters a
                // cube face. Same family as DFN_PS_DEBUG — the atlas dump
                // showed a surface 0.15-0.30 m under the flame that exists in
                // no world mesh, so the writer's inputs are the question.
                static const bool ps_log = [] {
                    const char* e = std::getenv("DFN_PS_LOG");
                    return e != nullptr && *e == '1';
                }();
                if (ps_log) {
                    static int ps_log_count = 0;
                    if (ps_log_count < 400) {
                        ++ps_log_count;
                        std::fprintf(stderr,
                                     "[ps_log] li %u f %u mesh %u center "
                                     "(%.2f %.2f %.2f) r %.2f dist %.2f\n",
                                     li, f, mesh.id, world_center.x,
                                     world_center.y, world_center.z,
                                     world_radius, glm::length(to_center));
                    }
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
    // z = highlight, w = an aux material sheet is bound (bark normals). Per DRAW, unlike u_envParams which is per
    // frame — that split is the whole reason the DrawParams sync happened.
    // ЭМИССИЯ — ОТРИЦАТЕЛЬНЫЙ z (24.08): hover неотрицателен по контракту,
    // и знак z говорит fs_prop «выведи альбедо без освещения» — пламя и
    // стекло фонаря горят независимо от бюджета точечных светов.
    // ОКОННАЯ ВСТАВКА — ВТОРОЙ РОД ЭМИССИИ, И РАЗЛИЧАЕТ ИХ ВЕЛИЧИНА ТОГО ЖЕ z
    // (28.08). Пламя светит СВОИМ альбедо круглые сутки (z = -1 ровно), а
    // лист окна светит НЕБОМ: днём холодно и ярко, ночью почти гаснет, —
    // значит у него есть СИЛА, и она меняется от часа суток. Сила приезжает в
    // DrawParams::aux1 («reserved, meaning is the program's» — вот его
    // значение для программы prop) и кодируется z = -(2 + сила), то есть
    // диапазоном (-inf, -2]. Порог различения -1.5 лежит между родами с
    // запасом в половину, и ни одно значение силы в него не попадает.
    //
    // ПОЧЕМУ НЕ НОВОЕ ПОЛЕ И НЕ НОВЫЙ UNIFORM. Поля DrawParams заводит
    // IRenderer.h — общий замороженный контракт (правило 26), и в нём прямо
    // сейчас лежит незакоммиченная правка соседней волны (u_matParams зоны
    // МАТЕРИАЛЫ). Взять его целиком под `git commit --only` значило бы унести
    // чужую строку (правило 29, случай 28.08), а aux1 объявлен ровно для
    // этого случая и до сих пор не имел читателя.
    const float emis_z = params_in.aux1 > 0.0f ? -(2.0f + params_in.aux1) : -1.0f;
    float params[4] = {0.0f, params_in.fade,
                       params_in.emissive ? emis_z : params_in.highlight,
                       params_in.aux0};
    const auto tex_it = im.textures.find(texture.id);
    if (tex_it != im.textures.end()) {
        // Point-sampled material textures: Daggerfall crunch, and no bleed
        // across the terrain atlas cells (stage-3 sync, informational item).
        //
        // A MASK IS THE ONE EXCEPTION, AND IT IS NOT A LOOK CHANGE. Only a
        // cutout mask carries a mip chain (create_texture builds one for
        // exactly those), and only its MINIFICATION filter is linear —
        // MAG stays POINT, so every texture magnified on screen is still the
        // hard-edged crunch the look is built on. What changes is the case
        // where one screen pixel covers hundreds of mask texels and the point
        // sampler was picking one at random: that is the treeline half of the
        // running shimmer, and it is invisible to MSAA because the edge lives
        // inside the texture fetch rather than on a triangle.
        const bool coverage = im.mipped_textures.contains(texture.id) && is_cutout
                              && im.internal_samples > 1;
        bgfx::setTexture(0, im.s_tex_color, tex_it->second,
                         coverage ? (BGFX_SAMPLER_MAG_POINT)
                                  : (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
                                     | BGFX_SAMPLER_MIP_POINT));
        // 2 == "mipped mask into an alpha-to-coverage draw"; fs_foliage reads
        // the distinction, every other shader only tests > 0.5.
        params[0] = coverage ? 2.0f : 1.0f;
    }
    // THE AUX SHEET (DrawParams::aux_texture — bark normals today). Bound at
    // stage 4 always: the neutral 1x1 stands in when the draw supplies none,
    // so Metal always has a real texture behind the sampler and the shader
    // needs no branch. params.w carries "a real sheet is bound", which is what
    // lets a program skip the perturbation entirely.
    {
        bgfx::TextureHandle aux = im.neutral_normal;
        if (params_in.aux_texture.valid()) {
            const auto aux_it = im.textures.find(params_in.aux_texture.id);
            if (aux_it != im.textures.end()) {
                aux = aux_it->second;
                params[3] = 1.0f;
            }
        }
        if (bgfx::isValid(aux)) {
            bgfx::setTexture(4, im.s_tex_aux, aux,
                             BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_POINT);
        }
    }
    // ПУТЕВОЙ АТЛАС (DrawParams::aux2_texture, стадия 5) — материал полотна
    // троп для террейна. Нейтральная подстановка та же, что у стадии 4: Metal
    // хочет реальную текстуру за каждым объявленным сэмплером, а шейдер
    // решает читать или нет по дозе и износу.
    {
        bgfx::TextureHandle aux2 = im.neutral_normal;
        if (params_in.aux2_texture.valid()) {
            const auto it2 = im.textures.find(params_in.aux2_texture.id);
            if (it2 != im.textures.end()) {
                aux2 = it2->second;
            }
        }
        if (bgfx::isValid(aux2) && bgfx::isValid(im.s_tex_path)) {
            bgfx::setTexture(5, im.s_tex_path, aux2,
                             BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
                                 | BGFX_SAMPLER_MIP_POINT);
        }
    }
    // МАСКА ТРОП (DrawParams::aux3_texture, стадия 6): износ линейной
    // фильтрацией (гладкая кромка полотна — весь смысл маски), класс шейдер
    // снимает вторым чтением по центру текселя. CLAMP — за прямоугольником
    // композиции маска не повторяется. Подстановка та же нейтральная: шейдер
    // решает читать или нет по u_pathMask.w.
    {
        bgfx::TextureHandle aux3 = im.neutral_normal;
        if (params_in.aux3_texture.valid()) {
            const auto it3 = im.textures.find(params_in.aux3_texture.id);
            if (it3 != im.textures.end()) {
                aux3 = it3->second;
            }
        }
        if (bgfx::isValid(aux3) && bgfx::isValid(im.s_tex_path_mask)) {
            bgfx::setTexture(6, im.s_tex_path_mask, aux3,
                             BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        }
    }
    bgfx::setUniform(im.u_params, params);
    // ОТРАЖАТЕЛЬНАЯ ПОЛОВИНА МАТЕРИАЛА (зона МАТЕРИАЛЫ, 28.08). Ставится
    // БЕЗУСЛОВНО, а не «если материал задан»: uniform в bgfx липкий, и дро,
    // промолчавший после золотого герба, унаследовал бы его блик. Один
    // setUniform на дро — это цена того, что материал НЕ течёт между дро.
    const float mat_params[4] = {params_in.roughness, params_in.metalness, 0.0f,
                                 0.0f};
    if (bgfx::isValid(im.u_mat_params)) {
        bgfx::setUniform(im.u_mat_params, mat_params);
    }
    if (bgfx::isValid(im.shadow_map)) {
        // Compare-sampler flags come from the texture creation; stage 1 is the
        // dfn_shadow.sh contract (unused by shaders that do not sample it).
        bgfx::setTexture(1, im.s_shadow_map, im.shadow_map);
    }
    if (bgfx::isValid(im.shadow_map_near)) {
        // Stage 3 is the near cascade half of the dfn_shadow.sh contract.
        // Bound unconditionally for the same reason as the point atlas: Metal
        // wants a real texture behind every sampler the program declares, and
        // it is the in-volume test in the shader that turns the lookup off.
        bgfx::setTexture(3, im.s_shadow_map_near, im.shadow_map_near);
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
    uint64_t state =
        is_transparent
            ? (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
               | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA)
            : (BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
               | BGFX_STATE_DEPTH_TEST_GREATER);
    // ALPHA TO COVERAGE FOR THE CUTOUT PATH. It is the half of the shimmer fix
    // that MSAA alone cannot deliver: a cutout writes the pixel or discards
    // it, so the mask edge is all-or-nothing no matter how many samples the
    // target has (measured: MSAA 4x and MSAA 8x give the identical treeline
    // number). With A2C the mask's own alpha becomes the number of samples
    // covered, so the fractional coverage the mip chain now computes survives
    // into the frame instead of being rounded to 0 or 1.
    //
    // Costs nothing and does nothing when the target is single-sampled
    // (DFN_MSAA=0), which is why it is gated: one sample has no fractions to
    // hand out, and enabling it there only pays for a pipeline variant.
    if (is_cutout && im.internal_samples > 1 && params[0] > 1.5f) {
        state |= BGFX_STATE_BLEND_ALPHA_TO_COVERAGE;
    }
    bgfx::setState(state);
    bgfx::submit(VIEW_SCENE, prog_it->second);

    // --- В28 FRAME STATS + CENTRE PICK -------------------------------------
    // Counted for every real scene draw — the early returns above already
    // dropped the misses, and this sits after the one scene submit so it
    // matches it one-to-one (transparents included: they are drawn too).
    // Triangles come from the mesh's own index count because bgfx exposes a
    // draw-call count but NO primitive count (see RenderFrameStats).
    ++im.scene_draws_accum;
    im.scene_tris_accum += mesh_it->second.tri_count;
    // Centre-of-screen ray against this draw's world bounding sphere (variant
    // A) — the same world_center / world_radius the shadow culls just used, so
    // the pick costs a handful of flops and no extra bounds. Nearest hit wins.
    {
        const glm::vec3 oc = im.pick_ray_origin - world_center;
        const float b = glm::dot(oc, im.pick_ray_dir);
        const float c = glm::dot(oc, oc) - world_radius * world_radius;
        const float disc = b * b - c;
        if (disc >= 0.0f) {
            const float s = std::sqrt(disc);
            float t = -b - s;          // near intersection
            if (t < 0.0f) {
                t = -b + s;            // eye inside the sphere: take the far side
            }
            if (t >= 0.0f && t < im.pick_best_t) {
                im.pick_best_t = t;
                im.pick_accum.hit = true;
                im.pick_accum.pick_id = params_in.pick_id;
                im.pick_accum.mesh = mesh;
                im.pick_accum.triangles = mesh_it->second.tri_count;
                im.pick_accum.distance_m = t;
                im.pick_accum.position = im.pick_ray_origin + im.pick_ray_dir * t;
            }
        }
    }
}

} // namespace dfn::platform
