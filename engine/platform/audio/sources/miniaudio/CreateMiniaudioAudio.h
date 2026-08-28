/*
Created: 10:08:2026 - 01:53:17
Last updated: 28:08:2026 - 14:10:00
Module: engine/platform/audio
File: engine/platform/audio/sources/miniaudio/CreateMiniaudioAudio.h

Responsibility:
- Factory for the miniaudio backend. The only miniaudio-facing symbol the rest
  of the engine ever sees; all miniaudio types stay inside the backend TU
  (Rule 1), exactly like CreateJoltPhysics.h does for Jolt.

Key items:
- create_miniaudio_audio(): the only symbol; concrete type stays private.

Dependencies:
- Uses: interfaces/IAudio.h, <memory>. NO miniaudio headers here.
- Used by: engine/app (backend wiring), audio-backed tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Never leak miniaudio types/includes through this header (Rule 1).
*/
/*
UPD:
- 10:08:2026 - 01:53:17: Stage 3 audio bring-up — factory per the lead's
                         integration convention (create_*_audio).
- 28:08:2026 - 14:10:00: Оговорка о пустом set_bus_reverb СНЯТА: зона «звук от
                         источника» завела ревер линией задержки. Вместе с ней
                         записан честный предел этого ревера.
*/

#pragma once

#include <memory>

#include "engine/platform/audio/interfaces/IAudio.h"

namespace dfn::platform {

// miniaudio backend (0.11.22, FetchContent-pinned). Implements the IAudio
// contract: buffer loading, one-shots with volume/pitch, loops, take-variation
// playback, bus tree, 3D listener/emitter attenuation, layered music.
// РЕВЕР ШИНЫ И ОККЛЮЗИЯ ГОЛОСА ИСПОЛНЯЮТСЯ (28.08): set_bus_reverb — линией
// задержки (ma_delay_node), set_voice_lowpass — фильтром (ma_lpf_node), оба
// узла заводятся лениво. ЧЕСТНЫЙ ПРЕДЕЛ РЕВЕРА: одна линия обратной связи даёт
// ХВОСТ, а не комнату — на больших помещениях слышна как эхо. Настоящей
// реверберации у miniaudio нет вовсе (docs/reports/audio-engines), и день,
// когда «звучит как подземелье» станет требованием, — день Steam Audio.
// init() failure (no device) returns false; the app falls back to the null
// backend (Rule 3).
[[nodiscard]] std::unique_ptr<IAudio> create_miniaudio_audio();

} // namespace dfn::platform
