#
# Created: 09:08:2026 - 00:45:00
# Last updated: 09:08:2026 - 14:11:37
# File: engine/platform/render/CMakeLists.txt
#
# Responsibility:
# - dfn_platform_render target: IRenderer backends (bgfx + null). Pins bgfx via
#   FetchContent (Rule 24) and runs shaderc as a build step (Q50), embedding the
#   compiled shaders as generated headers (--bin2c).
#
# Key items:
# - bgfx.cmake pin: v1.153.9398-566 (exact release tag, verified by build).
# - dfn_compile_shader(): shaderc custom command -> <build>/shaders_gen/*.h.
#
# Dependencies:
# - Uses: dfn_headers, bgfx/bimg/bx (bkaradzic/bgfx.cmake), shaderc tool.
# - Used by: root CMakeLists (auto-included), engine/app link.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. bgfx types never leak out of sources/.
# - macOS/Metal shaders only this stage; Windows profiles arrive with the
#   Windows bring-up sync.
#
# UPD:
# - 09:08:2026 - 00:45:00: Stage 2 — bgfx backend, shaderc step, null backend.
# - 09:08:2026 - 01:10:00: bgfx/bimg/bx include dirs marked SYSTEM (zero-warning
#   gate: third-party headers must not trip -Wpedantic in our TUs).
# - 09:08:2026 - 10:57:00: Stage 3 — sky/water shader pairs, dfn_env.sh shared
#   include (shader dir added to shaderc -i), BgfxPalette.cpp.
# - 09:08:2026 - 11:57:20: Stage 3b — fs_prop fragment shader ("prop" program
#   for placeholder scatter/site meshes; shares vs_terrain).
# - 09:08:2026 - 14:11:37: Dynamic sun shadows — vs_shadow/fs_shadow pair,
#   dfn_shadow.sh shared include added to the shaderc dependency list.

include(FetchContent)

set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)      # shaderc
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG v1.153.9398-566)            # Rule 24: exact pinned release tag
FetchContent_MakeAvailable(bgfx_cmake)

# --- Shader compilation (Q50): shaderc -> embedded headers -------------------
set(DFN_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sources/bgfx/shaders")
set(DFN_SHADER_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_gen")
file(MAKE_DIRECTORY "${DFN_SHADER_GEN_DIR}")

# dfn_compile_shader(<name> <vertex|fragment> <profile> <platform>)
# Emits ${DFN_SHADER_GEN_DIR}/<name>_<suffix>.h with array <name>_<suffix>.
function(dfn_compile_shader name type profile platform suffix)
    set(out "${DFN_SHADER_GEN_DIR}/${name}_${suffix}.h")
    add_custom_command(
        OUTPUT "${out}"
        COMMAND $<TARGET_FILE:shaderc>
                -f "${DFN_SHADER_SRC_DIR}/${name}.sc"
                -o "${out}"
                --bin2c "${name}_${suffix}"
                --type ${type}
                --platform ${platform}
                -p ${profile}
                -i "${bgfx_cmake_SOURCE_DIR}/bgfx/src"
                -i "${DFN_SHADER_SRC_DIR}"
                --varyingdef "${DFN_SHADER_SRC_DIR}/varying.def.sc"
        DEPENDS shaderc
                "${DFN_SHADER_SRC_DIR}/${name}.sc"
                "${DFN_SHADER_SRC_DIR}/varying.def.sc"
                "${DFN_SHADER_SRC_DIR}/dfn_env.sh"
                "${DFN_SHADER_SRC_DIR}/dfn_shadow.sh"
        COMMENT "shaderc ${name} (${profile})"
        VERBATIM)
    set_property(GLOBAL APPEND PROPERTY DFN_SHADER_OUTPUTS "${out}")
endfunction()

if(APPLE)
    foreach(shader vs_terrain vs_unlit vs_debug vs_upscale vs_sky vs_water vs_shadow)
        dfn_compile_shader(${shader} vertex metal osx mtl)
    endforeach()
    foreach(shader fs_terrain fs_unlit fs_debug fs_upscale fs_sky fs_water fs_prop
                   fs_shadow)
        dfn_compile_shader(${shader} fragment metal osx mtl)
    endforeach()
endif()

get_property(DFN_SHADER_OUTPUTS GLOBAL PROPERTY DFN_SHADER_OUTPUTS)
add_custom_target(dfn_shaders DEPENDS ${DFN_SHADER_OUTPUTS})

# --- The backend library -----------------------------------------------------
add_library(dfn_platform_render STATIC
    sources/bgfx/BgfxRenderer.cpp
    sources/bgfx/BgfxCallback.cpp
    sources/bgfx/BgfxPalette.cpp
    sources/null/NullRenderer.cpp)
target_link_libraries(dfn_platform_render
    PUBLIC dfn_headers
    PRIVATE bgfx bimg_encode bx)        # PRIVATE: bgfx never escapes this layer
# Third-party headers as SYSTEM so -Wall -Wextra -Wpedantic stays clean for us.
foreach(dep bgfx bimg bx)
    get_target_property(_dfn_dep_inc ${dep} INTERFACE_INCLUDE_DIRECTORIES)
    if(_dfn_dep_inc)
        target_include_directories(dfn_platform_render SYSTEM PRIVATE ${_dfn_dep_inc})
    endif()
endforeach()
target_include_directories(dfn_platform_render PRIVATE "${DFN_SHADER_GEN_DIR}")
add_dependencies(dfn_platform_render dfn_shaders)
