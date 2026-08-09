/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 21:20:00
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
- 09:08:2026 - 11:57:20: Stage 3b — Tour v3: lazy ground resolution
  (ground_relative steps + begin ground_at callback), focus_position() for
  tour-driven streaming, testbed_steps() aimed at the LANDSCAPE §7.1 layout
  (crag money shot, river ford, lake bluff, hamlet approach, forest species,
  overview).
- 09:08:2026 - 17:33:00: map_probe_steps() + DFN_MAP gate in testbed_steps.
- 09:08:2026 - 18:44:00: thin_shadow_probe_steps() (DFN_SHADOW_PROBE) — the
  acceptance vantage for thin-caster shadows (trunks + §6.2 standing stones).
- 09:08:2026 - 19:32:00: sky_probe_steps() (DFN_SKY_PROBE) — one sky-heavy
  vantage; the hour comes from DFN_TIME so day/dusk/night reuse it.
- 09:08:2026 - 21:20:00: massif_probe_steps(which) (DFN_MASSIF_PROBE=1|2) —
  design's §7.1b far VERDICT and near RHYTHM vantages, one frame per run
  because each needs its own hour.
*/

#include "engine/render/sources/Tour.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/platform/render/interfaces/IRenderer.h"

#include <cmath>
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

void Tour::begin(std::vector<TourStep> steps, std::string output_dir,
                 std::function<float(glm::vec2)> ground_at) {
    steps_ = std::move(steps);
    if (output_dir.empty()) {
        const char* dir = env_or_null("DFN_TOUR_DIR");
        output_dir = dir != nullptr ? dir : "screenshots";
    }
    output_dir_ = std::move(output_dir);
    ground_at_ = std::move(ground_at);
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

glm::vec3 Tour::focus_position() const {
    if (!active_) {
        return {0.0f, 0.0f, 0.0f};
    }
    return resolved_position(steps_[step_]);
}

glm::vec3 Tour::resolved_position(const TourStep& step) const {
    if (!step.ground_relative || !ground_at_) {
        return step.position;
    }
    // Lazy ground resolution: chunks around a far vantage are only resident
    // once the app streams around focus_position(), so the height is
    // re-queried every frame and settles before the shot is taken.
    return {step.position.x,
            ground_at_({step.position.x, step.position.z}) + step.position.y,
            step.position.z};
}

void Tour::apply(FirstPersonCamera& camera) const {
    if (!active_) {
        return;
    }
    const TourStep& step = steps_[step_];
    const CameraPose pose{resolved_position(step), step.yaw, step.pitch};
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
        {"eye_texture", {mid, eye, mid}, 2.0f, -0.5f, 30, false},
        {"eye_horizon", {mid, eye, mid}, 2.3562f, -0.02f, 10, false},
        {"slope_splat", {size * 0.7f, eye + 18.0f, size * 0.7f}, 2.3562f, -0.35f, 10,
         false},
        {"water_valley", {mid, ground_height + 30.0f, mid}, 2.3562f, -0.5f, 10, false},
        {"overview", {mid, ground_height + 60.0f, mid}, 2.3562f, -0.9f, 10, false},
        {"sky_sun", {mid, eye, mid}, 2.48f, 0.45f, 10, false},
    };
}

namespace {

// Yaw that aims from `from` to `to` (world x/z) under the frozen camera
// convention: yaw 0 -> -Z (north), positive yaw turns toward +X (east).
float aim_yaw(glm::vec2 from, glm::vec2 to) {
    const glm::vec2 d = to - from;
    return std::atan2(d.x, -d.y);
}

} // namespace

std::vector<TourStep> Tour::map_probe_steps() {
    // ONE frame, and it is the map screen (RenderSystem opens it on DFN_MAP=1).
    // Parked at the middle of the testbed so the streaming radius reveals the
    // whole valley, and yawed south-east so the arrow's facing is unambiguous
    // (a north-up arrow could be read as "the marker just points up").
    const float mid = static_cast<float>(config::TESTBED_SIZE) * 0.5f;
    const float eye = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    return {{"map_screen", {mid, eye, mid}, 2.36f, 0.0f, 90, true}};
}

