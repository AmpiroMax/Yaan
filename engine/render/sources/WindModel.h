/*
Module: engine/render
File: engine/render/sources/WindModel.h

Responsibility:
- The wind model (user request: foliage that «якобы перемещалась и шуршала»).
  Turns render-side visual time into ONE wind for the world — direction,
  current strength including the gust envelope, and flutter — which foliage
  reads today and grass and cloth will read later.

Key items:
- apply_wind(env, seconds): the single app-facing call.
- wind_gust_envelope(seconds): the gust curve, exposed so audio and gameplay
  can ask the same question the visuals answer.

Dependencies:
- Uses: IRenderer.h (RenderEnvironment), glm.
- Used by: engine/app (once per frame, next to apply_sky_time), tests, and
  later the audio layer for the rustle.

Notes:
- THE GUST ENVELOPE IS COMPUTED HERE, ON THE CPU, and shipped as one scalar in
  RenderEnvironment::wind_strength. The obvious implementation — deriving gusts
  from time inside the vertex shader — would put the wind's state somewhere
  nothing outside the GPU can read, and a rustle could then never be synced to
  the picture except by luck. Everything the shader adds on top is per-instance
  phase and a spatial travel term, which vary the LOOK without changing the
  global "how windy is it right now" that audio needs.
- Render-side visual time only (Rule 12): wind never touches the fixed tick and
  never affects determinism. A replay with different frame timing gets
  different leaf positions and identical simulation.
- Weather (clear/overcast/fog, decided but not built) modulates base strength
  and flutter and nothing else — the shape of this API does not change for it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep this pure: no GPU, no ECS, no clock reads. Time arrives as a parameter.
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"

#include <glm/vec2.hpp>

namespace dfn::render {

/// Look-dev defaults (NUMBERS.md migration list, like the other look values).
/// Base strength is the calm-day baseline; gusts swing around it.
inline constexpr float WIND_BASE_STRENGTH = 0.35f;
inline constexpr float WIND_GUST_AMPLITUDE = 0.45f;
/// Seconds per gust cycle (the slow breathing of a stand), and the faster
/// secondary period that stops the envelope reading as a pure sine.
inline constexpr float WIND_GUST_PERIOD_S = 11.0f;
inline constexpr float WIND_GUST_PERIOD2_S = 4.3f;
/// Default wind heading (world x/z), from the west-north-west.
inline const glm::vec2 WIND_DEFAULT_DIRECTION{0.87f, 0.50f};

/// The gust envelope at a moment: 0 = dead calm, 1 = the strongest gust this
/// model produces. Exposed separately from apply_wind so the audio layer can
/// drive a rustle from the SAME curve the leaves are bending to, and gameplay
/// can ask questions like "has the wind dropped".
[[nodiscard]] float wind_gust_envelope(float seconds);

/// Writes wind_direction / wind_strength / wind_flutter into `env`. Every
/// other field is untouched. Call once per frame before RenderSystem::render.
void apply_wind(platform::RenderEnvironment& env, float seconds);

} // namespace dfn::render
