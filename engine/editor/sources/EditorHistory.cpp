/*
Module: engine/editor
File: engine/editor/sources/EditorHistory.cpp

Responsibility:
- Реализация отмены и повтора. Устройство — в заголовке.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/editor/sources/EditorHistory.h"

namespace dfn::app {
namespace {
const std::string EMPTY;
} // namespace

void EditorHistory::record(std::string label, std::string before, std::string after) {
    // ДЕЙСТВИЕ, НИЧЕГО НЕ ИЗМЕНИВШЕЕ, В ИСТОРИЮ НЕ ПОПАДАЕТ. Иначе cmd+Z
    // начинает «срабатывать вхолостую»: человек жмёт, ничего не происходит, и
    // он думает, что отмена сломана. Пустой шаг хуже отсутствующего.
    if (before == after) {
        return;
    }
    done_.push_back({std::move(label), std::move(before), std::move(after)});
    // ПОВТОР ГИБНЕТ. Отменили три шага, сделали новое — повторять больше нечего:
    // иначе повтор восстановит состояние, которого в этой истории не было.
    undone_.clear();
    if (done_.size() > depth_) {
        done_.erase(done_.begin());
    }
}

const std::string& EditorHistory::undo_label() const {
    return done_.empty() ? EMPTY : done_.back().label;
}

const std::string& EditorHistory::redo_label() const {
    return undone_.empty() ? EMPTY : undone_.back().label;
}

std::string EditorHistory::undo() {
    if (done_.empty()) {
        return {};
    }
    HistoryEntry e = std::move(done_.back());
    done_.pop_back();
    std::string state = e.before;
    undone_.push_back(std::move(e));
    return state;
}

std::string EditorHistory::redo() {
    if (undone_.empty()) {
        return {};
    }
    HistoryEntry e = std::move(undone_.back());
    undone_.pop_back();
    std::string state = e.after;
    done_.push_back(std::move(e));
    return state;
}

void EditorHistory::clear() {
    done_.clear();
    undone_.clear();
}

} // namespace dfn::app
