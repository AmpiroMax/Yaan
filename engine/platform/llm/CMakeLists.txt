#
# Created: 09:08:2026 - 00:45:08
# Last updated: 09:08:2026 - 00:45:08
# File: engine/platform/llm/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_llm static library: ILlm backends. Stage 2 ships the null
#   backend only (Q62: the default runnable mode); the llama.cpp backend
#   (FetchContent, pinned) is stage 4 and will be added HERE. Zone-owned by
#   sim (Rule 25 / Q34).
#
# Dependencies:
# - Uses: dfn_headers.
# - Used by: engine/app wiring, tests.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Do not FetchContent llama.cpp before
#   the stage-4 scope says so.
#
# UPD:
# - 09:08:2026 - 00:45:08: Stage 2 — null backend only.

add_library(dfn_platform_llm STATIC
    sources/null/NullLlm.cpp)

target_link_libraries(dfn_platform_llm PUBLIC dfn_headers)
