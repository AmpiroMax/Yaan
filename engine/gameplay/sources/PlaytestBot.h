/*
Created: 10:08:2026 - 02:23:05
Last updated: 10:08:2026 - 02:23:05
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

// What the checks need from the world, bound app-side (Rule 34: the truth
// lives with its owner — heights and water come from core's queries).
struct PlaytestCheckEnv {
    std::function<std::optional<float>(glm::vec2)> terrain_height; // heightfield top
    std::function<std::optional<float>(glm::vec2)> water_analytic; // swum truth
    std::function<std::optional<float>(glm::vec2)> water_drawn;    // seen water
    float world_floor_y = -100.0f; // below EVERYTHING legitimate (app derives)
    float deep_margin = 60.0f;     // covers the deepest legitimate carve
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
