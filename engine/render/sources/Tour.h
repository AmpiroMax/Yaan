/*
Created: 09:08:2026 - 00:16:00
Last updated: 09:08:2026 - 10:29:00
Module: engine/render
File: engine/render/sources/Tour.h

Responsibility:
- The screenshot tour harness (Rule 27, Q24/Q51): env-var driven camera tour
  that visits fixed vantages, waits, saves screenshots via
  IRenderer::save_screenshot, then asks the app to quit.

Key items:
- TourStep: one vantage (label, position, yaw/pitch, wait frames).
- Tour: enabled_by_env / begin / apply / post_frame — the drivers.

Dependencies:
- Uses: FirstPersonCamera (pose override), IRenderer (screenshots, forward
  declaration only), C++ stdlib, glm.
- Used by: engine/app (owns a Tour when DFN_TOUR=1), stage-2 acceptance run,
  CI visual checks.

Notes:
- Modeled on Quicky's gloom Tour (games/gloom/src/debug/Tour.h), simplified to
  the first-person engine: no storeys/zoom/iso orientation; a step is a full
  camera pose. Same core loop: apply pose at frame start -> render -> wait ->
  save_screenshot -> advance -> request app close after the last step.
- Env contract: DFN_TOUR=1 enables the tour; DFN_TOUR_DIR overrides the output
  directory (default "screenshots/"). Files are "NN_label.png", NN = step index.
- Stage-2 acceptance (Q51): the default step list captures 4 frames; the
  checklist per frame — not black, ground visible, horizon at the expected
  height. Checked by eye against the checklist now, golden images later (Q24).
- The game stays tour-free (Quicky lesson): only engine/app knows the Tour
  exists; simulation and gameplay never see it.
- Null renderer returns false from save_screenshot: the tour still walks its
  steps and exits cleanly — headless smoke test of the camera path (Rule 3).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Debug tooling: keep it out of simulation and render hot paths.
*/
/*
UPD:
- 09:08:2026 - 00:16:00: Initial stage-1 contract (render zone).
- 09:08:2026 - 00:45:00: Stage 2 — added internal_res_from_env (DFN_INTERNAL_RES
  override for the 640x360 vs 320x180 user decision) and screenshot flush
  phase state (bgfx captures asynchronously into the following frame).
- 09:08:2026 - 10:29:00: default_steps(ground_height = 0.0f) — vantages are now
  offset by the terrain height at the chunk center. Root cause of the
  "vertically flipped image" report: the old absolute eye heights (ground
  assumed at y=0) sat 14-22 m UNDER the generated surface (~24 m at the
  center, seed 1), so frames showed the terrain underside above the horizon —
  terrain on top, sky below. Render orientation itself was correct (frame 03
  from y=60 proved it). Defaulted parameter, source-compatible; lead sync per
  Rule 26 recorded via team channel 09:08:2026.
*/

#pragma once

#include "engine/render/sources/FirstPersonCamera.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::platform {
class IRenderer;
}

namespace dfn::render {

// One tour vantage: teleport the camera, wait `wait_frames` rendered frames
// (lets streaming/upload settle), save one screenshot. Radians, meters.
struct TourStep {
    std::string label;                    // filename-safe, e.g. "spawn_east"
    glm::vec3 position{0.0f};             // eye position, world space
    float yaw = 0.0f;                     // radians, same convention as CameraPose
    float pitch = 0.0f;
    uint32_t wait_frames = 0;             // frames rendered before the shot
};

class Tour {
public:
    // True when the DFN_TOUR environment variable requests a tour run.
    [[nodiscard]] static bool enabled_by_env();

    // Parses DFN_INTERNAL_RES ("WxH", e.g. "320x180") for the app's
    // RendererInitParams; returns `fallback` (normally the NUMBERS.md
    // INTERNAL_RES) when unset or malformed. Lets the lead shoot the same
    // vantages at both candidate resolutions (Q9 user decision).
    [[nodiscard]] static glm::uvec2 internal_res_from_env(glm::uvec2 fallback);

    // Arms the tour. `output_dir` empty = DFN_TOUR_DIR or the default dir.
    void begin(std::vector<TourStep> steps, std::string output_dir);

    [[nodiscard]] bool active() const;
    [[nodiscard]] uint32_t current_step() const;

    // Frame start: force the camera to the current step's pose (both snapshots
    // set equal — a tour frame is deliberately static, interpolation is a no-op).
    void apply(FirstPersonCamera& camera) const;

    // After IRenderer::end_frame: counts down the wait, saves the screenshot,
    // advances. Returns true when the last step finished — the app then calls
    // IWindow::request_close.
    [[nodiscard]] bool post_frame(platform::IRenderer& renderer);

    // The default stage-2 acceptance route (Q51): 4 vantages over the test
    // chunk — ground + horizon in every frame. Definitions live in the stage-2
    // .cpp, positions derived from NUMBERS.md chunk constants, not hardcoded ad
    // hoc values. `ground_height` is the terrain height (meters) at the chunk
    // center; every vantage's y is offset by it so eye-level steps sit ABOVE
    // the generated surface (the tour has no world access by design — the app,
    // which knows the terrain, passes the reference height). The default 0.0f
    // keeps the historical flat-ground assumption.
    [[nodiscard]] static std::vector<TourStep> default_steps(float ground_height = 0.0f);

private:
    std::vector<TourStep> steps_;
    std::string output_dir_;
    uint32_t step_ = 0;
    uint32_t frames_waited_ = 0;
    uint32_t flush_left_ = 0; // frames rendered after scheduling a screenshot
    bool active_ = false;
};

} // namespace dfn::render
