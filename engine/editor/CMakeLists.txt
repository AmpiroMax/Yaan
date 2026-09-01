#
# File: engine/editor/CMakeLists.txt
#
# Responsibility:
# - dfn_editor target: the in-game editor's INTERFACE layer. This is the one
#   place in the repo where Dear ImGui may be included — docs/ARCHITECTURE.md
#   names it by hand ("editor may use Dear ImGui directly (documented
#   exception; nothing else may)") and tools/dag_check.py enforces exactly that.
#
# Key items:
# - EditorUi: the ImGui context, the Cyrillic font, input routing from OUR
#   platform/input, the shared style, and the panel contract other agents write
#   their tools against.
#
# Dependencies:
# - Uses: dfn_headers, dfn_imgui (root), dfn_platform_render (the bgfx ImGui
#   backend lives there — Rule 1 keeps third-party BRIDGES in platform sources).
# - Used by: engine/app (dfn_app links it; the DAG runs app -> editor and never
#   the reverse, which is why the localization table arrives as a function
#   pointer rather than as an include).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Panels belong HERE, not in engine/app:
#   a panel calls ImGui directly, and engine/app is not allowed to.

add_library(dfn_editor STATIC
    sources/EditorBrush.cpp
    sources/EditorToolbar.cpp
    sources/EditorHistory.cpp
    sources/EditorToolbox.cpp
    sources/EditorToolIcons.cpp
    sources/EditorToolsGround.cpp
    sources/EditorToolsObject.cpp
    sources/EditorToolPath.cpp
    sources/EditorToolHouse.cpp
    sources/EditorToolHouseUi.cpp
    sources/EditorBrushView.cpp
    sources/EditorUi.cpp
    sources/EditorPalette.cpp
    sources/EditorPaletteAxes.cpp
    sources/EditorPaletteFamily.cpp
    sources/EditorPaletteState.cpp
    sources/EditorPaletteThumb.cpp
    sources/EditorPaletteThumbCache.cpp
    sources/EditorPaletteView.cpp
    sources/EditorPropsView.cpp
    sources/EditorMorphView.cpp)
target_link_libraries(dfn_editor
    # dfn_world is PUBLIC because EditorBrush.h names its types in its own
    # interface (ReliefLayer, ScenePad): anyone who includes the brush needs
    # them too, and a PRIVATE link would compile here and fail at the caller.
    #
    # dfn_render is PUBLIC for the same reason and for one more: EditorPaletteThumb.h
    # names render::MeshData / RegistryObject / ProcTextureKind in ITS interface,
    # and engine/app builds the bake hook against exactly those types.
    PUBLIC dfn_headers dfn_world dfn_render
    PRIVATE dfn_imgui dfn_platform_render)
