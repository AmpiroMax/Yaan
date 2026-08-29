
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
  one TU). Implements the **full** contract: since 28.08 `set_bus_reverb` is a
  feedback delay line (`ma_delay_node`) instead of a no-op, and
  `set_voice_lowpass` is an `ma_lpf_node` inserted per voice. Both nodes are
  **lazy** — nothing enters the graph until something asks for it, so a run
  with no occlusion and no reverb pushes the same samples through the same
  graph as before the wave (that is the control arm, Rule 30). `init()` returns
  false with no device; the app then falls back to null (Rule 3).
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

## И ХОЗЯИН ОБЯЗАН БЫТЬ ТОЧКОЙ В МИРЕ (28.08)

**Владелец, 28.08:** «не должно быть просто так фонового шума — а он даже в
домах есть; у звука всегда должен быть источник, и звук должен распространяться
по физике. Фоновый шум сейчас привязать к деревьям, чтобы затихал с удалением
от деревьев».

Шина ответила на вопрос «чей звук», но не на вопрос «ОТКУДА он». Ветер имел
хозяина (мир) и всё равно был неправ: один непространственный голос, одинаково
громкий в поле, в роще и в запертой комнате. Правило продолжено:

> У излучателя есть не только хозяин, но и МЕСТО. Звук, у которого нельзя
> назвать точку в мире, не заводится. Громкость — функция расстояния до этой
> точки, и в мире, где источника нет, звука нет тоже.

Сегодняшние точки — `engine/gameplay/sources/WorldAmbience.h`:

| источник | точка | сила |
|---|---|---|
| шелест листвы | КРОНА дерева; рощи собраны по ячейке 24 м, роща звучит своей БЛИЖАЙШЕЙ кроной | размер крон (sqrt Σr², сложение по мощности) × ветер (`RenderEnvironment::wind_strength` — та же модель, что гнёт листву) |
| вода | БЛИЖАЙШАЯ ТОЧКА РУСЛА (`[river]` сцены) | ширина русла; ветру безразлична |

Бюджет: 6 мест листвы + 2 воды, до 16 живых голосов с перекличками. Затухание
СЧИТАЕТ ДВИЖОК, а не miniaudio (спатиализация оставлена ради панорамы):
кривая — 1/r с полкой у кроны и окном до ровного нуля на пределе слышимости,
и она измеряется тестом `sim_world_ambience`, а не ухом.

## Окклюзия: две половины одного ответа

Луч Jolt от уха к источнику (`IPhysics::raycast`, слой статики) — ДВА луча,
разнесённые на 0.9 м: стена ловит оба, ствол дерева один, и лес не щёлкает.
Что делает перекрытие: −10.5 дБ громкости И срез верха на 800 Гц
(`set_voice_lowpass`). Обе половины нужны: стена, которая только убавляет
громкость, звучит как расстояние, а не как стена.

В ЛОКАЦИИ луч не пускается вовсе, и это названо вслух: карман интерьера стоит
на километр ниже мира, между ним и деревьями города нет никакой геометрии, и
честный луч сделал бы улицу в доме ГРОМЧЕ, чем на улице. Внутри работает
модель: −22 дБ и срез 420 Гц вглубь дома, −9 дБ и 1.4 кГц у открытой двери.

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

## Реверб шины — линия задержки, и её предел назван

`set_bus_reverb` простоял пустым с 10.08 (интерфейс обещал то, чего бэкенд не
делал). Теперь это `ma_delay_node` между шиной и её родителем: задержка —
время пробега комнаты (`room_size / 343 м/с`, зажата в 20…200 мс), обратная
связь — та, при которой хвост падает на 60 дБ ровно за `decay_seconds`.

**Честный предел:** одна линия обратной связи даёт ХВОСТ, а не комнату; на
больших помещениях слышна как эхо. Настоящей реверберации у miniaudio нет
вовсе (`docs/reports/audio-engines/index.html`), и день, когда «звучит как
подземелье» станет требованием, — день Steam Audio. `wet = 0` УБИРАЕТ УЗЕЛ ИЗ
ГРАФА, а не ставит его на ноль: только так «ревербератора нет» и «ревербератор
выключен» дают один и тот же буфер.

## Placeholder sounds

`tools/gen_placeholder_sounds.py` (stdlib-only, deterministic seed) synthesizes
the current sound set into `games/daggerfall_n/assets/audio/`: 4 footstep takes
for each of the 5 surface classes (grass/gravel/rock/sand/water), jump, soft
and hard landings and the water-entry splash. **Ветровой петли там больше нет**
(28.08): у неё не было источника, и вместе с кодом ушёл файл.

Звуки МИРА — не заглушки и живут отдельно: `assets/audio/world/*.ogg`
(48 кГц моно, 12 с, петли проверены измерением), генератор
`tools/gen_world_ambience.py`, числа уровней и прослушивание —
`docs/reports/world-ambience.html`. Три ступени ветра на породу растут ~3 дБ
НАМЕРЕННО, и код их не выравнивает.
**These are placeholders**: the point is that surfaces DIFFER convincingly and
the step event has a same-tick voice. Real recorded sounds are a later asset
pass — replacing the files replaces the sound, no code changes.

## Dependencies

- Uses: stdlib + glm only in the interface (Rule 1); miniaudio inside the
  backend TU only.
- Used by: `engine/gameplay` (StepAudio: footsteps, jump/land/splash;
  WorldAmbience: шелест крон и вода; later dialogue voice), `engine/app`
  (buses, music, окклюзионные лучи), tests (sim_audio, sim_world_ambience).
