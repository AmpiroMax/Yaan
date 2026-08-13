/*
Created: 10:08:2026 - 02:23:05
Last updated: 13:08:2026 - 18:10:00
Module: engine/gameplay
File: engine/gameplay/sources/PlaytestBot.h

Responsibility:
- The autonomous playtest system v1 (spec: engine/gameplay/docs/PLAYTEST.md,
  user request by name). A bot that synthesizes PLAYER INPUT into the same
  PlayerState intents a human's keys write, runtime invariants that watch
  every tick, incident records, and the artifact writers. The app owns env
  parsing, the run directory and screenshots (wiring block supplied to lead).

Key items:
- BotMode / PlaytestConfig / PlaytestState / PlaytestIncident: plain data.
- playtest_drive(): per fixed tick BEFORE player_pre_step — steers.
- playtest_check(): per fixed tick AFTER player_post_step — invariants;
  returns the number of NEW incidents (the app screenshots when > 0).
- playtest_note_frame(): render-frame stats + the frame-budget invariant.
- playtest_write_artifacts(): incidents.log + summary.txt into the run dir.

Dependencies:
- Uses: PlayerMovement.h (PlayerState intents), Dice.h (seeded Rng), core ecs,
  generated constants, std.
- Used by: engine/app (DFN_PLAYTEST wiring), tests (sim_playtest).

Notes:
- NOT A SPECIAL PHYSICS MODE: the bot writes move_axes/pending_look/
  jump_pressed and the real movement code runs. That is the whole point.
- Invariants are cause-based (Rule 36); the two checks v1 cannot do honestly
  (capsule-overlap embedment, carve-aware below-surface) are documented in
  the spec's "deliberately NOT in v1" section rather than faked.
- Incident spam is bounded by a per-invariant cooldown (a fall that lasts 200
  ticks is one finding, not 200) — dedupe by CAUSE-episode, not by count cap.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Controls ship with the checker (Rule 30): see tests/sim/PlaytestTests.cpp —
  a deliberately broken run MUST produce incidents.
*/
/*
UPD:
- 10:08:2026 - 02:23:05: v1 per PLAYTEST.md.
- 10:08:2026 - 12:08:26: FOOT SLIP invariant (the instrument the movement
                         ruling asked for): a planted foot must not travel.
                         Slip is a MOTION artifact, so no still frame can show
                         it. Feet arrive through a callback seam (unbound =
                         skipped, never silently passing); bound by character.
                         Bot also picks its gear through the same modifiers a
                         player's keys set.
- 10:08:2026 - 12:13:41: FootSample semantics pinned with character: SOLE not
                         ankle (the forefoot rocker walks the ankle forward
                         while the sole is still planted, and this check is
                         horizontal), and stance runs to TOE-OFF so both feet
                         overlap in double support.
- 10:08:2026 - 19:48:10: Ground-contact counters (airborne/grounded ticks,
                         landings = dip retriggers, worst landing dip) and the
                         two foot-slip WITNESS counters. The witness counters
                         exist because worst_foot_slip_mm == 0 was printed by
                         every playtest run so far while the rig seam was
                         unbound -- bit-identical to a perfect pass.
- 10:08:2026 - 20:14:16: UNDEDUPED frame trace. The incident log cannot
                         answer "how many frames did the stall span" -- it
                         dedupes on a 120-tick cooldown and the survivor is not
                         the worst frame, so it under-reported a 779 ms event
                         as 346 ms and I passed that understatement to render.
- 10:08:2026 - 20:32:57: CORRECTION (Rule 16/17). The stamp on the entry
                         above was written 22 minutes AHEAD of the clock I had
                         just read; it now reads the true 20:26:56. Recorded as
                         an appended entry rather than a silent edit, because
                         UPD blocks are this project's only cross-zone ordering
                         record, so a forward stamp REORDERS history rather
                         than merely misdating a file (character2's catch,
                         independent of my own).
- 13:08:2026 - 18:00:00: THE BOT PRESSES THE VERB, and the world is counted
                         around it. No automated run had ever pressed one, so
                         "the prompt appears and nothing happens" could not be
                         reproduced without a human at the keyboard.
- 13:08:2026 - 18:10:00: The census counts TRANSITIONS as well as endpoints —
                         a toggling door is invisible to endpoints, and the
                         first version of it said so in a way that read as "the
                         verb never fires" (see the field note).
*/

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "engine/gameplay/sources/Dice.h"
#include "engine/gameplay/sources/PlayerMovement.h" // Gait (the one gait decision)

namespace dfn::ecs {
class World;
}

