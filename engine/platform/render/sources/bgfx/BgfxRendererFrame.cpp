/*
Created: 10:08:2026 - 01:47:53
Last updated: 10:08:2026 - 20:10:49
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxRendererFrame.cpp

Responsibility:
- The per-frame path of the bgfx backend: begin_frame / set_environment /
  end_frame, the environment + shadow uniform packing, debug lines, and
  screenshot scheduling. One of four translation units over BgfxRendererImpl.h
  (Rule 21 split); lifecycle is BgfxRenderer.cpp, draws BgfxRendererSubmit.cpp,
  handle bookkeeping BgfxRendererResources.cpp.

Key items:
- BgfxRenderer::begin_frame / set_environment / end_frame / debug_line /
  save_screenshot / reload_shaders.
- Impl::order_lights / touch_point_shadow_views / update_point_shadows /
  apply_environment / update_shadow / dest_rect.

Dependencies:
- Uses: BgfxRendererImpl.h, bgfx, glm.
- Used by: dfn_platform_render target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EMPTY DRAWS EAT UNIFORMS (named rule, docs/specs/render.md): every
  bgfx::touch of a frame must be issued BEFORE the frame's first setUniform.
  touch_point_shadow_views runs at the top of begin_frame for exactly this
  reason; a new view that needs a clear joins that block, never clears where
  it is convenient.
*/
/*
UPD:
- 10:08:2026 - 01:47:53: Created in the Rule 21 split of BgfxRenderer.cpp.
  Frame path moved verbatim; no behaviour change.
- 10:08:2026 - 03:04:30: Cloud slots 33/34 packed in apply_environment (W4
  state + the one drift offset; paired with dfn_env.sh's 35-slot layout).
- 10:08:2026 - 20:10:49: Slot 35 packed with the sun's body straight from the
  generated constants (SUN_ANGULAR_DIAMETER / SUN_GLARE_ANGULAR_DIAMETER /
  SUN_DISC_LUMA / SUN_GLARE_LUMA_MAX), never through RenderEnvironment — they
  are NUMBERS rows with two consumers and exist once (Rule 35).
*/

#include "engine/platform/render/sources/bgfx/BgfxRendererImpl.h"

#include "engine/core/config/sources/Constants.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dfn::platform {

namespace {

// Look-dev clear color (sky), not a gameplay constant: light steppe-sky blue.
constexpr uint32_t SKY_COLOR_RGBA = 0x87b5e0ff;

} // namespace

// Orders the frame's point lights so SHADOW CASTERS COME FIRST. The shader
// then treats the light slot as the cube-atlas index, which removes the
// only place a light->map mapping could disagree with itself. Must run
// before apply_environment and update_point_shadows, and both of those are
// driven from the same two call sites, so they cannot see different orders.
void BgfxRenderer::Impl::order_lights() {
    light_count = 0;
    shadow_light_count = 0;
    const uint32_t incoming = environment.point_light_count < MAX_POINT_LIGHTS
                                  ? environment.point_light_count
                                  : MAX_POINT_LIGHTS;
    const bool maps_ok = bgfx::isValid(point_shadow_fb)
                      && bgfx::isValid(point_shadow_program);
    for (uint32_t pass = 0; pass < 2; ++pass) {
        for (uint32_t i = 0; i < incoming; ++i) {
            const PointLight& l = environment.point_lights[i];
            if (l.radius_m <= 0.0f) {
                continue; // radius 0 = off (contract)
            }
            const bool shadowing = maps_ok && l.casts_shadow
                                && shadow_light_count < MAX_SHADOW_POINT_LIGHTS;
            if ((pass == 0) != shadowing) {
                continue;
            }
            lights[light_count] = l;
            ++light_count;
            if (pass == 0) {
                ++shadow_light_count;
            }
        }
    }
}

