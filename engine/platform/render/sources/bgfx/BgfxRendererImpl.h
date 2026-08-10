/*
Created: 10:08:2026 - 01:47:53
Last updated: 10:08:2026 - 03:04:00
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
*/

#pragma once

#include "engine/platform/render/sources/bgfx/BgfxCallback.h"
#include "engine/platform/render/sources/bgfx/BgfxRenderer.h"

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace dfn::platform {

inline constexpr bgfx::ViewId VIEW_SHADOW = 0;  // -> sun shadow map (depth only)
// Carried-light cube shadows: MAX_SHADOW_POINT_LIGHTS x 6 faces, each face one
// view into a shared atlas. Views render in id order, so every face is
// finished before the scene samples it.
inline constexpr bgfx::ViewId VIEW_POINT_SHADOW_FIRST = 1;
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

inline constexpr uint16_t ENV_PARAM_VEC4S = 35; // layout contract with dfn_env.sh
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
    bgfx::UniformHandle s_shadow_map = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_mtx = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params = BGFX_INVALID_HANDLE;
    bool shadow_active = false;   // this frame: sun above threshold + resources ok
    // The sun shadow map's LIGHT-SPACE view matrix for this frame. Valid only
    // while shadow_active; `submit` uses it to reject casters that cannot
    // reach the volume (see update_shadow).
    glm::mat4 shadow_view{1.0f};
    glm::vec3 frame_eye{0.0f};    // camera position, from begin_frame's view

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
