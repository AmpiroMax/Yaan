/*
Created: 18:08:2026 - 17:32:10
Last updated: 28:08:2026 - 19:30:00
Module: engine/app
File: engine/app/sources/AppDoors.h

Responsibility:
- THE DOORS: every DFN_* environment variable the app reads, in one table, with
  what it does, when it is read, and whether an open one means "nobody is
  playing this run". Until today the answer to "what doors are there" was a
  grep, and the answer to "does this door free the mouse" was a thirteen-term
  boolean expression written out by hand.

Key items:
- Door / doors() / find_door(): the table and its lookup.
- door_value(): the ONLY way the app reads a door. A name that is not in the
  table reads as nothing and says so.
- unattended_run(): derived from the table's own column, not from a list.

Dependencies:
- Uses: nothing but the standard library. No App, no window -- which is what
  lets tests/app/DoorsTests.cpp read it.
- Used by: App.cpp, AppInput.cpp, HudScreen.cpp, DebugOverlay.cpp,
  EditorHud.cpp, tests/app.

Notes:
- WHY THE READ GOES THROUGH THE TABLE. `std::getenv("DFN_WHATEVER")` scattered
  through six files is a door that exists and is documented nowhere, and that
  is not hypothetical: this table was assembled from fifty-eight scattered
  reads, several of which nobody outside the file that wrote them could have
  named. Now a read of an unlisted name returns nothing and prints why, so the
  cheapest way to add a door is to add its row.
- WHY THE DESCRIPTIONS ARE NOT LOCALIZED (Rule 5 does not apply). A door is
  never shown to a player. It is read by an agent building a recipe and by the
  person reading that recipe afterwards, and translating it would put the
  answer in one language and the recipe in another.
- READ ONCE VS READ EACH TIME, and it is a real distinction rather than a note:
  a door polled every frame is a SWITCH, and a switch inside an instrument
  lets two frames of one run disagree about what was tested (DebugOverlay.cpp
  says this in its own words). The column records which each door is, so the
  question can be asked of the table instead of of the code.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ADDING A DOOR? Add its row here first. A getenv() elsewhere in engine/app is
  caught by tests/app/DoorsTests.cpp, and door_value() of an unlisted name
  refuses at runtime.
*/
/*
UPD:
- 18:08:2026 - 17:32:10: Создан. Слой 2 разбора App.cpp (docs/PLAN_APP_DECOMPOSITION.md):
  58 дверей, читавшихся россыпью по шести файлам, собраны в одну таблицу с
  описанием и признаком «за этим прогоном никто не сидит». Последний перестал
  быть выражением из тринадцати слагаемых — а его уже дважды ломали правкой,
  которая выглядела безобидной (см. предупреждение у unattended_run()).
- 28:08:2026 - 19:30:00: counted_run() — вопрос «кадр этого прогона является
  доказательством». Дверь контроля к нему (DFN_UNPIN) читается в App.cpp: файл
  таблицы исключён из переписи читателей рукавом app_doors.
*/

#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace dfn::app {

// HOW OFTEN THE DOOR IS ASKED. Once = latched at the first read (a `static
// const`, or a read in init/enter_world), so every frame of one run agrees
// about it. EachRead = re-read at every call site; correct only for doors
// whose answer cannot change mid-run because nothing writes them.
enum class DoorRead : uint8_t { Once, EachRead };

struct Door {
    const char* name; // "DFN_TOUR"
    const char* what; // what it does -- prose, never empty
    DoorRead read;
    // DOES AN OPEN DOOR MEAN NOBODY IS PLAYING THIS RUN. Consumed by
    // unattended_run(): skip the menu, leave the desktop pointer alone, and
    // advance the world on a COUNTED clock instead of a wall clock.
    bool unattended = false;
};

[[nodiscard]] std::span<const Door> doors();
[[nodiscard]] const Door* find_door(std::string_view name);

// THE ONLY READ. Returns nullptr for a closed door AND for a name that has no
// row -- the second case also prints, once, because a door that silently does
// nothing is the failure mode doors exist to avoid.
[[nodiscard]] const char* door_value(std::string_view name);

// IS THIS RUN UNATTENDED? One definition, three consumers -- the menu skip, the
// cursor grab and the counted clock (Rule 35).
//
// The second consumer is why it exists. The user, working at his machine while
// agents shot frames, reported: "когда запускаются визуальные тесты у меня
// управление компом перехватывается, меня в игру перекидывает, мышью управлять
// не могу". Every automated door except the body probe grabbed the desktop
// pointer, because the exemption had been written for ONE door instead of for
// the PROPERTY the doors share.
//
// DFN_MENU_SHOT IS FLAGGED ON PURPOSE AND IT IS A TRAP FOR THE NEXT REFACTOR:
// it gates the CURSOR and the counted clock (nobody is playing), but it must
// NOT gate the MENU -- a door that exists to photograph a menu screen needs the
// menu SHOWN, the opposite of every other door here. This has been swept into
// the menu-SKIP twice already. The menu is re-asserted for DFN_MENU_SHOT and
// DFN_MENU_PAGE in App::init()'s branch (C) precisely so this stays one honest
// "nobody is aiming" without owning the menu question too. Do not "clean up"
// by acting on those two here.
[[nodiscard]] bool unattended_run();

// СЧЁТНЫЙ ЛИ ЭТО ПРОГОН — то есть обязан ли его кадр быть ЧИСТОЙ ФУНКЦИЕЙ
// НОМЕРА КАДРА. Сегодня это ровно unattended_run(): за беспилотным прогоном
// никто не играет, значит его кадр — доказательство, а доказательство не имеет
// права зависеть от загрузки машины. Отдельное имя — не синоним ради синонима:
// у двух вопросов разные потребители («не забирай мышь» и «считай кадрами»), и
// день, когда они разойдутся, не должен начинаться с поиска всех мест.
[[nodiscard]] bool counted_run();


} // namespace dfn::app