// Framebuffer/rect/clear + touch for EVERY face tile, whether a light uses
// it this frame or not. Two reasons it is separate from update_point_shadows
// and lives at the top of begin_frame:
//  1. A tile with no light must still be cleared to "unoccluded", or a
//     torch that goes out leaves its last frame baked into the atlas.
//  2. A touch is an EMPTY DRAW, and an empty draw SWALLOWS the pending
//     uniform range without ever applying it. Touching after
//     apply_environment cost this project one debugging session: the whole
//     world rendered with the default (daylit) environment the moment a
//     torch was lit, because the night env had been recorded into a touch
//     that bgfx then skipped. Every touch happens BEFORE any setUniform of
//     the frame; the uniforms then attach to the sky draw, which is real.
void BgfxRenderer::Impl::touch_point_shadow_views() {
    if (!bgfx::isValid(point_shadow_fb)) {
        return;
    }
    for (uint32_t tile = 0; tile < POINT_SHADOW_VIEWS; ++tile) {
        const uint16_t col = static_cast<uint16_t>(tile % POINT_SHADOW_ATLAS_COLS);
        const uint16_t row = static_cast<uint16_t>(tile / POINT_SHADOW_ATLAS_COLS);
        const auto vid =
            static_cast<bgfx::ViewId>(VIEW_POINT_SHADOW_FIRST + tile);
        bgfx::setViewFrameBuffer(vid, point_shadow_fb);
        bgfx::setViewRect(vid, static_cast<uint16_t>(col * POINT_SHADOW_FACE_PX),
                          static_cast<uint16_t>(row * POINT_SHADOW_FACE_PX),
                          POINT_SHADOW_FACE_PX, POINT_SHADOW_FACE_PX);
        // Clear to 1.0 = "farther than the radius" = lit, so a face with no
        // casters is transparent to light rather than black. Depth is a
        // normal LESS pass: this view is its own pipeline and owes nothing
        // to the scene's reversed-Z.
        bgfx::setViewClear(vid, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           0xffffffff, 1.0f, 0);
        bgfx::touch(vid);
    }
}

// Builds the six 90-degree face views for each shadow-casting light and
// uploads the matrices the receivers sample with. Each face is a rectangle
// of ONE atlas framebuffer, so the tile scale/offset is baked into the
// matrix and the shader needs no tile arithmetic.
void BgfxRenderer::Impl::update_point_shadows() {
    glm::vec4 rows[MAX_SHADOW_POINT_LIGHTS * POINT_SHADOW_FACES * 4]{};
    const bgfx::Caps& caps = *bgfx::getCaps();
    for (uint32_t li = 0; li < shadow_light_count; ++li) {
        const PointLight& l = lights[li];
        for (uint32_t f = 0; f < POINT_SHADOW_FACES; ++f) {
            const uint32_t tile = li * POINT_SHADOW_FACES + f;
            const uint16_t col = static_cast<uint16_t>(tile % POINT_SHADOW_ATLAS_COLS);
            const uint16_t row = static_cast<uint16_t>(tile / POINT_SHADOW_ATLAS_COLS);
            const glm::mat4 view = glm::lookAtRH(
                l.position, l.position + POINT_SHADOW_FACE_DIR[f],
                POINT_SHADOW_FACE_UP[f]);
            const float fov = glm::radians(90.0f);
            const glm::mat4 proj =
                caps.homogeneousDepth
                    ? glm::perspectiveRH_NO(fov, 1.0f, POINT_SHADOW_NEAR_M,
                                            l.radius_m)
                    : glm::perspectiveRH_ZO(fov, 1.0f, POINT_SHADOW_NEAR_M,
                                            l.radius_m);
            const bgfx::ViewId vid = static_cast<bgfx::ViewId>(
                VIEW_POINT_SHADOW_FIRST + tile);
            // Framebuffer, rect, clear and touch were done up front by
            // touch_point_shadow_views — NEVER touch from here (see the
            // comment there: an empty draw eats the pending uniforms).
            bgfx::setViewTransform(vid, glm::value_ptr(view),
                                   glm::value_ptr(proj));

            // clip -> [0,1] uv (API y flip), then into the atlas tile.
            glm::mat4 crop(1.0f);
            crop[0][0] = 0.5f;
            crop[1][1] = caps.originBottomLeft ? 0.5f : -0.5f;
            crop[3] = glm::vec4(0.5f, 0.5f, 0.0f, 1.0f);
            const float sx = 1.0f / static_cast<float>(POINT_SHADOW_ATLAS_COLS);
            const float sy = 1.0f / static_cast<float>(POINT_SHADOW_ATLAS_ROWS);
            glm::mat4 tile_mtx(1.0f);
            tile_mtx[0][0] = sx;
            tile_mtx[1][1] = sy;
            tile_mtx[3] = glm::vec4(static_cast<float>(col) * sx,
                                    static_cast<float>(row) * sy, 0.0f, 1.0f);
            const glm::mat4 m = tile_mtx * crop * proj * view;
            // Stored as ROWS: the shader indexes them dynamically and
            // takes three dot products, which needs no matrix assembly
            // from a computed offset.
            for (uint32_t r = 0; r < 4; ++r) {
                rows[tile * 4 + r] =
                    glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]);
            }
        }
    }
    bgfx::setUniform(u_point_shadow_rows, rows,
                     MAX_SHADOW_POINT_LIGHTS * POINT_SHADOW_FACES * 4);
    const float params[4] = {static_cast<float>(shadow_light_count),
                             POINT_SHADOW_NORMAL_OFFSET_M,
                             POINT_SHADOW_BIAS_FRAC, 0.0f};
    bgfx::setUniform(u_point_shadow_params, params);
}

