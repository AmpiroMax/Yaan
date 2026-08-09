/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 11:05:00
Module: engine/render
File: engine/render/sources/Tour.cpp

Responsibility:
- Screenshot tour implementation (Rule 27, Q51): env parsing, step walking,
  screenshot scheduling with flush frames, the default acceptance route.

Key items:
- Tour methods; default_steps() (4 vantages over the flat test chunk).

Dependencies:
- Uses: Tour.h, IRenderer, dfn::config, std::filesystem.
- Used by: dfn_render target; driven by engine/app when DFN_TOUR=1.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Debug tooling: no simulation or gameplay knowledge in here.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 10:29:00: default_steps takes ground_height (terrain height at
  the chunk center): the old absolute eye heights put vantages 00-02 well
  under the generated surface (~24 m at the center, seed 1), rendering the
  terrain underside above the horizon — mistaken for a vertically flipped
  image. See Tour.h UPD for the full root cause.
- 09:08:2026 - 11:05:00: Stage 3 — Tour v2 route: six vantages targeting the
  stage-3 checklist (texture tiling, fog/horizon, slope splat, water valley,
  overview, sky+sun).
*/

#include "engine/render/sources/Tour.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <utility>

namespace dfn::render {

namespace {

// Frames rendered after scheduling a screenshot so async backends (bgfx
// captures during the following frame) finish writing before we advance.
constexpr uint32_t FLUSH_FRAMES = 2;

const char* env_or_null(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

} // namespace

bool Tour::enabled_by_env() {
    const char* value = env_or_null("DFN_TOUR");
    return value != nullptr && !(value[0] == '0' && value[1] == '\0');
}

glm::uvec2 Tour::internal_res_from_env(glm::uvec2 fallback) {
    const char* value = env_or_null("DFN_INTERNAL_RES");
    if (value == nullptr) {
        return fallback;
    }
    unsigned w = 0;
    unsigned h = 0;
    if (std::sscanf(value, "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
        return {w, h};
    }
    std::fprintf(stderr, "[tour] malformed DFN_INTERNAL_RES '%s' (want WxH)\n", value);
    return fallback;
}

void Tour::begin(std::vector<TourStep> steps, std::string output_dir) {
    steps_ = std::move(steps);
    if (output_dir.empty()) {
        const char* dir = env_or_null("DFN_TOUR_DIR");
        output_dir = dir != nullptr ? dir : "screenshots";
    }
    output_dir_ = std::move(output_dir);
    std::error_code ec;
    std::filesystem::create_directories(output_dir_, ec); // best effort
    step_ = 0;
    frames_waited_ = 0;
    flush_left_ = 0;
    active_ = !steps_.empty();
}

bool Tour::active() const {
    return active_;
}

uint32_t Tour::current_step() const {
    return step_;
}

void Tour::apply(FirstPersonCamera& camera) const {
    if (!active_) {
        return;
    }
    const TourStep& step = steps_[step_];
    const CameraPose pose{step.position, step.yaw, step.pitch};
    camera.set_poses(pose, pose); // static vantage: interpolation is a no-op
}

bool Tour::post_frame(platform::IRenderer& renderer) {
    if (!active_) {
        return true;
    }
    const TourStep& step = steps_[step_];

    if (flush_left_ > 0) { // screenshot scheduled: let the backend finish
        if (--flush_left_ > 0) {
            return false;
        }
        // Flush done — advance.
        ++step_;
        frames_waited_ = 0;
        if (step_ >= steps_.size()) {
            active_ = false;
            return true; // the app closes the window now
        }
        return false;
    }

    if (frames_waited_ < step.wait_frames) {
        ++frames_waited_;
        return false;
    }

    char name[32];
    std::snprintf(name, sizeof(name), "%02u_", step_);
    const std::string path = output_dir_ + "/" + name + step.label + ".png";
    if (!renderer.save_screenshot(path)) {
        // Null backend (headless smoke run): keep walking the route (Rule 3).
        std::fprintf(stderr, "[tour] screenshot unsupported, skipped: %s\n",
                     path.c_str());
    }
    flush_left_ = FLUSH_FRAMES;
    return false;
}

std::vector<TourStep> Tour::default_steps(float ground_height) {
    // Stage-3 acceptance route («Картинка»): six vantages over the test chunk
    // (chunk (0,0) spans 0..CHUNK_SIZE on x/z), each aimed at one checklist
    // item — texture detail/tiling, fog+horizon blend, slope splat, water in
    // the valleys (with DFN_WATER), overview, sky gradient + sun. Positions
    // derive from NUMBERS.md constants; `ground_height` is the terrain height
    // at the chunk center, supplied by the app (the tour has no world access)
    // so eye-level vantages sit above the generated surface.
    const float size = static_cast<float>(config::CHUNK_SIZE);
    const float mid = size * 0.5f;
    const float eye = ground_height + static_cast<float>(config::PLAYER_EYE_HEIGHT);
    // All eye-level vantages aim INTO the testbed interior (from the spawn
    // corner that is yaw ~1.6..2.9): outward aims put the unloaded world edge
    // inside the fog-free range and break the horizon (seen in look-dev).
    return {
        {"eye_texture", {mid, eye, mid}, 2.0f, -0.5f, 30},
        {"eye_horizon", {mid, eye, mid}, 2.3562f, -0.02f, 10},
        {"slope_splat", {size * 0.7f, eye + 18.0f, size * 0.7f}, 2.3562f, -0.35f, 10},
        {"water_valley", {mid, ground_height + 30.0f, mid}, 2.3562f, -0.5f, 10},
        {"overview", {mid, ground_height + 60.0f, mid}, 2.3562f, -0.9f, 10},
        {"sky_sun", {mid, eye, mid}, 2.48f, 0.45f, 10},
    };
}

} // namespace dfn::render