std::vector<TourStep> Tour::thin_shadow_probe_steps() {
    // ONE frame at a dungeon entrance: the §6.2 standing stones and nearby
    // tree trunks in the same shot, from the south-east so the afternoon
    // southern sun throws their shadows AWAY from the camera across open
    // ground. This is the acceptance vantage for thin-caster shadows — the
    // class of object (trunks, standing stones, later fences and railings)
    // that a coarse shadow map silently drops.
    const glm::vec2 entrance{774.0f, 275.0f}; // NE entrance, probed from the map
    const glm::vec2 pos = entrance + glm::vec2{26.0f, 26.0f};
    const float eye = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    return {{"thin_caster_shadows", {pos.x, eye, pos.y}, aim_yaw(pos, entrance),
             -0.12f, 90, true}};
}

std::vector<TourStep> Tour::sky_probe_steps() {
    // ONE frame with a lot of sky and a lit horizon: the valley overview from
    // the south, raised, aimed north over the whole composition. The hour and
    // the moon phase come from DFN_TIME / DFN_MOON (RenderSystem::init), so
    // the same vantage shoots dawn, noon, dusk and night without new routes.
    // DFN_SKY_YAW (radians) re-aims it without a new route: the sun and moon
    // ride an east->south->west arc, so the default northward valley shot can
    // never contain them — proving the moon needs a different heading, not a
    // different place.
    const glm::vec2 pos{512.0f, 820.0f};
    float yaw = aim_yaw(pos, {500.0f, 380.0f});
    float pitch = 0.06f;
    if (const char* yenv = env_or_null("DFN_SKY_YAW")) {
        float parsed = 0.0f;
        if (std::sscanf(yenv, "%f", &parsed) == 1) {
            yaw = parsed;
            pitch = 0.16f; // celestial shots want a little sky headroom
        }
    }
    return {{"sky", {pos.x, 70.0f, pos.y}, yaw, pitch, 90, true}};
}

std::vector<TourStep> Tour::massif_probe_steps(int which) {
    // Coordinates and intent are DESIGN's (§7.1b), not render's — recorded here
    // so the shoot is reproducible from the repo rather than from a message.
    const float eye = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    if (which >= 2) {
        // FRAME 2 — RHYTHM. 287 m out on the ~280 deg bearing, which avoids the
        // castle sector (~208 deg) where §6.1.1 lets the castle fill the view:
        // a frame taken through it would be testing the castle, not the
        // mountain. Front-lit and low (DFN_TIME 0.72) so the near-vertical
        // risers take the light head-on while the benches graze it — that
        // value separation IS the band rhythm.
        const glm::vec2 pos{545.0f, 165.0f};
        return {{"massif_rhythm", {pos.x, eye, pos.y}, aim_yaw(pos, {830.0f, 200.0f}),
                 0.10f, 90, true}};
    }
    // FRAME 1 — VERDICT. 717 m out, the range the valley actually looks at the
    // massif from, backlit (DFN_TIME 0.30) because the complaint being answered
    // was a SILHOUETTE word: a landmark reads by value against sky (§1.5), so a
    // dark ribbed mass against a bright sky is the purest form of the test.
    const glm::vec2 pos{120.0f, 300.0f};
    return {{"massif_verdict", {pos.x, eye, pos.y}, aim_yaw(pos, {830.0f, 200.0f}),
             0.09f, 90, true}};
}

