/*
Created: 27:08:2026 - 20:00:24
Last updated: 27:08:2026 - 20:00:24
Module: engine/platform/audio
File: engine/platform/audio/sources/miniaudio/StbVorbis.cpp

Responsibility:
- The Ogg Vorbis DECODER, and nothing else: this translation unit exists only
  to compile stb_vorbis once. miniaudio decodes WAV, MP3 and FLAC on its own
  and has NO Vorbis decoder built in; it does, however, carry a complete
  ma_stbvorbis data source behind `#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H`.
  Defining that symbol (by including stb_vorbis before the miniaudio
  implementation, which MiniaudioAudio.cpp does) turns .ogg into a format
  ma_sound_init_from_file simply opens, with no vtable plumbing at the call
  sites and no second code path for music.

WHERE THE CODE COMES FROM, AND UNDER WHAT LICENCE:
- stb_vorbis.c v1.22 by Sean Barrett (nothings.org/stb_vorbis), taken from the
  PINNED miniaudio checkout: `${miniaudio_SOURCE_DIR}/extras/stb_vorbis.c`,
  miniaudio 0.11.22 (see the module CMakeLists for the tag).
- LICENCE: dual, at the user's choice — MIT, or public domain (Unlicense).
  Both are stated at the end of the file itself. Either permits shipping it in
  a commercial binary without attribution; we attribute here anyway, because a
  vendored file whose provenance is not written down is a file nobody can
  re-license, update or audit later.
- IT IS NOT COPIED INTO third_party/. It ships INSIDE the dependency we already
  pin, so taking it from there gives it one pin, one version and one
  provenance; a second copy in third_party/ would be a snapshot that silently
  disagrees with miniaudio's own expectations the first time either moves
  (Rule 24: everything through pinned FetchContent).

Key items:
- Nothing exported by us. The TU compiles stb_vorbis's implementation half;
  MiniaudioAudio.cpp includes the same file with STB_VORBIS_HEADER_ONLY and
  links against what is defined here.

Dependencies:
- Uses: stb_vorbis.c from the pinned miniaudio checkout. Nothing of ours.
- Used by: sources/miniaudio/MiniaudioAudio.cpp (link-time).

Notes:
- WHY A SEPARATE TU AND NOT ONE INCLUDE IN THE BACKEND. stb_vorbis is 5.6k
  lines of C. Folded into MiniaudioAudio.cpp it would be recompiled on every
  edit of our 500-line backend and its macros would sit in the same namespace
  as ours. Split, the backend keeps its own compile time and sees only
  declarations.
- It compiles as C++ (checked: one -Wtautological-compare warning on a pointer
  overflow test, which is stb's, not ours), so the module needs no C language
  enabled — the root project is `project(daggerfall_n CXX)` and this file must
  not be the reason that changes.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do NOT edit stb_vorbis itself, and do not copy it into the tree. If a fix is
  needed, move the miniaudio pin and say so in the module CMakeLists.
*/
/*
UPD:
- 27:08:2026 - 20:00:24: Created — Ogg Vorbis decoding for the main theme
  (owner's order, relayed through the music session: the title theme loops in the main menu). Vorbis was
  defect #2 of the three named in docs/reports/music-research.html §8.
*/

// Warnings suppressed for THIS FILE ONLY, and named one by one rather than
// swept with -w: stb_vorbis is third-party code we do not edit, but the next
// person must still be able to see what it warns about.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtautological-compare"
#endif

#include "stb_vorbis.c" // NOLINT — a .c include IS the documented stb usage

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
