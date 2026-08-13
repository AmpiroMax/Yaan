/*
Created: 10:08:2026 - 01:47:53
Last updated: 13:08:2026 - 18:18:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxRendererImpl.h

Responsibility:
- PRIVATE header shared by the BgfxRenderer translation units (Rule 21 split:
  lifecycle / frame / submit / resources). Holds BgfxRenderer::Impl and the
  backend constants more than one of those files needs. Never included outside
  sources/bgfx/ — bgfx types stay inside the backend (Rule 1 hygiene).

Key items:
- BgfxRenderer::Impl (all bgfx state + per-frame helpers).
- View layout constants (VIEW_SHADOW .. VIEW_UPSCALE), sun/point shadow
  constants, ENV_PARAM_VEC4S (dfn_env.sh contract).

Dependencies:
- Uses: BgfxRenderer.h, BgfxCallback.h, bgfx, glm, stdlib.
- Used by: BgfxRenderer.cpp, BgfxRendererFrame.cpp, BgfxRendererSubmit.cpp,
  BgfxRendererResources.cpp — and nothing else, by design.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This header is backend-PRIVATE. If anything outside sources/bgfx/ wants a
  symbol from here, that is a contract question for the lead, not an include.
- Constants that are look-dev values (SKY_COLOR_RGBA lives with the frame
  pass) are flagged on the NUMBERS.md migration list (Rule 14).
*/
/*
UPD:
- 10:08:2026 - 01:47:53: Created in the Rule 21 split of BgfxRenderer.cpp
  (1424 lines against the 800 ceiling). All state and constants moved
  verbatim; no behaviour change.
- 10:08:2026 - 03:04:00: ENV_PARAM_VEC4S 33 -> 35 (cloud slots 33/34, the
  W4 coverage-field state — change paired with dfn_env.sh per the contract).
- 10:08:2026 - 20:01:43: SHADOW_CASTER_MIN_FADE — a dissolving draw was fully
  present in the sun shadow map, so a terrain LOD cross-fade put two versions
  of the same ground in it at once and the visible one landed in the other's
  shadow.
- 11:08:2026 - 13:38:39: ENV_PARAM_VEC4S 36 -> 37 (slot 36 = the AIR: HAZE_SCALE_LENGTH /
  HAZE_HEIGHT_SCALE, aerial perspective / REFERENCE_FRAMES.md R1, same
  generated-header route as the sun's body).
- 10:08:2026 - 20:10:49: ENV_PARAM_VEC4S 35 -> 36 (slot 35 = the sun's body,
  paired with dfn_env.sh per the layout contract).
- 10:08:2026 - 23:24:48: Impl::internal_samples (MSAA sample count of the
  internal target) and Impl::mipped_textures (cutout masks that carry a mip
  chain). Both exist for the coverage-antialiasing fix; see
  BgfxRenderer.cpp's internal-target block and docs/specs/render.md.
- 11:08:2026 - 14:24:26: ENV_PARAM_VEC4S 37 -> 38 (slot 37 = the MIST BAND, R2).
- 13:08:2026 - 16:10:00: THE NEAR CASCADE (SHADOW_NEAR_*), and the view ids
  shifted by one to make room for it (VIEW_SHADOW_NEAR = 1, point shadows now
  from 2). It is the remedy this file had already named for itself and R6b
  finally sized: the far map's 0.156 m texel is 2-3x COARSER than the leaf
  mask's own texel (0.047-0.086 m), so the map is a 0.31 m low-pass on the
  canopy and passes only the blob. 4096 over 40 m = 0.0195 m fixes the
  bandwidth, not the amount — the dose arms say the amount was never the
  problem.
- 13:08:2026 - 18:10:00: ENV_PARAM_VEC4S 38 -> 39 (slot 38 = THE FILL'S
  DIRECTION) plus FILL_UP_DEFAULT / FILL_SUN_DEFAULT. Measured before it was
  touched: a bole standing in its own canopy's shadow ran p90/p10 = 1.01x over
  228 pixels — one colour — because dfn_surface_light's shadow half-space had no
  surface normal in it at all. Paired with dfn_env.sh per the layout contract.
- 13:08:2026 - 18:18:00: FILL_SUN_DEFAULT 0.25 -> 0.30 with the corrected fill
  shape (see dfn_env.sh): the sun term is now the ONLY one a vertical bole can
  use, the up term having been narrowed to undersides so it cannot dim a trunk.
*/

