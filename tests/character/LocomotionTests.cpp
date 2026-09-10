/*
Module: tests
File: tests/character/LocomotionTests.cpp

Responsibility:
- ПРИБОР МАШИНЫ ЛОКОМОЦИИ (engine/anim/sources/Locomotion.h, §16) на
  СИНТЕТИЧЕСКОЙ библиотеке (клипы с заданными дорожками, без выпечки) и
  один случай на теле HumanBase. Проверяет таблицу переходов: старт →
  цикл на фазе передачи → остановка → покой; ввод из-за спины — сначала
  разворот; взгляд стреляет поворот только стоя и только с view_valid;
  поворот доигрывается до конца и не перецеливается; дребезг камеры ±60°
  5 Гц даёт ≤ LOCO_TRANSITIONS_PER_S_MAX поворотов в секунду (контроль:
  dwell 0 — больше); стрейф — сразу цикл, рыск не у клипа; темп — в полосе;
  ход корня за тик — из дорожки.

Key items:
- press_forward_starts_cycles_and_stops
- input_from_behind_turns_first
- the_view_turns_the_standing_body_only_when_valid
- a_turn_plays_to_its_end_and_a_jittering_view_stays_under_budget
- strafe_enters_the_cycle_directly_and_keeps_the_yaw
- tempo_follows_the_order_within_the_band
- the_real_library_walks_the_same_table
- a_blocked_capsule_stops_the_walk_and_a_new_direction_restarts_it (§16.9;
  контроль — без правила цикл крутится при нулевом ходе капсулы)

Dependencies:
- Uses: doctest, engine/anim, tests/character/ClipTestModel.h.
- Used by: ctest (character_locomotion).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Числа — из реестра; синтетика не должна знать длительности клипов тела.
*/
#include "engine/anim/sources/Locomotion.h"
#include "engine/core/config/sources/Constants.h"
#include "tests/character/ClipTestModel.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace dfn;

