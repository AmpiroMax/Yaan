#
# Created: 09:08:2026 - 00:35:09
# Last updated: 28:08:2026 - 11:51:19
# File: CMakeLists.txt
#
# Responsibility:
# - Root build: project setup, shared dependencies (glm, doctest), the
#   NUMBERS.md constants generation step, and conditional inclusion of every
#   layer's own CMakeLists (Q34: zones own their layer files; the root only
#   aggregates, so parallel agents never edit this file).
#
# Key items:
# - dfn_headers INTERFACE target: repo-root + generated include dirs, glm,
#   project warnings. Every dfn_* target links it.
# - Target naming convention: dfn_core, dfn_world, dfn_platform_<module>,
#   dfn_render, dfn_gameplay, dfn_app (documented here, used by all zones).
#
# Dependencies:
# - Uses: tools/gen_constants.py, docs/NUMBERS.md, FetchContent (pinned tags).
# - Used by: every layer CMakeLists via add_subdirectory.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. This file is LEAD-owned (Rule 25):
#   add targets in your OWN layer's CMakeLists.txt, never here.
#
# UPD:
# - 09:08:2026 - 00:35:09: Created for stage 2 (skeleton walk).
# - 09:08:2026 - 00:55:00: include(CTest) at root so ctest works from the build
# - 09:08:2026 - 16:32:08: Default CMAKE_BUILD_TYPE=RelWithDebInfo when unset —
#                          an unset type compiles with no optimization at all.
#                          root (core's report, note 3).
# - 14:08:2026 - 20:47:52: dfn_bake — CLI выпечки мира в .dfw. Настоящая цель, а не
#                          инструмент, собираемый руками: после разворота на
#                          инструменты офлайн-выпечка есть НОРМАЛЬНЫЙ способ
#                          произвести мир, а нормальный способ обязан
#                          собираться вместе со всем остальным.
# - 14:08:2026 - 23:36:19: dfn_forge — CLI кузницы деревьев в реестр .dfo.
# - 15:08:2026 - 16:24:04: dfn_scene_check — проверка композиции карты по правилам.
# - 16:08:2026 - 21:08:52: dfn_kit — CLI строительного набора: 508 деталей (брус,
#                          столб, доска, стена, фронтон, скат, лестница, дверь,
#                          окно, фундамент, забор) в реестр .dfo, чтобы агент
#                          СОБИРАЛ дома из готового, а не писал дом кодом.
# - 16:08:2026 - 22:40:23: dfn_assemble — группа деталей .scene спекается в ОДИН
# - 17:08:2026 - 14:46:25: dfn_signs — CLI табличек: текст приходит файлом .signs, имя объекта несёт хэш строки.
#                          объект реестра. Второй уровень набора: из ста блоков
#                          стены собирается неограниченное число панелей, и
#                          запечатанная один раз панель не может протечь заново.
# - 17:08:2026 - 16:14:41: dfn_newmap — явное создание папки карты и её композиции. Заказ
#   пользователя 17.08: папки не должны появляться как побочный эффект записи.
# - 17:08:2026 - 19:17:13: dfn_imgui — Dear ImGui v1.91.9b-docking, интерфейс РЕДАКТОРА
#   (правило 1 называет editor единственным местом, где эта библиотека разрешена).
#   Объявлена здесь, а не в слое, потому что её берут ДВА слоя: бэкенд к bgfx в
#   engine/platform/render и каркас панелей в engine/app, а render стоит в
#   DFN_LAYERS раньше app — объявление внутри любого из них поставило бы
#   существование цели в зависимость от порядка строк списка.