#pragma once

#include "engine/platform/render/sources/bgfx/BgfxCallback.h"
#include "engine/platform/render/sources/bgfx/BgfxRenderer.h"

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>

#include <array>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dfn::platform {

inline constexpr bgfx::ViewId VIEW_SHADOW = 0;  // -> sun shadow map (depth only)
inline constexpr bgfx::ViewId VIEW_SHADOW_NEAR = 1; // -> near cascade (depth only)
// Carried-light cube shadows: MAX_SHADOW_POINT_LIGHTS x 6 faces, each face one
// view into a shared atlas. Views render in id order, so every face is
// finished before the scene samples it.
inline constexpr bgfx::ViewId VIEW_POINT_SHADOW_FIRST = 2;
inline constexpr uint32_t POINT_SHADOW_FACES = 6;
inline constexpr uint32_t POINT_SHADOW_VIEWS =
    MAX_SHADOW_POINT_LIGHTS * POINT_SHADOW_FACES;
inline constexpr bgfx::ViewId VIEW_SCENE =
    static_cast<bgfx::ViewId>(VIEW_POINT_SHADOW_FIRST + POINT_SHADOW_VIEWS);
inline constexpr bgfx::ViewId VIEW_BACKBUFFER = VIEW_SCENE + 1; // letterbox clear
inline constexpr bgfx::ViewId VIEW_UPSCALE = VIEW_SCENE + 2;    // integer-scaled quad

// Sun shadow map (user decision в1). Backend look-dev constants — flagged on
// the NUMBERS.md migration list (Rule 14). Eye-centered ortho along the sun
// direction, texel-snapped.
//
// TEXEL DENSITY IS THE THIN-OBJECT CONTRACT (user bug 09:08:2026: "тени у
// деревьев только крона без тени ствола"). A caster narrower than one shadow
// texel only darkens a texel when it happens to cover its center, so it
// flickers out entirely. The first version (2048 over a 640 m half extent =
// 0.625 m per texel) could not represent ANY trunk: oak 1.1 m = 1.8 texels
// (dashed), pine 0.6 m = 0.96, birch 0.28-0.44 m = 0.45-0.7 — while the 8 m
// oak crown covered 13 texels and shadowed solidly. Hence "canopy only".
// 4096 over 320 m = 0.156 m per texel puts the THINNEST trunk at ~1.8 texels
// and the 2 m standing stones (§6.2 entrance markers) at ~13.
// The rule for anything added later (fences, castle detail, railings):
//   shadow needs width >= ~2 x SHADOW_TEXEL_M, i.e. >= ~0.31 m today.
// The price is range: the volume shrinks from the loaded chunk ring to 320 m,
// which is where fog (LOOKDEV_FOG_START_FRAC x CAMERA_FAR = 300 m) starts
// hiding the difference anyway. Covering both near detail and the full ring
// needs a second cascade — a feature, not a constant (flagged in the spec).
inline constexpr uint16_t SHADOW_MAP_SIZE = 4096;
inline constexpr float SHADOW_HALF_EXTENT_M = 320.0f;
inline constexpr float SHADOW_DEPTH_HALF_M = 700.0f;  // along-light half range
inline constexpr float SHADOW_MIN_SUN_ELEVATION = 0.05f; // sun_dir.y below -> off
inline constexpr float SHADOW_TEXEL_M =
    2.0f * SHADOW_HALF_EXTENT_M / static_cast<float>(SHADOW_MAP_SIZE);
