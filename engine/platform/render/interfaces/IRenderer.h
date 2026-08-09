/*
Created: 09:08:2026 - 00:06:00
Last updated: 10:08:2026 - 02:56:25
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
- 09:08:2026 - 10:48:00: Stage-3 sync (Rule 26, render's batch approved):
                         RenderEnvironment + set_environment (atmosphere, splat,
                         water params as uniforms); palette_post init flag (Q9b).
- 09:08:2026 - 18:56:38: Night sky + one carried point light appended to
                         RenderEnvironment (render's diff, lead-authored per
                         Rule 26): moon direction/colour/phase/light, star
                         intensity, torch position/colour/radius. Pure
                         addition — no field changed or reordered, both
                         backends keep compiling.
- 09:08:2026 - 19:09:18: Point LIGHT ARRAY replaces the single point light
                         (user wants shadows from several sources) +
                         ambient_darkness for authored pitch-black places.
                         Render's diff, lead-authored per Rule 26.
- 09:08:2026 - 19:21:01: Deprecated single-point-light fields deleted now
                         that render's backend and tests use the array.
- 09:08:2026 - 20:01:56: Wind (direction/strength/flutter) for foliage, grass
- 09:08:2026 - 21:01:17: DrawParams — per-draw material parameters (fade,
                         highlight, two reserved). Render's diff, lead-authored
                         per Rule 26; the four-argument submit stays as a
                         convenience so no existing call site changes.
                         and cloth — one wind for the world. Render's diff,
                         lead-authored per Rule 26.
- 10:08:2026 - 02:56:25: Weather cloud slice: six additive RenderEnvironment fields (render's diff, Rule 26 sync). Defaults = the scattered state; cloud_offset_m is the ONE drift both sky and ground shadow read.
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
    bool palette_post = false;            // Q9b: quantize the final image to a fixed
                                          // palette inside the upscale pass
};

// One point light. `casts_shadow` is honoured for the first
// MAX_SHADOW_POINT_LIGHTS lights that request it; the rest light without
// shadowing.
struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{0.0f};     // linear; black = off
    float radius_m = 0.0f;     // 0 = off
    bool casts_shadow = false;
};

inline constexpr uint32_t MAX_POINT_LIGHTS = 8;
inline constexpr uint32_t MAX_SHADOW_POINT_LIGHTS = 2;

// PER-DRAW material parameters. Deliberately one small struct rather than a
// field per feature: the environment block is per FRAME, vertex alpha is
// worldgen's sky visibility, and the transform's unused row is not a place to
// smuggle a float through. At least three known consumers want exactly this —
// the LOD cross-fade (both levels of the same ground on screen at once,
// dithered against each other), the interaction highlight (HoverTarget already
// names the hovered entity and render has no way to draw it differently from
// its neighbours), and later damage flashes, magic glow and wetness. Inventing
// a special case per feature is how this ends up as three incompatible hacks.
struct DrawParams {
    float fade = 1.0f;      // 1 = fully drawn; screen-door dither below that
    float highlight = 0.0f; // 0 = none; interaction hover and similar
    float aux0 = 0.0f;      // reserved, meaning is the program's
    float aux1 = 0.0f;
};

// Per-frame environment + shared material parameters (atmosphere, splat
// thresholds, water). Values come from engine/render (look-dev constants now,
// design-doc-driven later); the backend maps them to shader uniforms, so they
// are adjustable without recompiling shaders.
struct RenderEnvironment {
    glm::vec3 sun_direction{0.35f, 0.8f, 0.45f}; // TOWARD the sun, normalized
    glm::vec3 sun_color{1.0f};
    glm::vec3 ambient_color{0.35f};
    glm::vec3 fog_color{0.63f, 0.71f, 0.80f};    // == sky horizon for seamless blend
    float fog_start_m = 300.0f;
    float fog_end_m = 850.0f;
    glm::vec3 sky_zenith_color{0.25f, 0.42f, 0.66f};
    glm::vec3 sky_horizon_color{0.63f, 0.71f, 0.80f};
    // Terrain splat (slope = 1 - normal.y):
    float sand_height_m = 0.0f;   // full sand below this height
    float sand_blend_m = 2.0f;    // sand->grass fade band
    float rock_slope_start = 0.45f;
    float rock_slope_end = 0.75f;
    float terrain_tiles_per_chunk = 32.0f; // texture repeats per CHUNK_SIZE
    // Water:
    glm::vec4 water_color{0.16f, 0.30f, 0.34f, 0.62f}; // rgba, a = opacity
    glm::vec2 water_scroll_uv{0.02f, 0.013f};          // uv units / second
    float time_seconds = 0.0f;    // render-side visual time (not sim time)

    // Night sky. The app's clock drives all of these (render::apply_sky_time).
    glm::vec3 moon_direction{0.0f, -1.0f, 0.0f}; // TOWARD the moon, normalized;
                                                 // below the horizon = not drawn
    glm::vec3 moon_color{0.72f, 0.76f, 0.90f};   // disc colour (cold white)
    float moon_phase = 0.5f;      // 0 = new, 0.5 = full, wraps at 1.0
    float moon_light = 0.0f;      // 0..1 directional moonlight on the ground;
                                  // separate from moon_color so an overcast
                                  // night can dim the light, not the disc
    float star_intensity = 0.0f;  // 0 = day, 1 = clear night. Explicit rather
                                  // than derived from sun elevation: overcast
                                  // means stars off with the sun untouched

    // Point lights (torch, braziers, lit windows). The user asked for shadows
    // cast by SEVERAL sources, so the single light this replaced was outgrown
    // before it shipped. Cost of the shadowing half, measured by render: a
    // point light needs a CUBE map (6 faces), so casters in range are
    // submitted 6x more — affordable only because casters are culled to the
    // light's sphere (in a tunnel ~6 casters = ~36 extra draws), which is why
    // the shadow-casting RADIUS is capped rather than the honesty.
    PointLight point_lights[MAX_POINT_LIGHTS];
    uint32_t point_light_count = 0;


    // Wind. ONE wind for the world: foliage now, grass and cloth later, so a
    // second wind can never be invented alongside this one and diverge.
    glm::vec2 wind_direction{1.0f, 0.0f}; // normalized, world x/z
    float wind_strength = 0.0f;           // 0..1, CURRENT value INCLUDING gusts.
                                          // Computed CPU-side per frame and
                                          // shipped as one scalar ON PURPOSE:
                                          // if gusts were derived from time
                                          // inside the vertex shader, nothing
                                          // outside the GPU could know when a
                                          // gust peaks, and an audio rustle
                                          // could never be synced to the
                                          // picture except by luck. Gameplay
                                          // can read it too ("wait for the wind
                                          // to drop").
    float wind_flutter = 1.0f;            // multiplier on the fast component

    // Authored darkness of the PLACE the player is in (user: night stays
    // playable, but some interiors are a black void where a torch lights only
    // a small patch). 0 = normal, 1 = void. The GEOMETRIC half of darkness is
    // separate: per-vertex sky visibility written by worldgen. This is the
    // authored half — the app sets it from the darkness zone and lerps across
    // the boundary, so it is deliberately NOT per-vertex.
    float ambient_darkness = 0.0f;

    // Weather state tuple, cloud slice (W1/W4 of WEATHER.md; render's diff,
    // lead-authored). Defaults are the "scattered" state so the sky is alive
    // before core's schedule function exists; the app will later write these
    // from the schedule without any render change.
    float cloud_cover = 0.45f;      // 0..1 layered-sheet coverage (W1 cloud_layer_cover)
    float cloud_cumulus = 0.5f;     // 0..1 horizon cumulus density (W1 cloud_cumulus)
    float cloud_shadow = 0.65f;     // 0..1 ground-shadow darkening strength (W1 cloud_shadow_cover)
    float weather_wind_mult = 1.0f; // W1 wind_strength: state multiplier on the SHARED wind (W3)
    glm::vec2 cloud_offset_m{0.0f, 0.0f}; // world-space drift of the ONE coverage field, meters.
                                    // Written per frame by render (CloudModel) from the shared
                                    // wind; BOTH the sky sheet and the ground shadow read THIS
                                    // value, which is what makes two drifting copies impossible
                                    // (W4's named reject).
    float cloud_wavelength_m = 600.0f; // coverage feature size (NUMBERS WIND_FIELD_WAVELENGTH)
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

    // Sets the frame environment; applies to subsequent submits until changed.
    // Null backend: accepted and ignored.
    virtual void set_environment(const RenderEnvironment& env) = 0;

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
    // The five-argument form takes per-draw material parameters; the shorter one
    // is a convenience that passes the defaults, so existing call sites are
    // unchanged. Backends implement the virtual.
    virtual void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                        TextureHandle texture, const DrawParams& params) = 0;
    void submit(MeshHandle mesh, ProgramHandle program, const glm::mat4& transform,
                TextureHandle texture = {}) {
        submit(mesh, program, transform, texture, DrawParams{});
    }

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