// Packs the cached RenderEnvironment into u_envParams; the index layout is
// the dfn_env.sh contract. Called once per frame in begin_frame — uniform
// values persist across submits within the frame.
void BgfxRenderer::Impl::apply_environment() const {
    const RenderEnvironment& e = environment;
    glm::vec4 packed[ENV_PARAM_VEC4S] = {  // trailing slots zero-init
        {e.sun_direction, 0.0f},
        {e.sun_color, 0.0f},
        {e.ambient_color, 0.0f},
        {e.fog_color, 0.0f},
        {e.fog_start_m, e.fog_end_m, e.time_seconds, 0.0f},
        {e.sky_zenith_color, 0.0f},
        {e.sky_horizon_color, 0.0f},
        {e.sand_height_m, e.sand_blend_m, e.rock_slope_start, e.rock_slope_end},
        {e.terrain_tiles_per_chunk, 0.0f, 0.0f, 0.0f},
        e.water_color,
        {e.water_scroll_uv, 0.0f, 0.0f},
        {e.moon_direction, e.moon_phase},
        {e.moon_color, e.moon_light},
        {0.0f, 0.0f, 0.0f, 0.0f}, // [13] reserved (was the single light)
        {0.0f, 0.0f, 0.0f, e.star_intensity},
    };
    // Lights come from the ORDERED array (order_lights), not straight from
    // the environment: shadow casters must occupy the first slots because
    // the shader uses the slot as the cube-atlas index.
    packed[15] = {e.ambient_darkness, static_cast<float>(light_count), 0.0f,
                  0.0f};
    packed[32] = {e.wind_direction, e.wind_strength, e.wind_flutter};
    // Clouds (W4): state tuple + the ONE drift offset both samplers read.
    packed[33] = {e.cloud_cover, e.cloud_cumulus, e.cloud_shadow,
                  e.cloud_wavelength_m};
    packed[34] = {e.cloud_offset_m, e.weather_wind_mult, 0.0f};
    // THE SUN'S BODY (W9). Straight from the generated header, never through
    // RenderEnvironment: these are NUMBERS rows with two consumers by
    // construction — design derives them, the shader measures the frame with
    // them — so they exist once and travel to the only place that reads them
    // (Rule 35). Radii, because the shader compares against an angle; the
    // rows are diameters because that is how a sky is described.
    packed[35] = {0.5f * static_cast<float>(config::SUN_ANGULAR_DIAMETER),
                  0.5f * static_cast<float>(config::SUN_GLARE_ANGULAR_DIAMETER),
                  static_cast<float>(config::SUN_DISC_LUMA),
                  static_cast<float>(config::SUN_GLARE_LUMA_MAX)};
    for (uint32_t i = 0; i < light_count; ++i) {
        const PointLight& l = lights[i];
        packed[16 + i] = {l.position, l.radius_m};
        packed[24 + i] = {l.color, l.casts_shadow ? 1.0f : 0.0f};
    }
    bgfx::setUniform(u_env_params, packed, ENV_PARAM_VEC4S);
}