std::vector<TourStep> Tour::testbed_steps() {
    // Verification hooks (Rule 27, user instruction "one variant, no
    // near-identical frames"): each of these collapses the tour to the single
    // frame that proves one thing, instead of re-shooting the whole route.
    if (env_or_null("DFN_MAP") != nullptr) {
        return map_probe_steps();
    }
    if (env_or_null("DFN_SHADOW_PROBE") != nullptr) {
        return thin_shadow_probe_steps();
    }
    if (env_or_null("DFN_SKY_PROBE") != nullptr) {
        return sky_probe_steps();
    }
    if (const char* menv = env_or_null("DFN_MASSIF_PROBE")) {
        return massif_probe_steps(std::atoi(menv));
    }
    // Tour v3 (stage 3b acceptance, Rule 27): vantages at the LANDSCAPE §7.1
    // layout coordinates (seed-1 testbed, world 0..1024 m). All ground_relative
    // (y = offset above terrain, resolved through the begin() callback while
    // the app streams around focus_position()). Wait frames cover streaming +
    // mesh upload for each refocus.
    const float eye = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    constexpr uint32_t WAIT = 45;

    // Vantages aim at the GENERATED seed-1 world, probed 09:08:2026 (the §7.1
    // plan table drifted in hydrology: the real river runs (730,320) ->
    // (560,500) into a flooded bend at (320..480, ~560); the lake spans
    // x 188..274 / z 460..700; the outflow leaves south at x ~300..335).
    const glm::vec2 crag{830.0f, 200.0f};

    std::vector<TourStep> steps;
    // (a) Ravenscar Crag from Vaelmere ground level. Look-dev finding (shots
    // 1-3): from town the foothill pine strips out-angle the 52 m peak — only
    // the tower tip breaks the skyline (C4 violation reported to design/core).
    // The frame documents the current state honestly.
    const glm::vec2 a_pos{320.0f, 480.0f};
    steps.push_back({"crag_from_vaelmere", {a_pos.x, eye, a_pos.y},
                     aim_yaw(a_pos, crag), 0.02f, WAIT, true});
    // (a2) The working L0 money shot: the tree-free §2.4 corridor
    // watchpoint -> barrow opens the only ground-level sightline to the rock.
    const glm::vec2 a2_pos{700.0f, 382.0f};
    steps.push_back({"crag_final_approach", {a2_pos.x, eye, a2_pos.y},
                     aim_yaw(a2_pos, {795.0f, 270.0f}), 0.08f, WAIT, true});
    // (b) The south outflow river, shot upstream so the descending surface
    // reads; banks + shore sand in frame.
    const glm::vec2 b_pos{330.0f, 905.0f};
    steps.push_back({"river_ford", {b_pos.x, eye + 2.0f, b_pos.y},
                     aim_yaw(b_pos, {305.0f, 810.0f}), -0.10f, WAIT, true});
    // (c) Lakeshore: standing on the east beach at the waterline (the lake
    // sits in its basin — from the meadow rim the surface hides).
    const glm::vec2 c_pos{278.0f, 638.0f};
    steps.push_back({"lake_bluff", {c_pos.x, eye, c_pos.y},
                     aim_yaw(c_pos, {195.0f, 505.0f}), -0.05f, WAIT, true});
    // (d) Hamlet buildings from the east corridor rise (the closest grass
    // standpoint — south of the ring lies the flooded river bend).
    const glm::vec2 d_pos{425.0f, 515.0f};
    steps.push_back({"hamlet_approach", {d_pos.x, eye + 0.5f, d_pos.y},
                     aim_yaw(d_pos, {372.0f, 500.0f}), -0.03f, WAIT, true});
    // (e) Species read from the foothill watchpoint: oak crowns, pine tiers,
    // birch river line, bush + stones in one frame (canopy-free standpoint).
    const glm::vec2 e_pos{655.0f, 430.0f};
    steps.push_back({"forest_species", {e_pos.x, eye, e_pos.y},
                     aim_yaw(e_pos, {790.0f, 275.0f}), 0.06f, WAIT, true});
    // (f) Overview: the whole valley composition from the south, looking north.
    const glm::vec2 f_pos{512.0f, 800.0f};
    steps.push_back({"overview", {f_pos.x, 85.0f, f_pos.y},
                     aim_yaw(f_pos, {500.0f, 380.0f}), -0.32f, WAIT, true});
    return steps;
}

} // namespace dfn::render
