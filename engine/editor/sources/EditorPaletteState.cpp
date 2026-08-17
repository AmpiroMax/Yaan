/*
Created: 17:08:2026 - 19:14:11
Last updated: 17:08:2026 - 19:22:54
Module: engine/editor
File: engine/editor/sources/EditorPaletteState.cpp

Responsibility:
- WHAT THE BUILDER KEEPS between sessions: the selection, the favourites, the
  recents and the quick slots — all of them PER MAP — and the file they survive
  a restart in.

WHY THIS IS ITS OWN FILE: EditorPalette.cpp reached Rule 21's hard limit, and
this is the seam that costs nothing to cut along. The query half answers "which
parts match"; this half answers "which parts does this builder keep", and the
two share only the model's fields.

WHY PER MAP (lead's ruling, 17.08): the ten parts that matter to a town are not
the ten that matter to a flora stand. One shared list is in both builders' way,
and the cost of getting it wrong is silent — a favourites row that is simply
never used.

Dependencies:
- Uses: EditorPalette.h, std.
- Used by: EditorPaletteView, App, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A ROW IN THE STATE FILE THAT PRECEDES ANY `map=` IS DROPPED, never filed
  under the current map: importing another map's favourites is worse than
  losing a line, because nobody can tell where the rows came from afterwards.
- save_state writes EVERY map, not the current one. Rewriting the file from one
  map would forget the others, and the loss would show up a week later.
*/
/*
UPD:
- 17:08:2026 - 19:14:11: Отделён от EditorPalette.cpp (правило 21: 832 строки).
- 17:08:2026 - 19:22:54: Переезд в engine/editor. ARCHITECTURE.md разрешает Dear ImGui
  ТОЛЬКО в engine/editor, а слой editor не имеет права включать engine/app
  (LAYERS в tools/dag_check.py) — значит панель и её модель обязаны жить
  по одну сторону, и эта сторона — editor. Ни строки логики не тронуто.
*/

#include "engine/editor/sources/EditorPalette.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace dfn::app {

// ---------------------------------------------------------------------------
// What the builder keeps, per map
// ---------------------------------------------------------------------------

PaletteModel::MapState& PaletteModel::state() {
    for (auto& [id, st] : maps_) {
        if (id == map_id_) {
            return st;
        }
    }
    maps_.emplace_back(map_id_, MapState{});
    return maps_.back().second;
}

const PaletteModel::MapState& PaletteModel::state() const {
    static const MapState none;
    for (const auto& [id, st] : maps_) {
        if (id == map_id_) {
            return st;
        }
    }
    return none;
}

void PaletteModel::set_map_id(std::string map_id) {
    map_id_ = std::move(map_id);
    selected_ = state().selected;
    invalidate();
}

void PaletteModel::toggle_favourite(std::string_view name) {
    MapState& st = state();
    const auto it = std::find(st.favourites.begin(), st.favourites.end(), name);
    if (it == st.favourites.end()) {
        st.favourites.emplace_back(name);
    } else {
        st.favourites.erase(it);
    }
    invalidate();
}

bool PaletteModel::is_favourite(std::string_view name) const {
    const MapState& st = state();
    return std::find(st.favourites.begin(), st.favourites.end(), name) != st.favourites.end();
}

const std::vector<std::string>& PaletteModel::favourites() const { return state().favourites; }

void PaletteModel::note_used(std::string_view name) {
    if (name.empty()) {
        return;
    }
    MapState& st = state();
    const auto it = std::find(st.recents.begin(), st.recents.end(), name);
    if (it != st.recents.end()) {
        st.recents.erase(it);
    }
    st.recents.emplace(st.recents.begin(), name);
    if (st.recents.size() > PALETTE_RECENTS_LIMIT) {
        st.recents.resize(PALETTE_RECENTS_LIMIT);
    }
    invalidate();
}

const std::vector<std::string>& PaletteModel::recents() const { return state().recents; }

std::size_t PaletteModel::recents_limit() { return PALETTE_RECENTS_LIMIT; }

void PaletteModel::set_quick_slot(int slot, std::string_view name) {
    if (slot < 1 || slot > PALETTE_QUICK_SLOTS) {
        return;
    }
    state().slots[slot - 1].assign(name);
}

const std::string& PaletteModel::quick_slot(int slot) const {
    static const std::string none;
    if (slot < 1 || slot > PALETTE_QUICK_SLOTS) {
        return none;
    }
    return state().slots[slot - 1];
}

bool PaletteModel::take_quick_slot(int slot) {
    const std::string picked = quick_slot(slot);
    if (picked.empty()) {
        return false;
    }
    select(picked);
    return true;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

std::string PaletteModel::state_text() const {
    std::ostringstream out;
    out << "# dfn editor palette state v1\n";
    for (const auto& [id, st] : maps_) {
        out << "map=" << id << '\n';
        if (!st.selected.empty()) {
            out << "selected=" << st.selected << '\n';
        }
        for (const std::string& f : st.favourites) {
            out << "fav=" << f << '\n';
        }
        for (const std::string& r : st.recents) {
            out << "recent=" << r << '\n';
        }
        for (int i = 0; i < PALETTE_QUICK_SLOTS; ++i) {
            if (!st.slots[i].empty()) {
                out << "slot" << (i + 1) << '=' << st.slots[i] << '\n';
            }
        }
    }
    return out.str();
}

void PaletteModel::load_state_text(std::string_view text) {
    maps_.clear();
    std::string current;
    // std::getline rather than a split helper: the query half owns that helper
    // and a second copy of it here is Rule 39 waiting to happen.
    std::istringstream in{std::string(text)};
    for (std::string raw; std::getline(in, raw);) {
        std::string_view line(raw);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.remove_suffix(1);
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        const std::string_view key = line.substr(0, eq);
        const std::string_view value = line.substr(eq + 1);
        if (key == "map") {
            current.assign(value);
            if (std::none_of(maps_.begin(), maps_.end(),
                             [&current](const auto& m) { return m.first == current; })) {
                maps_.emplace_back(current, MapState{});
            }
            continue;
        }
        // A row before any `map=` belongs to no map. Dropping it is deliberate:
        // filing it under the current session's map would import another map's
        // favourites, and a favourites list nobody chose is worse than none.
        auto it = std::find_if(maps_.begin(), maps_.end(),
                               [&current](const auto& m) { return m.first == current; });
        if (it == maps_.end()) {
            continue;
        }
        MapState& st = it->second;
        if (key == "selected") {
            st.selected.assign(value);
        } else if (key == "fav") {
            st.favourites.emplace_back(value);
        } else if (key == "recent") {
            if (st.recents.size() < PALETTE_RECENTS_LIMIT) {
                st.recents.emplace_back(value);
            }
        } else if (key.rfind("slot", 0) == 0 && key.size() == 5) {
            const int slot = key[4] - '0';
            if (slot >= 1 && slot <= PALETTE_QUICK_SLOTS) {
                st.slots[slot - 1].assign(value);
            }
        }
    }
    selected_ = state().selected;
    invalidate();
}

bool PaletteModel::load_state(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false; // a first run has no file, and that is not a failure
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    load_state_text(buf.str());
    return true;
}

bool PaletteModel::save_state(const std::string& path) const {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << state_text();
        if (!out) {
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

} // namespace dfn::app
