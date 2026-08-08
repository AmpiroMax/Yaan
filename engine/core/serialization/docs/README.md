<!--
Created: 09:08:2026 - 00:16:55
Last updated: 09:08:2026 - 00:16:55
-->
<!--
UPD:
- 09:08:2026 - 00:16:55: Stage 1 — public contract documented (headers only, no implementation yet).
-->

# engine/core/serialization

## Responsibility

Binary IO per Rule 7 (Q49): section-based container (magic + version + tagged
length-prefixed sections), explicit little-endian, skip-unknown; plus the frozen
64-bit content hash used by the voice pipeline (Q79) and asset name hashing.

## Key types

- `SectionTag` / `make_tag` (`sources/BinaryWriter.h`) — FourCC section ids.
- `BinaryWriter` (`sources/BinaryWriter.h`) — `begin_file` / `begin_section` /
  `end_section` + explicit-LE primitives; atomic `save_to_file`.
- `BinaryReader`, `SectionInfo` (`sources/BinaryReader.h`) — magic validation,
  `next_section()` iteration (skips unknown/unread payloads), bounds-checked
  reads with latched `ok()`.
- `fnv1a64`, `Fnv1a64` (`sources/ContentHash.h`) — FROZEN FNV-1a 64 hash;
  streaming accumulator with `update_length_prefixed` for multi-field identities.

## Usage example

```cpp
dfn::serialization::BinaryWriter w;
w.begin_file(WORLD_MAGIC, 1);
w.begin_section(dfn::serialization::make_tag('I','N','F','O'), 1);
w.write_u64(seed);
w.end_section();
bool ok = w.save_to_file(path);
```

## Dependencies

Uses std only. Used by engine/world (world file, save delta), gameplay save
sections, tools/voice_gen (content hashes), engine/render (asset name hashes,
agreed Rule 26). JSON/TOML content parsing is a separate stage-2+ concern and
is NOT part of this contract yet.