namespace dfn::gameplay {

enum class BotMode : uint8_t {
    WaypointPatrol = 0, // visit config.waypoints in order
    RandomExplorer = 1, // random targets in world bounds, occasional jumps
    Soak = 2,           // circle the spawn for the configured duration
};

struct PlaytestConfig {
    BotMode mode = BotMode::Soak;
    std::vector<glm::vec2> waypoints; // patrol only (world x/z)
    bool loop_waypoints = false;      // patrol: wrap instead of finishing
    float duration_seconds = 300.0f;  // 0 with patrol = once through the list
    uint64_t seed = 1;
    // Which gear the bot walks in. Walk is the default because it is the gait
    // whose clip matches the ground it covers; a slip run wants this, while a
    // coverage soak may prefer Jog.
    Gait gait = Gait::Walk;
    glm::vec2 world_min{0.0f};        // explorer target bounds (world x/z)
    glm::vec2 world_max{0.0f};
    float soak_radius = 20.0f;        // metres around the spawn
};

struct PlaytestIncident {
    std::string invariant;
    uint64_t tick = 0;
    glm::vec3 position{0.0f};
    std::string detail;
};

// One tick's view of the feet, supplied by character's rig (world space).
// SEMANTICS AGREED WITH character (10:08:2026), and both halves are
// load-bearing rather than pedantic:
//  - the position is the SOLE point, NOT the ankle. With the forefoot rocker
//    the foot pivots over the toe while still planted, which translates the
//    ankle FORWARD — and this check measures horizontal drift, so an ankle
//    would report slip that is not happening. The header said "ankle or sole"
//    when the seam was written; that ambiguity is resolved here before anyone
//    binds the wrong joint.
//  - `planted` runs touch-down to TOE-OFF, so after the knee fix BOTH feet
//    read planted across the plant instant (double support). That is real
//    walking, not a bug: each foot carries its own anchor, so overlapping
//    stances are measured independently and neither disturbs the other.
struct FootSample {
    glm::vec3 left{0.0f};
    glm::vec3 right{0.0f};
    bool left_planted = false;
    bool right_planted = false;
};

// What the checks need from the world, bound app-side (Rule 34: the truth
// lives with its owner — heights and water come from core's queries).
struct PlaytestCheckEnv {
    std::function<std::optional<float>(glm::vec2)> terrain_height; // heightfield top
    std::function<std::optional<float>(glm::vec2)> water_analytic; // swum truth
    std::function<std::optional<float>(glm::vec2)> water_drawn;    // seen water
    float world_floor_y = -100.0f; // below EVERYTHING legitimate (app derives)
    float deep_margin = 60.0f;     // covers the deepest legitimate carve
    // FOOT SLIP (the instrument the movement ruling asked for): world-space
    // feet + stance flags. Unbound = the check is skipped, so this costs
    // nothing until character binds it. A planted foot must not travel.
    std::function<std::optional<FootSample>()> foot_sample;
};

struct PlaytestState {
    PlaytestConfig config;
    Rng rng{};
    uint64_t tick = 0;
    double sim_seconds = 0.0;
    double distance_walked = 0.0;
    bool finished = false;
    std::vector<PlaytestIncident> incidents;

    // Steering internals (plain data; nothing here is contract).
    size_t waypoint_index = 0;
    glm::vec2 target{0.0f};
    bool has_target = false;
    bool soak_ring_built = false;
    float stuck_seconds = 0.0f;
    float jump_cooldown_seconds = 0.0f;
    float airborne_seconds = 0.0f;
    bool freefall_reported = false;
    bool stuck_reported = false;
    glm::vec3 last_position{0.0f};
    bool has_last_position = false;

    // Foot-slip bookkeeping: where each foot touched down, and whether this
    // stance episode has already been reported (one finding per stance, not
    // one per tick).
    glm::vec3 left_anchor{0.0f};
    glm::vec3 right_anchor{0.0f};
    bool left_was_planted = false;
    bool right_was_planted = false;
    bool left_slip_reported = false;
    bool right_slip_reported = false;
    double worst_slip_m = 0.0; // reported in the summary even when under bound
    // WHY THESE TWO COUNTERS EXIST, and they are not bookkeeping. A slip check
    // that never ran reports worst_slip_m == 0, which is bit-identical to a
    // perfect pass — so the summary line for a run where character's rig seam
    // was unbound looked exactly like the summary line for flawless feet, and
    // did so in every playtest run this project has produced so far. The
    // counters make "measured, and it was clean" and "nobody ever looked"
    // different strings. Same failure the invariant itself is designed against:
    // silence that reads as a pass.
    uint64_t foot_samples_seen = 0;   // ticks the rig actually handed feet over
    uint64_t foot_planted_ticks = 0;  // ticks with at least one foot planted

