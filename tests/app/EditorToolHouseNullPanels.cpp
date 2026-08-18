/*
Created: 18:08:2026 - 18:02:11
Last updated: 18:08:2026 - 18:02:11
Module: tests/app
File: tests/app/EditorToolHouseNullPanels.cpp

Responsibility:
- НУЛЕВАЯ ПАНЕЛЬ трёх инструментов постройки: draw_settings(), который ничего не
  рисует. Ровно то, что правило 3 называет нулевым бэкендом — не заглушка ради
  компоновки, а РАБОЧИЙ режим «без окна», в котором инструмент делает всё,
  кроме показа.

WHY THIS FILE EXISTS, и почему это не обход правила 32. Настоящие панели живут
в engine/editor/sources/EditorToolHouseUi.cpp и написаны на Dear ImGui. Взять их
в рукав нельзя: за ImGui тянется EditorUi (перевод строк), за ним палитра, за
ней выпечка предпросмотра — то есть окно. Но таблица виртуальных функций класса
требует ВСЕ его методы, даже те, которых проверка не зовёт, поэтому без тела
draw_settings() рукав не собирается вовсе.

ОДНО ОПРЕДЕЛЕНИЕ НА ДВОИЧНЫЙ ФАЙЛ, а не одно на репозиторий: в игре
компонуется файл с ImGui, в проверке — этот. Два тела одной функции в одной
программе не встречаются никогда, и перепутать их нельзя — они лежат в разных
целях CMake.

Dependencies:
- Uses: EditorToolHouse.h. Ни ImGui, ни EditorUi.
- Used by: цель app_editor_house.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НИ ОДНОГО РЕШЕНИЯ ЗДЕСЬ И НИКОГДА. Если в панели появится хоть одно
  вычисление, оно обязано переехать в EditorToolHouse.cpp — иначе оно окажется
  ровно в том месте, до которого проверка не дотягивается, а этот файл написан
  затем, чтобы такого места не было.
*/
/*
UPD:
- 18:08:2026 - 18:02:11: Создан вместе с рукавом app_editor_house.
*/

#include "engine/editor/sources/EditorToolHouse.h"

namespace dfn::app {

void HouseVertexTool::draw_settings() {}
void HouseLineTool::draw_settings() {}
void HouseSurfaceTool::draw_settings() {}

} // namespace dfn::app
