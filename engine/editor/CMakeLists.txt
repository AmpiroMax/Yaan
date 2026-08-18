#
# Created: 17:08:2026 - 19:22:32
# Last updated: 18:08:2026 - 13:08:07
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

# - 17:08:2026 - 20:06:29: EditorBrush.cpp и EditorBrushView.cpp — кисть земли и её панель
#                          (зона кистей рельефа). Механика кисти живёт ЗДЕСЬ, а не в
#                          engine/app, потому что панель, которая её правит, обязана
#                          быть в engine/editor (Dear ImGui разрешён только тут), а
#                          editor не имеет права смотреть вверх на app. Судейская
#                          половина посадки осталась в app/EditorPlant.cpp: ей нужен
#                          перевод вердикта из BuildTool. И dfn_world стал PUBLIC —
#                          EditorBrush.h называет его типы в СВОЁМ интерфейсе.

# - 17:08:2026 - 21:06:32: EditorPaletteAxes/Family — выбор детали по СВОЙСТВАМ внутри семейства
#   (заказ 17.08: «основное разбиение по семейству, свойства для каждого материала»).

# - 17:08:2026 - 22:01:29: EditorPropsView.cpp — колонка свойств выбранной расстановки
#   (заказ 17.08: «справа должно окно рисоваться с его характеристиками, я должен
#   уметь их менять»). Режим «Выбор» до сих пор не показывал НИЧЕГО.

# - 18:08:2026 - 01:00:57: EditorPaletteThumb/ThumbCache — предпросмотр детали (заказ 18.08:
#   «нужно чтобы там на белом фоне сам объект был нарисован»). Рисуется на ЦПУ:
#   офскрин-проход потребовал бы цели рендера, прохода, программы и сабмита —
#   четырёх вещей из engine/platform, ни одну из которых нельзя завести в тесте,
#   а деталь это пара тысяч плоскозатенённых треугольников. dfn_render стал
#   PUBLIC: заголовок называет его типы в СВОЁМ интерфейсе, и крючок выпечки
#   собирается в engine/app ровно по ним.

# - 18:08:2026 - 12:08:10: ИНСТРУМЕНТЫ КАК КЛАССЫ (docs/AUDIT_EDITOR_TOOLS.md): EditorToolbox
#   (единственный владелец «что в руке», без ImGui — его гоняет рукав),
#   EditorToolIcons (значки рисуются кодом, одинакового размера по построению),
#   EditorToolbar (двойные кнопки и шестерёнка) и EditorToolsGround/Object —
#   пять инструментов. Рисование отделено от решения намеренно: решение обязано
#   проверяться без окна (правило 3), поэтому оно в двух файлах без imgui.
# - 18:08:2026 - 13:08:07: EditorToolPath.cpp — ШЕСТОЙ ИНСТРУМЕНТ, тропа кривой (заказ 18.08).
#   Сама кривая, её поперечник и раскладка в отсчёты лежат в engine/world
#   (ReliefLayer): их обязан уметь читатель файла, а он там. Здесь только рука.

add_library(dfn_editor STATIC
    sources/EditorBrush.cpp
    sources/EditorToolbar.cpp
    sources/EditorToolbox.cpp
    sources/EditorToolIcons.cpp
    sources/EditorToolsGround.cpp
    sources/EditorToolsObject.cpp
    sources/EditorToolPath.cpp
    sources/EditorBrushView.cpp
    sources/EditorUi.cpp
    sources/EditorPalette.cpp
    sources/EditorPaletteAxes.cpp
    sources/EditorPaletteFamily.cpp
    sources/EditorPaletteState.cpp
    sources/EditorPaletteThumb.cpp
    sources/EditorPaletteThumbCache.cpp
    sources/EditorPaletteView.cpp
    sources/EditorPropsView.cpp)
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