// Receiver push-off, in TEXELS: it scales with the map, so the finer map also
// stops the offset from eroding thin shadows (it was 0.625 m — wider than a
// birch trunk's whole shadow — and is now 0.156 m).
inline constexpr float SHADOW_NORMAL_OFFSET_M = 1.0f * SHADOW_TEXEL_M; // anti-acne
inline constexpr float SHADOW_DEPTH_BIAS_M = 0.25f;   // compare bias, world meters

// --- THE NEAR CASCADE (R6b: the dapple's GRAIN) ------------------------------
//
// The far map above is sized for TRUNKS and it is exactly at its own floor for
// them (birch 0.28 m = 1.8 texels). The canopy dapple lives two scales below
// that, and the arithmetic says it cannot exist at all on that map:
//
//   SHADOW_TEXEL_M                        = 2 x 320 / 4096 = 0.1563 m
//   thin-caster floor (>= 2 texels)       = 0.3125 m
//   + SHADOW_NORMAL_OFFSET_M, both sides  -> a hole must clear ~0.31 m to
//                                            survive the receiver push-off too
//   leaf MASK texel (FloraCards: 64 px tile on a 3.0-5.5 m card)
//                                         = 0.047-0.086 m
//
// The last line is the finding, and it is stronger than "we sit on the floor":
// THE SHADOW MAP UNDERSAMPLES THE LEAF MASK BY 2-3x. The mask is a 64x64 image
// and the map cannot resolve one of its texels, so nothing the mask draws at
// its own resolution — the ragged rim of every lobe — reaches the ground. What
// still casts is only what flora AUTHORED above the floor on purpose: rim bites
// of 0.4-0.9 m (2.6-5.8 far texels) and the one or two 1 m interior gaps
// (6.4 texels). That is why the measured shadow contribution RISES with block
// size: the map is a low-pass filter with a 0.31 m cutoff, and it passes exactly
// the canopy-sized blob and nothing finer.
//
// So the near cascade is not "more shadow", it is BANDWIDTH. 4096 over 40 m:
//   SHADOW_NEAR_TEXEL_M = 2 x 40 / 4096 = 0.0195 m, 8x finer
//   thin-caster floor                   = 0.039 m, below the leaf mask's own
//                                          texel — the mask is now oversampled
//                                          rather than undersampled
// 40 m is chosen by what the cascade must COVER, not by what looks fine: it is
// the ground the near-canopy vantage actually stands on, and the far map keeps
// everything past it unchanged, so this can only add grain and can never remove
// a shadow that was already there.
//
// THE PRICE IS A SECOND DEPTH PASS for every caster within 40 m of the eye, and
// it was paid in the frame log, not in a comment.
//
// AND IT IS OFF BY DEFAULT, BECAUSE THE MEASUREMENT SAID SO. All of the above
// is correct and none of it pays yet. Three arms out of one binary at the near
// canopy (docs/acceptance/render-R6b-near-cascade.md), n=3 each, the cascade's
// OWN contribution to local contrast over the far map alone:
//   8 px +0.010 | 16 px +0.000 | 24 px +0.010 | 40 px +0.046   (run spread
//   0.003-0.016, so the two middle numbers are not distinguishable from zero)
// against a cost of +22 % mean frame time — the vantage held the 120 Hz cap in
// 3/7 runs with the cascade on and 6/7 with it off. Trading a vsync tier at the
// exact vantage the user's running-stutter complaint lives at, for a tenth of
// what the far map already delivers, is not a trade to ship.
//
// WHY IT BUYS SO LITTLE, and this is the finding that outlives the feature: the
// near forest floor has NO SUN ON IT TO INTERRUPT. Threshold-free, over the
// near band of the same frames, p90/p10 of luma:
//   reference 03's forest floor   3.23x   (36 -> 116: light and shade interleave
//                                          continuously across the whole range)
//   ours, sun shadow ON           1.15x   (27.5 -> 31.7: a flat DARK sheet)
//   ours, sun shadow OFF          1.15x   (49.6 -> 57.3: a flat BRIGHT sheet)
// Our canopy shadow is binary and total — it takes an evenly lit floor to an
// evenly dark one with nothing in between. No shadow-map resolution can invent
// a middle tone that has no source. The gate has MOVED, from this map's
// bandwidth (render, fixed here) to how much sun the canopy lets through
// (flora) and to how much tone the floor material carries (ground).
//
// TURN IT ON WITH DFN_SHADOW_NEAR=1 the day the canopy opens: the day our near
// floor stops reading 1.15x, the grain that comes through it will need this
// bandwidth, and the arithmetic at the top of this block will be waiting.
inline constexpr uint16_t SHADOW_NEAR_MAP_SIZE = 4096;
inline constexpr float SHADOW_NEAR_HALF_EXTENT_M = 40.0f;
inline constexpr float SHADOW_NEAR_TEXEL_M =
    2.0f * SHADOW_NEAR_HALF_EXTENT_M / static_cast<float>(SHADOW_NEAR_MAP_SIZE);
