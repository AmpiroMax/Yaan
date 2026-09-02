/*
Module: engine/anim
File: engine/anim/sources/RootMotion.cpp

Responsibility:
- Арифметика корневого движения от опорной стопы и постановок (RootMotion.h).

Key items:
- root_motion_step(), detect_footfalls().

Dependencies:
- Uses: RootMotion.h, реестр (FOOT_SWING_SPEED_MPS), glm.
- Used by: engine/app (SkinnedCharacter), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ТРИ ОТСЕЧКИ ВЕСА, И КАЖДАЯ — ЗАМЕР, А НЕ ВКУС (прибор character_clips_slide,
  трасса спринта по сэмплам): (1) высота — самая низкая стопа ведёт, вторая в
  полосе над ней (FOOT_SUPPORT_BAND_M); (2) стопа, идущая ВПЕРЁД — мах, даже
  по земле (клипы машут под землёй, FOOT_SWING_SPEED_MPS); (3) стопа, что
  отстаёт от скорости тела вдвое, — садится или отрывается, её Δp — не ход
  тела: без этого спринт коастил на скорости отклеивания (2,6 м/с при 9).
- ОДИН И ТОТ ЖЕ СУСТАВ в обеих позах: нижняя точка внутри опоры переключается
  с пятки на носок, и разница «носок минус пятка» — длина стопы, а не ход.
*/

#include "engine/anim/sources/RootMotion.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>

