/*
Created: 10:08:2026 - 02:23:05
Last updated: 10:08:2026 - 12:08:26
Module: engine/gameplay
File: engine/gameplay/sources/PlaytestBot.cpp

Responsibility:
- Implementation of the autonomous playtest v1 (PLAYTEST.md): steering,
  invariants, incident records, artifact writers.

Key items:
- playtest_drive / playtest_check / playtest_note_frame /
  playtest_write_artifacts.
- Tool thresholds (stuck window, frame budget, cooldowns) are look-dev-grade
  constants HERE with their reasons; they migrate to NUMBERS the moment a
  second zone must agree with one (Rule 35).

Dependencies:
- Uses: PlaytestBot.h, PlayerMovement.h, core ecs World, generated constants,
  <filesystem>/<fstream> (artifact writers only).
- Used by: engine/app wiring, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The bot must only ever write PlayerState INPUT INTENTS — never positions,
  never velocities. Writing state would turn the playtest into the special
  mode the spec forbids.
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
*/

#include "engine/gameplay/sources/PlaytestBot.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/StepFeel.h" // step_length: the slip bound scales with the stride

namespace dfn::gameplay {

namespace {

constexpr float DT = static_cast<float>(config::SIM_DT);
constexpr float PI = 3.14159265358979323846f;
constexpr float ARRIVE = static_cast<float>(config::NPC_ARRIVE_RADIUS) * 3.0f;
constexpr float SENSITIVITY = static_cast<float>(config::MOUSE_SENSITIVITY);

// Tool thresholds (header note): each with its reason, none load-bearing for
// a second zone.
constexpr float MAX_TURN_RATE = PI;        // rad/s: a deliberate, human-ish turn
constexpr float STUCK_DISPLACEMENT = 0.2f; // m/s: a fifth of crouch speed is "not moving"
constexpr float STUCK_RETRY_SECONDS = 1.5f;   // jump / re-pick before reporting
constexpr float STUCK_REPORT_SECONDS = 5.0f;  // then it is a finding
constexpr float FREEFALL_REPORT_SECONDS = 3.0f; // longer than any legal fall here
constexpr float WATER_MISMATCH_TOLERANCE = 0.05f; // m: sub-ankle
constexpr float FRAME_BUDGET_SECONDS = 0.050f;    // three 60 Hz frames = a felt hitch
constexpr float SPEED_BOUND_MARGIN = 1.2f;        // legal gait + 20 %
constexpr uint64_t INCIDENT_COOLDOWN_TICKS = 120; // one finding per 2 s episode
// FOOT SLIP tolerance. A planted foot should not travel at all — the world
// moves under it — so the bound is perceptual rather than physical: slip below
// ~5% of a step length is not seen. Derived from the rows so it tracks a
// retune: at WALK_SPEED the clip's residual is 0.8%, which leaves real margin
// (Rule 30: the threshold sits above the accepted case and below the rejected
// one — the 31% that WAS shipped for hours is the rejected instance).
constexpr float SLIP_TOLERANCE_FRACTION = 0.05f;
// The fastest legal gait: the debug sprint (a real key a real player holds).
const float MAX_LEGAL_SPEED =
    static_cast<float>(config::RUN_SPEED * config::DEBUG_SPRINT_MULTIPLIER);

[[nodiscard]] float wrap_angle(float a) {
    while (a > PI) {
        a -= 2.0f * PI;
    }
    while (a < -PI) {
        a += 2.0f * PI;
    }
    return a;
}

// Records an incident unless the same invariant fired within the cooldown —
// a fall lasting 200 ticks is one finding, not 200 (episode dedupe by cause).
bool record(PlaytestState& pt, const char* invariant, const glm::vec3& position,
            std::string detail) {
    for (auto& [name, last] : pt.last_fired) {
        if (name == invariant) {
            if (pt.tick - last < INCIDENT_COOLDOWN_TICKS) {
                return false;
            }
            last = pt.tick;
            pt.incidents.push_back({invariant, pt.tick, position, std::move(detail)});
            return true;
        }
    }
    pt.last_fired.emplace_back(invariant, pt.tick);
    pt.incidents.push_back({invariant, pt.tick, position, std::move(detail)});
    return true;
}

[[nodiscard]] glm::vec2 random_point(Rng& rng, glm::vec2 lo, glm::vec2 hi) {
    // roll_die is 1..sides; map to [0,1) with 1/1000 granularity — plenty for
    // picking walk targets, and it keeps the one seeded RNG vocabulary.
    const float u = static_cast<float>(roll_die(rng, 1000) - 1) / 1000.0f;
    const float v = static_cast<float>(roll_die(rng, 1000) - 1) / 1000.0f;
    return {lo.x + (hi.x - lo.x) * u, lo.y + (hi.y - lo.y) * v};
}

} // namespace

PlaytestState make_playtest(const PlaytestConfig& config) {
    PlaytestState pt;
    pt.config = config;
    pt.rng = make_rng(config.seed);
    return pt;
}

void playtest_drive(PlaytestState& pt, ecs::World& world) {
    for (auto [id, state, transform] :
         world.view<PlayerState, components::Transform>()) {
        (void)id;
        if (pt.finished) {
            state.move_axes = {0.0f, 0.0f};
            return;
        }
        pt.sim_seconds += DT;
        if (pt.config.duration_seconds > 0.0f
            && pt.sim_seconds >= pt.config.duration_seconds) {
            pt.finished = true;
        }

        const glm::vec2 here{transform.position.x, transform.position.z};

        // --- Target selection.
        switch (pt.config.mode) {
        case BotMode::WaypointPatrol: {
            if (pt.config.waypoints.empty()) {
                pt.finished = true;
                return;
            }
            pt.target = pt.config.waypoints[pt.waypoint_index];
            if (glm::length(pt.target - here) < ARRIVE) {
                ++pt.waypoint_index;
                if (pt.waypoint_index >= pt.config.waypoints.size()) {
                    if (pt.config.loop_waypoints) {
                        pt.waypoint_index = 0;
                    } else {
                        pt.finished = true;
                        return;
                    }
                }
                pt.target = pt.config.waypoints[pt.waypoint_index];
            }
            pt.has_target = true;
            break;
        }
        case BotMode::RandomExplorer: {
            const bool arrived = pt.has_target && glm::length(pt.target - here) < ARRIVE;
            const bool give_up = pt.stuck_seconds > STUCK_RETRY_SECONDS * 2.0f;
            if (!pt.has_target || arrived || give_up) {
                pt.target = random_point(pt.rng, pt.config.world_min, pt.config.world_max);
                pt.has_target = true;
                pt.stuck_seconds = 0.0f;
            }
            break;
        }
        case BotMode::Soak: {
            if (!pt.soak_ring_built) {
                // Eight posts around wherever the bot woke up.
                pt.config.waypoints.clear();
                for (int i = 0; i < 8; ++i) {
                    const float a = 2.0f * PI * static_cast<float>(i) / 8.0f;
                    pt.config.waypoints.push_back(
                        here + pt.config.soak_radius * glm::vec2{std::cos(a), std::sin(a)});
                }
                pt.soak_ring_built = true;
            }
            pt.target = pt.config.waypoints[pt.waypoint_index];
            if (glm::length(pt.target - here) < ARRIVE) {
                pt.waypoint_index = (pt.waypoint_index + 1) % pt.config.waypoints.size();
                pt.target = pt.config.waypoints[pt.waypoint_index];
            }
            pt.has_target = true;
            break;
        }
        }
        if (!pt.has_target) {
            return;
        }

        // --- Steering: bearing delivered through the SAME look path a mouse
        // uses (yaw 0 faces -Z, forward = (sin yaw, 0, -cos yaw)).
        const glm::vec2 to = pt.target - here;
        const float desired_yaw = std::atan2(to.x, -to.y);
        const float delta = wrap_angle(desired_yaw - state.yaw);
        const float turn = std::clamp(delta, -MAX_TURN_RATE * DT, MAX_TURN_RATE * DT);
        state.pending_look.x += turn / SENSITIVITY;
        state.pending_look.y = 0.0f;
        // Walk when roughly facing the target; turn in place otherwise.
        state.move_axes = std::abs(delta) < 0.5f * PI ? glm::vec2{0.0f, 1.0f}
                                                      : glm::vec2{0.0f, 0.0f};
        // The bot drives the SAME gear modifiers a player's keys set, so the
        // gait it walks in goes through the one gear-resolution path.
        state.jog = pt.config.gait == Gait::Jog;
        state.run = pt.config.gait == Gait::Run;
        state.debug_sprint = false; // never: the debug gear is not a playtest

        // --- Occasional jumps (explorer) and the stuck-retry jump.
        pt.jump_cooldown_seconds = std::max(0.0f, pt.jump_cooldown_seconds - DT);
        const bool explorer_hop = pt.config.mode == BotMode::RandomExplorer
                                  && pt.jump_cooldown_seconds <= 0.0f
                                  && percent_check(pt.rng, 20);
        const bool stuck_hop = pt.stuck_seconds > STUCK_RETRY_SECONDS;
        if (explorer_hop || stuck_hop) {
            state.jump_pressed = true;
            pt.jump_cooldown_seconds = 5.0f;
        } else if (pt.config.mode == BotMode::RandomExplorer
                   && pt.jump_cooldown_seconds <= 0.0f) {
            pt.jump_cooldown_seconds = 5.0f; // re-roll the hop every 5 s
        }
        return; // one player is the v1 contract
    }
}

size_t playtest_check(PlaytestState& pt, ecs::World& world,
                      const PlaytestCheckEnv& env) {
    size_t new_incidents = 0;
    for (auto [id, state, transform] :
         world.view<PlayerState, components::Transform>()) {
        (void)id;
        ++pt.tick;
        const glm::vec3 pos = transform.position;
        const glm::vec2 xz{pos.x, pos.z};

        // nan_position — integrator/solver corruption.
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
            new_incidents += record(pt, "nan_position", pos, "non-finite transform");
            return new_incidents; // nothing else is meaningful on NaN
        }

        // below_world — fell through everything.
        if (pos.y < env.world_floor_y) {
            new_incidents += record(pt, "below_world", pos,
                                    "y below world floor "
                                        + std::to_string(env.world_floor_y));
        }

        // below_surface — deep under the heightfield (margin covers carves).
        if (env.terrain_height) {
            if (const auto h = env.terrain_height(xz);
                h && pos.y < *h - env.deep_margin) {
                new_incidents += record(pt, "below_surface", pos,
                                        "surface " + std::to_string(*h) + ", margin "
                                            + std::to_string(env.deep_margin));
            }
        }

        // freefall — out of the collision world but above the floor.
        if (state.airborne && state.fall_speed > 0.0f) {
            pt.airborne_seconds += DT;
            if (pt.airborne_seconds > FREEFALL_REPORT_SECONDS && !pt.freefall_reported) {
                pt.freefall_reported = true;
                new_incidents += record(pt, "freefall", pos,
                                        std::to_string(pt.airborne_seconds)
                                            + " s falling");
            }
        } else {
            pt.airborne_seconds = 0.0f;
            pt.freefall_reported = false;
        }

        // speed_bound — teleports and solver explosions.
        if (pt.has_last_position) {
            const glm::vec2 moved{pos.x - pt.last_position.x, pos.z - pt.last_position.z};
            const float speed = glm::length(moved) / DT;
            pt.distance_walked += glm::length(moved);
            if (speed > MAX_LEGAL_SPEED * SPEED_BOUND_MARGIN) {
                new_incidents += record(pt, "speed_bound", pos,
                                        std::to_string(speed) + " m/s > legal "
                                            + std::to_string(MAX_LEGAL_SPEED));
            }

            // stuck — commanded motion, no displacement, on the ground.
            const bool commanded = glm::length(state.move_axes) > 0.5f;
            if (commanded && !state.airborne && speed < STUCK_DISPLACEMENT) {
                pt.stuck_seconds += DT;
                if (pt.stuck_seconds > STUCK_REPORT_SECONDS && !pt.stuck_reported) {
                    pt.stuck_reported = true;
                    new_incidents += record(pt, "stuck", pos,
                                            std::to_string(pt.stuck_seconds)
                                                + " s without displacement");
                }
            } else {
                pt.stuck_seconds = 0.0f;
                pt.stuck_reported = false;
            }
        }
        pt.last_position = pos;
        pt.has_last_position = true;

        // foot_slip — A PLANTED FOOT MUST NOT TRAVEL. This is the instrument
        // the movement ruling asked for: foot slide is a MOTION artifact, so
        // no still frame can show it and only a per-tick check catches it.
        // Unbound callback = skipped, not silently passing.
        if (env.foot_sample) {
            if (const auto feet = env.foot_sample()) {
                const float bound =
                    SLIP_TOLERANCE_FRACTION * step_length(state.stride_speed);
                const auto watch = [&](bool planted, const glm::vec3& now,
                                       glm::vec3& anchor, bool& was, bool& reported,
                                       const char* which) {
                    if (planted && !was) {
                        anchor = now; // touch-down: remember where it landed
                        reported = false;
                    } else if (planted && !reported) {
                        const glm::vec2 drift{now.x - anchor.x, now.z - anchor.z};
                        const float slipped = glm::length(drift);
                        pt.worst_slip_m = std::max(pt.worst_slip_m,
                                                   static_cast<double>(slipped));
                        if (slipped > bound) {
                            reported = true;
                            new_incidents +=
                                record(pt, "foot_slip", pos,
                                       std::string(which) + " foot slid "
                                           + std::to_string(slipped * 1000.0f)
                                           + " mm while planted (bound "
                                           + std::to_string(bound * 1000.0f) + " mm)");
                        }
                    }
                    was = planted;
                };
                watch(feet->left_planted, feet->left, pt.left_anchor,
                      pt.left_was_planted, pt.left_slip_reported, "left");
                watch(feet->right_planted, feet->right, pt.right_anchor,
                      pt.right_was_planted, pt.right_slip_reported, "right");
            }
        }

        // water_mismatch — seen water vs swum water at the player column.
        if (env.water_analytic && env.water_drawn) {
            const auto truth = env.water_analytic(xz);
            const auto drawn = env.water_drawn(xz);
            if (truth && drawn
                && std::abs(*truth - *drawn) > WATER_MISMATCH_TOLERANCE) {
                new_incidents += record(pt, "water_mismatch", pos,
                                        "analytic " + std::to_string(*truth)
                                            + " vs drawn " + std::to_string(*drawn));
            }
        }
        return new_incidents;
    }
    return new_incidents;
}

