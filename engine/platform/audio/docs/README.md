<!--
Created: 09:08:2026 - 00:18:26
Last updated: 10:08:2026 - 02:27:07
-->
<!--
UPD:
- 09:08:2026 - 00:18:26: Stage-1 state: interface only, no backends yet.
- 09:08:2026 - 01:02:15: Stage 2 — null backend implemented
  (CreateNullAudio.h); miniaudio backend remains stage 3.
- 10:08:2026 - 02:27:07: Audio bring-up (landscape stage, в12): miniaudio
  backend implemented (0.11.22, FetchContent GIT_SHALLOW); placeholder sound
  generator in tools/. set_bus_reverb is a DOCUMENTED v1 no-op.
-->

# engine/platform/audio

## Responsibility

The platform audio contract (Rule 0): sound load/playback, 3D spatialization,
bus tree with reverb (Q68 room-volume priority), footstep variation sets (Q68
surface-material priority), layered adaptive music. miniaudio lives only behind
`interfaces/IAudio.h`.

## Key types

- `IAudio` — init/shutdown/`update(listener)`, load/unload, `play`,
  `play_variation` (footsteps), voice control, buses + `set_bus_reverb`,
  `play_music`/`set_music_layer`/`stop_music`.
- `PlayParams`, `Spatial3d`, `ReverbParams`, `ListenerPose` — plain-data params.
- `SoundHandle`, `AudioVoiceHandle`, `BusHandle`, `MusicHandle` — opaque POD handles.

## Usage example

```cpp
auto step_takes = /* SoundHandles for the surface material */;
dfn::platform::PlayParams p{.bus = sfx_bus, .spatial = true,
                            .spatial_params = {feet_pos, min_d, max_d}};
audio.play_variation(step_takes, p);          // varied footstep (Q68)
audio.set_bus_reverb(sfx_bus, reverb_params); // computed from room volume (Q68)
audio.update(listener_pose);                  // once per frame
```

## Backends (current state)

- `sources/miniaudio/` — the real backend (miniaudio 0.11.22, FetchContent
  pinned + GIT_SHALLOW; the single header's implementation macro lives in its
  one TU). Implements the full contract EXCEPT `set_bus_reverb`, which is a
  documented no-op: miniaudio ships no reverb node, the room-reverb DSP node
  arrives with the dungeon-audio stage. `init()` returns false with no device;
  the app then falls back to null (Rule 3).
- `sources/null/` — runnable mode: silent success, inert handles,
  `is_playing()` false.

## Placeholder sounds

`tools/gen_placeholder_sounds.py` (stdlib-only, deterministic seed) synthesizes
the current sound set into `games/daggerfall_n/assets/audio/`: 4 footstep takes
for each of the 5 surface classes (grass/gravel/rock/sand/water), jump, soft
and hard landings, the water-entry splash, and a seamless 8 s wind loop.
**These are placeholders**: the point is that surfaces DIFFER convincingly and
the step event has a same-tick voice. Real recorded sounds are a later asset
pass — replacing the files replaces the sound, no code changes.

## Dependencies

- Uses: stdlib + glm only in the interface (Rule 1); miniaudio inside the
  backend TU only.
- Used by: `engine/gameplay` (StepAudio: footsteps, jump/land/splash, wind
  loop; later dialogue voice), `engine/app` (buses, music), tests (sim_audio).
