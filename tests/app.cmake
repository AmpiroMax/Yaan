#
# Created: 10:08:2026 - 19:24:11
# Last updated: 27:08:2026 - 12:58:00
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
#
# UPD:
# - 10:08:2026 - 19:24:11: Created -- state capture round-trip and the compass,
#                          each with its control.
# - 13:08:2026 - 23:06:40: app_menu и app_hud_screen в ctest (зона ui гоняла их руками). Тест ленты доказывает то, чего кадр не может: компас, едущий не в ту сторону, выглядит правильным на ЛЮБОМ снимке, поэтому знак проверяется поворотом в обе стороны.
# - 14:08:2026 - 18:57:03: app_editor_hud — раскладка редакторского блока. Он мерит то, чего не мерил никто: два оверлея делили верхний левый угол и печатались друг сквозь друга, а проверить это было нечем, потому что блок собирался прямо в App.cpp (окно, не тестируется). Файл порезан ведущим зоне editor ровно на эту регистрацию.

# - 14:08:2026 - 19:22:10: app_controls — таблица привязок клавиш; Controls.cpp добавлен и в app_menu (страница управления рисуется из той же таблицы). Файл порезан ведущим зоне editor на эту регистрацию, как и в прошлый раз.
# - 17:08:2026 - 16:53:31: app_build_tool — рука строителя: зелёное/красное решает СУДЬЯ, а не
# - 17:08:2026 - 19:18:24: app_editor_palette — модель меню объектов (фасеты, фильтр, избранное).
# - 17:08:2026 - 19:25:28: пути меню объектов — engine/editor (переезд под исключение про ImGui).
#   двойник правил в редакторе; тест держит именно это свойство.
# - 17:08:2026 - 19:38:10: app_editor_brush — кисть рельефа и посадка растительности (зона
# - 17:08:2026 - 21:06:32: оси и семейства меню объектов в наборе рукавов.
#   кистей). Две проверки, которых кадр сделать не может: пустой слой правок не
#   двигает землю НИ НА БИТ (вместе с рукой, которая её ДВИГАЕТ, иначе
#   утверждение непроверяемо), и отказ посадить дерево приходит из
#   world::check_scene, а не из второй копии правил в редакторе.
# - 17:08:2026 - 22:01:29: app_house_scenario — РУКАВ ПОСТРОЙКИ ДОМА БЕЗ ОКНА (заказ 17.08
#   п.3). Он держит то, чего кадр держать не может: кадр готового дома
#   доказывает, что дом СУЩЕСТВУЕТ, а вопрос был про ПОСЛЕДОВАТЕЛЬНОСТЬ —
#   пускает ли судья каждый промежуточный шаг. Три числа: дом демки судится
#   начисто (контроль модели земли), без группы рука получает 21 отказ из 46,
#   с группой по одной детали встают 38 из 46. Восемь оставшихся — КОЛЬЦО
#   конёк-фронтон, находка, а не недоделка.
# - 18:08:2026 - 00:07:07: app_editor_camera — ДОХОДИТ ЛИ МЫШЬ ДО КАМЕРЫ. Ровно
#   тот вопрос, который три захода подряд разбирал человек за игрой, потому что
#   прибора на него не было ни одного. И РЯДОМ — ПОЧИНЕН app_editor_palette: он
#   НЕ СОБИРАЛСЯ (EditorPaletteFamily.cpp тянет imgui.h, которого цель не
#   видит), а ctest брал прежний двоичный файл и рапортовал «прошло» — зелёный
#   отчёт о коде, которого в нём нет. Файл семейств из рукава убран: в нём одна
#   отрисовка, ни одной функции модели, и рукав не звал из него ничего.
#   Полный прогон после починки: 78 из 79. Красный один — render_proc_flora
#   (ива Reduced, 1 утверждение из 1 312 895), он был красным и до этой правки.
# - 18:08:2026 - 01:05:34: EditorBrushOutlineTests.cpp — на ТОТ ЖЕ рукав app_editor_brush.
#   Контур зоны кисти это тот же предмет и та же кисть; разрез — только правило 21
#   (800 строк). Отдельный исполняемый дал бы гонять половинки одного утверждения
#   порознь, а так теряется парное плечо.
# - 18:08:2026 - 01:07:17: EditorPaletteThumbTests.cpp — ПРЕДПРОСМОТР в рукаве меню объектов.
#   Кадр доказывает, что плитки нарисованы, и не может показать ровно три вещи,
#   которыми эта работа бывает тихо неправа: что связь 0.25 м и брус 4.6 м
#   кадрируются ОДНИМ правилом (побайтово один и тот же снимок), что деталь,
#   показанная в трёх местах, выпечена ОДИН раз, и что прокрутка всей полки не
#   набирает 356 МБ текстур. Всё три — числа здесь.
# - 18:08:2026 - 12:08:40: app_editor_toolbox — ЯЩИК ИНСТРУМЕНТОВ. Четыре свойства, которых
#   не видит ни один кадр и которые до сегодня не держало ничего: щелчок по
#   настройкам ЧУЖОГО инструмента не меняет руку; щелчок по иконке активного
#   кладёт его и гасит превью; R переключает режим указателя в обе стороны при
#   любых открытых окнах; щелчок дальше потолка не делает НИЧЕГО, ближе —
#   делает. Плюс то, ради чего всё затевалось: нажатие доходит РОВНО до одного
#   инструмента, и это счётчик у каждого, а не утверждение про поле.
# - 18:08:2026 - 13:08:07: EditorPathTests.cpp и EditorStrokeTests.cpp на рукав
#   app_editor_brush. Первый держит числом то, ради чего затевались тропы:
#   диагональ рисуется диагональю (0.12 м против 0.49 м у клеточной руки).
#   Второй — то, чего не покажет ни один кадр: за штрих земля обновилась
#   БОЛЬШЕ ОДНОГО РАЗА, и рядом стоит контроль на один кадр и рука прежнего
#   поведения.
#   Цель линкует ТОЛЬКО EditorToolbox.cpp и EditorToolIcons.cpp: в них нет ни
#   ImGui, ни окна — решение отделено от рисования именно затем, чтобы этот
#   рукав существовал (правило 3).
# - 18:08:2026 - 17:16:56: app_editor_history — отмена и повтор. Цель линкует ТОЛЬКО
#   EditorHistory.cpp: в истории нет ни модели, ни окна, и это сделано нарочно —
#   она хранит снимки строками и потому проверяется без мира (правило 3).
# - 18:08:2026 - 16:59:18: ActionRoutesTests.cpp + AppActions.cpp в app_controls — слой 1
#   разбора App.cpp (docs/PLAN_APP_DECOMPOSITION.md). Рукав держит то, чего до
#   сегодня не держало ничто: набор диспетчеров ПОЛОН и ОДНОЗНАЧЕН, и НИ ОДНА
#   клавиша не проходит сквозь открытое окно чата. Последнее до правки было не
#   свойством, а восемнадцатью одинаковыми условиями, написанными от руки.
#   AppActions.cpp линкуется, а AppInput.cpp нет: в нём определены методы App,
#   то есть он тянет App.cpp, то есть окно. Ровно затем таблица и отделена.
# - 18:08:2026 - 17:32:10: app_doors — ТАБЛИЦА ДВЕРЕЙ (слой 2). Держит её замкнутой с
#   ОБЕИХ сторон: имя, читаемое в зоне app, обязано иметь строку, а строка —
#   обязана где-то читаться; плюс «беспилотный прогон» стал колонкой и
#   проверяется по каждой двери ПОРОЗНЬ (13 из 58), а не выражением из
#   тринадцати слагаемых, в которое дверь дважды заметали правкой.
#   AppDoors.cpp добавлен и в app_debug_overlay/app_menu/app_editor_hud/
#   app_hud_screen: эти файлы читают двери, значит цели обязаны его линковать.
# - 18:08:2026 - 17:35:04: HudComposeTests.cpp + AppHud.cpp на app_hud_screen — слой 3.
#   Держит СБОРКУ кадра: подпись под прицелом (пустая рука, приговор судьи,
#   группа, перехват указателя панелью), чистый кадр по DFN_HUD=0 (НИ ОДНОГО
#   пикселя, а не «невидимый слой»), и то, ради чего слой затевался —
#   добавление редакторского блока НЕ МЕНЯЕТ НИ ОДНОГО ПИКСЕЛЯ, принадлежащего
#   отладочному выводу. Этот отказ (вывод в (3,3) и блок в (4,4)) три дня ловил
#   человек: порознь оба модуля были правы, неправа была только сборка.
# - 18:08:2026 - 17:36:58: app_after_frame — слой 4. Четыре решения хвоста кадра, и у
#   каждого своя история отказа: затвор тура (две руки одного бинарника
#   расходились на 17.4% пикселей, пока его не было), отсчёт до закрытия (.txt
#   без .png рядом), период телеметрии (счётное время, а не кадры) и приговор
#   восстановлению (порог стоял на 3D-расстоянии и кричал бы на каждом).
#   Цель не линкует НИ ОДНОГО .cpp зоны: решения лежат заголовком (inline).
# - 18:08:2026 - 18:02:11: app_editor_house — ТРИ ИНСТРУМЕНТА ПОСТРОЙКИ поверх HouseGraph.
#   Держит то, чего кадр не держит: число штрихов отвеса отличает вершину в
#   воздухе от вершины на земле (кадр показывает и то и другое одинаково —
#   высота и дальность на экране неразличимы), нормаль черновика
#   переворачивается порядком обхода ДО подтверждения, отказ на удаление
#   называет держателей, зажим длины садится на 5.00 м вверх и на 2.00 м вниз
#   при руке на 3.50 м, и отмена возвращает координату, а не только счётчик.
# - 27:08:2026 - 02:25:00: app_png — свой читатель .png (герб и знак студии в меню);
#   MenuArt.cpp и PngImage.cpp дописаны в app_menu: раскладка строк, по которой
#   ходит и мышь, меряет ШИРИНУ КРУПНОГО текста, то есть тянет за собой шрифт.
# - 27:08:2026 - 12:58:00: app_menu_emblem — объёмный герб главного меню. Свой
#   рукав, а не случай в app_menu: там меряется РАЗМЕТКА ХОЛСТА, здесь —
#   арифметика в осях камеры и свет кадра, то есть другой предмет и другие
#   зависимости. Держит ровно то, чего кадр приёмки держать не может: кадр
#   показывает ОДНУ фазу качания, а бок доски срезается ближней плоскостью в
#   КРАЙНЕЙ, и у каждого утверждения тут своя контрольная рука (заведомо
#   плохая раскладка, вторая несоизмеримая ось, другое соотношение сторон).

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