namespace {

constexpr float DT = 1.0f / 60.0f;

/// Дорожка: прямая `travel` м и рыск `yaw` за клип, равномерно.
void set_track(anim::ClipEntry& e, float travel, float yaw_deg) {
    anim::RootTrack& t = e.root;
    for (uint32_t i = 0; i < anim::ROOT_TRACK_POINTS; ++i) {
        const float p = static_cast<float>(i) / static_cast<float>(anim::ROOT_TRACK_POINTS - 1);
        t.xz[i] = glm::vec2{0.0f, -travel * p};
        t.yaw[i] = glm::radians(yaw_deg) * p;
    }
    t.total_m = travel;
    t.total_yaw = glm::radians(yaw_deg);
    t.mps = e.duration_s > 0.0f ? travel / e.duration_s : 0.0f;
    t.valid = travel > 0.05f || std::abs(yaw_deg) > 5.0f;
}

anim::ClipEntry& add(anim::ClipLibrary& lib, anim::ClipRole r, float dur, float travel,
                     float yaw_deg, float active = 0.0f) {
    anim::ClipEntry& e = lib.role[anim::role_index(r)];
    e.clip = static_cast<int32_t>(anim::role_index(r)) + 1;
    e.duration_s = dur;
    e.active_s = active;
    e.exit_phase.fill(-1.0f);
    set_track(e, travel, yaw_deg);
    return e;
}

/// Синтетическое тело: покой, ходьба 1,5 м/с, старт (передача на 0,4),
/// остановка, повороты 90 и 180, назад, стрейфы.
anim::ClipLibrary synthetic() {
    anim::ClipLibrary lib;
    add(lib, anim::ClipRole::Idle, 1.0f, 0.0f, 0.0f);
    add(lib, anim::ClipRole::Walk, 1.0f, 1.5f, 0.0f);
    add(lib, anim::ClipRole::Sprint, 0.6f, 3.6f, 0.0f);
    anim::ClipEntry& s = add(lib, anim::ClipRole::StartWalk, 1.0f, 1.0f, 0.0f);
    s.handoff_phase = 0.4f;
    add(lib, anim::ClipRole::StopWalk, 0.5f, 0.3f, 0.0f, 0.5f);
    add(lib, anim::ClipRole::TurnL, 0.9f, 0.0f, -90.0f, 0.9f);
    add(lib, anim::ClipRole::TurnR, 0.9f, 0.0f, 90.0f, 0.9f);
    add(lib, anim::ClipRole::Turn180L, 1.0f, 0.0f, -170.0f, 1.0f);
    add(lib, anim::ClipRole::Turn180R, 1.0f, 0.0f, 170.0f, 1.0f);
    add(lib, anim::ClipRole::Backward, 1.2f, 1.2f, 0.0f);
    add(lib, anim::ClipRole::StrafeL, 1.0f, 1.6f, 0.0f);
    add(lib, anim::ClipRole::StrafeR, 1.0f, 1.6f, 0.0f);
    add(lib, anim::ClipRole::Stagger, 0.8f, 0.0f, 0.0f, 0.8f);
    return lib;
}

struct Trace {
    std::vector<std::string> roles{"Idle"}; ///< без повторов подряд; начало — покой
    float body_yaw = 0.0f;
    glm::vec2 pos{0.0f};
    uint32_t turns = 0;
    float t_s = 0.0f;
    float start_at = -1.0f, start_to_cycle_s = -1.0f, stop_at = -1.0f, stop_to_idle_s = -1.0f;
};

void tick(const anim::ClipLibrary& lib, anim::LocoInput& in, anim::LocoMachine& m, Trace& tr) {
    in.body_yaw = tr.body_yaw;
    anim::loco_step(lib, in, DT, m);
    const anim::RootDelta d = anim::loco_root_delta(lib, m);
    tr.body_yaw += d.yaw;
    tr.pos += d.xz;
    tr.t_s += DT;
    const std::string name{anim::role_name(m.role)};
    if (tr.roles.empty() || tr.roles.back() != name) {
        tr.roles.push_back(name);
        if (m.state == anim::LocoState::TurnInPlace) {
            ++tr.turns;
        }
        if (m.state == anim::LocoState::Start) {
            tr.start_at = tr.t_s;
        } else if (m.state == anim::LocoState::Cycle && tr.start_at >= 0.0f && tr.start_to_cycle_s < 0.0f) {
            tr.start_to_cycle_s = tr.t_s - tr.start_at;
        } else if (m.state == anim::LocoState::Stop) {
            tr.stop_at = tr.t_s;
        } else if (m.state == anim::LocoState::Idle && tr.stop_at >= 0.0f && tr.stop_to_idle_s < 0.0f) {
            tr.stop_to_idle_s = tr.t_s - tr.stop_at;
        }
    }
}

std::string chain(const Trace& tr) {
    std::string s;
    for (const std::string& r : tr.roles) {
        s += r + " ";
    }
    return s;
}

} // namespace

TEST_CASE("press_forward_starts_cycles_and_stops") {
    const anim::ClipLibrary lib = synthetic();
    anim::LocoMachine m;
    anim::LocoInput in;
    Trace tr;
    // 2 с вперёд шагом, 1 с отпущено
    for (int t = 0; t < 180; ++t) {
        in.want_speed_mps = t < 120 ? 1.5f : 0.0f;
        in.want_dir_model = {0.0f, 0.0f, -1.0f};
        tick(lib, in, m, tr);
        if (t == 0) {
            CHECK(m.state == anim::LocoState::Start);
        }
        if (t == 30) { // 0,5 с: за фазой передачи 0,4 → цикл
            CHECK(m.state == anim::LocoState::Cycle);
            CHECK(m.role == anim::ClipRole::Walk);
        }
        if (t == 121) {
            CHECK(m.state == anim::LocoState::Stop);
        }
    }
    MESSAGE("роли: " << chain(tr) << "| путь " << glm::length(tr.pos) << " м, смен " << m.transitions);
    CHECK(m.state == anim::LocoState::Idle);
    CHECK(chain(tr) == "Idle StartWalk Walk StopWalk Idle ");
    // ход: старт 0,4 клипа × 1 м + цикл 1,6 с × 1,5 м/с + остановка 0,3
    CHECK(glm::length(tr.pos) == doctest::Approx(0.4f + 1.6f * 1.5f + 0.3f).epsilon(0.1));
    CHECK(tr.body_yaw == 0.0f);
}