# - 17:08:2026 - 20:42:24: ОДНА папка загрузок на все девять каталогов сборки (third_party/_deps):
#   зависимость качалась по разу на каталог и заново при каждой перенастройке.
# - 17:08:2026 - 21:05:14: GIT_SHALLOW у imgui. Измерено одной и той же командой git в чистый
#   каталог, разница в одном флаге — полная история 327.37 с и 138 МБ, снимок по
#   тегу 5.28 с и 10 МБ. Замер намеренно БЕЗ cmake и без общей папки третьих
#   сторон, чтобы число говорило о ГЛУБИНЕ, а не о том, что кто-то уже скачал.
#   Повод: полный клон трижды вставал намертво (21:46, 13:15 и 10:14 при 0% CPU),
#   и бинарник пользователя отстал на четыре коммита. Пин не тронут: shallow —
#   это `git clone --depth 1 --branch <тег>`.
# - 18:08:2026 - 00:07:07: bgfx.cmake ЗАВЕЗЕНА в third_party/bgfx.cmake, и берётся
#   через FETCHCONTENT_SOURCE_DIR_BGFX_CMAKE. Общая папка загрузок (запись выше)
#   развалила настройку на весь вечер: клон bgfx падал раз за разом
#   (fetch-pack: invalid index-pack output), а симптом выглядел как «cmake ничего
#   не делает» — ninja: error: rebuilding 'build.ninja': subcommand failed, без
#   причины в выводе. Причина видна ТОЛЬКО отдельным `cmake -S . -B <dir>` с
#   полным выводом; фильтр по слову «error» её отсеивает, и я на этом потерял
#   несколько заходов.
#   ПОЧЕМУ РЯДОМ С _deps, А НЕ ВНУТРИ: шаг клонирования FetchContent СТИРАЕТ
#   каталог назначения перед клонированием, если у подсборки нет своих штампов.
#   Подложенные в _deps исходники он однажды сотрёт — что и сделал у меня на
#   глазах: зеркалировал каталог целиком, следующая настройка его вычистила.
#   Проверено контрфактом: чистый каталог сборки настраивается за 2.0 с и БЕЗ
#   СЕТИ (прежде — падение на шаге клонирования). Пин в
#   engine/platform/render/CMakeLists.txt не тронут и остаётся описанием
#   происхождения (правило 24); нет каталога — строка не срабатывает, и всё
#   идёт прежним путём через сеть.
#   ПОБОЧНО ВЫЯСНИЛОСЬ: общая папка загрузок ЗАПИРАЕТ ГЕНЕРАТОР. Подсборки в
#   _deps помнят свой (у нас Ninja), и каталог сборки на Unix Makefiles падает
#   с «Does not match the generator used previously: Ninja». Новые каталоги
#   заводить с `-G Ninja`.
# - 20:08:2026 - 15:50:00: Цель dfn_houses — кузница готовых построек (tools/forge_houses.cpp).
# - 21:08:2026 - 13:20:00: Цель dfn_furniture — кузница предметов интерьера (tools/forge_furniture.cpp).
# - 24:08:2026 - 02:00:00: dfn_interior_check — приёмка интерьера-локации (И15 волна А)
#                          и две его дозы в ctest: чистый пилот 0 находок,
#                          испорченная оснастка ровно 4.
# - 27:08:2026 - 01:20:00: отрицательное плечо приёмки локаций переехало с КОПИИ
#   Вайтрана на боевой город (И15 волна Б): у самого Вайтрана теперь есть
#   внутренности, копия wr-int.scene снята, и плечо мерит то, что играется.

