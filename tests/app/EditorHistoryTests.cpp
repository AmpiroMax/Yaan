/*
Module: tests
File: tests/app/EditorHistoryTests.cpp

Responsibility:
- ОТМЕНА, КОТОРАЯ НЕ ВРЁТ. Заказ 18.08: «надо добавить cmd+z cmd+shift+z».

Key items:
- Отмена и повтор возвращают ИМЕННО те состояния, что были.
- Повтор гибнет при новом действии — иначе он восстановит небывшее.
- Действие, ничего не изменившее, в историю не попадает.
- Глубина ограничена, и старое вытесняется, а не растёт бесконечно.

Dependencies:
- Uses: doctest, EditorHistory.cpp.
- Used by: ctest (app_editor_history).

AI Agents Notice (must follow):
- Правило 30: проверка обязана краснеть. Здесь она краснеет, если повтор
  переживёт новое действие, если пустой шаг попадёт в историю и если стопки
  разъедутся.
*/

#include <doctest/doctest.h>

#include "engine/editor/sources/EditorHistory.h"

using dfn::app::EditorHistory;

TEST_CASE("отмена и повтор возвращают ИМЕННО те состояния, что были") {
    EditorHistory h;
    CHECK_FALSE(h.can_undo());
    CHECK_FALSE(h.can_redo());

    h.record("поставил вершину", "A", "B");
    h.record("двинул якорь", "B", "C");
    REQUIRE(h.can_undo());
    CHECK(h.undo_label() == "двинул якорь");

    // Отмена отдаёт состояние ДО последнего действия.
    CHECK(h.undo() == "B");
    CHECK(h.can_redo());
    CHECK(h.redo_label() == "двинул якорь");
    CHECK(h.undo() == "A");
    CHECK_FALSE(h.can_undo());

    // И обратно, тем же путём. Проверяется ПОРЯДОК, а не только содержимое:
    // стопка, отдающая шаги не в том порядке, вернёт человека не туда, и
    // выглядеть это будет как «отмена глючит».
    CHECK(h.redo() == "B");
    CHECK(h.redo() == "C");
    CHECK_FALSE(h.can_redo());
}

TEST_CASE("повтор гибнет при новом действии") {
    EditorHistory h;
    h.record("раз", "A", "B");
    h.record("два", "B", "C");
    CHECK(h.undo() == "B");
    REQUIRE(h.can_redo());

    // НОВОЕ ДЕЙСТВИЕ УБИВАЕТ ПОВТОР. Иначе повтор восстановит состояние "C",
    // которого в ЭТОЙ истории уже не было: человек отменил шаг и пошёл другой
    // дорогой, и вернуть его на старую — значит соврать.
    h.record("другой путь", "B", "D");
    CHECK_FALSE(h.can_redo());
    CHECK(h.undo() == "B");
}

TEST_CASE("действие, ничего не изменившее, в историю не попадает") {
    EditorHistory h;
    // Пустой шаг хуже отсутствующего: человек жмёт cmd+Z, ничего не
    // происходит, и он решает, что отмена сломана.
    h.record("двинул на ноль", "A", "A");
    CHECK_FALSE(h.can_undo());
    CHECK(h.undo_depth() == 0);
}

TEST_CASE("глубина ограничена, и вытесняется САМОЕ СТАРОЕ") {
    EditorHistory h(3);
    h.record("1", "A", "B");
    h.record("2", "B", "C");
    h.record("3", "C", "D");
    h.record("4", "D", "E");
    CHECK(h.undo_depth() == 3);

    // Три шага назад доступны, четвёртый — нет. Проверяется КОНЕЦ цепочки:
    // вытеснение не с того конца дало бы историю, которая помнит начало и
    // забыла только что сделанное.
    CHECK(h.undo() == "D");
    CHECK(h.undo() == "C");
    CHECK(h.undo() == "B");
    CHECK_FALSE(h.can_undo());
}
