<!--
Created: 09:08:2026 - 00:18:26
Last updated: 27:08:2026 - 21:09:48
-->
<!--
UPD:
- 09:08:2026 - 00:18:26: Stage-1 state: interface only, no backends yet.
- 09:08:2026 - 01:02:15: Stage 2 — null backend implemented
  (CreateNullAudio.h); miniaudio backend remains stage 3.
- 10:08:2026 - 02:27:07: Audio bring-up (landscape stage, в12): miniaudio
  backend implemented (0.11.22, FetchContent GIT_SHALLOW); placeholder sound
  generator in tools/. set_bus_reverb is a DOCUMENTED v1 no-op.
- 27:08:2026 - 21:09:48: ПРАВИЛО ПРИВЯЗКИ ЗВУКА К ХОЗЯИНУ (заказ владельца
  28.08: «звук должен быть привязан к чему-то конкретному»). Ветер пережил
  закрытие карты, потому что его хозяином было приложение, а звучал он про мир.
  Заведена шина world под шиной sfx, правило записано разделом выше.
- 27:08:2026 - 20:21:01: Ogg Vorbis (stb_vorbis from the pinned miniaudio
  checkout), music layers are no longer spatialized, and the FULL-DECODE
  ruling for music is written down with its measured price. Two of the three
  defects named in docs/reports/music-research.html §8 are closed; streaming
  (defect #1) is deliberately still open and now says so out loud.
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

## Formats, and the one that had to be added

WAV, MP3 and FLAC come with miniaudio. **Ogg Vorbis does not** — and Vorbis is
the only format the music pipeline produces (q5–q6; MP3 is ruled out because
its encoder padding puts a gap in every loop). miniaudio *ships* a complete
Vorbis data source, gated on `#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H`; all that
was missing was compiling stb_vorbis. `sources/miniaudio/StbVorbis.cpp` does
that in one TU and carries the provenance and licence (stb_vorbis v1.22, Sean
Barrett, MIT **or** public domain, taken from `extras/` of the miniaudio
checkout we already pin — not copied into `third_party/`, so there is one pin
and one version). `MiniaudioAudio.cpp` includes it with `STB_VORBIS_HEADER_ONLY`
**before** `MINIAUDIO_IMPLEMENTATION`, and an `#error` on `MA_HAS_VORBIS` makes
the wrong order a build failure instead of a game with no music.

## THE RULE: every emitter has an OWNER, and falls silent with it

**Владелец, 28.08:** «зашёл на Вайтран, поиграл, вышел в главное меню —
продолжили играть шумы фоновые/ветер/вода. К чему привязаны эти шумы, если они
сохранились после закрытия карты? Обязательно звук должен быть привязан к
чему-то конкретному».

Он был прав буквально: ветер заводился **один раз при старте приложения** и жил
до конца прогона. Его хозяином было ПРИЛОЖЕНИЕ, а звучал он про МИР — и выход в
меню мир не выгружает (так задумано: «Продолжить» обязано возвращать туда же),
поэтому не существовало события, на котором ветру полагалось бы замолчать.

**Правило, которое наследуют все будущие звуки:**

> У каждого излучателя есть ХОЗЯИН — то, о чём он звучит. Звук замолкает
> вместе с хозяином, и место, где это происходит, ОДНО на всех звуков хозяина.
> Хозяин выражается ШИНОЙ: выбрать шину — единственный способ завести звук,
> поэтому правило нельзя забыть, можно только нарушить вслух.

Дерево шин сегодня (создаётся в `App::init`):

```
master
├── sfx    (ползунок «громкость эффектов»)
│   └── world   ← ХОЗЯИН: МИР. Шаги, прыжки, приземления, всплески, ветер.
│                 Приглушается до нуля, пока мир не идёт (меню И пауза),
│                 пандусом 0.35 с в обе стороны.
├── music  (ползунок «громкость музыки»)  ← ХОЗЯИН: МЕНЮ. Заглавная тема,
│                 росчерк заставки. Тема — функция экрана, росчерк — один
│                 выстрел со своей длиной.
└── voice  (ползунок «громкость речи») — заведена вперёд голосов
```

`world` — РЕБЁНОК `sfx`, а не его брат, и это существенно: ползунок игрока и
приглушение по хозяину — два РАЗНЫХ множителя. Сложи их в одну ручку, и каждый
выход в меню стирал бы то, что игрок выбрал.

**Новый звук отвечает на два вопроса, прежде чем зазвучать:** чей он (какая
шина) и что с ним делает исчезновение хозяина. У звука без хозяина ответа нет —
и это тот самый звук, который потом играет в главном меню.

## Music is decoded whole, and this is the known ruling

`load_sound()` decodes everything up front (`MA_SOUND_FLAG_DECODE`), music
included. **Measured on the title theme** (main_theme_loop.ogg, 96.0 s stereo,
1.4 MB on disk): 102 ms to load, 32.3 MB resident as f32 at the device's
44.1 kHz. That is the accepted price for one track (owner's ruling, relayed
through the music session), and it buys a music path that has no second version
of itself today.

**It does not scale, and the number where it stops is known**: four four-minute
adaptive LAYERS would be ~370 MB. Streaming (`MA_SOUND_FLAG_STREAM |
NO_SPATIALIZATION | NO_PITCH`) is defect #1 of
`docs/reports/music-research.html` §8; it needs a separate load path, which is
an `IAudio` change, which is a group sync (Rule 26). It waits for the stage
that actually needs layers.

Two other notes from the same pass:

- `play_music()` **disables spatialization** on every layer. miniaudio
  spatializes by default, so before this the music was an emitter at the world
  origin — panned and attenuated by where the player stood. Nobody had heard it
  because nothing had ever called `play_music`.
- Loop points are not needed for the menu theme: the loop cut is trimmed to the
  musical length with the reverb tail wrapped onto the head, so the loop point
  IS the end of the file and plain `looping = true` is gapless.

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
