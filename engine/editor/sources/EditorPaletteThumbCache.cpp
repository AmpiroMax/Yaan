/*
Module: engine/editor
File: engine/editor/sources/EditorPaletteThumbCache.cpp

Responsibility:
- ThumbCache, declared in EditorPaletteThumb.h: which name has a picture, which
  is still waiting, how many may be drawn in one frame, and which gets thrown
  out when the ceiling is reached. The POLICY of the object menu's pictures —
  the pictures themselves are EditorPaletteThumb.cpp's business.

Dependencies:
- Uses: EditorPaletteThumb.h. Nothing else: the bake, the upload and the drop
  all arrive as functions, so nothing here names a GPU type or a window.
- Used by: engine/app (wires it into PaletteHooks::thumbnail), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE BUDGET IS THE FEATURE. Answering every request the frame it arrives is
  what turns "the menu opened" into a visible hitch: thirty-six visible tiles
  at a millisecond each is three frames' worth of work inside one frame. Six
  arrive now and the rest next frame, and the user sees the grid fill in.
- A NAME THAT CANNOT BE DRAWN IS GIVEN UP ON, not retried for ever. Without the
  cap one missing mesh eats a budget slot every frame and the parts behind it
  are never drawn — a whole menu held hostage by one bad row.
*/

#include "engine/editor/sources/EditorPaletteThumb.h"

#include <utility>

namespace dfn::app {

void ThumbCache::begin_frame() {
    baked_frame_ = 0;
    ++tick_;
}

std::uint64_t ThumbCache::get(const std::string& name, int size_px) {
    (void)size_px; // a WISH, not a key: one part is one picture (the hook's contract)
    if (name.empty()) {
        return 0;
    }

    const auto found = entries_.find(name);
    if (found != entries_.end()) {
        found->second.used = tick_;
        if (found->second.texture != 0 || found->second.given_up) {
            return found->second.texture;
        }
    }

    if (baked_frame_ >= budget_) {
        ++deferred_;
        return 0; // "not ready yet" — the panel draws its placeholder and asks again
    }
    if (!bake_ || !upload_) {
        return 0; // nobody wired a source; a name-only menu, which still works
    }

    Entry& entry = found != entries_.end() ? found->second : entries_[name];
    entry.used = tick_;
    ++baked_frame_;
    ++entry.attempts;
    if (!bake_(name, bake_px_, scratch_) || scratch_.empty()) {
        if (entry.attempts >= THUMB_MAX_ATTEMPTS) {
            entry.given_up = true;
            ++given_up_;
        }
        return 0;
    }
    ++bakes_;
    entry.texture = upload_(bake_px_, scratch_.data());
    if (entry.texture == 0) {
        // The interface layer refused (no context yet). Same treatment as a
        // failed bake: try again, but not for ever.
        if (entry.attempts >= THUMB_MAX_ATTEMPTS) {
            entry.given_up = true;
            ++given_up_;
        }
        return 0;
    }

    // EVICT AFTER INSERTING, NOT BEFORE. Evicting first would let the ceiling
    // throw out a picture the panel is drawing THIS frame, which is a tile that
    // flickers once per scroll — and the LRU below cannot pick the new entry
    // because it was just touched.
    while (entries_.size() > capacity_) {
        evict_one();
    }
    return entry.texture;
}

void ThumbCache::evict_one() {
    auto oldest = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->second.used >= tick_) {
            continue; // asked for during this frame: it is on screen right now
        }
        if (oldest == entries_.end() || it->second.used < oldest->second.used) {
            oldest = it;
        }
    }
    if (oldest == entries_.end()) {
        return; // every entry is on screen; the ceiling yields rather than flicker
    }
    if (oldest->second.texture != 0 && drop_) {
        drop_(oldest->second.texture);
    }
    entries_.erase(oldest);
    ++evictions_;
}

void ThumbCache::clear() {
    if (drop_) {
        for (auto& [name, entry] : entries_) {
            (void)name;
            if (entry.texture != 0) {
                drop_(entry.texture);
            }
        }
    }
    entries_.clear();
}

} // namespace dfn::app
