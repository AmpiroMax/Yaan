#
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
                "${DFN_SHADER_SRC_DIR}/dfn_ground.sh"
                "${DFN_SHADER_SRC_DIR}/dfn_shadow.sh"
                "${DFN_SHADER_SRC_DIR}/dfn_pointshadow.sh"
                "${DFN_SHADER_SRC_DIR}/dfn_skin.sh"
        COMMENT "shaderc ${name} (${profile})"
        VERBATIM)
    set_property(GLOBAL APPEND PROPERTY DFN_SHADER_OUTPUTS "${out}")
endfunction()

if(APPLE)
    foreach(shader vs_terrain vs_unlit vs_debug vs_upscale vs_sky vs_water vs_shadow
                   vs_foliage vs_shadow_cutout vs_point_shadow vs_imgui
                   vs_skinned vs_shadow_skinned)
        dfn_compile_shader(${shader} vertex metal osx mtl)
    endforeach()
    foreach(shader fs_terrain fs_unlit fs_debug fs_upscale fs_sky fs_water fs_prop
                   fs_shadow fs_foliage fs_shadow_cutout fs_point_shadow fs_path
                   fs_imgui)
        dfn_compile_shader(${shader} fragment metal osx mtl)
    endforeach()
endif()

get_property(DFN_SHADER_OUTPUTS GLOBAL PROPERTY DFN_SHADER_OUTPUTS)
add_custom_target(dfn_shaders DEPENDS ${DFN_SHADER_OUTPUTS})

# --- The backend library -----------------------------------------------------
add_library(dfn_platform_render STATIC
    sources/bgfx/BgfxRenderer.cpp
    sources/bgfx/BgfxRendererFrame.cpp
    sources/bgfx/BgfxRendererSubmit.cpp
    sources/bgfx/BgfxRendererResources.cpp
    sources/bgfx/BgfxCallback.cpp
    sources/bgfx/BgfxPalette.cpp
    sources/bgfx/ImGuiBackend.cpp
    sources/null/NullRenderer.cpp)
target_link_libraries(dfn_platform_render
    # dfn_core: РЕЕСТР ВЕЩЕСТВ (engine/core/materials). Бэкенд разрешает
    # DrawParams::material в шероховатость и металличность сам — числа вещества
    # живут в данных, а не на вызове отрисовки (MATERIALS.md §2.7). По DAG
    # платформа смотрит в core, и это единственное, что она оттуда берёт.
    PUBLIC dfn_headers dfn_core
    # PRIVATE: bgfx never escapes this layer. dfn_imgui is here for the same
    # reason bgfx is — the ImGui bgfx backend is a THIRD-PARTY BRIDGE, and
    # Rule 1 says third-party includes live in platform sources/. bgfx ships no
    # such backend (its own is in the examples), so this one is ours.
    PRIVATE bgfx bimg_encode bx dfn_imgui)
# Third-party headers as SYSTEM so -Wall -Wextra -Wpedantic stays clean for us.
foreach(dep bgfx bimg bx)
    get_target_property(_dfn_dep_inc ${dep} INTERFACE_INCLUDE_DIRECTORIES)
    if(_dfn_dep_inc)
        target_include_directories(dfn_platform_render SYSTEM PRIVATE ${_dfn_dep_inc})
    endif()
endforeach()
target_include_directories(dfn_platform_render PRIVATE "${DFN_SHADER_GEN_DIR}")
add_dependencies(dfn_platform_render dfn_shaders)
