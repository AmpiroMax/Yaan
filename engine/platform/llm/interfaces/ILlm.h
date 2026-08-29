/*
Module: engine/platform/llm
File: engine/platform/llm/interfaces/ILlm.h

Responsibility:
- The platform LLM inference contract (Rule 0). Fully asynchronous text
  completion; llama.cpp lives only behind it.

Key items:
- ILlm: init (model auto-selection by VRAM), submit/status/try_get_result/cancel,
  set_inference_allowed gate (Q64).
- CompletionRequest: prompt + MANDATORY scripted fallback_text (Q67).
- CompletionResult / LlmRequestHandle / ModelInfo: plain-data results.

Dependencies:
- Uses: C++ stdlib only.
- Used by: engine/gameplay (NPC dialogue text; later NPC planning via NpcAction),
  engine/app (init/gating), tests (null backend).

Notes:
- ASYNC BY CONTRACT: no method on this interface may block on inference.
  Inference runs on a backend-owned worker thread; the sim tick submits and
  polls. There is deliberately no callback API — results are consumed by
  polling from the fixed tick, which keeps the consumption point deterministic
  and thread-trivial for callers (Rule 12, Rule 13.3).
- Q64 gate: set_inference_allowed(false) is called by the app on entering combat
  or chunk-load states. While disallowed, queued requests stay queued and no new
  inference starts; an in-flight generation may finish. Submitting stays legal.
- Q67 contract: intent comes from the script, words from the LLM. Every request
  therefore CARRIES its scripted fallback_text. The null backend completes with
  the fallback instantly; the llama backend falls back on error, cancellation,
  or model-load failure. A caller can always use the result verbatim.
- Model auto-selection (Q63): init() inspects available VRAM against
  LLM_VRAM_BUDGET (NUMBERS.md) and picks the primary (<= LLM_MAX_PARAMS,
  quantized) or the LLM_FALLBACK_MODEL; ModelInfo reports the choice. init()
  returning false is not fatal to the game — callers must treat it exactly like
  the null backend (fallback text everywhere).
- seed: with a fixed seed and temperature the llama backend samples
  reproducibly; still, live-LLM runs are knowingly non-deterministic at the
  simulation level (Rule 13.3) and will be journaled for replay later.
- Null backend (Rule 3 — THE default runnable mode, Q62): submit() succeeds,
  status() is Done immediately, try_get_result() returns fallback_text with
  from_fallback = true. Instant, allocation-cheap, deterministic (Rule 13.2).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Do not add llama.cpp types, includes, or assumptions to this header.
- Never call inference synchronously from the sim tick — the game must be
  fully playable with this entire subsystem nulled.
- Contract frozen for stage 1 (Rule 26); changes only via group sync.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::platform {

// Opaque request handle. id == 0 means "invalid / none". Handles are unique per
// run and never reused (monotonic), so a stale poll is safe and returns Failed.
struct LlmRequestHandle {
    uint64_t id = 0;
    [[nodiscard]] bool valid() const { return id != 0; }
};

enum class LlmRequestStatus : uint8_t {
    Invalid,   // unknown/stale handle
    Queued,    // submitted, inference not started (or gated by Q64)
    Running,   // inference in progress
    Done,      // result available via try_get_result()
    Failed,    // backend error — result available, carries fallback_text
    Cancelled, // cancel() called — result available, carries fallback_text
};

struct LlmInitParams {
    std::string model_directory;   // where .gguf models live (app config)
    uint64_t vram_budget_bytes = 0;// LLM_VRAM_BUDGET from generated constants
    uint32_t context_length = 0;   // tokens; 0 = backend default
};

struct ModelInfo {
    std::string name;              // model file name, empty if none loaded
    uint64_t parameter_count = 0;  // approximate, 0 if none loaded
    bool loaded = false;
};

struct CompletionRequest {
    std::string prompt;
    // Q67: MANDATORY scripted words for this intent. Used verbatim by the null
    // backend and by any failure path. An empty fallback_text is a caller bug.
    std::string fallback_text;
    uint32_t max_tokens = 0;       // 0 = backend default
    float temperature = 1.0f;
    uint64_t seed = 0;             // 0 = backend-chosen; fixed seed = reproducible sampling
    std::vector<std::string> stop_sequences;
};

struct CompletionResult {
    LlmRequestStatus status = LlmRequestStatus::Invalid;
    std::string text;              // generated words, or fallback_text
    bool from_fallback = false;    // true when text is the scripted fallback
};

class ILlm {
public:
    virtual ~ILlm() = default;

    // Lifecycle ----------------------------------------------------------------
    // Selects and loads a model by available VRAM (Q63). Non-blocking beyond
    // model load at startup; returns false when no model fits — the game runs on
    // fallbacks exactly as with the null backend.
    [[nodiscard]] virtual bool init(const LlmInitParams& params) = 0;
    virtual void shutdown() = 0;

    [[nodiscard]] virtual ModelInfo active_model() const = 0;

    // Gating (Q64) -------------------------------------------------------------
    // false: no NEW inference starts (combat, chunk load). Queue is preserved.
    virtual void set_inference_allowed(bool allowed) = 0;

    // Async completion ---------------------------------------------------------
    // Enqueues a request; never blocks. Thread-safe with the worker; called from
    // the sim tick.
    [[nodiscard]] virtual LlmRequestHandle submit(const CompletionRequest& request) = 0;

    [[nodiscard]] virtual LlmRequestStatus status(LlmRequestHandle request) const = 0;

    // Non-blocking. Returns true and fills out_result once status is
    // Done/Failed/Cancelled; the request is then released and its handle stale.
    [[nodiscard]] virtual bool try_get_result(LlmRequestHandle request,
                                              CompletionResult& out_result) = 0;

    // Cancels a queued/running request; the (fallback) result stays retrievable.
    virtual void cancel(LlmRequestHandle request) = 0;
};

} // namespace dfn::platform
