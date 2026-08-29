/*
Module: engine/world
File: engine/world/sources/SceneStairRules.h

Responsibility:
- ЛЕСТНИЦА И ПРОЁМ НАД НЕЙ (docs/HOUSES.md §9). The user, 17.08: «надо
  лестницы крепить к пол-потолок, при этом над лестницей должна быть дырка,
  пространство через которое пройдет игрок». Two rules: the flight is seated
  on a horizontal joint at each end (StairSeat), and the player fits up it
  (StairHeadroom).

Key items:
- check_stair_rules(): appends findings to the caller's vector.
- opening_length_m() / opening_start_m(): ТОТ САМЫЙ КАЛЬКУЛЯТОР — the derived
  opening a generator should ask for. Exported because a generator has to
  size the hole BEFORE the judge can measure it.

Dependencies:
- Uses: engine/world/Scene.h, glm, std. NOT engine/render.
- Used by: check_scene() in Scene.cpp; tests/core/SceneStairRuleTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ИЕРАРХИЯ, И ОНА НЕ ОБСУЖДАЕТСЯ (HOUSES.md §9.3): измерение КАПСУЛОЙ ПО
  КАЖДОЙ СТУПЕНИ — это СУДЬЯ; opening_length_m() — КАЛЬКУЛЯТОР для
  генератора. Они выведены разными путями НАРОЧНО и обязаны остаться
  независимыми. При расхождении прав судья, и чинить надо калькулятор. Заход,
  который «починит» формулу подгонкой под неё правила, оставит игрока с
  шишкой и зелёным отчётом — а это ровно тот обмен, ради запрета которого
  правила и пишут.
- ПРОЁМ — ЭТО ОБЪЕКТ, А НЕ ОТСУТСТВИЕ ОБЪЕКТА. Настил объявляет прямоугольник
  пустоты своим ИМЕНЕМ (`deck-...-hole<x>x<z>x<l>x<w>-...`), и правило требует
  именно ОБЪЯВЛЕННОЙ пустоты. Не положенная панель даёт ту же дырку в воздухе
  и НЕ проходит: иначе «здесь по проекту проём» и «здесь панель забыли»
  становятся одной строкой, а второе — тот самый дефект, ради которого весь
  этот свод и написан.
*/

#pragma once

#include "engine/world/sources/Scene.h"

#include <vector>

namespace dfn::world {

/// ДЛИНА ПРОЁМА, ВЫВЕДЕННАЯ, А НЕ ВЫБРАННАЯ (HOUSES.md §9.2). `t` — тангенс
/// уклона марша (подъём/проступь), `thick` — толщина настила, `height` и
/// `radius` — мерки героя.
///
///     L = (H + thick)/t + R*t / (sqrt(1 + t^2) + 1)
///
/// Первый член — «сколько горизонтали нужно, чтобы макушка вышла из-под
/// плиты». Второй — НЕ «плюс R»: игрок не линия, его макушка это полусфера
/// радиуса R, и она задевает УГОЛ плиты раньше, чем ось дойдёт до края.
/// Множитель t/(sqrt(1+t^2)+1) — это тангенс ПОЛОВИНЫ угла между настилом и
/// маршем, то есть та самая касательная полусферы к углу.
[[nodiscard]] float opening_length_m(float t, float thick, float height,
                                     float radius);

/// ГДЕ ПРОЁМ НАЧИНАЕТСЯ, считая от подножия марша по горизонтали:
/// d0 = (H_low - H)/t - R*t/(sqrt(1+t^2)+1), где H_low = верх настила минус
/// его толщина. Ровно d1 - L, где d1 = H_top/t — горизонталь, на которой марш
/// доходит до верхнего уровня; эта тождественность и есть проверка вывода.
[[nodiscard]] float opening_start_m(float t, float thick, float height,
                                    float radius, float storey_m);

/// Judges the flights and the decks over them, appending what it finds.
/// Needs `world.object_box`; without it, quietly does nothing, the same door
/// every box-based rule uses.
void check_stair_rules(const SceneDoc& doc, const SceneWorld& world,
                       const SceneLimits& limits,
                       std::vector<SceneFinding>& found);

} // namespace dfn::world
