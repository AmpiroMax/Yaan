/*
Created: 09:08:2026 - 00:45:08
Last updated: 09:08:2026 - 00:45:08
Module: engine/platform/llm
File: engine/platform/llm/sources/null/NullLlm.cpp

Responsibility:
- Null ILlm backend (Rule 3, Q62): requests complete instantly with their
  scripted fallback_text (Q67); no threads, fully deterministic (Rule 13.2).

Key items:
- NullLlm (file-local) + create_null_llm() factory.

Dependencies:
- Uses: interfaces/ILlm.h, C++ stdlib.
- Used by: engine/app wiring, gameplay tests, the default play mode without LLM.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Semantics here are contract (ILlm.h notes): Done on submit, fallback verbatim,
  from_fallback = true; handles released after try_get_result.
*/
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial null backend implementation.
*/

#include "engine/platform/llm/sources/null/CreateNullLlm.h"

#include <unordered_map>
#include <utility>

namespace dfn::platform {
namespace {

class NullLlm final : public ILlm {
public:
    bool init(const LlmInitParams& params) override {
        (void)params;
        return true; // null loads no model and is always "ready"
    }
    void shutdown() override { results_.clear(); }

    ModelInfo active_model() const override {
        return ModelInfo{}; // name empty, loaded = false: honest "no model"
    }

    void set_inference_allowed(bool allowed) override {
        (void)allowed; // no inference exists; gate is trivially honored (Q64)
    }

    LlmRequestHandle submit(const CompletionRequest& request) override {
        const LlmRequestHandle handle{next_id_++};
        // Contract (Q62/Q67): complete instantly with the scripted words.
        results_[handle.id] = CompletionResult{
            .status = LlmRequestStatus::Done,
            .text = request.fallback_text,
            .from_fallback = true,
        };
        return handle;
    }

    LlmRequestStatus status(LlmRequestHandle request) const override {
        const auto it = results_.find(request.id);
        return it != results_.end() ? it->second.status : LlmRequestStatus::Invalid;
    }

    bool try_get_result(LlmRequestHandle request, CompletionResult& out_result) override {
        const auto it = results_.find(request.id);
        if (it == results_.end()) {
            return false;
        }
        out_result = std::move(it->second);
        results_.erase(it); // contract: handle is stale after retrieval
        return true;
    }

    void cancel(LlmRequestHandle request) override {
        // Already Done by construction; keep the (fallback) result retrievable
        // per the interface contract — nothing to do.
        (void)request;
    }

private:
    uint64_t next_id_ = 1; // 0 is the invalid handle; monotonic, never reused
    std::unordered_map<uint64_t, CompletionResult> results_;
};

} // namespace

std::unique_ptr<ILlm> create_null_llm() {
    return std::make_unique<NullLlm>();
}

} // namespace dfn::platform
