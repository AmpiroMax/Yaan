#
# File: engine/platform/window/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_window target: IWindow backends (glfw + null). Pins GLFW via
#   FetchContent (Rule 24). Owned by the render agent (Rule 25, Q34).
#
# Dependencies:
# - Uses: dfn_headers (root), GLFW 3.4 (pinned tag "3.4").
# - Used by: root CMakeLists (auto-included), engine/app link.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. GLFW types must never leak out of
#   sources/ (Rule 1): backend headers forward-declare GLFWwindow only.

include(FetchContent)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4)                        # Rule 24: exact pinned release tag
FetchContent_MakeAvailable(glfw)

add_library(dfn_platform_window STATIC
    sources/glfw/GlfwWindow.cpp
    sources/null/NullWindow.cpp)
target_link_libraries(dfn_platform_window
    PUBLIC dfn_headers
    PRIVATE glfw)                       # PRIVATE: GLFW never escapes this layer
