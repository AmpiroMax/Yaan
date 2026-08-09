<!--
Created: 09:08:2026 - 00:18:26
Last updated: 09:08:2026 - 01:02:15
-->
<!--
UPD:
- 09:08:2026 - 00:18:26: Stage-1 state: interface only, no backends yet.
- 09:08:2026 - 01:02:15: Stage 2 — null backend implemented (CreateNullLlm.h);
  llama.cpp backend remains stage 4.
-->

# engine/platform/llm

## Responsibility

The platform LLM inference contract (Rule 0): fully asynchronous text
completion with a mandatory scripted fallback per request (Q67), VRAM-based
model auto-selection (Q63), and the combat/chunk-load inference gate (Q64).
llama.cpp lives only behind `interfaces/ILlm.h`. Inference never blocks the
sim tick.

## Key types

- `ILlm` — `init` (model selection), `submit` / `status` / `try_get_result` /
  `cancel`, `set_inference_allowed` (Q64 gate), `active_model`.
- `CompletionRequest` — prompt + mandatory `fallback_text` + sampling params + seed.
- `CompletionResult` — text + `from_fallback`; `LlmRequestHandle` — opaque, monotonic.

## Usage example

```cpp
dfn::platform::CompletionRequest req{
    .prompt = built_prompt,
    .fallback_text = scripted_line_text, // Q67: intent from script
    .max_tokens = 64, .seed = journal_seed};
auto handle = llm.submit(req);           // never blocks
// later ticks:
dfn::platform::CompletionResult res;
if (llm.try_get_result(handle, res)) { say(res.text); } // fallback or generated
```

## Dependencies

- Uses: stdlib only.
- Used by: `engine/gameplay` (dialogue words; later NPC planning through
  NpcAction), `engine/app` (init + state gating), tests.
- Backends (stage 4): `sources/llama/` (llama.cpp, FetchContent pinned),
  `sources/null/` — THE default runnable mode (Q62): instant scripted fallback,
  deterministic (Rule 13.2); the game is fully playable on it.