    // --- THE VERB, AND WHAT IT DID (user, 13.08: «ни с чем взаимодействовать
    // --- не могу, хотя текст появляется»).
    //
    // A bot that walks past everything can never find out whether pressing the
    // key does anything, and until now this one did exactly that: no automated
    // run in this project has ever pressed a verb. So the bot presses it when a
    // prompt is up, and the world is COUNTED before and after — because the
    // suspicion worth testing is not "the press is lost" but "the press
    // arrives, the verb fires, and nothing in the world is different for it".
    // A press with no counted consequence is the finding, and it can only be
    // stated by counting.
    uint64_t hover_ticks = 0;       // ticks with a live prompt on screen
    uint64_t interact_presses = 0;  // times the bot pressed the verb key
    float interact_cooldown_s = 0.0f;
    // World census, sampled every tick. FIRST -> LAST IS NOT ENOUGH and the
    // first version of this counter proved it the expensive way: a door toggles,
    // so an even number of presses leaves it exactly as it started, and the
    // summary said "doors_open 0 -> 0" for a run in which the door opened and
    // shut sixteen times. That reads as "the verb never fires", which is the
    // opposite of the truth and would have been reported as the diagnosis.
    // Endpoints cannot see a toggle; TRANSITIONS can, so both are kept and the
    // transition counts are the ones that answer "did the press do anything".
    bool census_seeded = false;
    uint32_t interactables_first = 0, interactables_last = 0;
    uint32_t doors_open_first = 0, doors_open_last = 0;
    uint32_t levers_used_first = 0, levers_used_last = 0;
    uint32_t inventory_first = 0, inventory_last = 0;
    uint64_t door_state_changes = 0;   // a door opened or shut
    uint64_t lever_state_changes = 0;  // a lever went used
    uint64_t interactables_removed = 0; // a prop left the world (taken)
    uint64_t inventory_gained = 0;      // items that arrived in a bag

    // GROUND CONTACT, and why it is worth counting: the landing dip restarts
    // its curve from zero on every airborne->grounded edge, and `airborne` has
    // no hysteresis. So a capsule that skips over terrain crests re-kicks the
    // camera at the RATE OF THE SKIPPING, not at the rate of the stride — and
    // that rate is a function of speed, which makes it invisible at the gait
    // every automated run has ever used. Counted in SIM ticks, so the number
    // is deterministic even though the frame statistics next to it are not.
    uint64_t airborne_ticks = 0;
    uint64_t grounded_ticks = 0;
    uint64_t landings = 0;      // airborne -> grounded edges = dip retriggers
    double worst_landing_dip_m = 0.0;
    bool prev_airborne = false;
    bool has_prev_airborne = false;

    // FRAME TRACE. The incident log cannot answer "how many frames did the
    // stall span", for two independent reasons: it dedupes by a 120-tick
    // cooldown, so consecutive bad frames collapse into one record, and the
    // survivor is not the worst one. That distinction sizes a fix — one 345 ms
    // frame means N uploads landed together and a per-frame budget would split
    // it into N small ones; several consecutive frames means something is
    // serialising instead. So the trace is UNDEDUPED and its threshold is
    // lower than the incident budget, to catch the shoulders of an event and
    // not just its peak.
    struct FrameMark {
        uint64_t frame_index = 0;
        uint64_t tick = 0;
        float ms = 0.0f;
    };
    std::vector<FrameMark> frame_trace; // bounded; see TRACE_MAX in the .cpp

    // Frame statistics (render frames, wall-clock — not deterministic, not
    // meant to be; the summary reports them, the budget invariant gates).
    double frame_dt_sum = 0.0;
    double frame_dt_max = 0.0;
    uint64_t frame_count = 0;

    // Per-invariant cooldown bookkeeping (episode dedupe).
    std::vector<std::pair<std::string, uint64_t>> last_fired;
};

[[nodiscard]] PlaytestState make_playtest(const PlaytestConfig& config);

// Once per FIXED tick, BEFORE player_pre_step: writes the player's input
// intents (the same fields accumulate_input writes — the real input path).
void playtest_drive(PlaytestState& pt, ecs::World& world);

// Once per FIXED tick, AFTER player_post_step: every invariant, incident
// records, distance/steering bookkeeping. Returns NEW incidents this tick.
[[nodiscard]] size_t playtest_check(PlaytestState& pt, ecs::World& world,
                                    const PlaytestCheckEnv& env);

// Once per RENDER frame with the measured frame dt.
void playtest_note_frame(PlaytestState& pt, float frame_dt_seconds);

// Writes incidents.log and summary.txt into run_dir (created if needed).
void playtest_write_artifacts(const PlaytestState& pt, const std::string& run_dir);

} // namespace dfn::gameplay