// Recomputes the sun-light matrices from the cached environment + frame
// eye and uploads u_lightMtx / u_shadowParams. Called from begin_frame and
// from a mid-frame set_environment (uniform values are captured per
// submit, so the app's day/night set_environment right after begin_frame
// applies to every draw of the frame; view transforms resolve at frame
// render, last call wins).
void BgfxRenderer::Impl::update_shadow() {
    shadow_active = false;
    float enabled = 0.0f;
    glm::mat4 light_mtx(1.0f);
    glm::vec3 dir = environment.sun_direction;
    const float len = glm::length(dir);
    if (bgfx::isValid(shadow_fb) && bgfx::isValid(shadow_program)
        && len > 1e-5f) {
        dir /= len;
        if (dir.y > SHADOW_MIN_SUN_ELEVATION) {
            shadow_active = true;
            enabled = 1.0f;
            constexpr float H = SHADOW_HALF_EXTENT_M;
            constexpr float D = SHADOW_DEPTH_HALF_M;
            // Orientation-only light view; the volume center (the camera
            // eye) is snapped to the shadow texel grid in light space so
            // edges do not crawl as the camera moves.
            const glm::vec3 up = std::fabs(dir.y) > 0.99f
                                     ? glm::vec3(0.0f, 0.0f, 1.0f)
                                     : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::mat4 rot = glm::lookAtRH(dir, glm::vec3(0.0f), up);
            glm::vec3 c = glm::vec3(rot * glm::vec4(frame_eye, 1.0f));
            c.x = std::floor(c.x / SHADOW_TEXEL_M) * SHADOW_TEXEL_M;
            c.y = std::floor(c.y / SHADOW_TEXEL_M) * SHADOW_TEXEL_M;
            const glm::mat4 view =
                glm::translate(glm::mat4(1.0f), -c) * rot;
            // Kept so `submit` can reject casters that lie outside this
            // volume. Without it every LOD node at 1-4 km pays for a full
            // depth draw into a map that ends at 320 m — the terrain LOD
            // ladder's whole point is that distant ground is cheap, and a
            // free shadow draw per node undoes it.
            shadow_view = view;
            const bgfx::Caps& caps = *bgfx::getCaps();
            // near = -D / far = +D brackets the eye plane along the light.
            const glm::mat4 proj =
                caps.homogeneousDepth
                    ? glm::orthoRH_NO(-H, H, -H, H, -D, D)
                    : glm::orthoRH_ZO(-H, H, -H, H, -D, D);
            bgfx::setViewTransform(VIEW_SHADOW, glm::value_ptr(view),
                                   glm::value_ptr(proj));
            // NDC -> shadow-map uv/depth crop (API-dependent y flip and
            // depth range), premultiplied for the fragment shaders.
            glm::mat4 crop(1.0f);
            crop[0][0] = 0.5f;
            crop[1][1] = caps.originBottomLeft ? 0.5f : -0.5f;
            crop[2][2] = caps.homogeneousDepth ? 0.5f : 1.0f;
            crop[3] = glm::vec4(0.5f, 0.5f,
                                caps.homogeneousDepth ? 0.5f : 0.0f, 1.0f);
            light_mtx = crop * proj * view;
        }
    }
    const float params[4] = {enabled, SHADOW_NORMAL_OFFSET_M,
                             SHADOW_DEPTH_BIAS_M / (2.0f * SHADOW_DEPTH_HALF_M),
                             0.0f};
    bgfx::setUniform(u_shadow_params, params);
    bgfx::setUniform(u_light_mtx, glm::value_ptr(light_mtx));
}