// Same texel-denominated push-off as the far map, which is the whole point:
// at 0.0195 m it stops eroding the features the cascade exists to carry.
inline constexpr float SHADOW_NEAR_NORMAL_OFFSET_M = 1.0f * SHADOW_NEAR_TEXEL_M;
// The near volume shares SHADOW_DEPTH_HALF_M so that both cascades normalise
// depth identically and ONE u_shadowParams.z bias serves both. Making the near
// range shallower would buy depth precision the D16 map does not need here
// (1400 m / 65536 = 0.021 m, far under the 0.25 m compare bias) and would cost
// a second bias slot plus a second way to get it wrong.

// THE DOSE, read ONCE and read by BOTH init and the frame path — which is why
// it lives here and not beside the other doses. Default 0 (see the block
// above). At 0 the near target is allocated at 4x4 instead of 4096x4096, so a
// disabled cascade costs no VRAM while the sampler stays legally bound (Metal
// wants a real texture behind every sampler a program declares); nothing ever
// samples it, because update_shadow parks u_lightMtxNear outside the unit box
// and the shader's own in-volume test rejects it.
[[nodiscard]] inline bool shadow_near_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("DFN_SHADOW_NEAR");
        return e != nullptr && e[0] != '\0' && e[0] != '0';
    }();
    return on;
}

// A DISSOLVING DRAW IS HALF PRESENT ON SCREEN AND WAS FULLY PRESENT IN THE
// SHADOW MAP. DrawParams::fade drives a screen-door dither in the scene
// fragment shaders, but the caster pass runs vs_shadow/fs_shadow, which take
// no fade input at all — so during a cross-fade BOTH instances of the same
// ground wrote solid depth. They are not the same surface: the terrain LOD
// ladder's coarser level samples the height field at 4x the step, and the
// file that meshes them bounds their disagreement at "the relief across FOUR
// of the fine level's own cells" (LodTerrain skirt derivation). Whichever of
// the two sits higher wins the depth test, and the one you can actually SEE is
// then behind it — i.e. in its own shadow. That is a dark band along the LOD
// ring, lasting LOD_FADE_SECONDS, appearing every time the ring re-selects.
//
// The rule, stated so it survives the next fading caster (Rule 32 — the fade
// is a shared mechanism, not a terrain feature): A DRAW CASTS A SUN SHADOW
// ONLY WHILE IT IS THE DOMINANT INSTANCE. Strictly greater than one half is
// what makes "at most one" provable rather than likely: the residency fades
// the outgoing level down and the incoming one up at the same rate and the
// incoming one cannot start before the outgoing one leaves 1.0, so the pair
// sums to at most 1 and only one member can exceed 0.5. `>=` would let an
// exact 0.5/0.5 tie put both in the map, which is the very frame the band is
// worst. The accepted cost is the opposite tie: for at most one frame, or for
// as long as a late mesh delays the incoming fade, neither casts — a missing
// shadow 250 m away for a frame against a black stripe across the ground.
inline constexpr float SHADOW_CASTER_MIN_FADE = 0.5f;