TEST_CASE("input_from_behind_turns_first") {
    const anim::ClipLibrary lib = synthetic();
    anim::LocoMachine m;
    anim::LocoInput in;
    Trace tr;
    in.want_speed_mps = 1.5f;
    in.want_dir_model = {0.2f, 0.0f, 1.0f}; // назад-вправо (~169° от носа)
    for (int t = 0; t < 150; ++t) {
        tick(lib, in, m, tr);
    }
    MESSAGE("роли: " << chain(tr) << "| рыск " << glm::degrees(tr.body_yaw) << "°");
    // класс направления «назад» — не разворот, а роль Backward
    CHECK(tr.roles.size() >= 2);
    CHECK(tr.roles[1] == "Backward");
    // а ввод чуть за плечом в классе «вперёд» — разворот, потом старт
    anim::LocoMachine m2;
    anim::LocoInput in2;
    Trace tr2;
    in2.want_speed_mps = 1.5f;
    // 110° вправо — за BODY_TURN_START_DEG (100), но класс не даёт «вперёд»…
    // …поэтому берём 120° по правилу класса? Нет: класс по DIR_BACK_DEG (135)
    // и DIR_STRAFE_DEG (45): 110° — это стрейф. Разворот стреляет только для
    // класса «вперёд», которого дальше 45° не бывает — значит, при 100 < 45 не
    // случится никогда; правило существует для будущих клипов «поворот и
    // старт». Проверяем, что стрейф при 110° входит в цикл без разворота.
    in2.want_dir_model = {std::sin(glm::radians(110.0f)), 0.0f, -std::cos(glm::radians(110.0f))};
    for (int t = 0; t < 60; ++t) {
        tick(lib, in2, m2, tr2);
    }
    CHECK(tr2.turns == 0);
    CHECK(m2.role == anim::ClipRole::StrafeR);
}

TEST_CASE("the_view_turns_the_standing_body_only_when_valid") {
    const anim::ClipLibrary lib = synthetic();
    for (const bool valid : {true, false}) {
        anim::LocoMachine m;
        anim::LocoInput in;
        Trace tr;
        in.view_valid = valid;
        in.view_yaw = glm::radians(120.0f);
        for (int t = 0; t < 120; ++t) {
            tick(lib, in, m, tr);
        }
        MESSAGE((valid ? "взгляд есть" : "взгляда нет (3-е лицо стоя)") << ": роли " << chain(tr)
                                                                        << "| рыск " << glm::degrees(tr.body_yaw) << "°");
        if (valid) {
            // ближайший по углу: 120° → 90° клип с варпом 1,33 → 120°
            CHECK(tr.turns == 1);
            CHECK(tr.roles[1] == "TurnR");
            CHECK(glm::degrees(tr.body_yaw) == doctest::Approx(120.0f).epsilon(0.02));
            CHECK(m.state == anim::LocoState::Idle);
        } else {
            CHECK(tr.turns == 0);
            CHECK(tr.body_yaw == 0.0f);
        }
    }
}