# - 27:08:2026 - 10:18:14: dfn_furn_objects — печь полки мебели: чертёж
#   furn-*.dfh печётся ОДИН РАЗ в объект реестра assets/objects/furniture/*.dfo
#   (заказ владельца 27.08: кровать — единый запечённый объект, видимый в
#   перечне). Плюс dfn_interior_check получил dfn_render: судья локации теперь
#   читает .dfo, чтобы видеть убранство, пришедшее ссылкой [place].
# - 27:08:2026 - 10:34:00: МЕТКА ЗАПИСИ ВЫШЕ ИСПРАВЛЕНА НА РЕАЛЬНОЕ ВРЕМЯ (правило 16):
#   стояло 15:xx при стенных 10:18 — метка была выбрана позже чужой записи 14:30
#   в AppHouse.cpp, лишь бы сошлась сверка хука, вместо того чтобы разобраться с
#   чужой. Поставлено время коммита 9e2c8c9 (10:18:14). Текста и кода не трогал.
# - 27:08:2026 - 23:05:00: ОБА СУДЬИ ЗАВЕДЕНЫ В ctest ПО ВСЕЙ ИХ ОБЛАСТИ (аудит
#   «Большой мир», задачи 1 и 5). Было: у dfn_stairs_check стоял add_executable и НИ
#   ОДНОГО add_test — он был запущен рукой один раз, в день волны, а зелёные
#   96/96 о лестницах не знали ничего; dfn_interior_check шёл по ОДНОЙ сцене из
#   130, и по совпадению по той единственной, где находок нет. Стало: полка
#   assets/houses целиком и обе стопки локаций целиком, плюс шесть контрольных
#   рук на замороженных отвергнутых случаях (tests/fixtures/stairs) и список
#   известных находок с датой и причиной, который жжёт волна геометрии.
# - 28:08:2026 - 17:15:00: dfn_forge2 (tools/forge_trees_v2.cpp) — кузница ВТОРОЙ
#   ИТЕРАЦИИ ДЕРЕВЬЕВ (заказ владельца 28.08 по разнице с Готикой 3). Отдельный
#   бинарник, а не раздел dfn_forge: тот переписывает все 82 своих файла на
#   каждом прогоне, а железное правило волны — первая итерация не тронута.
#   Плюс ctest-рука trees_v2_untouched: пересобирает полку первой итерации во
#   временную папку и сверяет content_hash с INDEX.md — сторож неприкосновенности.
# - 28:08:2026 - 11:51:19: ПРИЁМКА МОЩЕНИЯ (tools/check_paving.py) ЗАВЕДЕНА В ctest — ПЯТЬ РУК
#   (волна хвостов). Прибор не имел ни одного add_test и потому ЗАСТАРЕЛ
#   МОЛЧА: мощение 24.08 переехало из тел плит в строки path/pp рельефа, а он
#   продолжал искать тела и печатал «покрыто 0.0%» на ОБОИХ городах при 100% по
#   приёмке генератора. Заведены оба города и три замороженные оснастки
#   (tests/fixtures/paving): paved обязан пройти, bare и blot — провалиться
#   (WILL_FAIL). Прогон всех пяти — 0.50 с.

cmake_minimum_required(VERSION 3.28)

# An unset CMAKE_BUILD_TYPE means NO optimization flag at all — every timing
# measured in such a tree is ~30x pessimistic (found by core while chasing a
# 30x gap between identical algorithms in two trees). Performance decisions
# were nearly made on those numbers, so the default is explicit here.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set — defaulting to RelWithDebInfo")
endif()

project(daggerfall_n CXX)

set(CMAKE_CXX_STANDARD 23)              # Rule 19: clang 22 primary; AppleClang 15
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # builds it as c++2b — guard 23-only libs.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clangd
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Generated constants (Q54): NUMBERS.md is the single source of truth -----
set(DFN_GENERATED_DIR "${CMAKE_BINARY_DIR}/dfn_generated")
execute_process(
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_constants.py"
            --numbers "${CMAKE_CURRENT_SOURCE_DIR}/docs/NUMBERS.md"
            --out "${DFN_GENERATED_DIR}/dfn_generated/Constants.h"
    RESULT_VARIABLE DFN_GEN_RESULT)
if(NOT DFN_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "gen_constants.py failed — fix docs/NUMBERS.md")
endif()
# Re-run configure when NUMBERS.md changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS docs/NUMBERS.md)