// Carried-light (torch) cube shadow maps. Interiors are the reason they exist:
// the crag tunnel is 158 m of carved passage and a torch that lights walls but
// casts nothing reads as a glowing fog, not as a flame.
//
// One 2D ATLAS holds every face (4 columns x 3 rows = 12 tiles = 2 lights x 6
// faces): one sampler stage, one framebuffer, and — the real reason — the face
// lookup in the shader is plain arithmetic on OUR face order instead of a
// cube-map sampling convention that would have to be guessed and then debugged
// through a screenshot.
//
// TEXEL DENSITY, the same contract as the sun map but far kinder: a 90-degree
// face at distance d spans 2d texels' worth of 2*d*tan(45) = 2d metres, so a
// texel is d / (FACE_PX/2). At 512 px and a tunnel wall 4 m away that is
// 0.016 m — a caster needs ~0.03 m of width to shadow here, against ~0.31 m
// for the sun map. Nothing we build will be too thin for THIS map.
// Storage: R32F 2048x1536 (12.6 MB) + a D16 depth attachment (6.3 MB).
inline constexpr uint16_t POINT_SHADOW_FACE_PX = 512;
inline constexpr uint16_t POINT_SHADOW_ATLAS_COLS = 4;
inline constexpr uint16_t POINT_SHADOW_ATLAS_ROWS = 3;
inline constexpr uint16_t POINT_SHADOW_ATLAS_W =
    POINT_SHADOW_FACE_PX * POINT_SHADOW_ATLAS_COLS;
inline constexpr uint16_t POINT_SHADOW_ATLAS_H =
    POINT_SHADOW_FACE_PX * POINT_SHADOW_ATLAS_ROWS;
inline constexpr float POINT_SHADOW_NEAR_M = 0.05f;   // the flame can touch a wall
inline constexpr float POINT_SHADOW_NORMAL_OFFSET_M = 0.05f;
// Bias as a FRACTION of the light radius, because that is the unit the atlas
// stores: 0.012 of a 9 m torch is 11 cm.
inline constexpr float POINT_SHADOW_BIAS_FRAC = 0.012f;

// Face order — a contract with dfn_pointshadow.sh (major axis of the direction
// to the fragment picks the face, so these must stay +X, -X, +Y, -Y, +Z, -Z).
inline constexpr glm::vec3 POINT_SHADOW_FACE_DIR[POINT_SHADOW_FACES] = {
    {1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f},
};
inline constexpr glm::vec3 POINT_SHADOW_FACE_UP[POINT_SHADOW_FACES] = {
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
};

// bgfx's handle pools for vertex and index buffers. MIRRORED, not included:
// the value lives in bgfx's PRIVATE src/config.h (BGFX_CONFIG_MAX_VERTEX_BUFFERS
// / BGFX_CONFIG_MAX_INDEX_BUFFERS, both 4<<10 in the pinned v1.153.9398-566)
// and bgfx's src/ is not on our include path. It is used for DIAGNOSTICS ONLY —
// nothing branches on it — so a drift misreports a number in an error message
// and cannot change behaviour. The real guard is bgfx::isValid on every handle.
inline constexpr int BGFX_MESH_HANDLE_BUDGET = 4 << 10;

inline constexpr uint16_t ENV_PARAM_VEC4S = 39; // layout contract with dfn_env.sh

