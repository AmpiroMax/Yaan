/*
Created: 18:08:2026 - 13:08:07
Last updated: 18:08:2026 - 13:08:07
Module: engine/editor
File: engine/editor/sources/EditorToolPath.h

Responsibility:
- ТРОПА КАК ИНСТРУМЕНТ: человек ставит точки, между ними идёт дуга, износ
  ложится вдоль неё. Постановка, правка и удаление точек — здесь, и настройки
  тропы тоже здесь, потому что они её и ничьи больше.

Key items:
- PathTool: шестой инструмент по IEditorTool. Ни строки о нём не живёт снаружи.

WHY THIS EXISTS (пользователь, 18.08.2026): «добавь в покраску установку
тропинок, только тропинки надо уметь вести не по квадратам, как сейчас песок, а
по любым направлениям, между любыми точками. Возможно тут потребуется
реализация через кривые безье, тоже самое касается любых тропинок.»

ПОЧЕМУ ЭТО НЕ РЕЖИМ КИСТИ ПОВЕРХНОСТИ, а отдельный инструмент. Кисть — это
МАЗОК: центр, радиус, спад по кругу, и результат на решётке всегда лесенка,
потому что класс поверхности — перечисление, а перечисление не
интерполируется (замерено: край уходит от прямой на 0.49 м, то есть на
пол-ячейки). Тропа — это ЛИНИЯ: у неё есть точки, порядок, ширина и два конца,
и её износ — число, которое земля интерполирует сама (замерено: 0.12 м). Это
разные предметы с разным состоянием, и один инструмент с флагом «а сейчас я
тропа» был бы ровно тем спагетти, которое разбирал docs/AUDIT_EDITOR_TOOLS.md.

ЧТО ЛЕЖИТ ГДЕ, и шов не выбран, а вынужден: САМА кривая, её поперечник и
раскладка в отсчёты живут в world::ReliefLayer — их обязан уметь читатель
файла, а он в engine/world. Здесь только рука: какой узел схвачен, куда
поставить следующий и что нарисовать на экране.

Dependencies:
- Uses: EditorTool.h, engine/world (ReliefPath — тип, который и хранится, и
  сохраняется), Dear ImGui внутри draw_settings().
- Used by: engine/app (создаёт инструмент и даёт ему крючки мира).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НИ ОДНОГО ПРАВИЛА ПРО ФОРМУ ТРОПЫ ЗДЕСЬ. Ширина, мягкость, поперечник и
  раскладка — в ReliefLayer. Второй поперечник в редакторе означал бы тропу,
  которая на экране одна, а в файле другая.
*/
/*
UPD:
- 18:08:2026 - 13:08:07: Создан — шестой инструмент, тропа кривой (заказ 18.08).
*/

#pragma once

#include "engine/editor/sources/EditorTool.h"
#include "engine/world/sources/ReliefLayer.h"

#include <cstddef>
#include <glm/vec3.hpp>
#include <vector>

namespace dfn::app {

/// «Ни одной тропы не правится» — настоящее состояние, а не отсутствие числа.
inline constexpr std::size_t NO_PATH = static_cast<std::size_t>(-1);

// Хватка узла — config::PATH_GRAB_M (docs/NUMBERS.md, правило 14): метры по
// земле, а не пиксели, потому что пиксельная хватка на близком узле накрыла бы
// пол-карты, а на дальнем не поймала бы ничего.

/// 6 — тропа. Щелчок ставит точку, щелчок по точке её тащит, панель правит
/// ширину, мягкость и список троп.
class PathTool final : public IEditorTool {
public:
    PathTool();

    [[nodiscard]] ToolIdentity identity() const override;
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) override;
    void on_release(ToolWorld& world) override;
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    void draw_settings() override;
    [[nodiscard]] float max_reach_m() const override { return 60.0f; }
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const override;
    void on_deselected(ToolWorld& world) override;

    /// Крючки, нужные ВНЕ щелчка: превью кладёт линию на землю, а панель читает
    /// список троп. Отдаются один раз тем же кодом, что заполняет ToolWorld.
    void set_world(ToolWorld* world) { world_ = world; }

    /// Тропа, которая сейчас в работе. Публично ради проверок и двери сдачи —
    /// это её собственная тропа и ничья больше.
    [[nodiscard]] const world::ReliefPath& draft() const { return draft_; }
    [[nodiscard]] std::size_t editing_index() const { return editing_; }

private:
    /// Записать текущую тропу в мир: добавить, заменить или (при пустой) убрать.
    void commit(ToolWorld& world);

    world::ReliefPath draft_;
    /// Какую из проведённых троп правит рука. NO_PATH — новая, ещё не в мире.
    std::size_t editing_ = NO_PATH;
    /// Какой узел схвачен протяжкой. draft_.points.size() — ни одного.
    std::size_t grabbed_ = 0;
    bool dragging_ = false;

    ToolWorld* world_ = nullptr;

    /// Буферы превью. mutable, потому что preview() — const, а линию надо
    /// класть на землю каждый кадр: земля под ней меняется, пока её же и правят.
    mutable std::vector<glm::vec3> line_;
    mutable std::vector<glm::vec3> handles_;
};

} // namespace dfn::app