// Largest integer factor of the internal target fitting the framebuffer,
// centered; never zero (Q9).
void BgfxRenderer::Impl::dest_rect(uint32_t& x, uint32_t& y, uint32_t& w,
                                   uint32_t& h) const {
    const uint32_t fx = internal_width > 0 ? fb_width / internal_width : 1;
    const uint32_t fy = internal_height > 0 ? fb_height / internal_height : 1;
    const uint32_t factor = std::max(1u, std::min(fx, fy));
    w = std::min(internal_width * factor, fb_width);
    h = std::min(internal_height * factor, fb_height);
    x = (fb_width - w) / 2;
    y = (fb_height - h) / 2;
}

void BgfxRenderer::begin_frame(const glm::mat4& view, const glm::mat4& proj) {
    Impl& im = *impl_;
    if (!im.initialized) {
        return;
    }
    im.in_frame = true;
    bgfx::setViewFrameBuffer(VIEW_SCENE, im.internal_fb);
    bgfx::setViewRect(VIEW_SCENE, 0, 0, static_cast<uint16_t>(im.internal_width),
                      static_cast<uint16_t>(im.internal_height));
    // REVERSED-Z: the far plane is depth 0 and the near plane depth 1, so the
    // buffer clears to 0 and comparisons are GREATER. With CAMERA_FAR at 8 km
    // against a 0.1 m near plane this is what stops distant mountains from
    // z-fighting: floating-point depth has its precision where the exponent is
    // small, which reversed-Z lines up with the far distances instead of
    // wasting it all in the first metre.
    bgfx::setViewClear(VIEW_SCENE, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       SKY_COLOR_RGBA, 0.0f, 0);
    // Reverse the depth range here rather than in the camera: the depth buffer
    // format and its comparison direction are backend concerns, and
    // FirstPersonCamera stays a plain RH_ZO perspective that tests can reason
    // about. z' = 1 - z on a 0..1 clip range.
    glm::mat4 reversed = proj;
    reversed[0][2] = -proj[0][2];
    reversed[1][2] = -proj[1][2];
    reversed[2][2] = -proj[2][2] + proj[2][3];
    reversed[3][2] = -proj[3][2] + proj[3][3];
    bgfx::setViewTransform(VIEW_SCENE, glm::value_ptr(view), glm::value_ptr(reversed));
    bgfx::touch(VIEW_SCENE); // clear even on an empty frame

    // Shadow view: renders (view id order) before the scene consumes the map.
    im.frame_eye = glm::vec3(glm::inverse(view)[3]);
    if (bgfx::isValid(im.shadow_fb)) {
        bgfx::setViewFrameBuffer(VIEW_SHADOW, im.shadow_fb);
        bgfx::setViewRect(VIEW_SHADOW, 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
        bgfx::setViewClear(VIEW_SHADOW, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
        bgfx::touch(VIEW_SHADOW);
    }
    // Every empty draw of the frame happens HERE, before the first setUniform:
    // bgfx attaches pending uniforms to the next submitted draw, and a touch is
    // a draw that gets skipped, taking the uniforms with it.
    im.touch_point_shadow_views();
    im.update_shadow();

    // Frame environment (cached; struct defaults are valid pre-first-set).
    // order_lights runs FIRST: both the uniform packing and the cube-face
    // views read the ordered array, and they must never see different orders.
    im.order_lights();
    im.apply_environment();
    im.update_point_shadows();

    // Sky background: fullscreen quad, no depth test/write; everything solid
    // draws over it (sequential view — this is always the first draw).
    if (bgfx::isValid(im.sky_program)) {
        bgfx::setVertexBuffer(0, im.quad_vb);
        bgfx::setIndexBuffer(im.quad_ib);
        bgfx::setState(BGFX_STATE_WRITE_RGB);
        bgfx::submit(VIEW_SCENE, im.sky_program);
    }
}

void BgfxRenderer::set_environment(const RenderEnvironment& env) {
    impl_->environment = env;
    // Uniforms are (re)applied at the next begin_frame; if we are mid-frame,
    // update them now so this frame's remaining submits see the new values —
    // including the shadow matrices, so the app-animated sun (day/night, в2)
    // moves the shadows in the same frame it moves the light.
    if (impl_->initialized && impl_->in_frame) {
        impl_->order_lights();
        impl_->apply_environment();
        impl_->update_shadow();
        // The carried light moves with the player every frame, so its faces
        // are rebuilt here as well — a torch whose shadow map lagged the flame
        // by a frame would smear on every step.
        impl_->update_point_shadows();
    }
}

void BgfxRenderer::end_frame() {
    Impl& im = *impl_;
    if (!im.initialized || !im.in_frame) {
        return;
    }

    // One-frame debug lines (accumulated by debug_line).
    if (!im.debug_lines.empty() && bgfx::isValid(im.debug_program)) {
        const auto count = static_cast<uint32_t>(im.debug_lines.size());
        if (bgfx::getAvailTransientVertexBuffer(count, im.debug_layout) >= count) {
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, count, im.debug_layout);
            std::memcpy(tvb.data, im.debug_lines.data(), count * sizeof(DebugVertex));
            bgfx::setVertexBuffer(0, &tvb, 0, count);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                           | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER
                           | BGFX_STATE_PT_LINES);
            bgfx::submit(VIEW_SCENE, im.debug_program);
        }
    }
    im.debug_lines.clear();

    // Letterbox clear, then the integer-scaled point-sampled upscale (Q9).
    bgfx::setViewFrameBuffer(VIEW_BACKBUFFER, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(VIEW_BACKBUFFER, 0, 0, static_cast<uint16_t>(im.fb_width),
                      static_cast<uint16_t>(im.fb_height));
    bgfx::setViewClear(VIEW_BACKBUFFER, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
    bgfx::touch(VIEW_BACKBUFFER);

    if (bgfx::isValid(im.upscale_program)) {
        uint32_t dx = 0;
        uint32_t dy = 0;
        uint32_t dw = 0;
        uint32_t dh = 0;
        im.dest_rect(dx, dy, dw, dh);
        bgfx::setViewFrameBuffer(VIEW_UPSCALE, BGFX_INVALID_HANDLE);
        bgfx::setViewRect(VIEW_UPSCALE, static_cast<uint16_t>(dx),
                          static_cast<uint16_t>(dy), static_cast<uint16_t>(dw),
                          static_cast<uint16_t>(dh));
        const float post[4] = {im.palette_post ? 1.0f : 0.0f,
                               static_cast<float>(PALETTE_SIZE),
                               static_cast<float>(im.internal_width),
                               static_cast<float>(im.internal_height)};
        bgfx::setUniform(im.u_post_params, post);
        if (im.palette_post) {
            bgfx::setUniform(im.u_palette, im.palette.data(), PALETTE_SIZE);
        }
        bgfx::setTexture(0, im.s_tex_color, bgfx::getTexture(im.internal_fb, 0));
        bgfx::setVertexBuffer(0, im.quad_vb);
        bgfx::setIndexBuffer(im.quad_ib);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(VIEW_UPSCALE, im.upscale_program);
    }

    if (!im.pending_screenshot.empty()) {
        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, im.pending_screenshot.c_str());
        im.pending_screenshot.clear();
    }

    bgfx::frame();
    im.in_frame = false;
}

void BgfxRenderer::debug_line(const glm::vec3& from, const glm::vec3& to,
                              uint32_t color_rgba) {
#if defined(NDEBUG)
    (void)from;
    (void)to;
    (void)color_rgba;
#else
    impl_->debug_lines.push_back({from.x, from.y, from.z, color_rgba});
    impl_->debug_lines.push_back({to.x, to.y, to.z, color_rgba});
#endif
}

bool BgfxRenderer::save_screenshot(const std::string& path) {
    Impl& im = *impl_;
    if (!im.initialized) {
        return false;
    }
    // Scheduled into the next end_frame (bgfx captures during frame
    // processing); the Tour renders flush frames after scheduling.
    im.pending_screenshot = path;
    return true;
}

void BgfxRenderer::reload_shaders() {
    // Debug no-op this stage: shaders are embedded at build time. Disk-based
    // hot-reload (Q50) arrives with the stage-3 material work.
}

} // namespace dfn::platform
