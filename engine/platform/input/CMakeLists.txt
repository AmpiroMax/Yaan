#
# File: engine/platform/input/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_input target: IInput backends (glfw + null). Owned by the
#   render agent (Rule 25, Q34).
#
# Dependencies:
# - Uses: dfn_headers, glfw (declared/pinned in engine/platform/window — same
#   zone, single pin point; window layer is added before input by the root).
# - Used by: root CMakeLists (auto-included), engine/app link.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. GLFW types never leak out of sources/.

add_library(dfn_platform_input STATIC
    sources/glfw/GlfwInput.cpp
    sources/null/NullInput.cpp)
target_link_libraries(dfn_platform_input
    PUBLIC dfn_headers
    PRIVATE glfw dfn_platform_window)   # window: GlfwWindow.h for the factory cast
