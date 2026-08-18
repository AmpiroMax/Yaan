#
# Created: 09:08:2026 - 00:50:00
# Last updated: 19:08:2026 - 02:05:30
# File: engine/app/CMakeLists.txt
#
# Responsibility:
# - dfn_app executable: the composition root linking every layer.
#
# Dependencies:
# - Uses: dfn_core, dfn_world, dfn_render, dfn_gameplay, platform backends.
# - Used by: root CMakeLists (conditional add_subdirectory).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. LEAD-owned (Rule 25).
#
# UPD:
# - 09:08:2026 - 00:50:00: Created for stage 2 integration; guarded so partial
#                          trees (agents mid-stage) still configure and build.
# - 09:08:2026 - 15:07:13: Link dfn_physics (terrain collision helper) — the ferry now
# - 10:08:2026 - 02:42:46: Link dfn_anim (first-person body wiring).
#                          uses it instead of hand-filling TerrainDesc.
# - 10:08:2026 - 19:26:40: DebugOverlay.cpp в сборке; хэш коммита штампуется в бинарник (DFN_BUILD_COMMIT) — снимок состояния обязан называть сборку, иначе он невоспроизводим.
# - 10:08:2026 - 19:57:06: Хэш коммита штампуется во время СБОРКИ через tools/stamp_build_commit.cmake, с пометкой +dirty для незакоммиченного дерева.
# - 13:08:2026 - 20:41:07: HudScreen.cpp в сборке — прицел и будущий игровой интерфейс зоны ui.
# - 14:08:2026 - 16:11:00: EditorCamera.cpp в сборке — свободная камера режима AppMode::Editor.
# - 14:08:2026 - 16:50:36: MapCatalog.cpp в сборке — каталог карт браузера редактора (docs/MAP_LAYOUT.md).
# - 14:08:2026 - 17:36:02: ChatLog.cpp + TrajectoryRecord.cpp в сборке — ящик chat/<карта>.chat.jsonl (замечание человека + самодокументация демки, В28/O1), кольцевая телеметрия и запись/детерминированный повтор траектории (O3).
# - 14:08:2026 - 18:03:08: ChatOverlay.cpp в сборке — живое окно чата (ввод UTF-8 через text_input(), В28).
# - 14:08:2026 - 18:57:03: EditorHud.cpp в сборке — редакторский блок оверлея, вынесенный из App.cpp. Вынос не косметический: App.cpp держит окно и потому не тестируется, из-за чего наложение блока на отладочный вывод не мог поймать ни один прибор (tests/app/EditorHudTests.cpp ловит).

# - 14:08:2026 - 19:22:10: Controls.cpp в сборке — таблица привязок клавиш. Из неё И диспатчатся клавиши, И рисуется экран управления, поэтому список не может разъехаться с кодом: клавиша без строки в таблице просто не диспатчится.
# - 17:08:2026 - 14:50:07: sources/AssetBake.cpp — печь ассетов при первом запуске.
# - 17:08:2026 - 16:53:31: BuildTool.cpp — рука строителя (меню объектов, призрак, зелёное/красное).
# - 17:08:2026 - 19:17:13: EditorUi.cpp + линковка dfn_imgui — каркас интерфейса
#   РЕДАКТОРА на Dear ImGui (правило 1 называет editor единственным местом, где
#   эта библиотека разрешена). Игровое меню, HUD и экран управления остаются на
#   PixelCanvas и идут через палитру и пост — их эта библиотека не касается.
# - 17:08:2026 - 19:22:54: EditorUi.cpp УЕХАЛ отсюда в engine/editor/sources, а
#   dfn_imgui заменён на dfn_editor. Причина не вкусовая: dag_check не пускает
#   третью сторону в engine/app и правильно делает, а ARCHITECTURE.md давно
#   назвала engine/editor единственным местом, где Dear ImGui разрешён. Из
#   этого следует правило для соседей: ЛЮБАЯ панель, вызывающая ImGui, живёт в
#   engine/editor/sources, а не здесь — иначе тот же хук поймает её.
# - 17:08:2026 - 19:25:28: меню объектов уехало в engine/editor целиком: ImGui законен только там,
#   а слой editor не может включать app — значит панель и её модель обязаны лежать
#   по одну сторону, и эта сторона editor.
# - 17:08:2026 - 20:06:29: EditorPlant.cpp — судейская половина посадки растительности
#                          (зона кистей). Раскол EditorBrush по слоям: механика
#                          уехала в engine/editor вместе с панелью, а вердикт
#                          остался здесь, где живёт BuildTool.
# - 18:08:2026 - 16:59:18: AppActions.cpp + AppInput.cpp — слой 1 разбора App.cpp
#   (docs/PLAN_APP_DECOMPOSITION.md). Обработчики клавиш уехали из run() в
#   AppInput.cpp, а таблица «действие → метод» — в AppActions.cpp, у которой нет
#   ни App, ни окна, и потому её читает рукав app_controls. Разрез на ДВА файла
#   не косметика: положи таблицу рядом с обработчиками — и она утянет за собой
#   App.cpp в тестовую цель, то есть окно, то есть непроверяемость.
# - 18:08:2026 - 17:32:10: AppDoors.cpp — слой 2 разбора: 58 дверей DFN_* одной таблицей.
#   Тоже без App и без окна, и по той же причине: рукав app_doors читает её.
# - 18:08:2026 - 17:35:04: AppHud.cpp — слой 3: сборка кадра оверлеев (факты + холст).
#   Читает её app_hud_screen, потому что и здесь нет ни App, ни окна.
# - 18:08:2026 - 17:36:58: AppAfterFrame.cpp — слой 4: хвост кадра. Решения, которые он
#   принимает, лежат ЗАГОЛОВКОМ (AppAfterFrame.h, inline), чтобы рукав включал
#   их и не тянул за собой ни цели, ни окна.
set(DFN_APP_DEPS
    dfn_core
    dfn_world
    dfn_render
    dfn_editor
    dfn_anim
    dfn_physics
    dfn_gameplay
    dfn_platform_window
    dfn_platform_input
    dfn_platform_render
    dfn_platform_physics
    dfn_platform_anim
    dfn_platform_audio
    dfn_platform_llm)

