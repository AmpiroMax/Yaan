/*
Created: 18:08:2026 - 17:32:10
Last updated: 27:08:2026 - 14:30:00
Module: tests/app
File: tests/app/DoorsTests.cpp

Responsibility:
- Holds the door table CLOSED IN BOTH DIRECTIONS -- every DFN_* name the app
  reads has a row, and every row is a name the app reads -- and holds
  unattended_run() equal to the table's own column, door by door.

Dependencies:
- Uses: engine/app AppDoors, doctest.
- Used by: ctest (app_doors).

Notes:
- WHY TWO OF THESE CASES READ SOURCE FILES. The claim "there is no door outside
  this table" is a claim about six .cpp files, four of which own a window or
  are compiled into a binary that does. The alternative to a weak instrument
  here is no instrument, and the question it answers -- "what doors are there"
  -- was, until this table existed, a grep whose result nobody wrote down.
- THE UNATTENDED CASE IS THE ONE THAT PAYS FOR ITSELF. Its predecessor was a
  thirteen-term boolean, and the comment above it recorded that a door had been
  swept into it twice by edits that read as harmless. Here every door is tried
  ALONE, so a flag set on the wrong row is a failure with the row's name in it.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 18:08:2026 - 17:32:10: Создан. Слой 2 разбора App.cpp: рукав на таблицу дверей.
- 18:08:2026 - 18:19:47: AppWorld.cpp и AppSettings.cpp в списке. СПИСОК ПОЙМАЛ ПЕРЕНОС САМ:
  DFN_TOUR_DIR читается в enter_world, и как только она уехала, дверь стала
  «описанной, но никем не читаемой». Ровно то, ради чего рукав написан.
- 20:08:2026 - 16:40:00: Безлюдных дверей 14 — прибавилась DFN_PLAYTEST_ARRIVE.
- 22:08:2026 - 14:30:00: Безлюдных дверей 15 — прибавилась DFN_PLAYTEST_GLANCE.
- 22:08:2026 - 17:45:00: AppHouse.cpp в списке сканируемых (читает DFN_HOUSE_AO через door_value).
- 24:08:2026 - 01:20:00: AppInterior.cpp в списке файлов; безлюдных дверей 17
  (DFN_INTERIOR, DFN_INTERIOR_EXIT — И15 волна А).
- 27:08:2026 - 14:30:00: безлюдных дверей 19 (DFN_INTERIOR_TURN, DFN_DOOR_OPEN —
  починка «из дома не выйти» и декоративные створки). Число ПЕРЕСЧИТАНО по
  таблице, а не прибавлено на глаз: рукав поймал бы ошибку, но красный тест на
  чужом столе стоит дороже, чем сложение на своём.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/AppDoors.h"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using dfn::app::Door;
using dfn::app::DoorRead;

namespace {

// The files that may mention a door. AppDoors.cpp is excluded on purpose: it
// IS the table, so every name appears there by definition.
const std::vector<std::string>& app_sources() {
    static const std::vector<std::string> v{
        // AppWorld.cpp и AppSettings.cpp дописаны 18.08 при выносе enter_world и
        // настроек. Список ПОЙМАЛ ЭТОТ ПЕРЕНОС САМ: DFN_TOUR_DIR читается в
        // enter_world, и как только она уехала в другой файл, дверь стала
        // «описанной, но никем не читаемой». Ровно то, ради чего рукав написан.
        "engine/app/sources/App.cpp",       "engine/app/sources/AppInput.cpp",
        "engine/app/sources/AppWorld.cpp",  "engine/app/sources/AppSettings.cpp",
        "engine/app/sources/AppActions.cpp", "engine/app/sources/DebugOverlay.cpp",
        "engine/app/sources/HudScreen.cpp", "engine/app/sources/EditorHud.cpp",
        "engine/app/sources/Menu.cpp",      "engine/app/sources/ChatLog.cpp",
        "engine/app/sources/ChatOverlay.cpp", "engine/app/sources/BuildTool.cpp",
        "engine/app/sources/Controls.cpp",  "engine/app/sources/EditorCamera.cpp",
        "engine/app/sources/EditorPlant.cpp", "engine/app/sources/AssetBake.cpp",
        "engine/app/sources/MapCatalog.cpp", "engine/app/sources/Localization.cpp",
        "engine/app/sources/TrajectoryRecord.cpp", "engine/app/sources/Main.cpp",
        // AppHouse.cpp дописан 22.08: DFN_HOUSE_AO читается там (через
        // door_value, как и всё), и список обязан это видеть.
        "engine/app/sources/AppHouse.cpp",
        // AppInterior.cpp дописан 24.08 (И15): DFN_LOAD_LOG читается и там —
        // экран загрузки ведёт тот же список этапов, что и прибор.
        "engine/app/sources/AppInterior.cpp",
    };
    return v;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Every COMPLETE string literal of the shape "DFN_SOMETHING". Complete is the
// load-bearing word: a diagnostic like "[editor] дверь DFN_EDITOR_TOOL=%d" also
// contains those characters, and counting it would make the check unusable in
// exactly the files that print the most about their doors.
std::set<std::string> door_literals(const std::string& text) {
    static const std::regex re("\"(DFN_[A-Z0-9_]+)\"");
    std::set<std::string> out;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re);
         it != std::sregex_iterator(); ++it) {
        out.insert((*it)[1].str());
    }
    return out;
}

void close_every_door() {
    for (const Door& d : dfn::app::doors()) {
        ::unsetenv(d.name);
    }
}

} // namespace

TEST_CASE("every door has a name, a description and a cadence") {
    const auto rows = dfn::app::doors();
    REQUIRE_FALSE(rows.empty());

    std::set<std::string> seen;
    for (const Door& d : rows) {
        REQUIRE(d.name != nullptr);
        const std::string name = d.name;
        CAPTURE(name);
        // The prefix is the convention every recipe on disk is written to.
        CHECK(name.rfind("DFN_", 0) == 0);
        // TWO ROWS FOR ONE NAME would make find_door() answer with whichever
        // came first, and the two descriptions would drift apart quietly.
        CHECK(seen.insert(name).second);
        CHECK(&dfn::app::find_door(name)[0] == &d);

        // A DESCRIPTION, NOT A PLACEHOLDER. The whole point of the table is
        // that "what does DFN_SHOWCASE do" has an answer here; a row saying
        // "showcase door" would satisfy a non-empty check and answer nothing.
        REQUIRE(d.what != nullptr);
        const std::string what = d.what;
        CHECK(what.size() >= 20);
        CHECK(what != name);
        CHECK((d.read == DoorRead::Once || d.read == DoorRead::EachRead));
    }
}

TEST_CASE("no door is read that the table does not describe") {
    // DIRECTION ONE. A getenv() somewhere in engine/app naming a variable with
    // no row is a feature that exists for whoever wrote it and for nobody else
    // -- which is what the whole zone looked like before this table: fifty-odd
    // reads across six files and no list anywhere.
    std::set<std::string> table;
    for (const Door& d : dfn::app::doors()) {
        table.insert(d.name);
    }

    std::size_t scanned = 0;
    for (const std::string& f : app_sources()) {
        const std::string text = read_file(f);
        if (text.empty()) {
            continue; // a file that moved is not this suite's failure to report
        }
        ++scanned;
        // AND NOBODY READS THE ENVIRONMENT BEHIND THE TABLE'S BACK. door_value()
        // refuses an unlisted name at runtime, but only if it is the one asked.
        CAPTURE(f);
        CHECK(text.find("std::getenv(") == std::string::npos);
        for (const std::string& lit : door_literals(text)) {
            CAPTURE(lit);
            CHECK(table.count(lit) == 1);
        }
    }
    // The list of files is itself something that can go stale; if it stops
    // matching the tree, this suite would pass by scanning nothing.
    CHECK(scanned >= 6);
}

TEST_CASE("no door is described that nothing reads") {
    // DIRECTION TWO, and it is the one that keeps the table from becoming
    // documentation: a row nobody reads is a door a recipe can be written
    // against that will never open, which is worse than an absent row because
    // it comes with a promise.
    std::string all;
    for (const std::string& f : app_sources()) {
        all += read_file(f);
    }
    REQUIRE_FALSE(all.empty());

    // ...OR IT IS READ BY unattended_run() THROUGH THE TABLE ITSELF, which is
    // a real second way to be read and exactly one door uses it. DFN_TOUR
    // belongs to render's Tour (engine/render/sources/Tour.cpp names it); the
    // app never asks what it says, only whether it is open, and it asks that
    // by walking this table. Written as a named exception rather than as a
    // loophole in the rule: an unattended flag is not a licence to add doors
    // no one reads, so the count is pinned.
    int only_via_the_column = 0;
    for (const Door& d : dfn::app::doors()) {
        const std::string name = d.name;
        CAPTURE(name);
        const bool in_source = all.find("\"" + name + "\"") != std::string::npos;
        CHECK((in_source || d.unattended));
        if (!in_source) {
            ++only_via_the_column;
            CHECK(name == "DFN_TOUR");
        }
    }
    CHECK(only_via_the_column == 1);
}

TEST_CASE("a name with no row does not open, however loudly it is set") {
    close_every_door();
    ::setenv("DFN_NOT_A_DOOR_AT_ALL", "1", 1);
    // The read refuses and says so on stderr. Returning the value would make
    // "undocumented" and "working" the same state, which is precisely how a
    // door ends up known to one agent and invisible to the next.
    CHECK(dfn::app::door_value("DFN_NOT_A_DOOR_AT_ALL") == nullptr);
    CHECK(dfn::app::find_door("DFN_NOT_A_DOOR_AT_ALL") == nullptr);
    ::unsetenv("DFN_NOT_A_DOOR_AT_ALL");

    // ...while a listed door that is simply closed also reads as nothing, so
    // the two states are not told apart by their return value alone -- the
    // difference is the line on stderr and the row in the table.
    CHECK(dfn::app::door_value("DFN_TOUR") == nullptr);
}

TEST_CASE("whether a run is unattended is the table's column, door by door") {
    close_every_door();
    // THE CONTROL FIRST: with every door shut, somebody is playing.
    CHECK_FALSE(dfn::app::unattended_run());

    // EACH DOOR ALONE. Its predecessor was a thirteen-term `||` written out by
    // hand, into which a door had twice been swept by a harmless-looking edit.
    // Trying them one at a time is what puts the offending door's name in the
    // failure instead of "the expression is wrong".
    int unattended_doors = 0;
    for (const Door& d : dfn::app::doors()) {
        const std::string name = d.name;
        CAPTURE(name);
        ::setenv(d.name, "1", 1);
        CHECK(dfn::app::unattended_run() == d.unattended);
        ::unsetenv(d.name);
        unattended_doors += d.unattended ? 1 : 0;
    }
    CHECK_FALSE(dfn::app::unattended_run()); // и всё убрано за собой

    // The count is written out because it is the number a reader wants and
    // because a table that lost the flag entirely would otherwise pass every
    // assertion above.
    // 14 с 20.08: DFN_PLAYTEST_ARRIVE — спутница DFN_PLAYTEST_ROUTE, руками
    // её не выставляют. 15 с 22.08: DFN_PLAYTEST_GLANCE — тоже спутница
    // маршрута (ровный взгляд операторской ленты).
    // 17 с 24.08 (И15): DFN_INTERIOR (открыть карту сразу внутри локации) и
    // DFN_INTERIOR_EXIT (выйти через N кадров) — обе существуют ровно затем,
    // чтобы вход и выход мог снять автомат: руками они не выставляются.
    // 19 с 27.08: DFN_INTERIOR_TURN (доворот после входа — без него прогон
    // смотрит в комнату, а дверь остаётся за спиной) и DFN_DOOR_OPEN (открыть
    // декоративные створки). Обе — руки автомата: поворот головы и нажатие E
    // перед дверью иначе умеет только человек.
    CHECK(unattended_doors == 19);

    // PRESENCE, NOT TRUTH. `DFN_TOUR=0` still means a tour is being run by a
    // script -- every door here is opened by being set at all, and a door that
    // read "0" as closed would strand the recipes that spell out their
    // variables explicitly.
    ::setenv("DFN_TOUR", "0", 1);
    CHECK(dfn::app::unattended_run());
    ::unsetenv("DFN_TOUR");
}