void playtest_note_frame(PlaytestState& pt, float frame_dt_seconds) {
    pt.frame_dt_sum += frame_dt_seconds;
    pt.frame_dt_max = std::max(pt.frame_dt_max, static_cast<double>(frame_dt_seconds));
    ++pt.frame_count;
    if (frame_dt_seconds > FRAME_BUDGET_SECONDS) {
        record(pt, "frame_budget", pt.last_position,
               std::to_string(frame_dt_seconds * 1000.0f) + " ms frame");
    }
}

void playtest_write_artifacts(const PlaytestState& pt, const std::string& run_dir) {
    std::filesystem::create_directories(run_dir);

    {
        std::ofstream log(run_dir + "/incidents.log");
        for (const auto& inc : pt.incidents) {
            log << "tick " << inc.tick << "  " << inc.invariant << "  pos ("
                << inc.position.x << ", " << inc.position.y << ", " << inc.position.z
                << ")  " << inc.detail << '\n';
        }
    }

    std::map<std::string, size_t> by_invariant;
    for (const auto& inc : pt.incidents) {
        ++by_invariant[inc.invariant];
    }
    std::ofstream summary(run_dir + "/summary.txt");
    summary << "mode " << static_cast<int>(pt.config.mode) << "  seed "
            << pt.config.seed << '\n'
            << "incidents " << pt.incidents.size() << '\n';
    for (const auto& [name, count] : by_invariant) {
        summary << "  " << name << ' ' << count << '\n';
    }
    summary << "distance_walked_m " << pt.distance_walked << '\n'
            << "sim_seconds " << pt.sim_seconds << '\n'
            << "ticks " << pt.tick << '\n'
            << "gait " << static_cast<int>(pt.config.gait) << '\n'
            // Reported even when no incident fired: "how close did we get" is
            // the number that shows a retune drifting toward visibility long
            // before it crosses the bound.
            << "worst_foot_slip_mm " << pt.worst_slip_m * 1000.0 << '\n';
    if (pt.frame_count > 0) {
        const double mean = pt.frame_dt_sum / static_cast<double>(pt.frame_count);
        summary << "frames " << pt.frame_count << '\n'
                << "mean_fps " << (mean > 0.0 ? 1.0 / mean : 0.0) << '\n'
                << "min_fps " << (pt.frame_dt_max > 0.0 ? 1.0 / pt.frame_dt_max : 0.0)
                << '\n';
    }
}

} // namespace dfn::gameplay
