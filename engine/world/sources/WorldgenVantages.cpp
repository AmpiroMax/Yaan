/*
Module: engine/world
File: engine/world/sources/WorldgenVantages.cpp

Responsibility:
- forest_vantages(): the §8.1 stand's acceptance standpoints and their
  controls. See WorldgenVantages.h for the list and the Rule 27 obligation.

Dependencies:
- Uses: WorldgenVantages.h, WorldgenForest.h (the LF-2 field), config, glm.
- Used by: ChunkManager, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 13.1: fixed lattice, fixed order, first-hit tie-breaks.
*/

#include "engine/world/sources/WorldgenVantages.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/WorldgenForest.h"

#include <cmath>
#include <glm/geometric.hpp>
#include <string>

namespace dfn::world {

namespace {

constexpr float EYE_M = static_cast<float>(config::PLAYER_EYE_HEIGHT);
constexpr float HIDE_MIN_RUN_M = static_cast<float>(config::HIDE_REVEAL_MIN_RUN_M);

/// The tour's own convention (render::Tour::aim_yaw), reproduced here because
/// the vantage arrives at the camera already aimed. If render's convention
/// ever changes this is the one line that has to move with it.
[[nodiscard]] float aim_yaw(glm::vec2 from, glm::vec2 to) {
    const glm::vec2 d = to - from;
    return std::atan2(d.x, -d.y);
}

[[nodiscard]] math::StandVantage aimed(std::string label, glm::vec2 from, glm::vec2 at,
                                       float pitch, float eye = EYE_M) {
    math::StandVantage v;
    v.label = std::move(label);
    v.position = from;
    v.eye_offset = eye;
    v.yaw = aim_yaw(from, at);
    v.pitch = pitch;
    v.subject = at;
    return v;
}

[[nodiscard]] const char* class_name(PathClass c) {
    switch (c) {
    case PathClass::Cobble: return "cobble";
    case PathClass::Dirt: return "dirt";
    case PathClass::FaintTrail: return "faint_trail";
    case PathClass::StoneSteps: return "stone_steps";
    }
    return "dirt";
}

[[nodiscard]] const char* goal_name(GoalKind k) {
    switch (k) {
    case GoalKind::ClearingShrine: return "clearing_shrine";
    case GoalKind::Spring: return "spring";
    case GoalKind::WoodcuttersHut: return "woodcutters_hut";
    case GoalKind::SpireGroup: return "spire_group";
    case GoalKind::CrestCairn: return "crest_cairn";
    }
    return "goal";
}

/// THE LOCAL GRAIN OF LF-2, as an axis angle in [0, pi).
///
/// The grives are direction-coherent ridges, so the field varies LEAST along
/// them: the axis of minimum variation IS the swale/ridge axis. One instrument,
/// used twice — the swale frame looks ALONG it (down the channel) and the crest
/// frame looks ACROSS it (through successive ridge and swale, which is what
/// makes the rhythm read at all).
///
/// Probed to 25 m: shorter than the 100 m wavelength, so the sample stays on
/// one grive rather than averaging over the next one and reporting the mean
/// direction of the whole field. (The elongation instrument in the acceptance
/// suite made exactly the opposite mistake at 72 m and measured itself.)
[[nodiscard]] float grive_axis(uint64_t seed, glm::vec2 p) {
    constexpr int AXES = 8;
    constexpr float PROBE_M = 25.0f;
    constexpr int RINGS = 3;
    const float c0 = forest_grive_component(seed, p, false);
    float best_dev = 1e18f;
    float best_ang = 0.0f;
    for (int i = 0; i < AXES; ++i) {
        const float a = 3.14159265f * static_cast<float>(i) / static_cast<float>(AXES);
        const glm::vec2 d{std::cos(a), std::sin(a)};
        float dev = 0.0f;
        for (int s = 1; s <= RINGS; ++s) {
            const float t = PROBE_M * static_cast<float>(s) / static_cast<float>(RINGS);
            dev += std::fabs(forest_grive_component(seed, p + d * t, false) - c0);
            dev += std::fabs(forest_grive_component(seed, p - d * t, false) - c0);
        }
        if (dev < best_dev) { // strict <: first of equals wins (Rule 13.1)
            best_dev = dev;
            best_ang = a;
        }
    }
    return best_ang;
}

/// Nearest station of any route to `p`, or -1/-1 when the network is empty.
struct NearStation {
    int route = -1;
    int station = -1;
    float dist = 1e18f;
};
[[nodiscard]] NearStation nearest_station(const PathNetwork& net, glm::vec2 p) {
    NearStation best;
    for (std::size_t ri = 0; ri < net.routes.size(); ++ri) {
        const PathRoute& r = net.routes[ri];
        for (std::size_t si = 0; si < r.points.size(); ++si) {
            const float d = glm::length(r.points[si] - p);
            if (d < best.dist) {
                best.dist = d;
                best.route = static_cast<int>(ri);
                best.station = static_cast<int>(si);
            }
        }
    }
    return best;
}

/// Unit tangent of a route at a station, pointing forward along it.
[[nodiscard]] glm::vec2 tangent_at(const PathRoute& r, std::size_t i) {
    const std::size_t j = (i + 1 < r.points.size()) ? i + 1 : i;
    const std::size_t k = (j == i && i > 0) ? i - 1 : i;
    const glm::vec2 d = (j == i) ? (r.points[k] - r.points[i]) : (r.points[j] - r.points[i]);
    const float len = glm::length(d);
    return (len > 1e-4f) ? d / len : glm::vec2{0.0f, 1.0f};
}

// ---- the individual vantage families ---------------------------------------

/// ON the tread, aimed down the longest STRAIGHT run of each class the network
/// actually built. Straightness is the selection criterion because the claim
/// under test is about the MARGINS: on a bend the far verge swings out of
/// frame and the near one fills it, and no reader could tell a rich edge from
/// a wide one.
void push_path_along(const PathNetwork& net, std::vector<math::StandVantage>& out) {
    constexpr int RUN = 8;          // stations of tread that must stay in class
    constexpr float PITCH = -0.13f; // the rich edge is GROUND: put it in frame
    for (int ci = 0; ci <= static_cast<int>(PathClass::StoneSteps); ++ci) {
        const auto want = static_cast<PathClass>(ci);
        float best_straight = -1.0f;
        int best_route = -1;
        int best_station = -1;
        for (std::size_t ri = 0; ri < net.routes.size(); ++ri) {
            const PathRoute& r = net.routes[ri];
            if (r.points.size() < static_cast<std::size_t>(RUN) + 2) {
                continue;
            }
            for (std::size_t si = 0; si + RUN + 1 < r.points.size(); ++si) {
                if (r.classes[si] != want) {
                    continue;
                }
                const glm::vec2 t0 = tangent_at(r, si);
                float straight = 0.0f;
                bool same_class = true;
                for (int k = 1; k <= RUN; ++k) {
                    const std::size_t sk = si + static_cast<std::size_t>(k);
                    if (r.classes[sk] != want) {
                        same_class = false;
                        break;
                    }
                    straight += glm::dot(t0, tangent_at(r, sk));
                }
                if (!same_class) {
                    continue;
                }
                if (straight > best_straight) {
                    best_straight = straight;
                    best_route = static_cast<int>(ri);
                    best_station = static_cast<int>(si);
                }
            }
        }
        if (best_route < 0) {
            continue; // the network never built this class — omit, never fake
        }
        const PathRoute& r = net.routes[static_cast<std::size_t>(best_route)];
        const auto si = static_cast<std::size_t>(best_station);
        const glm::vec2 look = r.points[std::min(si + RUN, r.points.size() - 1)];
        out.push_back(aimed(std::string("path_along_") + class_name(want), r.points[si], look,
                            PITCH));
    }
}

/// Short of each goal on the route that reaches it. BR-2 clause (i) is a
/// property of the data — these frames are what makes it a property of the
/// WORLD, and a goal whose route arrives at bare ground fails them.
void push_goals(const PathNetwork& net, std::vector<math::StandVantage>& out) {
    constexpr float STAND_BACK_M = 18.0f;
    for (std::size_t gi = 0; gi < net.goals.size(); ++gi) {
        const Goal& g = net.goals[gi];
        // The station on any route that is closest to STAND_BACK_M from this
        // goal: an approach frame, not an overhead one.
        float best_err = 1e18f;
        glm::vec2 stand{0.0f};
        bool found = false;
        for (const PathRoute& r : net.routes) {
            if (r.goal_a != static_cast<int>(gi) && r.goal_b != static_cast<int>(gi)) {
                continue;
            }
            for (const glm::vec2& p : r.points) {
                const float err = std::fabs(glm::length(p - g.position) - STAND_BACK_M);
                if (err < best_err) {
                    best_err = err;
                    stand = p;
                    found = true;
                }
            }
        }
        if (!found) {
            continue;
        }
        out.push_back(aimed(std::string("goal_") + goal_name(g.kind), stand, g.position, -0.05f));
    }
}

/// BR-1's own evidence, AS A PAIR (Rule 30b). Only the route with the longest
/// hidden run is shot: the claim is that the trace hides its destination
/// SOMEWHERE, and six copies of it is six chances to pick the flattering one.
void push_br1(const PathNetwork& net, std::vector<math::StandVantage>& out) {
    // THE PAIR IS THE UNIT, so the route is chosen by the quality of the PAIR
    // and not by the size of the claim. Picking the longest hidden run first
    // and taking whatever control that route happened to offer produced a
    // 402 m frame against a 185 m control — two pictures at different ranges,
    // which is exactly the confound the control exists to remove. So: among
    // routes whose hidden run clears the ruled minimum, take the one whose
    // control range matches its claim range best, and require the match.
    constexpr float MAX_RANGE_MISMATCH_M = 25.0f;
    constexpr float FRAME_RANGE_M = 120.0f; // WorldgenPaths.cpp's BR1_FRAME_RANGE_M
    int best_route = -1;
    float best_score = 1e18f;
    for (std::size_t ri = 0; ri < net.routes.size(); ++ri) {
        const PathRoute& r = net.routes[ri];
        if (r.hidden_station < 0 || r.visible_station < 0
            || r.longest_hidden_run_m < HIDE_MIN_RUN_M) {
            continue;
        }
        const glm::vec2 goal = net.goals[static_cast<std::size_t>(r.goal_b)].position;
        const float claim_range =
            glm::length(r.points[static_cast<std::size_t>(r.hidden_station)] - goal);
        const float err = std::fabs(
            claim_range
            - glm::length(r.points[static_cast<std::size_t>(r.visible_station)] - goal));
        if (err >= MAX_RANGE_MISMATCH_M) {
            continue; // unmatched: not a control, whatever else it has going for it
        }
        // Among the matched pairs, the one shot closest to the range at which
        // the goal would READ. Matching alone is satisfied perfectly by two
        // adjacent stations 400 m out, where neither frame could show the goal.
        // Both terms, weighted: a tight range match is worth more than a
        // perfect standoff, because the match is what makes it a CONTROL and
        // the standoff only makes it a nicer picture.
        const float score = std::fabs(claim_range - FRAME_RANGE_M) + 2.0f * err;
        if (score < best_score) {
            best_score = score;
            best_route = static_cast<int>(ri);
        }
    }
    if (best_route < 0) {
        // No route both hides its goal for the ruled distance AND offers a
        // range-matched control. Half a pair is not evidence, so nothing ships.
        return;
    }
    const PathRoute& r = net.routes[static_cast<std::size_t>(best_route)];
    const glm::vec2 goal = net.goals[static_cast<std::size_t>(r.goal_b)].position;
    const std::string tag = "_r" + std::to_string(best_route);
    // THE LABEL CARRIES THE PAIRING, and mechanically: a control is its
    // claim's label plus "_control", so a test can walk the list and catch an
    // orphan. The first cut named them "br1_hidden" / "br1_visible_control",
    // which reads better and pairs only in a human's head — and a Rule 27
    // obligation that only a human can check is one that stops being checked.
    const std::string claim = "br1_hidden" + tag;
    out.push_back(aimed(claim, r.points[static_cast<std::size_t>(r.hidden_station)], goal, 0.0f));
    out.push_back(aimed(claim + "_control",
                        r.points[static_cast<std::size_t>(r.visible_station)], goal, 0.0f));
}

/// LF-2's two reads and the flat control they are judged against.
void push_lf2(uint64_t seed, const TestbedLayout& layout, const PathNetwork& net,
              glm::vec2 domain_min, glm::vec2 domain_max,
              std::vector<math::StandVantage>& out) {
    constexpr float STEP_M = 16.0f;
    constexpr float PATH_CLEAR_M = 14.0f; // these frames are about the LANDFORM
    constexpr float LOOK_M = 60.0f;
    /// Keep the standpoint clear of the world's edge by more than the frame's
    /// own depth. The first cut scanned from domain_min and duly reported the
    /// extremes at (16,16) and (784,32) — a swale-floor frame with the end of
    /// the world in it, which judges the streaming boundary rather than LF-2.
    /// Twice the look distance so the flanks either side are real ground too.
    constexpr float EDGE_MARGIN_M = LOOK_M * 2.0f;
    const glm::vec2 mid = (domain_min + domain_max) * 0.5f;
    const float glade_r = layout.forests.forced_clearing_radius;
    const glm::vec2 glade_c = layout.forests.forced_clearing_center;

    /// How tall the grives around a candidate stand. THE SWALE FLOOR CANNOT BE
    /// FOUND BY MINIMISING: LF2_SWALE_FLOOR_FRAC holds 55 % of the ground at
    /// the floor EXACTLY, so "the minimum" is a plateau of thousands of tied
    /// zeros and the winner is whichever the scan reached first — which is why
    /// the first cut put the camera in the corner of the domain. The floor is
    /// therefore chosen among the ties by what makes the FRAME: the floor with
    /// the tallest flanks around it, since a swale with nothing either side of
    /// it is indistinguishable from the glade this frame is controlled against.
    const auto flank_height = [&](glm::vec2 p) {
        float hi = 0.0f;
        for (int i = 0; i < 8; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / 8.0f;
            const glm::vec2 d{std::cos(a), std::sin(a)};
            for (const float t : {30.0f, 60.0f}) {
                hi = std::max(hi, forest_grive_component(seed, p + d * t, false));
            }
        }
        return hi;
    };

    glm::vec2 floor_p{0.0f};
    glm::vec2 crest_p{0.0f};
    float floor_flank = -1.0f;
    float crest_v = -1e18f;
    bool any = false;
    for (float z = domain_min.y + EDGE_MARGIN_M; z < domain_max.y - EDGE_MARGIN_M; z += STEP_M) {
        for (float x = domain_min.x + EDGE_MARGIN_M; x < domain_max.x - EDGE_MARGIN_M;
             x += STEP_M) {
            const glm::vec2 p{x, z};
            // Outside the authored calm plain (its grives are deliberately
            // tapered away — a floor found in there is the glade, not a swale)
            // and off the treads, whose flatten delta is not LF-2's doing.
            if (glade_r > 0.0f && glm::length(p - glade_c) < glade_r * 1.6f) {
                continue;
            }
            if (nearest_station(net, p).dist < PATH_CLEAR_M) {
                continue;
            }
            const float v = forest_grive_component(seed, p, false);
            // "On the floor" is a THRESHOLD, not an argmin: the field is
            // exactly 0 across the whole floor network (see flank_height).
            if (v < 0.05f) {
                const float fl = flank_height(p);
                if (fl > floor_flank) {
                    floor_flank = fl;
                    floor_p = p;
                }
            }
            if (v > crest_v) {
                crest_v = v;
                crest_p = p;
            }
            any = true;
        }
    }
    if (!any) {
        return;
    }

    const float axis = grive_axis(seed, floor_p);
    glm::vec2 along{std::cos(axis), std::sin(axis)};
    // Look INTO the domain, not off the edge of the world.
    if (glm::dot(mid - floor_p, along) < 0.0f) {
        along = -along;
    }
    out.push_back(aimed("lf2_swale_floor", floor_p, floor_p + along * LOOK_M, -0.06f));

    const float caxis = grive_axis(seed, crest_p);
    glm::vec2 across{-std::sin(caxis), std::cos(caxis)};
    if (glm::dot(mid - crest_p, across) < 0.0f) {
        across = -across;
    }
    out.push_back(aimed("lf2_crest", crest_p, crest_p + across * LOOK_M, -0.10f));

    // THE CONTROL, and it is not a synthetic one: в9's authored calm plain is
    // real shipped ground on this stand. Same pitch and the SAME COMPASS
    // BEARING as the swale frame, so the two differ in the landform underfoot
    // and in nothing else. If the swale frame looks like this one, LF-2 is not
    // there — which is precisely what the swale frame alone could never say.
    if (glade_r > 0.0f) {
        out.push_back(aimed("lf2_swale_floor_control", glade_c, glade_c + along * LOOK_M,
                            -0.06f));
    }
}

/// One find per BR-6 regime, at approach range, standing where a walker coming
/// off the nearest path would first have it in frame.
void push_finds(const PathNetwork& net, const std::vector<Find>& finds, glm::vec2 domain_min,
                glm::vec2 domain_max, std::vector<math::StandVantage>& out) {
    constexpr float APPROACH_M = 14.0f;
    constexpr float EDGE_MARGIN_M = 120.0f;
    for (const auto regime : {FindRegime::NearRoad, FindRegime::Wilderness}) {
        const Find* pick = nullptr;
        // First of the regime clear of the world's edge (fixed order, so the
        // choice is reproducible); the first of the regime at all otherwise,
        // because a find frame's subject fills it at 14 m and a near-edge
        // background is a blemish rather than a disqualification.
        for (const Find& f : finds) {
            if (f.regime != regime) {
                continue;
            }
            const bool inside = f.position.x > domain_min.x + EDGE_MARGIN_M
                             && f.position.x < domain_max.x - EDGE_MARGIN_M
                             && f.position.y > domain_min.y + EDGE_MARGIN_M
                             && f.position.y < domain_max.y - EDGE_MARGIN_M;
            if (inside) {
                pick = &f;
                break;
            }
            if (pick == nullptr) {
                pick = &f;
            }
        }
        if (pick == nullptr) {
            continue;
        }
        const NearStation ns = nearest_station(net, pick->position);
        glm::vec2 from_dir{1.0f, 0.0f};
        if (ns.route >= 0) {
            const glm::vec2 d =
                net.routes[static_cast<std::size_t>(ns.route)]
                    .points[static_cast<std::size_t>(ns.station)]
                - pick->position;
            const float len = glm::length(d);
            if (len > 1e-3f) {
                from_dir = d / len;
            }
        }
        const glm::vec2 stand = pick->position + from_dir * APPROACH_M;
        out.push_back(aimed(regime == FindRegime::NearRoad ? "find_near_road"
                                                           : "find_wilderness",
                            stand, pick->position, -0.12f));
    }
}

} // namespace

std::vector<math::StandVantage> forest_vantages(uint64_t seed, const TestbedLayout& layout,
                                                const PathNetwork& net,
                                                const std::vector<Find>& finds) {
    std::vector<math::StandVantage> out;
    push_path_along(net, out);
    push_goals(net, out);
    push_br1(net, out);
    // The stand's domain, taken from the oak mass that covers it (§8.1) rather
    // than from a constant, so a resized stand moves the search with it.
    const glm::vec4 r = layout.forests.oak_rects[0];
    if (r.z > r.x && r.w > r.y) {
        push_lf2(seed, layout, net, {r.x, r.y}, {r.z, r.w}, out);
        push_finds(net, finds, {r.x, r.y}, {r.z, r.w}, out);
    }
    return out;
}

} // namespace dfn::world