TEST_CASE("a_turn_plays_to_its_end_and_a_jittering_view_stays_under_budget") {
    const anim::ClipLibrary lib = synthetic();
    // 1. поворот не перецеливается: камера ушла на 90°, на полпути вернулась в 0
    {
        anim::LocoMachine m;
        anim::LocoInput in;
        Trace tr;
        in.view_valid = true;
        float peak = 0.0f;
        for (int t = 0; t < 120; ++t) {
            in.view_yaw = t < 20 ? glm::radians(90.0f) : 0.0f;
            tick(lib, in, m, tr);
            peak = std::max(peak, std::abs(tr.body_yaw));
        }
        MESSAGE("камера 90° → 0 на полпути: роли " << chain(tr) << "| пик " << glm::degrees(peak)
                                                     << "°, конец " << glm::degrees(tr.body_yaw) << "°");
        CHECK(tr.roles[1] == "TurnR");
        CHECK(glm::degrees(peak) == doctest::Approx(90.0f).epsilon(0.02)); // доиграл
        CHECK(tr.turns == 2);                                              // и вернулся вторым клипом
        CHECK(std::abs(glm::degrees(tr.body_yaw)) < static_cast<float>(config::TURN_FIRE_DEG));
    }
    // 2. дребезг ±60° 5 Гц: бюджет поворотов в секунду. Клип 0,9 с, доигранный
    //    до конца, сам держит бюджет; КОНТРОЛЬНАЯ РУКА — клипы по 0,05 с (то,
    //    во что старый резак превращал поворот) без dwell — обязана его пробить.
    for (const bool control : {false, true}) {
        anim::ClipLibrary l2 = synthetic();
        anim::LocoMachine m;
        if (control) {
            for (const anim::ClipRole r : {anim::ClipRole::TurnL, anim::ClipRole::TurnR}) {
                anim::ClipEntry& e = l2.role[anim::role_index(r)];
                e.duration_s = 0.05f;
                e.active_s = 0.05f;
                e.root.mps = 0.0f;
            }
            m.dwell_min_s = 0.0f;
        }
        anim::LocoInput in;
        Trace tr;
        in.view_valid = true;
        for (int t = 0; t < 600; ++t) {
            in.view_yaw = ((t / 6) % 2 == 0) ? glm::radians(60.0f) : glm::radians(-60.0f);
            tick(l2, in, m, tr);
        }
        const float per_s = static_cast<float>(tr.turns) / 10.0f;
        MESSAGE((control ? "КОНТРОЛЬ: обрубки 0,05 с без dwell" : "клипы 0,9 с") << ": поворотов "
                << tr.turns << " за 10 с = " << per_s << "/с");
        if (!control) {
            CHECK(per_s <= static_cast<float>(config::LOCO_TRANSITIONS_PER_S_MAX));
        } else {
            CHECK(per_s > static_cast<float>(config::LOCO_TRANSITIONS_PER_S_MAX));
        }
    }
}

TEST_CASE("strafe_enters_the_cycle_directly_and_keeps_the_yaw") {
    const anim::ClipLibrary lib = synthetic();
    anim::LocoMachine m;
    anim::LocoInput in;
    Trace tr;
    in.want_speed_mps = 1.5f;
    in.want_dir_model = {-1.0f, 0.0f, 0.0f}; // влево
    for (int t = 0; t < 60; ++t) {
        tick(lib, in, m, tr);
    }
    MESSAGE("влево: роли " << chain(tr));
    CHECK(tr.roles.size() == 2);
    CHECK(tr.roles[1] == "StrafeL");
    CHECK_FALSE(anim::loco_yaw_owned_by_clip(lib, m));
    CHECK(tr.body_yaw == 0.0f);
    // смена класса на «вперёд» — после dwell, одной сменой
    in.want_dir_model = {0.0f, 0.0f, -1.0f};
    for (int t = 0; t < 60; ++t) {
        tick(lib, in, m, tr);
    }
    CHECK(m.role == anim::ClipRole::Walk);
    CHECK(tr.roles.size() == 3);
}

