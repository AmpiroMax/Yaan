
# Documentation Rules — Permanent

Docs live per-module at `engine/<group>/<module>/docs/README.md`
(e.g. `engine/core/ecs/docs/README.md`, `engine/platform/render/docs/README.md`).

## Before working on any module

1. READ `engine/<group>/<module>/docs/README.md` for the module you are about to modify.
2. If documentation does not exist yet — note this, and create it after your changes.

## After modifying code

1. UPDATE the corresponding `docs/README.md` to reflect the new state.
2. ADD a UPD entry with the current timestamp describing what changed.
3. Documentation must describe CURRENT code state — not plans, not history.

## Documentation format

Each module doc must contain:

1. **Responsibility** — what this module does (1-3 sentences).
2. **Key types** — structs, classes, functions with brief descriptions.
3. **Usage example** — minimal code snippet showing how to use it.
4. **Dependencies** — what this module uses, and who depends on it.

## Specs (`docs/specs/<agent>.md`)

Each zone owner maintains a spec with exactly seven sections (Q35):
**Zone of responsibility / Public interface / Internal design / Dependencies /
Step-by-step plan / How it is verified / What this zone does NOT do.**
The spec is written before implementation and updated when the group sync changes a
contract. Write it so that a newcomer could continue the code from it alone.

## Devlog (`artifacts/devlog/`)

Every group sync produces `artifacts/devlog/YYYY-MM-DD-NN-<slug>.md`: what was decided,
which options were rejected and why, what changed in contracts, screenshots of the
current build where relevant. `artifacts/devlog/INDEX.md` keeps a one-line summary per
entry. Devlog entries are written in Russian (they are the user-facing history);
everything else in the repo is English.

## Enforcement

- AI agents MUST read relevant docs before making changes.
- AI agents MUST update docs in the same changeset as code changes.
- Merges that change engine logic without updating docs are violations.
