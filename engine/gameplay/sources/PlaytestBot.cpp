/*
Created: 10:08:2026 - 02:23:05
Last updated: 10:08:2026 - 20:14:20
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
- 10:08:2026 - 19:48:10: Foot-slip bound is now ABSOLUTE (30 mm), not 5% of
                         step length: the fraction grew to 122 mm at RUN_SPEED,
                         where no value of it separated an accepted run from a
                         rejected one (Rule 30 -- the quantity was wrong, not
                         the threshold). Ground-contact counters added, which
                         REFUTED the landing-dip-retrigger hypothesis for the
                         running judder by measurement.
- 10:08:2026 - 20:14:20: Frame trace at a 20 ms threshold, below the 50 ms
                         incident gate on purpose: an event's shape lives in
                         its shoulders and a threshold at the gate records only
                         the peak. It answered render's sizing question -- the
                         chunk-boundary stall is TWO consecutive ~390 ms frames
                         (3/3 runs), not one, so a per-frame budget would not
                         decompose it.
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
// The TRACE threshold is deliberately lower than the incident budget: an
// event's shape is in its shoulders, and a threshold set at the gate would
// only ever record the peak. 20 ms is ~2.3x the observed mean frame, low
// enough to catch a re-ship spread over several frames and high enough that a
// healthy 45 s run traces nothing.
constexpr float FRAME_TRACE_SECONDS = 0.020f;
constexpr size_t TRACE_MAX = 256; // bounded: a trace is evidence, not a log
constexpr float SPEED_BOUND_MARGIN = 1.2f;        // legal gait + 20 %
constexpr uint64_t INCIDENT_COOLDOWN_TICKS = 120; // one finding per 2 s episode
// FOOT SLIP tolerance, and it is ABSOLUTE ON PURPOSE — this replaced a
// fraction-of-step-length bound, for a reason worth keeping written down.
//
// The old bound was 5% of step_length(stride_speed). That tracks a retune,
// which sounds like a virtue, but it makes the bound GROW WITH SPEED: 49 mm at
// WALK_SPEED, 122 mm at RUN_SPEED. A foot sliding 122 mm through a stance is
// grossly visible, and the check would have called it a pass — so at the very
// gait the movement ruling was about, no value of the fraction separated an
// accepted run from a rejected one. Rule 30's mechanical form says that makes
// the QUANTITY wrong, not the threshold: perceptibility of a slide is a
// distance on screen, and it does not get more forgiving because the player is
// going faster.
//
// The bound is placed between the two REAL instances, not chosen for roundness:
//  - accepted: at WALK_SPEED the swing clamp still binds and leaves 7.8 mm of
//    residual per step (step_length(1.8) = 0.980 m against the clamped
//    0.55 x 0.884 x 2 = 0.972 m). That is CORRECT code — Rule 38 — so the
//    bound must sit above it or it goes red on the day it is written.
//  - rejected: the 31% mismatch that shipped for hours, ~434 mm at JOG_SPEED.
// 30 mm is 3.8x above the accepted residual and 14x below the rejected
// instance. Rule 38's corollary, re-verified rather than assumed: the control
// still fails this bound by more than an order of magnitude.
//
// SECOND ZONE WARNING (Rule 35): character tunes clips against this number the
// moment the gait clips land, at which point it stops being a tool threshold
// and becomes a registry row. Row requested from the lead.
constexpr float SLIP_TOLERANCE_M = 0.030f;
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

        // GROUND CONTACT bookkeeping (measured, never gated — there is no
        // threshold here yet, so there is nothing for a control to reject).
        // The landing edge is the DIP RETRIGGER: player_post_step restarts
        // dip_elapsed at every airborne->grounded transition.
        if (state.airborne) {
            ++pt.airborne_ticks;
        } else {
            ++pt.grounded_ticks;
        }
        if (pt.has_prev_airborne && pt.prev_airborne && !state.airborne) {
            ++pt.landings;
            pt.worst_landing_dip_m =
                std::max(pt.worst_landing_dip_m, static_cast<double>(state.dip_depth));
        }
        pt.prev_airborne = state.airborne;
        pt.has_prev_airborne = true;

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
                ++pt.foot_samples_seen;
                if (feet->left_planted || feet->right_planted) {
                    ++pt.foot_planted_ticks;
                }
                const float bound = SLIP_TOLERANCE_M;
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
    // The trace, before the deduped incident: every frame over the trace
    // threshold, in order, so consecutive bad frames stay consecutive.
    if (frame_dt_seconds > FRAME_TRACE_SECONDS && pt.frame_trace.size() < TRACE_MAX) {
        pt.frame_trace.push_back(
            PlaytestState::FrameMark{pt.frame_count, pt.tick, frame_dt_seconds * 1000.0f});
    }
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
            // FOOT SLIP. The verdict line comes FIRST and is a word, not a
            // number, because the number 0 means two opposite things and a
            // reader scanning a summary will take the friendlier one.
            << "foot_slip_verdict "
            << (pt.foot_samples_seen == 0
                    ? "NOT_MEASURED(rig seam unbound - this is not a pass)"
                    : (pt.foot_planted_ticks == 0
                           ? "NOT_MEASURED(feet never reported planted)"
                           : "measured"))
            << '\n'
            << "foot_sample_ticks " << pt.foot_samples_seen << '\n'
            << "foot_planted_ticks " << pt.foot_planted_ticks << '\n'
            << "foot_slip_bound_mm " << SLIP_TOLERANCE_M * 1000.0f << '\n'
            << "worst_foot_slip_mm " << pt.worst_slip_m * 1000.0 << '\n'
            // Ground contact. `landings_per_sim_second` is the rate the camera
            // dip is RE-ARMED; compare it against the stride's own footfall
            // rate (2 * speed / (2 * step_length)) — a landing rate above the
            // footfall rate means the punctuation curve never finishes, which
            // is a judder rather than a landing.
            << "airborne_ticks " << pt.airborne_ticks << '\n'
            << "grounded_ticks " << pt.grounded_ticks << '\n'
            << "airborne_fraction "
            << (pt.tick > 0 ? static_cast<double>(pt.airborne_ticks)
                                  / static_cast<double>(pt.tick)
                            : 0.0)
            << '\n'
            << "landings " << pt.landings << '\n'
            << "landings_per_sim_second "
            << (pt.sim_seconds > 0.0 ? static_cast<double>(pt.landings) / pt.sim_seconds
                                     : 0.0)
            << '\n'
            << "worst_landing_dip_mm " << pt.worst_landing_dip_m * 1000.0 << '\n';
    if (pt.frame_count > 0) {
        const double mean = pt.frame_dt_sum / static_cast<double>(pt.frame_count);
        summary << "frames " << pt.frame_count << '\n'
                << "mean_fps " << (mean > 0.0 ? 1.0 / mean : 0.0) << '\n'
                << "min_fps " << (pt.frame_dt_max > 0.0 ? 1.0 / pt.frame_dt_max : 0.0)
                << '\n';
        summary << "frame_trace_threshold_ms " << FRAME_TRACE_SECONDS * 1000.0f << '\n'
                << "frame_trace_count " << pt.frame_trace.size()
                << (pt.frame_trace.size() >= TRACE_MAX ? " (TRUNCATED)" : "") << '\n';
        // frame_index is CONSECUTIVE-READABLE on purpose: adjacent indices mean
        // adjacent frames, which is the whole question this trace answers.
        for (const auto& m : pt.frame_trace) {
            summary << "  frame " << m.frame_index << "  tick " << m.tick << "  "
                    << m.ms << " ms\n";
        }
    }
}

} // namespace dfn::gameplay
