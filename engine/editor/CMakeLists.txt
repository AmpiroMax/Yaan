#
# Created: 17:08:2026 - 19:22:32
# Last updated: 17:08:2026 - 19:25:46
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
#
# UPD:
# - 17:08:2026 - 19:22:32: Создан — слой интерфейса редактора; EditorUi переехал сюда
# - 17:08:2026 - 19:25:46: меню объектов: модель (EditorPalette, EditorPaletteState) и панель
#   (EditorPaletteView). Модель не включает ни ImGui, ни render — при нужде
#   вынимается отдельной целью без интерфейса.
#   из engine/app, потому что dag_check (правильно) не пускает imgui.h в app, а
#   архитектура давно назвала engine/editor тем самым местом, где он разрешён.

add_library(dfn_editor STATIC
    sources/EditorUi.cpp
    sources/EditorPalette.cpp
    sources/EditorPaletteState.cpp
    sources/EditorPaletteView.cpp)
target_link_libraries(dfn_editor
    PUBLIC dfn_headers
    PRIVATE dfn_imgui dfn_platform_render)