# --- Shared third-party (Rule 24: pinned, FetchContent only) -----------------
# ОДНА ПАПКА ЗАГРУЗОК НА ВСЕ СБОРКИ (пользователь, 17.08: «так скачайте имгуи в
# одно место, в папку с thirdpartylibs, и туда скачайте; если скачено, то не надо
# качать постоянно»).
#
# По умолчанию FetchContent кладёт исходники ВНУТРЬ каталога сборки, а каталогов
# сборки у нас девять — по одному на зону. Значит каждая новая зависимость
# скачивалась ДЕВЯТЬ раз, и хуже того: перенастройка (её вызывает почти любая
# правка списков сборки) заставляла качать снова. В этот вечер три сессии
# одновременно тянули один и тот же ImGui, делили канал и душили друг друга —
# `cmake` выглядел зависшим, хотя ждал сеть. Молчаливое ожидание сети
# неотличимо от сломанного инструмента, и пользователь прочёл его именно так.
#
# Общая папка вне каталогов сборки: скачано один раз — берётся всеми, переживает
# и перенастройку, и удаление build_*. В .gitignore, потому что это ВЫВОД
# загрузчика, а не наш исходник (та же причина, по которой из git ушла полка
# испечённых деталей).
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party/_deps"
    CACHE PATH "Общий кэш загруженных зависимостей на все каталоги сборки")

# ЗАВЕЗЁННАЯ bgfx.cmake, и она лежит НЕ в _deps намеренно. Шаг клонирования
# FetchContent СТИРАЕТ каталог назначения перед клонированием, если у подсборки
# нет своих штампов, — поэтому всё, что подложено внутрь _deps, однажды будет
# стёрто, и настройка новой сборки снова упрётся в сеть. Это стоило вечера:
# клон bgfx падал раз за разом (fetch-pack: invalid index-pack output), а
# симптом выглядел как «cmake ничего не делает» — ninja: error: rebuilding
# 'build.ninja': subcommand failed, без причины в выводе.
#
# При заданной FETCHCONTENT_SOURCE_DIR_<ИМЯ> FetchContent не делает ни
# клонирования, ни обновления, ни подсборки: каталог берётся как есть, и сеть
# не нужна вовсе. Тег объявлен в engine/platform/render/CMakeLists.txt (правило
# 24) и остаётся описанием происхождения; сам завоз — v1.153.9398-565 с
# проинициализированными подмодулями bgfx/bimg/bx.
#
# Каталога нет — строка не срабатывает, и всё идёт прежним путём через сеть.
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_BGFX_CMAKE
   AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/bgfx.cmake/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_BGFX_CMAKE "${CMAKE_SOURCE_DIR}/third_party/bgfx.cmake"
        CACHE PATH "Завезённая bgfx.cmake: без сети, без клонирования")
    message(STATUS "bgfx.cmake: завезённая копия ${FETCHCONTENT_SOURCE_DIR_BGFX_CMAKE}")
endif()

include(FetchContent)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11)
FetchContent_MakeAvailable(glm doctest)

