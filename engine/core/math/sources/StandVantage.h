/*
Module: engine/core/math
File: engine/core/math/sources/StandVantage.h

Responsibility:
- ONE ACCEPTANCE STANDPOINT, as plain data crossing the world -> render seam.
  A stand knows where its own evidence is; the Tour lives in render and cannot
  see `dfn::world` (DAG siblings). This is the type that carries the answer.

Key items:
- StandVantage: label + standpoint + aim, everything a TourStep needs.

Dependencies:
- Uses: glm, <string>.
- Used by: world::forest_vantages / ChunkManager::stand_vantages (producer),
  render::Tour::forest_steps (consumer; requested by render 10.08.2026).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO ORDINALS CROSS THIS SEAM. There is deliberately no `kind` enum here: an
  enum mapped positionally across a DAG seam is the defect flora caught on
  PathClass, and it is silent by construction. The vantage's kind travels in
  its LABEL, which is a string the consumer already uses as the frame's
  filename — a rename is a visible diff in the acceptance archive, a renumber
  is nothing at all.
- RULE 27 IS THE POINT OF THIS FILE. A vantage that cannot fail is not
  evidence, so producers emit CONTROLS alongside claims (the flat-glade frame
  next to the swale-floor frame, the goal-visible station next to the
  goal-hidden one) and the consumer shoots the pair or neither.
*/

#pragma once

#include <glm/vec2.hpp>
#include <string>

namespace dfn::math {

/// One acceptance standpoint. Everything is already resolved into the form a
/// camera takes: the producer has made the framing decision, the consumer
/// decides only ordering, settle time and whether to shoot it at all.
struct StandVantage {
    /// Filename-safe frame name, and the only place the vantage's KIND is
    /// encoded (see the notice above). Controls are named so the pairing is
    /// legible in a directory listing: `lf2_swale_floor` /
    /// `lf2_glade_control`, `br1_hidden_r3` / `br1_visible_control_r3`.
    std::string label;
    /// Standpoint, world x/z. The eye height is `eye_offset` ABOVE the terrain
    /// here — the producer does not know the shipped terrain at query time in
    /// every caller, and the consumer resolves ground per frame anyway.
    glm::vec2 position{0.0f};
    float eye_offset = 0.0f; ///< meters above the terrain at `position`
    float yaw = 0.0f;        ///< radians, atan2(d.x, -d.y) toward `subject`
    float pitch = 0.0f;      ///< radians, NEGATIVE = looking down
    /// What the frame is aimed at, world x/z. Redundant with `yaw` and kept
    /// anyway: a consumer that wants to re-aim (a wider lens, a step back for
    /// headroom) needs the SUBJECT, and recovering it from a yaw plus a guessed
    /// range is how a frame ends up pointing at nothing in particular.
    glm::vec2 subject{0.0f};
};

} // namespace dfn::math
