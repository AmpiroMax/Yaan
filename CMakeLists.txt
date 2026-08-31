#
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

# ИМПОРТЁР glTF (dfn_import_gltf, волна персонажа 30.08). Та же осанка, что у
# кузниц: часть конвейера, а не прибор, — и по тому же доводу собирается со
# всем остальным. Пишет .dfo v5 (SKIN/SKEL/ANIM); игра читает только .dfo, и
# ни одна строка glTF в кадр не попадает (в1 на импорте).
#
# cgltf берётся из вендоренной bgfx.cmake — единственный третьесторонний
# заголовок вне platform/sources во всём дереве, и он не в движке, а в
# ОФЛАЙН-инструменте (довод в шапке tools/import_gltf.cpp).
if(NOT DEFINED bgfx_cmake_SOURCE_DIR)
    include(FetchContent)
    FetchContent_GetProperties(bgfx_cmake)
endif()
set(DFN_CGLTF_DIR "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/cgltf")
if(EXISTS "${DFN_CGLTF_DIR}/cgltf.h")
    add_executable(dfn_import_gltf tools/import_gltf.cpp)
    target_link_libraries(dfn_import_gltf PRIVATE dfn_render dfn_anim dfn_core)
    target_include_directories(dfn_import_gltf SYSTEM PRIVATE "${DFN_CGLTF_DIR}")
else()
    message(WARNING "cgltf not found at ${DFN_CGLTF_DIR}; dfn_import_gltf skipped")
endif()

# СУДЬЯ ПРОПОРЦИЙ (dfn_human_scale, заказ владельца 30.08: «пропорции тела
# делать сразу»). Мерит по СКЕЛЕТУ импортированной модели и судит по канону
# docs/design/HUMAN_SCALE.md; числа печатает всегда, не только приговор.
add_executable(dfn_human_scale tools/check_human_scale.cpp)
target_link_libraries(dfn_human_scale PRIVATE dfn_render dfn_anim dfn_core)