# DEAR IMGUI — THE EDITOR'S INTERFACE, AND NOTHING ELSE (Rule 1's documented
# exception: "editor may use Dear ImGui directly"). The game menu, the HUD and
# the controls screen stay on PixelCanvas and keep going through the palette
# and the post chain; this library draws at native resolution after the upscale
# and is deliberately not part of the game's look.
#
# DECLARED HERE, NOT IN A LAYER, because TWO layers need it: the bgfx backend
# (engine/platform/render, which owns every third-party include) and the panel
# frame (engine/app). engine/platform/render is added to DFN_LAYERS BEFORE
# engine/app, so a declaration inside either one would make the target's
# existence depend on the order layers happen to be listed in.
#
# THE PIN IS 1.91.9b AND NOT THE NEWEST ON PURPOSE (Rule 24 wants an exact tag;
# WHICH exact tag is a decision). 1.92 rewrote font atlas and backend texture
# ownership (ImTextureData / WantCreate / WantDestroy), which our own backend
# would have to implement on the first day it exists. "Newest" would buy a risk
# nobody asked to pay for. The -docking suffix is the docking branch: the
# properties column the user asked for is dockable there and hand-written here.
#
# GIT_SHALLOW BECAUSE THE FULL CLONE WEDGED THREE BUILDS, AND THE NUMBERS SAY SO
# rather than "it felt slow". Measured on this machine, 17.08.2026:
#   full history (what this line used to fetch): 138 MB, and the configure that
#     did it took 291 s -- after which three separate `cmake` invocations across
#     two sessions sat on `imgui-populate-gitclone` for 21:46, 13:15 and 10:14
#     at 0% CPU, i.e. not slow, STOPPED. The user's binary went four commits
#     stale behind a fix he was waiting for.
#   shallow at the same tag: 5.28 s, 10 MB.
# The pin is untouched -- shallow means `git clone --depth 1 --branch <tag>`, so
# Rule 24's "exact pinned release tag" is exactly as exact as it was. What is
# given up is the history of a library we never bisect.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b-docking        # Rule 24: exact pinned release tag
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)
add_library(dfn_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp")
# SYSTEM so -Wall -Wextra -Wpedantic stays clean for our own translation units,
# exactly as bgfx/bimg/bx are treated one layer down.
target_include_directories(dfn_imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")

include(CTest) # root-level so `ctest --test-dir <build>` finds zone tests

# --- Common interface target -------------------------------------------------
add_library(dfn_headers INTERFACE)
target_include_directories(dfn_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"        # absolute-from-root includes (rules/structure.md)
    "${DFN_GENERATED_DIR}")
target_link_libraries(dfn_headers INTERFACE glm::glm)
target_compile_options(dfn_headers INTERFACE -Wall -Wextra -Wpedantic)

# --- Layers (zone-owned; included once their CMakeLists lands) ---------------
set(DFN_LAYERS
    engine/core
    engine/world
    engine/platform/window
    engine/platform/input
    engine/platform/render
    engine/platform/physics
    engine/platform/anim
    engine/platform/audio
    engine/platform/llm
    engine/render
    engine/physics
    engine/anim
    engine/gameplay
    engine/editor
    engine/app
    tests)
foreach(layer IN LISTS DFN_LAYERS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${layer}/CMakeLists.txt")
        add_subdirectory(${layer})
    endif()
endforeach()

# ---------------------------------------------------------------------------
# THE BAKE CLI (dfn_bake). Part of the pipeline, not an instrument, so it is a
# real target rather than a hand-compiled measuring tool like
# tools/dump_heightmap.cpp: the tooling pivot (в1) makes "the generator writes
# offline, the game only reads" the normal way the world is produced, and the
# normal way has to build with everything else or it rots.
add_executable(dfn_bake tools/bake_world.cpp)
target_link_libraries(dfn_bake PRIVATE dfn_world dfn_core)

# THE TREE FORGE CLI (dfn_forge): builds registry objects offline and writes
# .dfo files. Same standing as dfn_bake — part of the pipeline, not an
# instrument, so it builds with everything else.
add_executable(dfn_forge tools/forge_trees.cpp)
target_link_libraries(dfn_forge PRIVATE dfn_render dfn_core)

# THE SECOND ITERATION'S FORGE (dfn_forge2, 28.08): bakes the *-v2-* recipes
# beside the first iteration's files. A separate binary on purpose — dfn_forge
# rewrites all 82 files it owns on every run, and this wave's iron rule is that
# nothing of the first iteration is touched.
add_executable(dfn_forge2 tools/forge_trees_v2.cpp)
target_link_libraries(dfn_forge2 PRIVATE dfn_render dfn_core)

# СТОРОЖ НЕПРИКОСНОВЕННОСТИ ПЕРВОЙ ИТЕРАЦИИ (волна деревьев-v2, 28.08). Печёт
# полку dfn_forge во временную папку и сверяет content_hash со строками
# assets/objects/trees/INDEX.md. Клятва «старые деревья не тронуты» становится
# проверяемой в ctest, а не обещанием в отчёте.
add_test(NAME trees_v1_frozen
         COMMAND python3 tools/check_trees_frozen.py $<TARGET_FILE:dfn_forge>
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# THE SCENE CHECKER (dfn_scene_check): judges a composed map against the rules
# in engine/world/sources/Scene.h. A pipeline tool like dfn_bake and dfn_forge,
# so it builds with everything else and cannot rot.
add_executable(dfn_scene_check tools/check_scene.cpp)
target_link_libraries(dfn_scene_check PRIVATE dfn_world dfn_render dfn_core)

# ПРИЁМКА ИНТЕРЬЕРА-ЛОКАЦИИ (dfn_interior_check, И15 волна А): руки 1/2/5/7 по
# геометрии локации и рука 6 по парности ссылок города. Стоит рядом с судьёй
# сцены и по той же причине: прибор, который перестал собираться, обнаруживают
# в день, когда он нужнее всего.
add_executable(dfn_interior_check tools/check_interior.cpp)
target_link_libraries(dfn_interior_check PRIVATE dfn_world dfn_render dfn_core)

# ДВЕ ДОЗЫ ОДНОГО ПРИБОРА В ctest (правило 30 и свод И15, дельта-5).
# Отрицательное плечо: верная локация пилота обязана дать НОЛЬ находок.
# Положительное: нарочно испорченная — РОВНО ЧЕТЫРЕ, по одной на руку.
# Без второго теста молчание прибора неотличимо от прибора неработающего;
# без первого «четыре находки» доказывали бы только, что он шумит.
# ОТРИЦАТЕЛЬНОЕ ПЛЕЧО ПЕРЕЕХАЛО С КОПИИ НА БОЕВОЙ ГОРОД (И15 волна Б, 27.08).
# Волна А держала его на пилоте — доме из КОПИИ Вайтрана (wr-int.scene),
# потому что боевой город тогда не имел права меняться (правило 47). Теперь
# внутренности есть у самого Вайтрана, копия снята, и плечо мерит ТО, ЧТО
# ИГРАЕТСЯ: настоящий дом настоящего города плюс парность ссылок ВСЕХ его
# локаций (рука 6 идёт по --city целиком, а не по одному дому).
add_test(NAME interiors_pilot_clean
         COMMAND dfn_interior_check assets/scenes/int/whiterun/x103z86.scene
                 --city assets/scenes/whiterun.scene --expect 0
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME interiors_broken_fixture
         COMMAND dfn_interior_check assets/scenes/int/broken.scene --expect 4
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ...И ТО ЖЕ ПО ВСЕМ СТА ТРИДЦАТИ ЛОКАЦИЯМ ОБОИХ ГОРОДОВ (27.08).
# Два теста выше мерили ОДНУ сцену и одну оснастку. Аудит «Большой мир» прогнал судью
# по всем 130 и получил 169 находок — то есть всё, ради чего судья написан,
# жило ВНЕ ctest, а зелёный ctest про интерьеры не значил ничего. Прибор,
# который не запускают по своей области, эквивалентен отсутствию прибора.
#
# ЖДЁТ НОЛЬ, А ИЗВЕСТНОЕ ЗНАЕТ ПО ИМЕНИ. Геометрию чинит параллельная волна, и
# до её конца тест был бы красным всегда — а красный всегда читают как
# «сломано у соседа» и перестают смотреть. Поэтому известные находки
# перечислены в tests/known_interior_findings.txt: каждая с датой, причиной и
# ЧИСЛОМ. Находка сверх числа — красный сразу; строка, на которую находок
# больше не пришло, печатается как протухшая и жжётся волной геометрии.
# Порог здесь один и он честный: НОЛЬ. Список — это отсрочка с именем, а не
# послабление.
add_test(NAME interiors_whiterun_all
         COMMAND dfn_interior_check assets/scenes/int/whiterun
                 --city assets/scenes/whiterun.scene
                 --known tests/known_interior_findings.txt
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME interiors_cornhall_all
         COMMAND dfn_interior_check assets/scenes/int/cornhall
                 --city assets/scenes/cornhall.scene
                 --known tests/known_interior_findings.txt
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# СУДЬЯ ВНУТРИДОМОВОГО МАРША (dfn_stairs_check, волна лестниц 27.08): проход от
# двери, уклон, просвет над носком каждой ступени, марш внутри оболочки, стык
# невидимого пандуса. Три крита владельца про лестницы не имели ни одного
# прибора — приёмка прошлых заходов печатала просвет по ПЛИТАМ, а бьёт не плита.
add_executable(dfn_stairs_check tools/check_stairs.cpp)
target_link_libraries(dfn_stairs_check PRIVATE dfn_world dfn_core)

# СУДЬЯ МАРШЕЙ — В ctest, ПО ВСЕЙ ПОЛКЕ (27.08). До этой строки у него был
# add_executable и ни одного add_test: он был запущен рукой ОДИН раз, в день
# волны лестниц, а зелёные 96/96 о лестницах не знали ничего. Прогон по всем
# 194 телам стоит ~35 с — дешевле любой из трёх переделок, которые он ловит.
add_test(NAME stairs_shelf
         COMMAND dfn_stairs_check assets/houses
                 --known tests/known_stair_findings.txt
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ШЕСТЬ КОНТРОЛЬНЫХ РУК (правило 30: тест публикуется вместе со случаем,
# который он ОБЯЗАН отвергать, и этот случай обязан его проваливать).
#
# Каждая пара — НАСТОЯЩИЙ отвергнутый случай и его положительное плечо,
# отличающееся ОДНИМ числом (правило 30a: у теста должен быть и случай, который
# МОЖЕТ его пройти). Числа отвергнутых сняты с живых тел 27.08 и ЗАМОРОЖЕНЫ в
# tests/fixtures/stairs: живые рецепты чинит волна геометрии, а контрольная
# рука, которую чинят, перестаёт быть контрольной.
add_test(NAME stairs_ladder_rejected
         COMMAND dfn_stairs_check tests/fixtures/stairs/ladder-tread.dfh
                 --expect ПРОСТУПЬ:1 --expect 1
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME stairs_ladder_ok
         COMMAND dfn_stairs_check tests/fixtures/stairs/ladder-tread-ok.dfh
                 --expect 0
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME stairs_gentle_rejected
         COMMAND dfn_stairs_check tests/fixtures/stairs/gentle-1to2.dfh
                 --expect ПОЛОГО:1 --expect 1
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME stairs_gentle_ok
         COMMAND dfn_stairs_check tests/fixtures/stairs/gentle-1to2-ok.dfh
                 --expect 0
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME stairs_lowbeam_rejected
         COMMAND dfn_stairs_check tests/fixtures/stairs/lowbeam.dfh
                 --expect ПОДЪЁМ:1 --expect ПРОСВЕТ:1 --expect 2
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ЗАКОННОЕ ИСКЛЮЧЕНИЕ ОБЯЗАНО БЫТЬ ПРОВЕРЯЕМЫМ. frame-replica.dfh — демо-стенд
# кузницы, он не чинится сознательно, и его находки перечислены в списке
# известных. Ровно поэтому нужен отдельный тест, требующий, чтобы он ОСТАВАЛСЯ
# отвергнутым: исключение, за которым никто не следит, через месяц скрывает не
# демо-стенд, а починенное вместе с ним настоящее тело.
add_test(NAME stairs_replica_still_rejected
         COMMAND dfn_stairs_check assets/houses/frame-replica.dfh
                 --expect ПРОСВЕТ:1 --expect ПОДЪЁМ:1
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ПРИЁМКА МОЩЕНИЯ (tools/check_paving.py) — В ctest, ОБА ГОРОДА И ТРИ РУКИ.
# До 28.08 у неё не было ни одного add_test: прибор запускали рукой, и он
# ЗАСТАРЕЛ МОЛЧА. 24.08 мощение перестало быть телами плит и стало строками
# path/pp рельефа; прибор продолжал искать в .scene тела city-cobble*, находил
# ноль и печатал «покрыто 0.0%, ПРИЁМКА НЕ ПРОЙДЕНА» на ОБОИХ городах — при
# 100% по собственной приёмке генератора. Два отчёта подряд переписали его
# вердикт в «известную ложную тревогу», то есть выключили его.
#
# ЧЕТЫРЕ РУКИ, И ТРИ ИЗ НИХ — ОСНАСТКА (правило 30). Порог здесь ничего не
# доказывает сам по себе: два города зелены, и без отвергнутых образцов это
# значило бы только, что прибор молчит. Оснастка заморожена в
# tests/fixtures/paving — один чертёж и ТРИ рельефа, отличающиеся ровно
# полотном: paved (лента мостовой по оси) обязан пройти, bare (лент нет)
# и blot (лента тёсаного камня поперёк улицы, к концам не примыкающая)
# обязаны провалиться. WILL_FAIL, а не --expect: у питонового прибора вердикт
# и есть код возврата.
add_test(NAME paving_whiterun
         COMMAND python3 tools/check_paving.py
                 assets/scenes/whiterun.relief docs/WHITERUN_PLAN.json
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME paving_cornhall
         COMMAND python3 tools/check_paving.py
                 assets/scenes/cornhall.relief docs/CORNHALL_PLAN.json
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME paving_fixture_ok
         COMMAND python3 tools/check_paving.py
                 tests/fixtures/paving/paved.relief tests/fixtures/paving/plan.json
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME paving_bare_rejected
         COMMAND python3 tools/check_paving.py
                 tests/fixtures/paving/bare.relief tests/fixtures/paving/plan.json
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(paving_bare_rejected PROPERTIES WILL_FAIL TRUE)
add_test(NAME paving_blot_rejected
         COMMAND python3 tools/check_paving.py
                 tests/fixtures/paving/blot.relief tests/fixtures/paving/plan.json
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(paving_blot_rejected PROPERTIES WILL_FAIL TRUE)

# THE BUILDING KIT CLI (dfn_kit): forges the construction parts an agent
# assembles houses out of. Same standing again — a kit that stopped compiling
# would be discovered by a builder mid-house.
add_executable(dfn_kit tools/forge_kit.cpp)
target_link_libraries(dfn_kit PRIVATE dfn_render dfn_core)

# THE SIGN FORGE CLI (dfn_signs): bakes tablichki from a .signs text file. The
# TEXT is content (Rule 5), so it arrives in a file and adding a sign never
# means a rebuild.
add_executable(dfn_signs tools/forge_signs.cpp)
target_link_libraries(dfn_signs PRIVATE dfn_render dfn_core)

# THE ASSEMBLER (dfn_assemble): freezes a GROUP of a .scene into one registry
# object — the kit's second level, where small parts become a wall panel. Same
# standing as the rest of the pipeline.
add_executable(dfn_assemble tools/assemble_object.cpp)
target_link_libraries(dfn_assemble PRIVATE dfn_world dfn_render dfn_core)

# NEW MAP (dfn_newmap): the ONE visible way a map folder is created. Links
# nothing on purpose — making a folder must not wait for the world to compile.
# КУЗНИЦА ГОТОВЫХ ПОСТРОЕК (dfn_houses): пять домов через HouseGraph API в
# assets/houses/*.dfh — артефакты в git, правки только в рецептах кузницы.
add_executable(dfn_houses tools/forge_houses.cpp)
target_link_libraries(dfn_houses PRIVATE dfn_world dfn_core)

# КУЗНИЦА ВНУТРЕННЕГО ОФОРМЛЕНИЯ (dfn_furniture): предметы интерьера тем же
# HouseGraph API в assets/houses/furn-*.dfh — каждый предмет отдельной готовой
# постройкой, чтобы расставлять их так же, как дома.
add_executable(dfn_furniture tools/forge_furniture.cpp)
target_link_libraries(dfn_furniture PRIVATE dfn_world dfn_core)

# ПЕЧЬ ПОЛКИ МЕБЕЛИ (dfn_furn_objects): чертёж furn-*.dfh печётся ОДИН РАЗ в
# объект реестра assets/objects/furniture/*.dfo — тело, свет и плитки материала
# посчитаны заранее, город и локация ставят ссылку. Линкует и мир (построитель
# тела), и рендер (контейнер .dfo), как dfn_assemble.
add_executable(dfn_furn_objects tools/forge_furniture_objects.cpp)
target_link_libraries(dfn_furn_objects PRIVATE dfn_world dfn_render dfn_core)

add_executable(dfn_newmap tools/new_map.cpp)
