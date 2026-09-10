/*
Module: engine/anim
File: engine/anim/sources/Locomotion.cpp

Responsibility:
- Реализация машины состояний локомоции (Locomotion.h): таблица переходов,
  часы клипа, темп в полосе, выбор поворота и варп.

Key items:
- enter(): вход в состояние — единственное место, где меняется роль.
- loco_step(): см. заголовок; события конца клипа читаются по фазе.

Dependencies:
- Uses: Locomotion.h, Constants.h.
- Used by: SkinnedCharacter, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Порог «ввод есть» — тот же MOVING_SPEED_MPS, что у ролей.
*/
#include "engine/anim/sources/Locomotion.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {

namespace {

constexpr float INPUT_MPS = 0.15f; ///< как MOVING_SPEED_MPS у ролей

[[nodiscard]] float wrap_pi_l(float a) {
    return std::atan2(std::sin(a), std::cos(a));
}

[[nodiscard]] float dwell_min(const LocoMachine& m) {
    return m.dwell_min_s >= 0.0f ? m.dwell_min_s : static_cast<float>(config::LOCO_STATE_DWELL_S);
}

[[nodiscard]] ClipRole cycle_role(const ClipLibrary& lib, Gait gait, MoveDir dir) {
    const ClipRole fwd = role_for_gait(gait);
    const bool run = gait != Gait::Walk;
    ClipRole want = fwd;
    switch (dir) {
    case MoveDir::Backward: want = ClipRole::Backward; break;
    case MoveDir::StrafeL:
        want = run && lib.has(ClipRole::StrafeRunL) ? ClipRole::StrafeRunL : ClipRole::StrafeL;
        break;
    case MoveDir::StrafeR:
        want = run && lib.has(ClipRole::StrafeRunR) ? ClipRole::StrafeRunR : ClipRole::StrafeR;
        break;
    case MoveDir::Forward: break;
    }
    if (lib.has(want)) {
        return want;
    }
    return lib.has(fwd) ? fwd : ClipRole::Idle;
}

[[nodiscard]] ClipRole start_role(const ClipLibrary& lib, Gait gait) {
    const ClipRole r = gait == Gait::Walk ? ClipRole::StartWalk : ClipRole::StartRun;
    return lib.has(r) ? r : ClipRole::Idle;
}

[[nodiscard]] ClipRole stop_role(const ClipLibrary& lib, Gait gait) {
    const ClipRole r = gait == Gait::Walk ? ClipRole::StopWalk : ClipRole::StopRun;
    return lib.has(r) ? r : ClipRole::Idle;
}

/// Угол ввода от носа корпуса (рад, знак сим'а: вправо +).
/// Темп цикла: заказ / скорость дорожки, в полосе LOCOMOTION_TEMPO_BAND.
[[nodiscard]] float tempo_for(const ClipEntry& e, float want_mps) {
    const float band = static_cast<float>(config::LOCOMOTION_TEMPO_BAND);
    if (!e.root.valid || e.root.mps <= 1.0e-3f || want_mps <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(want_mps / e.root.mps, 1.0f - band, 1.0f + band);
}

/// Конец одноразового клипа по фазе: где кончилось движение по дорожке
/// (settle_phase), иначе active_s, иначе вся длительность.
[[nodiscard]] float end_phase(const ClipEntry& e) {
    if (e.duration_s <= 0.0f) {
        return 1.0f;
    }
    if (e.settle_phase > 0.0f) {
        return e.settle_phase;
    }
    return e.active_s > 0.0f ? std::min(1.0f, e.active_s / e.duration_s) : 1.0f;
}

/// ВХОД В СОСТОЯНИЕ — единственное место смены роли и часов.
void enter(const ClipLibrary& lib, LocoMachine& m, LocoState state, ClipRole role, float phase) {
    if (state != m.state || role != m.role) {
        ++m.transitions;
    }
    m.state = state;
    m.role = role;
    m.phase = phase;
    m.prev_phase = phase;
    m.dwell_s = 0.0f;
    m.entered = true;
    // петля — всё, что не одноразовое (locomotion_role() включает и переходы)
    m.cyclic = !one_shot_role(role);
    if (state != LocoState::TurnInPlace) {
        m.turn_warp = 1.0f;
        m.turn_sign = 0;
        m.turn_want_rad = 0.0f;
    }
    if (state != LocoState::Cycle) {
        m.rate = 1.0f;
    }
    (void)lib;
}

/// Фаза цикла новой роли, сохраняющая положение стопы: сдвиг по постановке.
[[nodiscard]] float matched_cycle_phase(const ClipEntry& from, const ClipEntry& to, float phase) {
    const float p = phase - from.footfall_phase + to.footfall_phase;
    return p - std::floor(p);
}

void enter_turn(const ClipLibrary& lib, LocoMachine& m, float want) {
    const TurnPick pick = loco_pick_turn(lib, want);
    if (!pick.ok) {
        return;
    }
    enter(lib, m, LocoState::TurnInPlace, pick.role, 0.0f);
    m.turn_want_rad = want;
    m.turn_warp = pick.warp;
    m.turn_sign = want > 0.0f ? 1 : -1;
}

/// Старт хода из покоя: вперёд — клип старта (если есть), иначе цикл сразу.
void enter_move(const ClipLibrary& lib, LocoMachine& m, const LocoInput& in) {
    m.dir = move_dir_class(in.want_dir_model, MoveDir::Forward);
    if (m.dir == MoveDir::Forward && lib.transitions) {
        const ClipRole s = start_role(lib, in.gait);
        if (s != ClipRole::Idle) {
            enter(lib, m, LocoState::Start, s, 0.0f);
            return;
        }
    }
    const ClipRole c = cycle_role(lib, in.gait, m.dir);
    enter(lib, m, LocoState::Cycle, c, 0.0f);
    m.rate = tempo_for(lib[c], in.want_speed_mps);
}

void enter_stop(const ClipLibrary& lib, LocoMachine& m, const LocoInput& in) {
    const ClipRole s = stop_role(lib, in.gait);
    if (s != ClipRole::Idle && m.dir == MoveDir::Forward && lib.transitions) {
        enter(lib, m, LocoState::Stop, s, 0.0f);
    } else {
        enter(lib, m, LocoState::Idle, ClipRole::Idle, 0.0f);
    }
}

} // namespace

const char* loco_state_name(LocoState s) {
    switch (s) {
    case LocoState::Idle: return "Idle";
    case LocoState::TurnInPlace: return "TurnInPlace";
    case LocoState::Start: return "Start";
    case LocoState::Cycle: return "Cycle";
    case LocoState::Stop: return "Stop";
    case LocoState::Stagger: return "Stagger";
    case LocoState::Air: return "Air";
    }
    return "?";
}

TurnPick loco_pick_turn(const ClipLibrary& lib, float want) {
    TurnPick best;
    const float amag = std::abs(want);
    if (amag < 1.0e-4f) {
        return best;
    }
    const float wmin = static_cast<float>(config::TURN_WARP_MIN);
    const float wmax = static_cast<float>(config::TURN_WARP_MAX);
    float best_cost = 1.0e9f;
    for (const ClipRole r : {ClipRole::TurnL, ClipRole::TurnR, ClipRole::Turn180L, ClipRole::Turn180R}) {
        const ClipEntry& e = lib[r];
        if (!e.present() || !e.root.valid || std::abs(e.root.total_yaw) < 1.0e-3f) {
            continue;
        }
        if ((e.root.total_yaw > 0.0f) != (want > 0.0f)) {
            continue; // не тот знак
        }
        const float raw = amag / std::abs(e.root.total_yaw);
        const float warp = std::clamp(raw, wmin, wmax);
        // цена — насколько варп далёк от 1 (и сколько не покрыто предел)
        const float cost = std::abs(std::log(raw));
        if (cost < best_cost) {
            best_cost = cost;
            best.role = r;
            best.warp = warp;
            best.ok = true;
        }
    }
    return best;
}

bool loco_yaw_owned_by_clip(const ClipLibrary& lib, const LocoMachine& m) {
    if (m.state == LocoState::TurnInPlace) {
        return true;
    }
    const ClipEntry& e = lib[m.role];
    return e.present() && e.root.valid && std::abs(e.root.total_yaw) > glm::radians(5.0f);
}

RootDelta loco_root_delta(const ClipLibrary& lib, const LocoMachine& m) {
    const ClipEntry& e = lib[m.role];
    if (!e.present() || !e.root.valid) {
        return RootDelta{};
    }
    return root_track_delta(e.root, m.prev_phase, m.phase, m.cyclic,
                            m.state == LocoState::TurnInPlace ? m.turn_warp : 1.0f);
}

void loco_step(const ClipLibrary& lib, const LocoInput& in, float dt, LocoMachine& m) {
    m.entered = false;
    m.prev_phase = m.phase;
    m.dwell_s += dt;
    m.since_stagger_s += dt;
    const bool input = in.want_speed_mps > INPUT_MPS;
    const float dmin = dwell_min(m);

    // --- ЧАСЫ ТЕКУЩЕГО КЛИПА -----------------------------------------------
    const ClipEntry& cur = lib[m.role];
    if (cur.present() && cur.duration_s > 0.0f) {
        const float adv = dt * m.rate / cur.duration_s;
        if (m.cyclic) {
            m.phase += adv;
            if (m.phase >= 1.0f) {
                m.phase -= std::floor(m.phase);
            }
        } else {
            m.phase = std::min(1.0f, m.phase + adv);
        }
    }

    // --- ВОЗДУХ И УДАР — вне очереди ----------------------------------------
    if (!in.grounded) {
        if (m.state != LocoState::Air) {
            enter(lib, m, LocoState::Air, lib.has(ClipRole::JumpLoop) ? ClipRole::JumpLoop : ClipRole::Idle,
                  0.0f);
        }
        return;
    }
    if (m.state == LocoState::Air) {
        // приземлились: ход продолжается циклом, иначе покой
        if (input) {
            m.dir = move_dir_class(in.want_dir_model, MoveDir::Forward);
            const ClipRole c = cycle_role(lib, in.gait, m.dir);
            enter(lib, m, LocoState::Cycle, c, 0.0f);
            m.rate = tempo_for(lib[c], in.want_speed_mps);
        } else {
            enter(lib, m, LocoState::Idle, ClipRole::Idle, 0.0f);
        }
        return;
    }
    if (in.push_mps >= static_cast<float>(config::STAGGER_PUSH_MPS) && lib.has(ClipRole::Stagger)
        && m.state != LocoState::Stagger
        && m.since_stagger_s >= static_cast<float>(config::STAGGER_MIN_GAP_S)) {
        enter(lib, m, LocoState::Stagger, ClipRole::Stagger, 0.0f);
        m.since_stagger_s = 0.0f;
        return;
    }

    switch (m.state) {
    case LocoState::Idle: {
        if (input) {
            // ВВОД ДАЛЬШЕ BODY_TURN_START_DEG ОТ КОРПУСА — СНАЧАЛА РАЗВОРОТ КЛИПОМ,
            // ПОТОМ СТАРТ (§16.4). «Дальше» мерится по ВЗГЛЯДУ: от третьего лица
            // взгляд на ходу = направление ввода (тело идёт, куда просят), от
            // первого прицел привязан к корпусу ближе порога и не стреляет; у
            // НПС взгляд — заказ исполнителя (Face/MoveTo). Класс направления
            // тут ни при чём: назад-вправо при корпусе по взгляду — Backward.
            if (lib.transitions && in.view_valid) {
                const float d = wrap_pi_l(in.view_yaw - in.body_yaw);
                if (std::abs(d) > glm::radians(static_cast<float>(config::BODY_TURN_START_DEG))) {
                    enter_turn(lib, m, d);
                    if (m.state == LocoState::TurnInPlace) {
                        return;
                    }
                }
            }
            enter_move(lib, m, in);
            return;
        }
        if (lib.transitions && in.view_valid && m.dwell_s >= dmin) {
            const float d = wrap_pi_l(in.view_yaw - in.body_yaw);
            if (std::abs(d) > glm::radians(static_cast<float>(config::TURN_FIRE_DEG))) {
                enter_turn(lib, m, d);
            }
        }
        return;
    }
    case LocoState::TurnInPlace: {
        if (m.phase < end_phase(cur)) {
            return; // доигрывается ДО КОНЦА, ни порог, ни новый ввод не режут
        }
        if (input) {
            enter_move(lib, m, in);
            return;
        }
        // остаток — следующим поворотом после dwell (не перецеливая этот)
        enter(lib, m, LocoState::Idle, ClipRole::Idle, 0.0f);
        return;
    }
    case LocoState::Start: {
        if (!input) {
            enter_stop(lib, m, in);
            return;
        }
        // ЗАКОН ОТЗЫВЧИВОСТИ (владелец 07.09): старт отдаёт ход циклу на фазе
        // передачи, но не позже START_CLIP_MAX_S — MX_Start_Walking выходит на
        // скорость цикла к 1,9 с, а «нажал — сразу пошёл» не терпит и секунды.
        float handoff = cur.handoff_phase > 0.0f ? cur.handoff_phase : end_phase(cur);
        if (cur.duration_s > 0.0f) {
            handoff = std::min(handoff, static_cast<float>(config::START_CLIP_MAX_S) / cur.duration_s);
        }
        if (m.phase >= handoff) {
            const ClipRole c = cycle_role(lib, in.gait, m.dir);
            const float ep = cur.exit_phase[role_index(c)];
            enter(lib, m, LocoState::Cycle, c, ep >= 0.0f ? ep : 0.0f);
            m.rate = tempo_for(lib[c], in.want_speed_mps);
        }
        return;
    }
    case LocoState::Cycle: {
        if (!input) {
            enter_stop(lib, m, in);
            return;
        }
        m.rate = tempo_for(cur, in.want_speed_mps);
        // смена передачи — сразу, с сохранением стопы; смена класса
        // направления — если продержалась dwell
        const MoveDir cls = move_dir_class(in.want_dir_model, m.dir);
        const ClipRole want = cycle_role(lib, in.gait, cls);
        if (want != m.role) {
            const bool dir_change = cls != m.dir;
            if (!dir_change || m.dwell_s >= dmin) {
                const float ph = matched_cycle_phase(cur, lib[want], m.phase);
                m.dir = cls;
                enter(lib, m, LocoState::Cycle, want, ph);
                m.rate = tempo_for(lib[want], in.want_speed_mps);
            }
        }
        return;
    }
    case LocoState::Stop: {
        if (input && m.dwell_s >= dmin) {
            enter_move(lib, m, in);
            return;
        }
        // …и остановка — не дольше STOP_CLIP_MAX_S: у MX_Stop_Walking корень
        // ползёт 0,2…0,5 м/с четыре секунды (замер 10.09), «отпустил — сразу
        // встал» это не терпит; остаток гасит инерциализация в покой.
        float end = end_phase(cur);
        if (cur.duration_s > 0.0f) {
            end = std::min(end, static_cast<float>(config::STOP_CLIP_MAX_S) / cur.duration_s);
        }
        if (m.phase >= end) {
            enter(lib, m, LocoState::Idle, ClipRole::Idle, 0.0f);
        }
        return;
    }
    case LocoState::Stagger: {
        if (m.phase >= end_phase(cur)) {
            if (input) {
                enter_move(lib, m, in);
            } else {
                enter(lib, m, LocoState::Idle, ClipRole::Idle, 0.0f);
            }
        }
        return;
    }
    case LocoState::Air:
        return;
    }
}

void rotate_root_joints(const skel::Skeleton& skeleton, std::span<const int32_t> roots, float yaw,
                        std::span<JointLocal> sample) {
    if (sample.size() < skeleton.size() || std::abs(yaw) < 1.0e-6f) {
        return;
    }
    // Рыск сим'а растёт по часовой, а glm::rotate(+θ, Y) поворачивает против —
    // поэтому «вычесть yaw из позы» это повернуть её на +yaw в glm.
    const glm::quat turn = glm::angleAxis(yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    for (const int32_t r : roots) {
        if (r < 0 || static_cast<std::size_t>(r) >= sample.size()) {
            continue;
        }
        JointLocal& jl = sample[static_cast<std::size_t>(r)];
        jl.rotation = turn * glm::normalize(jl.rotation);
        jl.translation = turn * jl.translation;
    }
}

} // namespace dfn::anim
