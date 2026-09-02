#
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
    sources/AppStand.cpp
    sources/BodyHitboxes.cpp
    sources/CharacterFactory.cpp
        sources/AppEditorWiring.cpp
        sources/AppHouse.cpp
        sources/AppHud.cpp
        sources/AppInput.cpp
        sources/AppInterior.cpp
        sources/AppPoses.cpp
        sources/AppProps.cpp
        sources/AppSeats.cpp
    sources/AppSettings.cpp
    sources/AppWorld.cpp
        sources/AssetBake.cpp
        sources/BuildTool.cpp
        sources/ChatLog.cpp
        sources/ChatOverlay.cpp
        sources/Controls.cpp
        sources/DebugOverlay.cpp
        sources/DoorAim.cpp
        sources/FurnitureSeats.cpp
        sources/GrabDrive.cpp
        sources/PropPhysics.cpp
    sources/SkinnedCharacter.cpp
    sources/CharacterTextures.cpp
    sources/CharacterParts.cpp
    sources/ThirdPersonRig.cpp
        sources/SeatAim.cpp
        sources/EditorCamera.cpp
        sources/EditorPlant.cpp
        sources/EditorHud.cpp
        sources/HudScreen.cpp
        sources/IntroVideo.cpp
        sources/Localization.cpp
        sources/MapCatalog.cpp
        sources/Menu.cpp
        sources/MenuArt.cpp
        sources/MenuEmblem.cpp
        sources/ModelViewer.cpp
        sources/ModelConvert.cpp
        sources/AppViewer.cpp
        sources/CharGen.cpp
        sources/CharGenBody.cpp
        sources/Peoples.cpp
        sources/AppCharGen.cpp
        sources/UiSlider.cpp
        sources/PngImage.cpp
        sources/TrajectoryRecord.cpp
        sources/UiFont.cpp)
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
# - 20:08:2026 - 02:02:41: AppEditorWiring.cpp — wire_editor_panels() целиком (разрез А
#   третьего аудита). Перенос без изменения тела: объявление осталось в App.h.
# - 23:08:2026 - 23:50:00: AppInterior.cpp — переход в интерьер-локацию и обратно
#                          (И15 волна А): карман, подвес, второй слот, портал,
#                          экран загрузки, точка возврата.
# - 27:08:2026 - 12:58:00: MenuEmblem.cpp — объёмный герб главного меню (заказ
#   владельца 27.08). Отдельный файл, а не десять строк в Menu.cpp, по той же
#   причине, по которой отделены слои разбора: Menu.cpp рисует ХОЛСТ и не
#   касается ни видеокарты, ни камеры, а герб — меш, программа и свет кадра.
#   Разрез заодно делает раскладку и темп проверяемыми без окна
#   (tests/app/MenuEmblemTests.cpp, рукав app_menu_emblem).
# - 28:08:2026 - 12:20:00: sources/AppSeats.cpp, FurnitureSeats.cpp, SeatAim.cpp —
#   сидеть и лежать (обязательство эпохи, заказ владельца 28.08).
# - 27:08:2026 - 21:40:30: sources/DoorAim.cpp — геометрия прицела двери
#                          (радиус до полотна и луч взгляда против его
#                          прямоугольника). Отдельный файл потому, что его
#                          линкует рукав app_door_aim: в нём нет ни App, ни
#                          окна, ни физики, и только поэтому вердикт «целюсь ли
#                          я в дверь» можно провалить нарочно.

