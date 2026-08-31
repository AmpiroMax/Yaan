/*
Module: engine/app
File: engine/app/sources/AppPoses.cpp

Responsibility:
- ПЕРЕБОР ПОЗ РЕЕСТРА С КЛАВИАТУРЫ И ДОЗОЙ. Одно место, где решается, какую
  запись anim::PoseLibrary держит тело: клавиши [ и ], доза DFN_POSE на
  беспилотном прогоне, и заявка в привод тела.

Key items:
- App::step_pose(): шаг по кольцу «живое тело + 23 позы». Обе клавиши зовут
  его со знаком, потому что «следующая» и «предыдущая» — одно действие.
- App::on_pose_next / on_pose_prev: сами клавиши.
- App::apply_pose_dose(): DFN_POSE=<имя записи>, один раз за прогон.

Dependencies:
- Uses: engine/anim (PoseLibrary, BodyDrive), App.h. Ни физики, ни рендера.
- Used by: AppInput.cpp (разбор действия), App.cpp (доза при появлении тела).

Notes:
- НУЛЕВОЙ СЛОТ — ЭТО ЖИВОЕ ТЕЛО, А НЕ ПОЗА «СТОЯ». Разница видна на стенде:
  живое тело ходит, дышит и приседает, а «стоя» — запись реестра, которую
  тело ДЕРЖИТ. Отдай нулевой слот записи — и с ленты пропало бы состояние, из
  которого во все позы входят.
- ЗАЯВКА, А НЕ КАРТИНКА. Клавиша пишет цель маршрута (pose_transit_begin) и
  флаг слоя; всё остальное — доли перехода и вес слоя — ведёт update_bodies.
  Приложение, которое ставило бы позу напрямую, стало бы вторым местом, где
  тело обретает вид, и разошлось бы с зоной на первом же перехвате.
- ПЕРЕХВАТ НА ПОЛПУТИ ЗАКОНЕН И ДЕШЁВ: pose_transit_begin строит маршрут от
  БЛИЖАЙШЕГО узла, поэтому быстрый перебор клавишей не гоняет тело обратно на
  пол за каждой нажатой скобкой.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Имена поз ЖИВУТ В РЕЕСТРЕ (engine/anim/sources/PoseLibrary.cpp). Второго
  списка имён здесь не заводить: доза ищет имя через anim::pose_by_name.
*/

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/PoseLibrary.h"
#include "engine/app/sources/App.h"
#include "engine/app/sources/AppStand.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace dfn::app {

namespace {

/// Сколько слотов в кольце: живое тело плюс каждая запись реестра.
[[nodiscard]] uint32_t pose_slot_count() { return anim::POSE_COUNT + 1; }

} // namespace

void App::step_pose(int delta) {
    const auto n = static_cast<int>(pose_slot_count());
    // По кольцу в обе стороны без отрицательного остатка.
    const int next = ((static_cast<int>(pose_slot_) + delta) % n + n) % n;
    set_pose_slot(static_cast<uint32_t>(next));
}

void App::set_pose_slot(uint32_t slot) {
    auto* drive = world_.get<anim::BodyDrive>(player_);
    if (drive == nullptr) {
        return;
    }
    pose_slot_ = slot % pose_slot_count();
    if (pose_slot_ == 0) {
        drive->pose_active = false;
        std::fprintf(stderr, "[pose] живое тело\n");
        return;
    }
    const auto id = static_cast<anim::PoseId>(pose_slot_ - 1);
    // ЕСЛИ СЛОЙ БЫЛ ВЫКЛЮЧЕН, тело входит В ПОЗУ ИЗ СТОЙКИ, а не из той
    // записи, на которой перебор стоял до выключения: маршрут обязан начаться
    // там, где тело сейчас видно.
    if (!drive->pose_active) {
        drive->pose_transit = anim::pose_transit_at(anim::PoseId::Stand);
    }
    anim::pose_transit_begin(drive->pose_transit, id);
    drive->pose_active = true;
    const anim::PoseRoute& r = drive->pose_transit.route;
    std::fprintf(stderr, "[pose] %.*s: маршрут в %u узлов, %.2f с\n",
                 static_cast<int>(anim::pose_name(id).size()), anim::pose_name(id).data(),
                 r.count, static_cast<double>(r.total_s()));
}

// НАПРАВЛЕНИЕ БЕРЁТСЯ У КЛАВИШИ, А НЕ У ВТОРОЙ СТРОКИ ТАБЛИЦЫ. Строка одна
// (см. Controls.h, Action::PoseCycle), и здесь единственное место, где видно,
// какая из двух скобок нажата. Спрашивается ЛЕВАЯ, а «иначе вперёд»: если обе
// лежат под пальцами, шаг назад — тот, который человек может отменить одним
// нажатием, а бесконечный список вперёд — нет.
void App::on_pose_cycle() {
    const bool back = input_ != nullptr && input_->is_down(platform::Key::LEFT_BRACKET);
    step_pose(back ? -1 : +1);
}

void App::tick_pose_tape(float dt) {
    if (!pose_tape_read_) {
        pose_tape_read_ = true;
        const char* on = std::getenv("DFN_POSE_TAPE");
        pose_tape_ = on != nullptr && *on == '1';
        if (pose_tape_) {
            std::fprintf(stderr, "[pose] лента: %u слотов, %.1f с\n", pose_slot_count(),
                         static_cast<double>(pose_tape_length_s(pose_slot_count())));
        }
    }
    if (!pose_tape_) {
        return;
    }
    pose_tape_t_ += dt;
    const uint32_t want = pose_tape_slot_at(pose_tape_t_, pose_slot_count());
    if (want != pose_slot_) {
        set_pose_slot(want);
    }
}

void App::apply_pose_dose() {
    if (pose_dose_done_) {
        return;
    }
    const char* want = std::getenv("DFN_POSE");
    if (want == nullptr || *want == '\0') {
        pose_dose_done_ = true;
        return;
    }
    auto* drive = world_.get<anim::BodyDrive>(player_);
    if (drive == nullptr) {
        return; // тела ещё нет — доза ждёт, а не теряется
    }
    pose_dose_done_ = true;
    bool found = false;
    const anim::PoseId id = anim::pose_by_name(std::string_view{want}, &found);
    if (!found) {
        // ГРОМКО И БЕЗ ПОДМЕНЫ: доза с опечаткой обязана быть видна, а не
        // молча снять кадр стойки, который потом уйдёт в отчёт как поза.
        std::fprintf(stderr, "[pose] DFN_POSE=%s: такой записи в реестре нет; позы будут:\n",
                     want);
        for (uint32_t i = 0; i < anim::POSE_COUNT; ++i) {
            const std::string_view n = anim::pose_name(static_cast<anim::PoseId>(i));
            std::fprintf(stderr, "        %.*s\n", static_cast<int>(n.size()), n.data());
        }
        return;
    }
    pose_slot_ = anim::pose_index(id) + 1;
    // ДОЗА СТАВИТ ПОЗУ СРАЗУ, а не ведёт в неё маршрутом: первый кадр
    // беспилотного прогона обязан уже показывать позу, иначе кадр приёмки
    // зависит от того, на какой секунде его сняли.
    drive->pose_transit = anim::pose_transit_at(id);
    drive->pose_active = true;
    drive->pose_weight = 1.0f;
    std::fprintf(stderr, "[pose] доза: %s\n", want);
}

} // namespace dfn::app