set(DFN_APP_READY TRUE)
foreach(dep IN LISTS DFN_APP_DEPS)
    if(NOT TARGET ${dep})
        set(DFN_APP_READY FALSE)
        message(STATUS "dfn_app: skipped (waiting for ${dep})")
        break()
    endif()
endforeach()

if(DFN_APP_READY)
    add_executable(dfn_app
        sources/Main.cpp
        sources/App.cpp
        sources/AppActions.cpp
        sources/AppAfterFrame.cpp
        sources/AppDoors.cpp
        sources/AppHouse.cpp
        sources/AppHud.cpp
        sources/AppInput.cpp
    sources/AppSettings.cpp
    sources/AppWorld.cpp
        sources/AssetBake.cpp
        sources/BuildTool.cpp
        sources/ChatLog.cpp
        sources/ChatOverlay.cpp
        sources/Controls.cpp
        sources/DebugOverlay.cpp
        sources/EditorCamera.cpp
        sources/EditorPlant.cpp
        sources/EditorHud.cpp
        sources/HudScreen.cpp
        sources/Localization.cpp
        sources/MapCatalog.cpp
        sources/Menu.cpp
        sources/TrajectoryRecord.cpp)
    # dfn_editor: the editor's INTERFACE layer (Dear ImGui). It is a separate
    # target and not a few files here because docs/ARCHITECTURE.md names
    # engine/editor as the ONE place that library may be included, and
    # tools/dag_check.py enforces exactly that — a panel that calls ImGui
    # belongs in engine/editor/sources, not in engine/app/sources.
    target_link_libraries(dfn_app PRIVATE dfn_headers ${DFN_APP_DEPS})

    # The commit the binary was built from, stamped into every state capture.
    # A bug report carries the build it was seen on or it is not reproducible.
    #
    # STAMPED AT BUILD TIME, NOT CONFIGURE TIME. The first version of this
    # resolved the hash during cmake configure, and sim's capture then named a
    # build TWO COMMITS STALE. That is worse than having no field at all: the
    # question it exists to answer is "was this taken before or after the fix",
    # and a wrong answer converts "I do not know which build this is" into a
    # confident false premise (Rule 34) that two agents can each reason
    # soundly from. The script also appends "+dirty" for uncommitted edits,
    # because a hash alone cannot describe the working tree that captures are
    # normally taken from.
    set(DFN_BUILDINFO_DIR "${CMAKE_CURRENT_BINARY_DIR}/dfn_buildinfo")
    set(DFN_BUILDINFO_H "${DFN_BUILDINFO_DIR}/BuildInfo.h")
    file(MAKE_DIRECTORY "${DFN_BUILDINFO_DIR}")
    add_custom_target(dfn_buildinfo
        BYPRODUCTS "${DFN_BUILDINFO_H}"
        COMMAND ${CMAKE_COMMAND}
                -DOUT=${DFN_BUILDINFO_H}
                -DSRC=${CMAKE_SOURCE_DIR}
                -P ${CMAKE_SOURCE_DIR}/tools/stamp_build_commit.cmake
        COMMENT "Stamping the build commit into BuildInfo.h"
        VERBATIM)
    add_dependencies(dfn_app dfn_buildinfo)
    target_include_directories(dfn_app PRIVATE "${DFN_BUILDINFO_DIR}")
endif()
# - 18:08:2026 - 18:19:47: AppSettings.cpp, AppWorld.cpp, AppInternal.h — реализация класса
#   разложена по нескольким .cpp по заказу пользователя. Заголовок остаётся
#   один: «app.h нормальный по размерам, может по разным cpp файлам реализацию
#   класс раскидаем?».
# - 19:08:2026 - 02:05:30: AppHouse.cpp — знание App о постройке в одном файле (второй аудит, долг 1).

