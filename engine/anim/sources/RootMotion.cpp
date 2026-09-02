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

} // namespace

glm::vec3 root_motion_step(const ContactState& prev, ContactState& curr, float dt,
                           RootMotionState& state) {
    if (!prev.valid || !curr.valid || dt <= 0.0f) {
        return glm::vec3{0.0f};
    }
    const float swing = static_cast<float>(config::FOOT_SWING_SPEED_MPS);
    // НА ЗЕМЛЕ ЛИ ХОТЬ ОДНА СТОПА — по высоте, ДО отсечек движением: инерция
    // полёта положена только полёту. Стоящее тело (обе стопы на земле, ни
    // одна не идёт назад) обязано дать ноль, а не прошлый ход: владелец убрал
    // руки с клавиатуры — персонаж ехал вперёд на скорости последней опоры.
    const bool on_ground = std::min(curr.height[0], curr.height[1]) <= FOOT_IK_RELEASE_M;
    std::array<glm::vec3, 2> d{};
    for (std::size_t side = 0; side < 2; ++side) {
        d[side] = same_joint_delta(prev, curr, side);
        const float forward = -d[side].z / dt; // лицом в −Z: вперёд = −z
        // (2) мах: вперёд быстрее порога — не опора
        curr.support[side] *= 1.0f - smooth01(forward / swing);
        // (3) отстающая стопа: назад медленнее половины скорости тела — садится
        // или отрывается; полосу держит та же смягчённая ступень
        if (state.speed_est_mps > 1.0e-3f) {
            const float backward = -forward;
            curr.support[side] *= smooth01((backward / state.speed_est_mps - 0.25f) / 0.25f);
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
    glm::vec3 delta;
    if (total > 1.0e-4f) {
        delta = -sum / total;
        state.window_sum += delta;
        ++state.window_n;
    } else {
        if (state.window_n > 0) {
            state.coast = state.window_sum / float(state.window_n);
            state.window_sum = glm::vec3{0.0f};
            state.window_n = 0;
        }
        if (on_ground) {
            state.coast = glm::vec3{0.0f};
            delta = glm::vec3{0.0f};
        } else {
            delta = state.coast;
        }
    }
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