// SLOT 38 — THE FILL'S DIRECTION (user: "тёмные деревья, словно их нет, как
// чёрное пятно ... она должна быть темнее переда, но цвет одинаковый").
//
// MEASURED BEFORE IT WAS TOUCHED. A tree bole standing in its own canopy's
// shadow, 228 pixels strictly inside the bark: p10 21.8, p50 21.8, p90 22.1 —
// **p90/p10 = 1.01x, sixteen distinct luma values in 228 pixels.** Open sunlit
// ground in the same frame runs 3.26x over 233 values and a lit crown 3.75x
// over 364. The shadow side is not "darker", it is ONE COLOUR.
//
// And it could not have been anything else. In dfn_surface_light the whole
// shadow half-space collapses to
//     light = u_ambientColor * sky_vis
// with no surface normal in it anywhere, so every normal returns the same
// number and what survives is the albedo texture times a constant. Sixteen
// tones is the BARK, not the tree.
//
// x = FILL_UP: the sky above is brighter than the ground below, so n.y earns
//     a face its top and bottom back. Does nothing for a vertical bole, whose
//     normals are horizontal — which is why there is a second term.
// y = FILL_SUN: the sky is brightest around the sun and the sunlit ground
//     bounces from that side, so dot(n, sun) gives a VERTICAL cylinder an
//     azimuth: the side facing the sun is lighter than the side away. That is
//     the user's sentence — "darker than the front" — as arithmetic.
//
// BOTH TERMS HAVE ZERO MEAN OVER A SPHERE OF NORMALS, deliberately, so this
// moves the DISTRIBUTION of the fill and cannot move its AVERAGE. That is the
// property that makes the acceptance readable at all: any change that also
// shifts the mean would be indistinguishable from simply turning the ambient
// up, and this zone has already been burned twice this week by a number whose
// mean and distribution said different things.
inline constexpr float FILL_UP_DEFAULT = 0.35f;   // undersides only: 1.00 -> 0.65
inline constexpr float FILL_SUN_DEFAULT = 0.30f;  // around a bole: 1.30/0.70 = 1.86x
inline constexpr uint16_t PALETTE_SIZE = 64;

struct DebugVertex {
    float x, y, z;
    uint32_t abgr;
};

struct BgfxRenderer::Impl {
    BgfxCallback callback;
    bool initialized = false;

    uint32_t fb_width = 0;
    uint32_t fb_height = 0;
    uint32_t internal_width = 0;
    uint32_t internal_height = 0;
    uint32_t reset_flags = BGFX_RESET_NONE;
    // Coverage antialiasing on the INTERNAL target (samples per internal
    // pixel: 1 = off, 2/4/8 = MSAA). See BgfxRenderer.cpp's internal-target
    // block for why this is the fix for the running shimmer and why it does
    // NOT make the picture less pixelated.
    uint32_t internal_samples = 1;

    bgfx::VertexLayout mesh_layout;
    bgfx::VertexLayout debug_layout;
    bgfx::VertexLayout upscale_layout;

    bgfx::FrameBufferHandle internal_fb = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_env_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_post_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_palette = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle quad_vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle quad_ib = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle upscale_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle debug_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle sky_program = BGFX_INVALID_HANDLE;

    // Sun shadow map (в1): depth-only target + internal program + uniforms.
    bgfx::TextureHandle shadow_map = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle shadow_fb = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadow_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadow_cutout_program = BGFX_INVALID_HANDLE;
    std::unordered_map<uint32_t, bool> cutout; // program id -> alpha cutout
    // Texture ids that carry a mip chain (cutout masks only — see
    // create_texture; the terrain atlas must never be mipped).
    std::unordered_set<uint32_t> mipped_textures;
    std::unordered_map<uint32_t, bool> non_casting; // program id -> never a sun caster
    bgfx::UniformHandle s_shadow_map = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_mtx = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params = BGFX_INVALID_HANDLE;
    bool shadow_active = false;   // this frame: sun above threshold + resources ok
    // The sun shadow map's LIGHT-SPACE view matrix for this frame. Valid only
    // while shadow_active; `submit` uses it to reject casters that cannot
    // reach the volume (see update_shadow).
    glm::mat4 shadow_view{1.0f};
    glm::vec3 frame_eye{0.0f};    // camera position, from begin_frame's view