# ВЫПЕЧКА ПЕРСОНАЖЕЙ (dfn_characters). Правило дерева: исходник в git, выпечка
# производная (.gitignore, решение владельца 17.08) — значит .glb лежит в
# репозитории с лицензией рядом, а .dfo печётся здесь и в git не едет.
# Печь на сборке, а не при первом запуске: импортёр — отдельный бинарник, и
# приложение о нём знать не должно.
if(TARGET dfn_import_gltf)
    set(DFN_CHAR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/objects/characters")
    set(DFN_CHAR_OUTPUTS)
    # <исходник> <имя> <высота, м> <рыск, град> <канон?> <имя выпечки>
    #
    # ВИДИМЫЙ ПЕРСОНАЖ — ТОЛЬКО HumanBase, и только с --fit-canon и --reshape.
    # --fit-canon (заказ владельца 30.08 «пропорции тела делать сразу») ставит
    # СУСТАВЫ по docs/design/HUMAN_SCALE.md; --reshape (заказ 31.08 «чрезмерно
    # перекачен, слишком длинные руки, живота нет») правит МЯСО. Два разных
    # заказа и два разных прохода, потому что на этой модели все девять
    # суставных ориентиров лежали внутри 2 % от канона ровно тогда, когда все
    # три жалобы были про неё правдой: у сустава нет радиуса, последний сустав
    # руки — запястье, а живот лежит МЕЖДУ суставами.
    #
    # HumanBaseV1 — ТА ЖЕ МОДЕЛЬ БЕЗ --reshape, и она печётся не «на всякий
    # случай»: приёмка «до/после» обязана выходить из одной сборки (правило 47),
    # а судья, который пропускает всех, неотличим от судьи, который ничего не
    # мерит. Она стоит в ctest ОТВЕРГНУТОЙ — это контрольная рука волны формы
    # (правило 30) — и она же «до» на кадрах отчёта.
    #
    # Knight и RiggedFigure — ФИКСТУРЫ ИМПОРТЁРА: рыцарь стилизован (2.11
    # головы), фигурка мала (19 костей) — обе прекрасно ловят дефекты читателя
    # и обе отвергаются судьёй пропорций, что и есть их вторая работа.
    foreach(spec "HumanBase.glb;human-base;1.75;180;shape;HumanBase"
                 "HumanBase.glb;human-base-v1;1.75;180;canon;HumanBaseV1"
                 "Knight.glb;knight-reference;2.0;180;raw;Knight"
                 "RiggedFigure.gltf;reference-figure;1.8;0;raw;RiggedFigure")
        list(GET spec 0 _src)
        list(GET spec 1 _name)
        list(GET spec 2 _height)
        list(GET spec 3 _yaw)
        list(GET spec 4 _fit)
        list(GET spec 5 _stem)
        set(_extra)
        if(_fit STREQUAL "canon" OR _fit STREQUAL "shape")
            # ВИДИМЫЙ ПЕРСОНАЖ ПЕРЕКРАШИВАЕТСЯ ПО ЧАСТЯМ ТЕЛА. Собственные
            # материалы HumanBase — манекенные (M_Main оранжевый, M_Joints
            # фиолетовый): импортировать их честно значит нарисовать оранжевого
            # человека. --skin-palette кладёт те же пять цветов одежды, что
            # носит коробочное тело, — иначе доза DFN_BODY_BOXES сравнивала бы
            # два гардероба, а не два тела. Фикстуры импортёра (raw) остаются
            # со своими материалами: на них проверяется как раз чтение
            # baseColorFactor.
            set(_extra --fit-canon --skin-palette)
        endif()
        if(_fit STREQUAL "shape")
            list(APPEND _extra --reshape)
        endif()
        if(EXISTS "${DFN_CHAR_DIR}/${_src}")
            add_custom_command(
                OUTPUT "${DFN_CHAR_DIR}/${_stem}.dfo"
                COMMAND dfn_import_gltf "${DFN_CHAR_DIR}/${_src}"
                        --out "${DFN_CHAR_DIR}/${_stem}.dfo"
                        --name "${_name}" --height ${_height} --yaw ${_yaw} ${_extra}
                DEPENDS dfn_import_gltf "${DFN_CHAR_DIR}/${_src}"
                COMMENT "import ${_src} -> ${_stem}.dfo"
                VERBATIM)
            list(APPEND DFN_CHAR_OUTPUTS "${DFN_CHAR_DIR}/${_stem}.dfo")
        endif()
    endforeach()
    if(DFN_CHAR_OUTPUTS)
        add_custom_target(dfn_characters ALL DEPENDS ${DFN_CHAR_OUTPUTS})
    endif()
endif()

# ПРИЁМКА ПРОПОРЦИЙ — В ctest, С КОНТРОЛЬНОЙ РУКОЙ (правило 30). Зелёная
# строка на видимом персонаже одна ничего не значит: судья, который всех
# пропускает, неотличим от судьи, который ничего не мерит. Рядом стоит рыцарь
# KayKit — 2.11 головы, — и он ОБЯЗАН быть отвергнут.
if(TARGET dfn_human_scale AND TARGET dfn_characters)
    add_test(NAME human_scale_player
             COMMAND dfn_human_scale assets/objects/characters/HumanBase.dfo
                     --tolerance 0.05)
    add_test(NAME human_scale_chibi_rejected
             COMMAND dfn_human_scale assets/objects/characters/Knight.dfo
                     --tolerance 0.05)
    set_tests_properties(human_scale_chibi_rejected PROPERTIES WILL_FAIL TRUE)
    # ВТОРАЯ КОНТРОЛЬНАЯ РУКА, И ОНА БЛИЖЕ ПЕРВОЙ. Рыцарь отвергается за
    # стилизацию — такого не пропустил бы и судья, мерящий одни суставы.
    # HumanBaseV1 это ТА ЖЕ МОДЕЛЬ и те же суставы, что у видимого персонажа:
    # она проходит все девять суставных строк и обязана валиться РОВНО на
    # силуэте (рука 22 % длинная, плечо 62 % толстое, живота нет). Пока эта
    # строка красная, зелёная строка соседа ничего не значит.
    add_test(NAME human_scale_shape_v1_rejected
             COMMAND dfn_human_scale assets/objects/characters/HumanBaseV1.dfo
                     --tolerance 0.05)
    set_tests_properties(human_scale_shape_v1_rejected PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(human_scale_player human_scale_chibi_rejected
                         human_scale_shape_v1_rejected
                         PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# КУЗНИЦА ЯРУСОВ ФЛОРЫ (dfn_forge_tiers, 28.08): подрост и платы ковра в свою
# полку assets/objects/tiers. Третий бинарник по тому же доводу, что и второй —
# «не порти чужого» должно быть свойством того, куда программа дотягивается, а
# не аккуратности запускающего: страж внутри отказывает на любом имени без
# приставки "tier-".
add_executable(dfn_forge_tiers tools/forge_tiers.cpp)
target_link_libraries(dfn_forge_tiers PRIVATE dfn_render dfn_core)

# СТОРОЖ НЕПРИКОСНОВЕННОСТИ ПЕРВОЙ ИТЕРАЦИИ (волна деревьев-v2, 28.08). Печёт
# полку dfn_forge во временную папку и сверяет content_hash со строками
# assets/objects/trees/INDEX.md. Клятва «старые деревья не тронуты» становится
# проверяемой в ctest, а не обещанием в отчёте.
add_test(NAME trees_v1_frozen
         COMMAND python3 tools/check_trees_frozen.py $<TARGET_FILE:dfn_forge>
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ПРИБОР ЗАКОНА ПОСЕВА ЯРУСОВ (волна флоры 28.08). Четыре руки, у каждой
# КОНТРОЛЬ, который обязан провалиться: куртинность мерится индексом
# Кларка-Эванса, и контрольное плечо — тот же счёт растений равномерным
# рассевом — обязано дать R около 1.0. Без него «R = 0.33» было бы свойством
# метрики, а не посева. Прибор не читает ни одного .dfo и не зовёт ни одного
# бинарника: закон — чистая функция, и мерить его надо без полки.
add_test(NAME flora_sow_law
         COMMAND python3 tools/check_flora_sow.py
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

# ПРИБОР ФАСКИ (dfn_bevel_check, критерий К4 ТЗ материалов): доля выпуклых
# рёбер с фаской И счёт треугольников, которым она куплена, — на одном прогоне
# и по одному мешу. Оба плеча живут в одном двоичном файле (`--zero` — это
# «до»), потому что «до» из прошлого коммита мерило бы заодно и всё остальное,
# что за сутки поменялось в дереве.
add_executable(dfn_bevel_check tools/check_bevel.cpp)
target_link_libraries(dfn_bevel_check PRIVATE dfn_world dfn_core)

# ПРИБОР ЛЕСТНИЦЫ ДАЛЬНИХ ФОРМ (dfn_house_lod, И13): треугольники всех трёх
# ступеней одного чертежа рядом, одним прогоном и одним бинарником. «До» — это
# столбец «полная», а не прошлый коммит (правило 47).
add_executable(dfn_house_lod tools/check_house_lod.cpp)
target_link_libraries(dfn_house_lod PRIVATE dfn_world dfn_core)

# СТОРОЖ ПОЛНОЙ ФОРМЫ НА СТЕНДЕ ПОСТРОЕК (И13), ДВА ПЛЕЧА В ОДНОМ БИНАРНИКЕ.
# Положительное: четыре чертежа стенда — кирпич, сруб, камень, кровля — обязаны
# дать РОВНО тот хэш полных форм, что стоял до лестницы дальних форм. Лестница
# обязана быть ТОЛЬКО добавлением, и «полка не поменялась» проверяется
# ГЕОМЕТРИЕЙ, а не числом треугольников: равное число при разной геометрии —
# ровно тот случай, ради которого сторож и заведён. Отрицательное: тот же
# прогон с заведомо чужим хэшем обязан ПРОВАЛИТЬСЯ, иначе зелёный сторож
# доказывал бы только, что он всегда зелен.
#
# ЕСЛИ ЭТОТ РУКАВ ПОКРАСНЕЛ, а полную форму меняли НАМЕРЕННО (правка .dfh,
# правка печи) — прогоните dfn_house_lod на стенде, впишите новый хэш сюда и
# скажите об этом записью UPD. Молча подогнанный хэш обесценивает сторожа.
set(DFN_HOUSE_LOD_STAND
    assets/houses/l-house.dfh assets/houses/u-house.dfh
    assets/houses/log-replica.dfh assets/houses/stone-replica.dfh)
add_test(NAME house_lod_full_unchanged
         COMMAND dfn_house_lod ${DFN_HOUSE_LOD_STAND}
                 --require-full 9e4fd1f528f72aba
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME house_lod_full_unchanged_control
         COMMAND dfn_house_lod ${DFN_HOUSE_LOD_STAND}
                 --require-full 0000000000000000
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(house_lod_full_unchanged_control PROPERTIES WILL_FAIL TRUE)

# К4 НА СТЕНДЕ УБРАНСТВА, ДВА ПЛЕЧА В ОДНОМ БИНАРНИКЕ (правила 30 и 47).
# Положительное: полка убранства при штатной фаске обязана дать 100 % рёбер с
# фаской. Отрицательное: она же при нулевой дозе обязана ПРОВАЛИТЬ тот же
# порог — это измеренный дефект 3 ТЗ материалов («ни одной фаски во всём
# убранстве»), и без него «сто процентов» доказывало бы только, что прибор
# всегда зелен.
#
# ПОЛКА, А НЕ ГОРОД: К4 назван в ТЗ на ОБЪЕКТЕ УБРАНСТВА. Ряды кладки и кровли
# фаски не носят по бюджету (HOUSE_BEVEL_COURSES, довод и цена там же), и
# требовать с города ста процентов значило бы требовать вчетверо больший мир.
set(DFN_BEVEL_STAND
    assets/houses/furn-table.dfh assets/houses/furn-chair.dfh
    assets/houses/furn-chest.dfh assets/houses/furn-barrel.dfh
    assets/houses/furn-bed.dfh assets/houses/furn-cupboard.dfh
    assets/houses/furn-shelf.dfh assets/houses/furn-bench.dfh)
add_test(NAME bevel_k4_stand
         COMMAND dfn_bevel_check ${DFN_BEVEL_STAND} --require-k4 1.0
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME bevel_k4_stand_zero_dose
         COMMAND dfn_bevel_check ${DFN_BEVEL_STAND} --zero --require-k4 1.0
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(bevel_k4_stand_zero_dose PROPERTIES WILL_FAIL TRUE)

# ПРИБОР ПРИЁМКИ ПОВЕРХНОСТИ К1-К6 (tools/quality/measure_surface.py) СО
# СВОИМИ КОНТРОЛЬНЫМИ РУКАМИ. Прогоняются фикстуры, которые обязаны
# ПРОВАЛИТЬСЯ (ровная заливка — К1; два побитово одинаковых стула — К6; блик,
# выбитый в белый, — ветвь гладких), и те, что обязаны ПРОЙТИ (структура,
# полированное золото, кадр владельца, если он лежит на машине).
add_test(NAME quality_surface_selftest
         COMMAND python3 tools/quality/measure_surface.py --self-test
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# СТОРОЖ ПЕРЕПЕЧКИ ПОЛКИ УБРАНСТВА: габарит, бюджет тела, повторяемость.
# Отрицательное плечо — тот же сторож с нулевым пределом усадки: он обязан
# покраснеть на голове зверя и табурете, чью крайнюю точку держит острая
# вершина (срезанный угол короче несрезанного — это определение фаски).
add_test(NAME furn_bevel_gabarit
         COMMAND python3 tools/quality/check_furn_gabarit.py
                 $<TARGET_FILE:dfn_furn_objects>
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME furn_bevel_gabarit_control
         COMMAND python3 tools/quality/check_furn_gabarit.py
                 $<TARGET_FILE:dfn_furn_objects> --limit 0
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(furn_bevel_gabarit_control PROPERTIES WILL_FAIL TRUE)

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
                 assets/scenes/whiterun.relief docs/cities/WHITERUN_PLAN.json
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME paving_cornhall
         COMMAND python3 tools/check_paving.py
                 assets/scenes/cornhall.relief docs/cities/CORNHALL_PLAN.json
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

# ПОВТОРЯЕМОСТЬ БЕСПИЛОТНОГО КАДРА — В ctest, С ОБЕИМИ РУКАМИ (волна
# детерминизма тура, 29.08). Прибор приёмки, который никто не прогоняет
# автоматически, застаревает молча: волна материалов обнаружила дрейф тура не
# приборной проверкой, а тем, что её собственная пара хэшей не сошлась, и
# ЛЮБОЕ утверждение по городу до этого дня стояло на неизвестно чём.
#
# ТРИ ТЕСТА, А НЕ ОДИН. Стенд — регрессия: карта с коротким стримингом давала
# один md5 и ДО волны, значит починка не имеет права её сдвинуть. Город — сам
# предмет: там дрейф и жил. Контроль — DFN_UNPIN=all возвращает прогон на
# стенные часы и ОБЯЗАН краснеть; зелёный тест без красного контроля не
# отличим от теста, который ничего не мерит (правило 30, та же пара, что у
# bevel_k4_stand / _zero_dose выше).
#
# ЦЕНА ЧЕСТНО: каждый тест — ДВА полных прогона игры с окном и загрузкой
# карты. Стенд ~40 с, город ~90 с на незагруженной машине. Поэтому у них свой
# ярлык unattended_frames: `ctest -LE unattended_frames` отсекает всю тройку
# одной строкой, когда нужен быстрый круг.
add_test(NAME frame_determinism_stand
         COMMAND python3 tools/check_frame_determinism.py --recipe stand
                 --app $<TARGET_FILE:dfn_app> --out ${CMAKE_BINARY_DIR}/frame_det
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME frame_determinism_city
         COMMAND python3 tools/check_frame_determinism.py --recipe city
                 --app $<TARGET_FILE:dfn_app> --out ${CMAKE_BINARY_DIR}/frame_det
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
#
# ПОЧЕМУ КОНТРОЛЬ НЕ НА WILL_FAIL, хотя рядом (bevel_k4_stand_zero_dose) он
# именно на нём. WILL_FAIL засчитывает ЛЮБОЙ ненулевой выход: пропавший
# dfn_app, пустой каталог кадров, упавший питон — всё это «красное плечо
# сработало». То есть контроль, который обязан ловить отсутствие эффекта, сам
# не отличает отсутствия ПРОГОНА. Здесь красное плечо выражено как
# УТВЕРЖДЕНИЕ О РАСХОЖДЕНИИ (--expect differ): прибор возвращает 0 только
# когда кадры действительно разошлись, 1 — когда сошлись (пин остался на
# месте), 2 — когда мерить было нечем. Все три исхода читаются верно, и тест
# краснеет на каждом, кроме настоящего.
add_test(NAME frame_determinism_city_wallclock
         COMMAND python3 tools/check_frame_determinism.py --recipe city
                 --unpin all --expect differ
                 --app $<TARGET_FILE:dfn_app> --out ${CMAKE_BINARY_DIR}/frame_det
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(frame_determinism_stand frame_determinism_city
                     frame_determinism_city_wallclock
                     PROPERTIES LABELS unattended_frames TIMEOUT 900
                     RUN_SERIAL TRUE)