namespace dfn::anim {

namespace {

[[nodiscard]] float smooth01(float u) {
    u = std::clamp(u, 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

/// Δp одного и того же сустава стопы между позами (носок, если он нижний
/// сейчас, иначе лодыжка).
[[nodiscard]] glm::vec3 same_joint_delta(const ContactState& prev, const ContactState& curr,
                                         std::size_t side) {
    return curr.toe_point[side] ? curr.toe[side] - prev.toe[side]
                                : curr.ankle[side] - prev.ankle[side];
}

/// Скорость тела помнится как спадающий пик: одна секунда — дольше любого
/// полёта, короче любой остановки.
constexpr float SPEED_MEMORY_S = 1.0f;
/// Память среднего хода опоры — один шаг ходьбы.
constexpr float STEP_MEMORY_S = 0.25f;

} // namespace

/// Окно смены опоры, тиков сима: столько тело едет по инерции опоры, пока
/// новая стопа не взяла вес; дольше — стоп (клавиши отпущены).
constexpr uint32_t HANDOFF_TICKS = 5;

glm::vec3 root_motion_step(const ContactState& prev, ContactState& curr, float dt,
                           RootMotionState& state, const glm::vec3& travel) {
    if (!prev.valid || !curr.valid || dt <= 0.0f) {
        return glm::vec3{0.0f};
    }
    const float swing = static_cast<float>(config::FOOT_SWING_SPEED_MPS);
    // НА ЗЕМЛЕ ЛИ ХОТЬ ОДНА СТОПА — по высоте, ДО отсечек движением: инерция
    // полёта положена только полёту. Стоящее тело (обе стопы на земле, ни
    // одна не идёт назад) обязано дать ноль, а не прошлый ход: владелец убрал
    // руки с клавиатуры — персонаж ехал вперёд на скорости последней опоры.
    // ПОЛЁТ — КОГДА ОБЕ СТОПЫ ВЫШЕ ПОЛОСЫ ОПОРЫ, а не выше высоты отпускания
    // IK (12 см): в беге трусцой нижняя стопа в полёте поднимается меньше, и
    // тело на эти тики вставало (скорость 0 → 4 м/с через тик — «дёрганый»
    // бег). На земле без опорной стопы (клавиши отпущены, клип сходит к
    // покою) инерции по-прежнему нет.
    std::array<glm::vec3, 2> d{};
    const std::size_t lowest = curr.height[0] <= curr.height[1] ? 0 : 1;
    for (std::size_t side = 0; side < 2; ++side) {
        d[side] = same_joint_delta(prev, curr, side);
    }
    // ...И ОТРЫВ — ТОЖЕ ПОЛЁТ: нижняя стопа ещё в полосе, но уже идёт вперёд
    // (толчковая уходит в мах) — тело летит, а не стоит.
    // ОСЬ ХОДА (§9.3): «вперёд» стопы — против оси travel, «назад под телом» —
    // вдоль неё; для хода вперёд travel = +Z, для стрейфа — ±X, назад — −Z.
    const bool on_ground = curr.height[lowest] <= static_cast<float>(config::FOOT_SUPPORT_BAND_M)
                           && -glm::dot(d[lowest], travel) / dt <= swing;
    for (std::size_t side = 0; side < 2; ++side) {
        const float forward = -glm::dot(d[side], travel) / dt;
        // (2) мах: вперёд быстрее порога — не опора
        curr.support[side] *= 1.0f - smooth01(forward / swing);
        // (3) отстающая стопа: назад медленнее половины скорости тела — садится
        // или отрывается; полосу держит та же смягчённая ступень
        if (state.speed_est_mps > 1.0e-3f) {
            const float backward = -forward;
            curr.support[side] *= smooth01((backward / state.speed_est_mps - 0.25f) / 0.25f);
        }
    }
    // ТЕЛО НЕСЁТ ТА СТОПА, ЧТО УХОДИТ НАЗАД БЫСТРЕЕ. На смене опоры
    // приземлившаяся стопа ещё гасит ход (в ходьбе UAL — с 0,4 до 1,3 м/с за
    // несколько тиков), а полоса высоты уже даёт ей полный вес: среднее
    // проваливалось за тик с 1,3 до 0,4 м/с — владелец 02.09-2: «шаги очень
    // дёрганные». Стопа медленнее 60 % самой быстрой опорной теряет вес,
    // быстрее 90 % — держит; тело едет с толчковой, пока она не оторвётся.
    {
        float back_max = 0.0f;
        for (std::size_t side = 0; side < 2; ++side) {
            if (std::min(prev.support[side], curr.support[side]) > 1.0e-4f) {
                back_max = std::max(back_max, glm::dot(d[side], travel) / dt);
            }
        }
        if (back_max > swing) {
            for (std::size_t side = 0; side < 2; ++side) {
                const float back = glm::dot(d[side], travel) / dt;
                curr.support[side] *= smooth01((back / back_max - 0.6f) / 0.3f);
            }
        }
    }
    glm::vec3 sum{0.0f};
    float total = 0.0f;
    for (std::size_t side = 0; side < 2; ++side) {
        const float w = std::min(prev.support[side], curr.support[side]);
        if (w <= 1.0e-4f) {
            continue;
        }
        sum += w * glm::vec3{d[side].x, 0.0f, d[side].z};
        total += w;
    }
    // ИНЕРЦИЯ: средний ход этого окна опоры; после полёта окно пусто — коаст.
    // (Пробовал брать только сильные тики опоры — в беге-смеси тело скакало
    // 7,8 ↔ 4,5 м/с: у клипа-смеси стопа за опору сама замедляется, и «сильный»
    // ход раннего касания не был ходом тела.)
    const auto momentum = [&]() -> glm::vec3 {
        return state.window_has ? state.window_mean : state.coast;
    };
    const auto remember = [&](const glm::vec3& d0) {
        if (!state.window_has) {
            state.window_mean = d0;
            state.window_has = true;
        } else {
            state.window_mean = glm::mix(state.window_mean, d0, std::min(1.0f, dt / STEP_MEMORY_S));
        }
    };
    glm::vec3 delta;
    if (total > 0.05f) {
        delta = -sum / total;
        // СМЕНА ОПОРЫ: единственная опорная стопа — приземлившаяся пятка, что
        // ещё гасит ход (0,4 м/с при теле 1,3), и среднее из одной стопы
        // весом не поправить: провал за тик 1,3 → 0,4 → 1,3 — «дёрганые
        // шаги». Пока опора слабая (Σw < 1) и не дольше окна смены (3 тика),
        // тело едет по инерции текущей опоры; дольше — стопам виднее (стоп,
        // клавиши отпущены: инерция кончается через 3 тика, не тянет).
        const float conf = std::clamp(total, 0.0f, 1.0f);
        state.weak_n = conf < 1.0f ? state.weak_n + 1 : 0;
        if (conf < 1.0f && state.weak_n <= HANDOFF_TICKS) {
            const glm::vec3 inertia = momentum();
            if (glm::length(inertia) > 1.0e-6f) {
                delta = glm::mix(inertia, delta, conf);
            }
        }
        remember(delta);
    } else if (on_ground && state.weak_n < HANDOFF_TICKS
               && glm::length(momentum()) > 1.0e-6f) {
        // СТОПА КАСАЕТСЯ, НО ВЕСА НЕ ПОЛУЧИЛА (приземлившаяся пятка в беге
        // трусцой: назад идёт медленнее четверти хода тела) — та же смена
        // опоры, то же окно инерции; после полёта окно пусто, и инерция —
        // это коаст полёта.
        ++state.weak_n;
        delta = momentum();
        remember(delta);
    } else {
        state.weak_n = 0;
        if (state.window_has) {
            state.coast = state.window_mean;
            state.window_has = false;
        }
        if (on_ground) {
            state.coast = glm::vec3{0.0f};
            delta = glm::vec3{0.0f};
        } else {
            delta = state.coast;
        }
    }
    // ПРЕДЕЛ УСКОРЕНИЯ ТЕЛА (владелец 02.09-2: «шаги очень дёрганные»): стопа
    // клипа за опору гуляет — пятка после касания четыре тика идёт назад
    // медленнее тела (1,0 против 1,3 м/с), в перецепку пятка→носок носок
    // прыгает до 1,55, в беге-смеси стопа за одну опору ходит 4,3…8,0 — а
    // тело массой 80 кг за тик на 0,9 м/с не ускоряется. Скорость корня
    // меняется не быстрее ROOT_ACCEL_MAX_MPS2; расхождение со стопой (доли
    // сантиметра за опору) закрывает замок стопы, как и любое другое.
    if (state.has_last) {
        const float step = static_cast<float>(config::ROOT_ACCEL_MAX_MPS2) * dt * dt;
        const float have = glm::length(state.last_delta);
        const float want = glm::length(delta);
        const float got = std::clamp(want, have - step, have + step);
        if (got > 1.0e-6f) {
            const glm::vec3 dir = want > 1.0e-6f ? delta / want : state.last_delta / have;
            delta = dir * got;
        } else {
            delta = glm::vec3{0.0f};
        }
    }
    state.last_delta = delta;
    state.has_last = true;
    const float speed = glm::length(delta) / dt;
    state.speed_est_mps = std::max(state.speed_est_mps * std::exp(-dt / SPEED_MEMORY_S),
                                   speed);
    return delta;
}

std::array<bool, 2> detect_footfalls(const ContactState& prev, const ContactState& curr,
                                     float on_weight) {
    std::array<bool, 2> out{};
    if (!prev.valid || !curr.valid) {
        return out;
    }
    for (std::size_t side = 0; side < 2; ++side) {
        out[side] = prev.support[side] < on_weight && curr.support[side] >= on_weight;
    }
    return out;
}

} // namespace dfn::anim
