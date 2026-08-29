/*
Module: engine/gameplay
File: engine/gameplay/sources/Dialogue.h

Responsibility:
- The dialogue data model (Q71/Q74/Q79/Q80): a line = stable id + conditions +
  segments; segments carry text plus emotion/tone/volume/speed markup and an
  optional paralinguistic tag; the per-segment content hash that names audio
  files.

Key items:
- DialogueSegment: text + tone + volume + speed + optional paralinguistic tag.
- DialogueLine: ConditionAtom conditions (Condition.h, shared with quests)
  gate availability; lines do NOT know who speaks them (Q71 — hard requirement).
- segment_content_hash(): hash(text + markup + voice) -> names the audio file.

Dependencies:
- Uses: engine/gameplay Ids.h, C++ stdlib. Implementation (stage 2) uses
  dfn::serialization::Fnv1a64 (engine/core/serialization/sources/ContentHash.h,
  agreed with core, stage-1 sync).
- Used by: dialogue system, NpcAction executor (Say), tools/voice_gen (reads
  the same model from content files), quests.

Notes:
- Lines live in content files under games/daggerfall_n/assets/ (Rule 5); this
  header is only the in-memory shape. `text` is the localized text of the
  ACTIVE language; markup (tone/volume/speed/tag) is authored once per line and
  shared across all languages, keyed by line id + segment index (Q74).
- Speaker-agnostic (Q71): nothing here references an NPC. The speaker's
  VoiceTimbre component (NpcComponents.h, Q80) supplies the VoiceId at playback
  and hashing time.
- Content hash (Q79): per SEGMENT, over text + tone + volume + speed + tag +
  voice id, via the frozen core FNV-1a 64 accumulator with length-prefixed
  fields in that documented order. volume/speed are hashed as their authored
  decimal strings from the content file (never float bits — platform-stable).
  The audio file is named by the 16-lowercase-hex digest + ".opus"; only
  changed segments are re-synthesized; playback crossfades segment joins
  (Q79 seam note). The hash algorithm and field order are FROZEN once audio
  ships — changing either orphans every file on disk.
- Tone and tag vocabularies are content conventions validated by
  tools/voice_gen, not C++ enums — extending them must not require
  recompilation (Rule 6).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER add speaker/time-of-speech fields to DialogueLine (Q71).
- Never change segment_content_hash() inputs, order, or algorithm after
  audio ships.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/gameplay/sources/Condition.h"
#include "engine/gameplay/sources/Ids.h"

namespace dfn::gameplay {

// One voiced span of a line (Q71). Markup is one vocabulary across languages
// (Q74) and drives the expressive TTS (Q69).
struct DialogueSegment {
    std::string text;    // localized text, active language
    std::string tone;    // emotion/tone token ("neutral", "angry", ...); content vocabulary
    float volume = 1.0f; // relative, 1 = neutral
    float speed = 1.0f;  // relative, 1 = neutral
    std::string tag;     // optional paralinguistic token ("sigh", "laugh"); empty = none
};

// A line: stable id + conditions + segments. A line does NOT know who speaks
// it or when (Q71) — the speaker brings the voice (Q80). Conditions use the
// closed atom vocabulary shared with the quest system (Condition.h,
// QUEST_FORMAT.md §2.1); a flat vector is a conjunction (AND).
struct DialogueLine {
    DialogueLineId id{};
    std::vector<ConditionAtom> conditions;
    std::vector<DialogueSegment> segments;
};

// Names the audio file for one segment as spoken by one voice (Q79/Q80):
// FNV-1a 64 over (text, tone, volume, speed, tag, voice) — length-prefixed, in
// that order. File name = 16 lowercase hex digits + ".opus". FROZEN once audio
// ships. volume/speed enter as authored decimal strings; the loader keeps them
// (implementation detail, stage 2).
[[nodiscard]] uint64_t segment_content_hash(const DialogueSegment& segment, VoiceId voice);

} // namespace dfn::gameplay
