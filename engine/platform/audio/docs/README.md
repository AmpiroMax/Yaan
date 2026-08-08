<!--
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 00:18:26
-->
<!--
UPD:
- 09:08:2026 - 00:18:26: Stage-1 state: interface only, no backends yet.
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

## Dependencies

- Uses: stdlib + glm only (Rule 1).
- Used by: `engine/gameplay` (footsteps, dialogue voice, interactions),
  `engine/app` (buses, music), tests.
- Backends (stage 2/3): `sources/miniaudio/` (FetchContent pinned),
  `sources/null/` — runnable mode: silent success, inert handles.
