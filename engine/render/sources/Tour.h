/*
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

#pragma once

#include "engine/core/math/sources/StandVantage.h"
#include "engine/render/sources/FirstPersonCamera.h"

#include <cstdint>
#include <functional>
#include <span>
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
    // Stage 3b: when true, position.y is an offset above the terrain at
    // position.xz, resolved each frame through the begin() ground callback
    // (heights become exact once the vantage's chunk streams in).
    bool ground_relative = false;
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
    // `ground_at` (stage 3b, optional) returns the terrain height (m) at a
    // world x/z — used every frame to resolve ground_relative steps; without
    // it those steps treat position.y as absolute.
    void begin(std::vector<TourStep> steps, std::string output_dir,
               std::function<float(glm::vec2)> ground_at = {});

    [[nodiscard]] bool active() const;
    [[nodiscard]] uint32_t current_step() const;

    // The current vantage position (ground-resolved when possible) — the app
    // streams chunks around this while the tour runs, so every vantage gets
    // resident terrain regardless of where the player is parked.
    [[nodiscard]] glm::vec3 focus_position() const;

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

    // Tour v3 (stage 3b): the LANDSCAPE.md §7.1 acceptance route — vantages at
    // the testbed layout coordinates (crag from the hamlet, river ford, lake
    // shore with the bluff, hamlet approach, forest species, overview). All
    // steps are ground_relative; pass a ground_at callback to begin().
    [[nodiscard]] static std::vector<TourStep> testbed_steps();

    // Map screen evidence shoot (Rule 27): a SINGLE vantage at the middle of
    // the testbed. The tour cannot press M, so RenderSystem opens the map on
    // DFN_MAP=1 and testbed_steps() returns this route under the same variable
    // — one frame, not seven copies of the same overlay.
    [[nodiscard]] static std::vector<TourStep> map_probe_steps();

    // Thin-caster shadow evidence shoot (Rule 27): a SINGLE vantage at a
    // dungeon entrance where tree trunks and the §6.2 standing stones stand on
    // open ground with the sun behind the camera. Selected by DFN_SHADOW_PROBE.
    // Thin vertical objects are the class a coarse shadow map drops silently,
    // so they get their own acceptance frame.
    /// DFN_FONT_PROBE=1: one vantage with the glyph specimen on the HUD.
    /// Aimed at a background of several values, because the question the font
    /// has to answer is legibility over the world, not "do glyphs exist".
    [[nodiscard]] static std::vector<TourStep> font_probe_steps();

    [[nodiscard]] static std::vector<TourStep> thin_shadow_probe_steps();

    // Sky/day-night evidence shoot (Rule 27): one sky-heavy vantage over the
    // valley. Selected by DFN_SKY_PROBE; the HOUR comes from DFN_TIME and the
    // moon phase from DFN_MOON, so one route covers every time of day.
    [[nodiscard]] static std::vector<TourStep> sky_probe_steps();

    // Cloud evidence shoot (W4, Rule 27): DFN_CLOUD_PROBE=1. TWO frames —
    // (1) the raised valley overview pitched DOWN so the ground is most of
    // the frame: cloud shadows patterning the valley floor is the claim under
    // test and a sky-heavy frame could not fail it; (2) the same standpoint
    // aimed UPWIND with sky headroom: the cumulus announcement on the horizon
    // ring plus both drifting sheets. The hour comes from DFN_TIME, the drift
    // timestamp from DFN_VISTIME (two runs 30 s apart = the drift pair), and
    // DFN_CLOUD=0 is the control run in which every cloud artifact of both
    // frames must vanish at once.
    [[nodiscard]] static std::vector<TourStep> cloud_probe_steps();

    // Massif shape evidence shoot (design's §7.1b, Rule 27): the two vantages
    // that judge the §2.8 mountain. ONE frame per run — DFN_MASSIF_PROBE=1 is
    // the far VERDICT frame (does it read as a ribbed concave mass rather than
    // a dome, backlit against sky), =2 is the near RHYTHM frame (does the flank
    // alternate cliff and bench at irregular spacing, front-lit and low).
    // They are separate runs because each needs its own HOUR: design chose the
    // light per frame to expose that frame's failure mode, and the other
    // frame's light would hide it. Pass the matching DFN_TIME.
    [[nodiscard]] static std::vector<TourStep> massif_probe_steps(int which);

    // CRAG ACCEPTANCE ROUTE (DFN_CRAG_PROBE=1, Rule 27). Ravenscar from FOUR
    // BEARINGS at TWO RANGES, standing on the valley floor at eye height.
    //
    // Why this route exists and why it is not massif_probe_steps: the 600/717 m
    // vantage was sized for LR, the temple mountain — which is a NUMBERS row
    // and a design section and NOTHING IN THE GENERATOR. The testbed's only
    // real landform is the crag: 115 m of relief on a 120 m base radius, whose
    // equivalent acceptance range is 253 m, comfortably inside streaming. So
    // the frame everyone has been waiting on LOD for was never blocked on LOD;
    // it was aimed at the wrong mountain.
    //
    // Bearings are given as the compass direction FROM the peak TO the eye and
    // are chosen for what the world can actually hold: the peak sits at
    // (830, 200) in a 1024 m box, so north and east run out of world before
    // 253 m. 180/225/270/300 all fit at both ranges. ONE frame is not enough
    // for a shape verdict — a dome and a three-lobed cone look identical from
    // the one bearing that happens to face a lobe.
    [[nodiscard]] static std::vector<TourStep> crag_acceptance_steps();

    // CANOPY SPECKLE PROBE (DFN_FLORA_PROBE=1|2, Rule 27 + Rule 30). ONE frame
    // per run, pinned, and built to be shot TWICE with only the eye moved:
    // DFN_FLORA_STEP=<m> slides the eye along its own forward vector, so
    // DFN_FLORA_STEP=0.05 is exactly one 120 fps frame at RUN_SPEED = 6 m/s —
    // the user's «при беге трясет» expressed as a pair of frames a pixel diff
    // can read. The control arm is the SAME route with STEP unset (or 0), and
    // per tools/pngdiff.py it must come back at 0.000 % before any other
    // number out of this route means anything.
    //   1 = NEAR CANOPY: standing under the crowns, pitched up into them.
    //   2 = TREELINE: open ground, the forest edge at reading distance.
    // DFN_FLORA_EYE="x,z", DFN_FLORA_YAW / DFN_FLORA_PITCH (rad) re-place it
    // without a new route.
    [[nodiscard]] static std::vector<TourStep> flora_probe_steps(int which);

    // THE ROUTE FOR WHATEVER STAND IS OPEN. `vantages` is
    // ChunkManager::stand_vantages() — the standpoints the STAND publishes,
    // controls included and already paired with their claims.
    //
    // This is the app's only tour entry point and it needs no stand check
    // (Rule 32): a probe variable still wins (those are single-frame evidence
    // shoots and they own the run), an empty vantage list falls back to the
    // testbed route, and a stand that publishes vantages gets them. Before it
    // existed, `DFN_MAP=forest` walked and streamed correctly and then shot ONE
    // frame at the TESTBED's first coordinate and stopped, because the forest
    // stand's other six vantages were underground or outside the world.
    //
    // DFN_VANTAGE=<prefix> shoots only the vantages whose label starts with it
    // — one variant per run (the standing verification cadence), and the way a
    // single acceptance frame is re-shot without re-running the route.
    [[nodiscard]] static std::vector<TourStep>
    stand_steps(std::span<const math::StandVantage> vantages);

    // The vantage list as tour steps, in a deliberate order: the path frames
    // first, then the paired claims, then everything else. Exposed for tests.
    [[nodiscard]] static std::vector<TourStep>
    vantage_steps(std::span<const math::StandVantage> vantages);

private:
    // Step position with ground_relative y resolved via ground_at_ (absolute
    // passthrough otherwise).
    [[nodiscard]] glm::vec3 resolved_position(const TourStep& step) const;

    std::vector<TourStep> steps_;
    std::string output_dir_;
    std::function<float(glm::vec2)> ground_at_;
    uint32_t step_ = 0;
    uint32_t frames_waited_ = 0;
    uint32_t flush_left_ = 0; // frames rendered after scheduling a screenshot
    bool active_ = false;
};

} // namespace dfn::render