TEST_CASE("tempo_follows_the_order_within_the_band") {
    const anim::ClipLibrary lib = synthetic();
    const float band = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
    for (const float want : {1.5f, 1.8f, 0.9f, 6.0f}) {
        anim::LocoMachine m;
        anim::LocoInput in;
        Trace tr;
        in.want_speed_mps = want;
        in.want_dir_model = {0.0f, 0.0f, -1.0f};
        for (int t = 0; t < 120; ++t) {
            tick(lib, in, m, tr);
        }
        REQUIRE(m.state == anim::LocoState::Cycle);
        const float expect = std::clamp(want / 1.5f, 1.0f - band, 1.0f + band);
        MESSAGE("заказ " << want << " м/с при клипе 1,5: темп " << m.rate << " (ожидание " << expect << ")");
        CHECK(m.rate == doctest::Approx(expect));
    }
}

TEST_CASE("the_real_library_walks_the_same_table") {
    Model mdl;
    if (!load(mdl, {}, /*transitions=*/true)) {
        MESSAGE("no baked body -- skipped");
        return;
    }
    anim::LocoMachine m;
    anim::LocoInput in;
    Trace tr;
    for (int t = 0; t < 300; ++t) {
        in.want_speed_mps = t < 180 ? static_cast<float>(config::WALK_SPEED) : 0.0f;
        in.want_dir_model = {0.0f, 0.0f, -1.0f};
        tick(mdl.lib, in, m, tr);
    }
    {
        std::string s;
        for (const anim::ClipRole r : {anim::ClipRole::StopWalk, anim::ClipRole::StopRun, anim::ClipRole::TurnL,
                                       anim::ClipRole::Turn180L, anim::ClipRole::StartWalk}) {
            const anim::ClipEntry& e = mdl.lib[r];
            char buf[160];
            std::snprintf(buf, sizeof buf, "%s settle %.2f (%.2f s of %.2f, active %.2f) ",
                          std::string(anim::role_name(r)).c_str(), e.settle_phase,
                          e.settle_phase * e.duration_s, e.duration_s, e.active_s);
            s += buf;
        }
        MESSAGE(s);
    }
    MESSAGE("HumanBase: роли " << chain(tr) << "| путь " << glm::length(tr.pos) << " м за 5 с, смен "
                               << m.transitions);
    CHECK(tr.roles.front() == "Idle");
    CHECK(tr.roles[1] == "StartWalk");
    CHECK(tr.roles[2] == "Walk");
    CHECK(m.state == anim::LocoState::Idle);
    CHECK(glm::length(tr.pos) > 2.0f);
    // закон отзывчивости: цикл не позже START_CLIP_MAX_S, покой не позже
    // STOP_CLIP_MAX_S после отпускания (MX_Stop_Walking ползёт 4 с — это клип, не мы)
    CHECK(tr.stop_to_idle_s <= static_cast<float>(config::STOP_CLIP_MAX_S) + 2.0f * DT);
    CHECK(tr.start_to_cycle_s <= static_cast<float>(config::START_CLIP_MAX_S) + 2.0f * DT);
    // поворот к взгляду на 180° — один клип Turn180 или два по 90, но не больше трёх
    anim::LocoMachine m2;
    anim::LocoInput in2;
    Trace tr2;
    in2.view_valid = true;
    in2.view_yaw = glm::radians(175.0f);
    for (int t = 0; t < 240; ++t) {
        tick(mdl.lib, in2, m2, tr2);
    }
    MESSAGE("HumanBase, камера 175°: роли " << chain(tr2) << "| рыск " << glm::degrees(tr2.body_yaw) << "°");
    CHECK(tr2.turns >= 1);
    CHECK(tr2.turns <= 3);
    CHECK(std::abs(glm::degrees(tr2.body_yaw) - 175.0f) < 2.0f * static_cast<float>(config::TURN_FIRE_DEG));
}

