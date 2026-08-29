#
# File: tests/app.cmake
#
# Responsibility:
# - Registers the app (lead) zone's test executables. The app is an EXECUTABLE,
#   not a library, so its testable pieces are compiled into the test binary
#   directly rather than linked -- App.cpp itself owns a window and is not
#   testable, but DebugOverlay.cpp is pure data in / data out and is.
#
# Dependencies:
# - Uses: add_dfn_test() from tests/CMakeLists.txt; dfn_render, dfn_core.
# - Used by: tests/CMakeLists.txt (conditional include), ctest.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. LEAD-owned.

if(TARGET dfn_render AND TARGET dfn_core)
    add_dfn_test(app_debug_overlay app/DebugOverlayTests.cpp dfn_render dfn_core)
    target_sources(app_debug_overlay PRIVATE
        # AppDoors.cpp: дверь читается ТОЛЬКО через таблицу (слой 2), а эти
        # файлы её читают — значит цель обязана её линковать.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppDoors.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    add_dfn_test(app_menu app/MenuTests.cpp dfn_render dfn_core)
    target_sources(app_menu PRIVATE
        # AppDoors.cpp: дверь читается ТОЛЬКО через таблицу (слой 2), а эти
        # файлы её читают — значит цель обязана её линковать.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppDoors.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Controls.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Menu.cpp
        # MenuArt/PngImage: страницы меню рисуются крупным шрифтом и гербом
        # (27.08), и раскладка строк — та же функция, что читает мышь.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/MenuArt.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/PngImage.cpp
        # IntroVideo: страница заставки — предзаписанное видео (заказ 27.08),
        # и рукав меню его ЧИТАЕТ: длительность интро и есть длительность
        # заставки, а её теперь можно проверить без окна.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/IntroVideo.cpp
        # UiFont: интерфейс перешёл на испечённую антикву (27.08), и раскладка
        # страниц меряет ЕЁ метрики -- рукав обязан линковать тот же шрифт.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/UiFont.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    # ОБЪЁМНЫЙ ГЕРБ ГЛАВНОГО МЕНЮ. Свой рукав, а не случай в app_menu: там
    # меряется РАЗМЕТКА ХОЛСТА, а здесь — арифметика в осях камеры и свет
    # кадра, то есть другой предмет и другие зависимости (dfn_render).
    # Держит ровно то, чего кадр приёмки держать не может: кадр показывает
    # ОДНУ фазу качания, а бок доски срезается ближней плоскостью в КРАЙНЕЙ.
    add_dfn_test(app_menu_emblem app/MenuEmblemTests.cpp dfn_render dfn_core)
    target_sources(app_menu_emblem PRIVATE
        # AppDoors.cpp: фаза качания читается через таблицу дверей (слой 2).
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppDoors.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/MenuEmblem.cpp)

    # ЧТЕНИЕ .png — ОТДЕЛЬНЫЙ РУКАВ, потому что предмет отдельный: разжатие и
    # расфильтровка не про меню, а про формат. Ожидаемые пиксели получены
    # НЕЗАВИСИМОЙ реализацией (python zlib), а не этой же — иначе рукав
    # подтверждал бы сам себя.
    add_dfn_test(app_png app/PngImageTests.cpp)
    target_sources(app_png PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/PngImage.cpp)

    # THE EDITOR'S OVERLAY BLOCK, and it is a layout suite rather than a text
    # one. The defect it was written for -- the readout and the editor banner
    # printing through each other in the top-left corner -- was invisible to
    # every test here and obvious to anyone who launched the game, because the
    # block was composed inline in App.cpp, which owns a window and cannot be
    # instantiated. Extracting it is what made the overlap measurable.
    add_dfn_test(app_editor_hud app/EditorHudTests.cpp dfn_render dfn_core)
    target_sources(app_editor_hud PRIVATE
        # AppDoors.cpp: дверь читается ТОЛЬКО через таблицу (слой 2), а эти
        # файлы её читают — значит цель обязана её линковать.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppDoors.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/EditorHud.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    # МЕНЮ ОБЪЕКТОВ. Полка 2411 деталей: рукав держит то, чего кадр не покажет —
    # что имя РАЗОБРАНО, а не угадано, что счётчик на фишке значит «сколько
    # останется, если нажму», и что размер из имени сходится с меркой меша.
    add_dfn_test(app_editor_palette app/EditorPaletteTests.cpp dfn_world dfn_render dfn_core)
    target_sources(app_editor_palette PRIVATE ${CMAKE_SOURCE_DIR}/tests/app/EditorPaletteAxesTests.cpp)
    # ПРЕДПРОСМОТР ДЕТАЛИ. Он попадает в рукав ровно потому, что нарисован на
    # ЦПУ: ракурс, кадрирование, затенение, бюджет кадра и потолок кэша — всё
    # это числа, которые читаются обратно ИЗ ПИКСЕЛЕЙ, без окна и без ImGui
    # (правило 3). Офскрин-проход дал бы то же меню и ни одной проверки.
    target_sources(app_editor_palette PRIVATE ${CMAKE_SOURCE_DIR}/tests/app/EditorPaletteThumbTests.cpp)
    target_sources(app_editor_palette PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorPalette.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorPaletteAxes.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorPaletteState.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorPaletteThumb.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorPaletteThumbCache.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/BuildTool.cpp)
    # EditorPaletteFamily.cpp СЮДА НЕ ВХОДИТ, и это не забывчивость: в нём одна
    # только отрисовка ImGui, ни одной функции модели, и рукав не зовёт из него
    # ничего. Пока он был в списке, цель НЕ СОБИРАЛАСЬ (imgui.h не виден), а
    # ctest брал прежний двоичный файл и рапортовал «прошло» — зелёный отчёт о
    # коде, которого в нём нет. Появится в нём решение, а не рисование, — вносить
    # вместе с dfn_imgui и EditorUi.cpp, иначе не слинкуется.

    # THE BUILD HAND. Worth its own suite for the same reason EditorHud is:
    # the decision lives in a module rather than in App.cpp, so an instrument
    # can see it. The property it holds — the ghost is coloured by ITS OWN
    # findings — is invisible in a screenshot and obvious in a test.
    add_dfn_test(app_build_tool app/BuildToolTests.cpp dfn_world dfn_render dfn_core)
    target_sources(app_build_tool PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/BuildTool.cpp)

    # THE EDITOR'S HAND ON THE GROUND. It links dfn_world for the same reason
    # the build hand's suite does — the properties worth holding are about the
    # JUDGE and about the WORLD's pass stack, and neither can be faked here.
    # Two of its claims are invisible in any screenshot: that an empty edit
    # layer moves the terrain by nothing at all (with the arm that DOES move
    # it, or the claim is untestable), and that a plant's refusal comes from
    # world::check_scene rather than from a second copy of its rules.
    add_dfn_test(app_editor_brush app/EditorBrushTests.cpp dfn_world dfn_render dfn_core)
    # THE OUTLINE OF THE ZONE rides on the SAME target rather than a new one:
    # it is the same subject and the same brush, and the split is Rule 21's 800
    # line limit and nothing else. A second executable would let the two halves
    # of one claim be run separately, which is how a paired arm goes missing.
    target_sources(app_editor_brush PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/app/EditorBrushOutlineTests.cpp
        # ТРОПА И ПОКАЗ ЗЕМЛИ — на тот же рукав и по той же причине, что и
        # контур зоны: это та же рука на той же земле, а отдельный исполняемый
        # дал бы гонять половинки одного утверждения порознь.
        ${CMAKE_SOURCE_DIR}/tests/app/EditorPathTests.cpp
        ${CMAKE_SOURCE_DIR}/tests/app/EditorStrokeTests.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/BuildTool.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/EditorPlant.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorBrush.cpp)

    # THE BINDING TABLE, and this suite is the reason the controls screen is
    # worth more than a paragraph of documentation: it holds the table TOTAL and
    # UNAMBIGUOUS, so a key added to App.cpp without a row cannot be dispatched
    # and a row without a description cannot be drawn.
    # THE HOUSE, BUILT PART BY PART. Links dfn_world for the judge and
    # dfn_render for the ruler — the same two the build hand uses, because a
    # sleeve with its own rules or its own tape measure would be judging a
    # different world than the editor does.
    add_dfn_test(app_house_scenario app/HouseScenarioTests.cpp dfn_world dfn_render dfn_core)
    target_sources(app_house_scenario PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/BuildTool.cpp)

    # ДОХОДИТ ЛИ МЫШЬ ДО КАМЕРЫ. Прибора на это не было, и потому три захода
    # подряд отказ ловил человек за игрой, а не проверка. Гейт вынесен из
    # кадрового цикла App.cpp выражением в EditorCamera.h ровно затем, чтобы
    # сюда дотянуться: App.cpp держит окно и потому не тестируется.
    add_dfn_test(app_editor_camera app/EditorCameraTests.cpp dfn_render dfn_core)
    target_sources(app_editor_camera PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/EditorCamera.cpp)

    add_dfn_test(app_editor_toolbox app/EditorToolboxTests.cpp dfn_world dfn_render dfn_core)
    target_sources(app_editor_toolbox PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorToolbox.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorToolIcons.cpp)

    # ОТМЕНА, КОТОРАЯ НЕ ВРЁТ. Цель линкует ТОЛЬКО EditorHistory.cpp: в истории
    # нет ни модели, ни окна, и это сделано нарочно — она хранит снимки строками
    # и потому проверяется без мира (правило 3).
    add_dfn_test(app_editor_history app/EditorHistoryTests.cpp dfn_core)
    target_sources(app_editor_history PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorHistory.cpp)

    # ТРИ ИНСТРУМЕНТА ПОСТРОЙКИ. Цель линкует EditorToolHouse.cpp и НЕ линкует
    # EditorToolHouseUi.cpp: во втором живёт ImGui, а вместе с ним и контекст
    # окна. Разрез проведён ровно затем, чтобы эти вопросы можно было задать
    # вообще: куда смотрит нормаль ДО подтверждения, кто держит вершину, которую
    # не дали удалить, на сколько зажалась длина и чем отвес отличает вершину в
    # воздухе от вершины на земле (правило 3).
    add_dfn_test(app_editor_house app/EditorToolHouseTests.cpp dfn_world dfn_render dfn_core)
    target_sources(app_editor_house PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorToolHouse.cpp
        ${CMAKE_SOURCE_DIR}/engine/editor/sources/EditorHistory.cpp
        # НУЛЕВАЯ ПАНЕЛЬ вместо ImGui-шной: таблица виртуальных функций требует
        # тело draw_settings(), а настоящее тело тянет EditorUi и через него
        # окно. Правило 3 своими словами — нулевой бэкенд, а не заглушка.
        app/EditorToolHouseNullPanels.cpp)

    # ХВОСТ КАДРА. Решения, которые он принимает, лежат заголовком (inline), и
    # эта цель НИЧЕГО из зоны app не линкует — ни одного .cpp. Так и задумано:
    # затвор тура, отсчёт до закрытия, период телеметрии и приговор
    # восстановлению были числами внутри кадрового цикла, а цикл держит окно.
    add_dfn_test(app_after_frame app/AfterFrameTests.cpp dfn_core)

    # ДВЕРИ. Своя цель, а не довесок к app_controls: клавиша и дверь — разные
    # предметы (одна принадлежит человеку за клавиатурой, другая рецепту на
    # диске), и рукав дверей линкует ТОЛЬКО AppDoors.cpp — в нём нет ни App, ни
    # окна, ни единой чужой зависимости, и это единственная причина, по которой
    # «какие вообще есть двери» стало вопросом с проверяемым ответом.
    add_dfn_test(app_doors app/DoorsTests.cpp dfn_core)
    target_sources(app_doors PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppDoors.cpp)

    # ПРИЦЕЛ ДВЕРИ. Своя цель, и линкует она ТОЛЬКО DoorAim.cpp — ни App, ни
    # окна, ни физики: геометрия «целюсь ли я в это полотно» отделена от них
    # ровно затем, чтобы её можно было провалить нарочно. Отвергнутый образец
    # настоящий (поза, в которой прежний прицел зажигал «Выйти» без взгляда), а
    # не синтетический — правило 30.
    add_dfn_test(app_door_aim app/DoorAimTests.cpp dfn_core)
    target_sources(app_door_aim PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DoorAim.cpp)

    # ПОЗЫ МЕБЕЛИ: прицел предмета и МЕТА, выведенная из его геометрии. Та же
    # выгородка, что у прицела двери, и по той же причине — правило «самая
    # широкая горизонтальная площадка» обязано проверяться на треугольниках
    # НАСТОЯЩИХ чертежей полки, без окна, без физики и без карты.
    add_dfn_test(app_seat_aim app/SeatAimTests.cpp dfn_world dfn_core)
    target_sources(app_seat_aim PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/SeatAim.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/FurnitureSeats.cpp)

    # ФИЗИЧЕСКИЕ СВОЙСТВА ПРЕДМЕТА (зона big-grab). Та же выгородка, что у
    # прицела двери: масса считается из треугольников полки, поэтому цель
    # линкует чтение реестра и НИЧЕГО из окна и физики. Отвергаемый случай
    # настоящий — двухсоткилограммовый сундук из записки №4 ресёрчера.
    add_dfn_test(app_prop_physics app/PropPhysicsTests.cpp dfn_render dfn_core)
    target_sources(app_prop_physics PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/PropPhysics.cpp)

    # НАСТРОЙКИ. Цель линкует ТОЛЬКО AppSettings.cpp: разбор текста отделён от
    # файла и от окна, и это единственная причина, по которой вынос из App.cpp
    # имеет смысл — раньше проверить его было нечем.
    add_dfn_test(app_settings app/SettingsTests.cpp dfn_render dfn_core)
    target_sources(app_settings PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppSettings.cpp)

    add_dfn_test(app_controls app/ControlsTests.cpp dfn_render dfn_core)
    # ТАБЛИЦА ДИСПЕТЧЕРИЗАЦИИ — на тот же рукав и по той же причине, по которой
    # контур кисти сидит на рукаве кисти: это тот же предмет — клавиша, — только
    # с другого конца. Controls отвечает «какая клавиша», AppActions — «кто
    # отвечает». Порознь они проходят по отдельности и вместе не значат ничего:
    # действие с привязкой и без обработчика молчит, обработчик без привязки
    # недостижим.
    #
    # AppActions.cpp ЛИНКУЕТСЯ, А AppInput.cpp НЕТ, и это не забывчивость: в
    # AppInput.cpp определены методы App, то есть он тянет за собой App.cpp,
    # то есть окно. Ровно затем таблица и вынесена в отдельную единицу
    # трансляции, чтобы у рукава была её половина без окна (правило 3).
    target_sources(app_controls PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/app/ActionRoutesTests.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppActions.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Controls.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)

    # The HUD's tests earn their place by proving what a FRAME CANNOT: a compass
    # ribbon running the wrong way looks right in any single screenshot, so the
    # sign is only provable by turning both ways and watching the marks move.
    add_dfn_test(app_hud_screen app/HudScreenTests.cpp dfn_render dfn_core)
    # СБОРКА КАДРА — на тот же рукав (слой 3 разбора App.cpp). Отдельные куски
    # оверлея уже держат свои наборы; здесь держится то, что происходит, когда
    # их складывают вместе, — а именно там и жил отказ, который ловил человек:
    # отладочный вывод и редакторский блок печатались друг сквозь друга, будучи
    # порознь совершенно правы.
    target_sources(app_hud_screen PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/app/HudComposeTests.cpp
        # UiFont/PngImage: надписи мира (подсказка прицела, лента компаса,
        # плашка инструмента) с 27.08 рисуются испечённой антиквой, а не
        # блочным шрифтом — рукав обязан линковать её и её читатель .png.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/UiFont.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/PngImage.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppHud.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/EditorHud.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/ChatOverlay.cpp
        # AppDoors.cpp: дверь читается ТОЛЬКО через таблицу (слой 2), а эти
        # файлы её читают — значит цель обязана её линковать.
        ${CMAKE_SOURCE_DIR}/engine/app/sources/AppDoors.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/HudScreen.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/DebugOverlay.cpp
        ${CMAKE_SOURCE_DIR}/engine/app/sources/Localization.cpp)
endif()
