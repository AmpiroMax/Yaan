/*
Created: 10:08:2026 - 01:56:45
Last updated: 10:08:2026 - 20:00:23
Module: engine/anim
File: engine/anim/sources/Body.h

Responsibility:
- The rigid-segmented humanoid body in the ECS: spawning a body's segment
  entities, evaluating its pose each fixed tick (locomotion from sim's stride
  clock, showcase clips), the mirror puppet (grill в11), and writing segment
  Transforms for render's ordinary interpolated pass.

Key items:
- BodyRig / BodyDrive / MirrorPuppet: plain-data components (Rule 8), this
  zone's own (only the app composes them; no other zone includes this).
- spawn_body / destroy_body: create/remove the 15 segment entities.
- spawn_mirror_puppet: a second body that mirrors a source body across a
  vertical plane, or floats and cycles showcase clips.
- update_bodies(world): fixed-tick system — pose, FK, segment Transform pairs.
- note_landed(): landing-dip trigger, keyed off sim's Landed event (app ferry).

Dependencies:
- Uses: Rig/Pose/Clips/BodyMesh, core ecs + components, generated constants.
- Used by: engine/app (spawn + per-tick update + drive ferry), tests.

Notes:
- THE DRIVE IS A FERRY, NOT A CLOCK (agreed with sim, 10:08:2026): the app
  copies sim's PlayerState stride phase/speed/etc. into BodyDrive each fixed
  tick. This zone never advances the phase. The DAG is why it is a copy at
  the composition root: anim sits below gameplay and cannot read PlayerState.
- Call update_bodies AFTER player_post_step (same-tick pose, the ViewModel
  precedent: a stale pose reads as the body lagging the camera) and BEFORE
  render. Segments snapshot curr->prev themselves inside update_bodies.
- The player's body hides the HEAD segment (camera sits inside the skull);
  everything else — chest, arms, legs, feet — is deliberately visible
  (user decision в11: full first-person body).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Components are plain data (Rule 8). Segment entities are spawned at init
  paths, never on streaming paths (Rule 11 does not bite here).
*/
/*
UPD:
- 10:08:2026 - 01:56:45: Initial body/puppet systems.
- 10:08:2026 - 20:00:23: BodyDrive::gait — the ferry target lead's parked switch writes into.
*/

#pragma once

#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/ecs/sources/EntityId.h"

#include <array>
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace dfn::ecs {
class World;
}

namespace dfn::anim {

inline constexpr uint8_t SHOWCASE_NONE = 0xFF;

// The body's segment entities, one per bone (Rule 8: ids, not pointers).
struct BodyRig {
    std::array<ecs::EntityId, BONE_COUNT> segments{};
    bool hide_head = false; // first person: camera sits inside the skull
};

// Per-tick drive values, FERRIED by the app from sim's PlayerState (see
// header note). This zone only reads them; the app only writes them.
struct BodyDrive {
    float stride_phase = 0.0f;      // sim's clock, [0,1)
    float step_length_m = 0.0f;     // sim's length(v) model output
    float speed_mps = 0.0f;         // horizontal speed
    // THE GEAR sim CHOSE, not the speed it was derived from. This zone used to
    // re-derive it by comparing speed_mps against WALK_SPEED and RUN_SPEED,
    // which is the two-copies defect (Rule 35) and became a visible one when a
    // third gear landed between the two rows (Rule 37). speed_mps survives for
    // the idle<->moving fade only, which is a question about whether the feet
    // are moving at all and has no gears in it.
    Gait gait = Gait::Walk;
    float facing_yaw = 0.0f;        // radians, sim's yaw convention
    bool grounded = true;
    float vertical_velocity = 0.0f; // m/s, + up
    float crouch_blend = 0.0f;      // sim's eased 0..1
    // Internal animation state (this zone's, decayed/advanced in update).
    float land_dip = 0.0f;          // 1 at touchdown -> 0
    float anim_time_s = 0.0f;       // idle breathing clock (fixed-tick sum)
    // Showcase override (mirror map techno-demo): SHOWCASE_NONE = live body.
    uint8_t showcase_clip = SHOWCASE_NONE;
    float showcase_time_s = 0.0f;
};

// Mirrors `source`'s pose across the vertical plane each tick (I turn left,
// it turns right); or, in showcase mode, floats at hover_height cycling clips.
struct MirrorPuppet {
    ecs::EntityId source{};
    glm::vec3 plane_point{0.0f};
    glm::vec2 plane_normal_xz{0.0f, 1.0f}; // unit, horizontal
    bool showcase = false;
    float hover_height_m = 0.0f;           // showcase float height
    float clip_seconds = 0.0f;             // per-clip dwell before advancing
};

// Adds BodyRig + BodyDrive to `owner` (which must have Transform) and spawns
// the segment entities (Transform + PreviousTransform + RenderMesh +
// LocalBounds). hide_head omits the head MESH, not the head bone.
void spawn_body(ecs::World& world, ecs::EntityId owner, const Rig& rig, bool hide_head);

// Destroys the segment entities and removes the body components.
void destroy_body(ecs::World& world, ecs::EntityId owner);

// Spawns a standalone puppet body whose root Transform this system owns:
// mirror of `source` across the plane, or the floating showcase double.
[[nodiscard]] ecs::EntityId spawn_mirror_puppet(ecs::World& world, const Rig& rig,
                                                ecs::EntityId source,
                                                const glm::vec3& plane_point,
                                                const glm::vec2& plane_normal_xz);

// Landing-dip trigger: the app calls this when sim's Landed event fires.
void note_landed(ecs::World& world, ecs::EntityId owner, float impact_speed_mps);

// Pure pose evaluation (exposed for tests and the puppet): locomotion blend
// (idle <-> gait by speed, air when not grounded, crouch and landing layers)
// or the showcase clip when drive.showcase_clip != SHOWCASE_NONE.
[[nodiscard]] LocalPose evaluate_body_pose(const Rig& rig, const BodyDrive& drive);

// Fixed-tick system: advances internal clocks (SIM_DT), evaluates every body,
// runs FK, writes segment Transform pairs. Mirror puppets are evaluated after
// their sources (two passes) so they mirror THIS tick's pose.
void update_bodies(ecs::World& world, const Rig& rig);

} // namespace dfn::anim