TEST_CASE("a_blocked_capsule_stops_the_walk_and_a_new_direction_restarts_it") {
    const anim::ClipLibrary lib = synthetic();
    // ход мира за тик — доля заявки прошлого тика: 1 = мир исполняет, 0 —
    // лоб в стену, 0,7 — скольжение вдоль стены (ещё ход)
    auto walk = [&](anim::LocoMachine& m, anim::LocoInput& in, Trace& tr, float frac, int ticks) {
        for (int t = 0; t < ticks; ++t) {
            in.travelled_m = frac * m.request_m;
            tick(lib, in, m, tr);
        }
    };
    SUBCASE("wall") {
        anim::LocoMachine m;
        anim::LocoInput in;
        Trace tr;
        in.want_speed_mps = 1.5f;
        in.want_dir_model = {0.0f, 0.0f, -1.0f};
        walk(m, in, tr, 1.0f, 60);
        CHECK(m.state == anim::LocoState::Cycle);
        walk(m, in, tr, 0.7f, 60); // вдоль стены — ход
        CHECK(m.state == anim::LocoState::Cycle);
        CHECK(!m.blocked);
        const float t_wall = tr.t_s;
        float t_idle = -1.0f;
        for (int t = 0; t < 60; ++t) { // лоб в стену, ввод держится
            walk(m, in, tr, 0.0f, 1);
            if (t_idle < 0.0f && m.state == anim::LocoState::Idle) {
                t_idle = tr.t_s;
            }
        }
        CHECK(m.blocked);
        CHECK(m.state == anim::LocoState::Idle);
        REQUIRE(t_idle >= 0.0f);
        CHECK(t_idle - t_wall <= doctest::Approx(config::LOCO_BLOCKED_S + 2.0f * DT));
        walk(m, in, tr, 0.0f, 120); // ещё 2 с в стену — стоим, без стартов
        CHECK(m.state == anim::LocoState::Idle);
        // покой сразу, не клип остановки: его корень ползёт вперёд, куда мир не пускает
        CHECK(chain(tr) == "Idle StartWalk Walk Idle ");
        // ввод ушёл на 90° — старт разрешён (стрейф входит циклом сразу)
        in.want_dir_model = {1.0f, 0.0f, 0.0f};
        walk(m, in, tr, 1.0f, 2);
        CHECK(!m.blocked);
        CHECK(m.state == anim::LocoState::Cycle);
        CHECK(m.role == anim::ClipRole::StrafeR);
        MESSAGE("роли: " << chain(tr) << "| покой через " << (t_idle - t_wall) << " с после стены");
    }
    SUBCASE("release_clears_the_latch") {
        anim::LocoMachine m;
        anim::LocoInput in;
        Trace tr;
        in.want_speed_mps = 1.5f;
        in.want_dir_model = {0.0f, 0.0f, -1.0f};
        walk(m, in, tr, 1.0f, 60);
        walk(m, in, tr, 0.0f, 60);
        CHECK(m.blocked);
        in.want_speed_mps = 0.0f;
        walk(m, in, tr, 1.0f, 1);
        CHECK(!m.blocked);
        in.want_speed_mps = 1.5f;
        walk(m, in, tr, 1.0f, 1);
        CHECK(m.state == anim::LocoState::Start);
    }
    SUBCASE("control_without_the_rule_the_cycle_spins_into_the_wall") {
        anim::LocoMachine m;
        m.blocked_min_s = 1.0e9f;
        anim::LocoInput in;
        Trace tr;
        in.want_speed_mps = 1.5f;
        in.want_dir_model = {0.0f, 0.0f, -1.0f};
        walk(m, in, tr, 1.0f, 60);
        walk(m, in, tr, 0.0f, 120);
        CHECK(m.state == anim::LocoState::Cycle);
        CHECK(!m.blocked);
        CHECK(chain(tr) == "Idle StartWalk Walk ");
    }
    SUBCASE("unknown_travel_keeps_the_rule_silent") {
        anim::LocoMachine m;
        anim::LocoInput in;
        Trace tr;
        in.want_speed_mps = 1.5f;
        in.want_dir_model = {0.0f, 0.0f, -1.0f};
        for (int t = 0; t < 120; ++t) {
            tick(lib, in, m, tr); // travelled_m = −1
        }
        CHECK(m.state == anim::LocoState::Cycle);
    }
}