    // The NEAR CASCADE (R6b). Same three things as above at 8x the texel
    // density over 40 m; `near_shadow_active` is separately false when its
    // resources failed or its dose is 0, so the far map alone is always a valid
    // frame and a valid control arm.
    bgfx::TextureHandle shadow_map_near = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle shadow_fb_near = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadow_map_near = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_mtx_near = BGFX_INVALID_HANDLE;
    bool shadow_near_active = false;
    glm::mat4 shadow_view_near{1.0f};

    // Carried-light cube shadows: one atlas, one program, per-face view state.
    bgfx::TextureHandle point_shadow_atlas = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle point_shadow_depth = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle point_shadow_fb = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle point_shadow_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_point_shadow = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_rows = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_caster = BGFX_INVALID_HANDLE;
    // Lights REORDERED so shadow casters come first — the shader then uses the
    // light slot as the cube index and no second lookup table can drift.
    std::array<PointLight, MAX_POINT_LIGHTS> lights{};
    uint32_t light_count = 0;
    uint32_t shadow_light_count = 0; // <= MAX_SHADOW_POINT_LIGHTS, faces valid

    RenderEnvironment environment;       // last set_environment (defaults valid)
    std::array<glm::vec4, 64> palette{}; // fixed palette (Q9b)
    bool palette_post = false;

    struct MeshRes {
        bgfx::VertexBufferHandle vb;
        bgfx::IndexBufferHandle ib;
        // Model-space bounding sphere, measured at upload. It exists so the
        // backend can CULL casters to a light's sphere without a contract
        // change: IRenderer::submit carries no bounds, but create_mesh sees
        // every vertex, so the one place that already has the data keeps it.
        glm::vec3 center{0.0f};
        float radius = 0.0f;
    };
    std::unordered_map<uint32_t, MeshRes> meshes;
    // GPU BUFFER BUDGET. bgfx hands out at most BGFX_MESH_HANDLE_BUDGET
    // (4096) vertex-buffer handles and the same number of index buffers; past
    // that createVertexBuffer returns BGFX_INVALID_HANDLE. Counted here because
    // the failure is otherwise INVISIBLE until it kills the process at
    // shutdown, in a stack that names none of the code that caused it.
    uint32_t peak_meshes = 0;
    uint64_t meshes_created = 0;
    uint64_t meshes_destroyed = 0;
    bool mesh_budget_warned = false;
    std::unordered_map<uint32_t, bgfx::TextureHandle> textures;
    std::unordered_map<uint32_t, bgfx::ProgramHandle> programs;
    std::unordered_map<uint32_t, bool> transparent; // program id -> water-style state
    uint32_t next_id = 1;

    std::vector<DebugVertex> debug_lines; // flushed each end_frame
    std::string pending_screenshot;       // scheduled into the next end_frame
    bool in_frame = false;

    // Embedded-shader lookup by logical name (BgfxRenderer.cpp — the table and
    // the generated headers live with the lifecycle code).
    [[nodiscard]] bgfx::ProgramHandle make_program(const char* name) const;

    // Per-frame helpers (BgfxRendererFrame.cpp). Call-order contracts are
    // documented at the definitions; the short form: every bgfx::touch of a
    // frame must happen before the frame's first setUniform, and order_lights
    // runs before apply_environment / update_point_shadows.
    void order_lights();
    void touch_point_shadow_views();
    void update_point_shadows();
    void apply_environment() const;
    void update_shadow();
    void dest_rect(uint32_t& x, uint32_t& y, uint32_t& w, uint32_t& h) const;
};

} // namespace dfn::platform
