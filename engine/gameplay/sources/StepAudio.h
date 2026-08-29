/*
Module: engine/gameplay
File: engine/gameplay/sources/StepAudio.h

Responsibility:
- The first audio consumers (в12): footsteps by surface class played ON THE
  TICK their FootfallEvent fires (the research's one non-negotiable — delayed
  footfall feedback destroys the walking illusion), jump/land/water one-shots
  wired to the movement events. ВЕТРА ЗДЕСЬ БОЛЬШЕ НЕТ (28.08) — см. ниже.

Key items:
- StepSoundBank: loaded handles per surface class + one-shots (plain data).
- load_step_sound_bank(): binds the placeholder asset set (documented Rule 5
  exception, moves to the content loader with the JSON reader).
- wire_step_audio(): subscribes the handlers on the EventBus.
- (снято 28.08) WindLoop / start_wind_loop / update_wind_loop — «ветровая
  подушка», игравшая БЕЗ ИСТОЧНИКА: один непространственный голос, слышный
  одинаково в чистом поле, в лесу и внутри дома. Заказ владельца 28.08 назвал
  это дефектом дословно («не должно быть просто так фонового шума — а он даже
  в домах есть»), и замена — не «ветер потише», а другой предмет:
  gameplay::WorldAmbience, где ветер слышен ТОЛЬКО из крон и только на
  расстоянии от них. Строчка оставлена здесь надгробием: следующий, кто пойдёт
  искать start_wind_loop по гриву, найдёт причину, а не пустоту.

Dependencies:
- Uses: platform IAudio, core events EventBus, StepEvents.h, core math
  SurfaceClass, generated constants.
- Used by: engine/app (wiring block), tests.

Notes:
- SAME-TICK GUARANTEE: FootfallEvents are posted in player_post_step and the
  app pumps the bus before the frame is rendered — the handler calls
  IAudio::play_variation inside that pump, so bob minimum, foot plant and
  sound share one tick by construction.
- Footsteps are SPATIAL voices at the event position: for the player that is
  distance zero (full volume), and the same handler serves NPC feet later
  with no change.
- Volumes/pitch jitter are look-dev constants in the .cpp (WindModel
  tradition); they migrate to NUMBERS when a second zone needs them.
- PLACEHOLDER SOUNDS: the wavs this bank loads are synthesized
  (engine/platform/audio/tools/gen_placeholder_sounds.py). Real recorded
  footsteps/wind are a LATER ASSET PASS — recorded here and in the module
  README so nobody mistakes the placeholders for the goal.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not play footsteps from anywhere else: one publisher (the stride clock),
  one consumer (this file) — a second play site desyncs sound from step.
*/

#pragma once

#include <string_view>
#include <vector>

#include "engine/core/math/sources/SurfaceField.h"
#include "engine/platform/audio/interfaces/IAudio.h"

namespace dfn::events {
class EventBus;
}

namespace dfn::gameplay {

// Loaded sound handles for the step vocabulary. Plain data (Rule 8); filled
// once at startup, read by the event handlers.
struct StepSoundBank {
    // Indexed by math::SurfaceClass value; each entry is the take set
    // play_variation rotates through. WaterBed's takes double as the wading
    // override: a wet step splashes regardless of the bed material.
    std::vector<platform::SoundHandle> takes[5];
    platform::SoundHandle jump{};
    platform::SoundHandle land_soft{};
    platform::SoundHandle land_hard{};
    platform::SoundHandle splash{};
    platform::BusHandle bus{}; // the sfx bus (invalid = master)
};

// Loads the placeholder set from `dir` (default asset layout:
// footstep_{grass|gravel|rock|sand|water}_{1..4}.wav, jump_takeoff.wav,
// land_soft.wav, land_hard.wav, splash_enter.wav).
// Rule 5 exception, same standing as the testbed content block in App.cpp:
// these paths move to the content loader the day core's JSON reader lands.
// Missing files yield invalid handles, which the handlers skip — the game
// runs silent-but-correct on a broken asset dir (Rule 3 spirit).
[[nodiscard]] StepSoundBank load_step_sound_bank(platform::IAudio& audio,
                                                 std::string_view dir,
                                                 platform::BusHandle bus);

// Subscribes the FootfallEvent/Jumped/Landed/WaterEntered handlers. The bank
// is copied into the subscriptions; `audio` must outlive the bus (it does:
// both are app-owned singletons). Call once at startup.
void wire_step_audio(events::EventBus& bus, platform::IAudio& audio,
                     const StepSoundBank& bank);

} // namespace dfn::gameplay
