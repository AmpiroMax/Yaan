/*
Module: engine/gameplay
File: engine/gameplay/sources/CameraBoom.cpp

Responsibility:
- Реализация стрелы камеры третьего лица (CameraBoom.h).

Dependencies:
- Uses: CameraBoom.h, glm, <algorithm>, <cmath>.
- Used by: dfn_gameplay.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Кроме camera_boom_sweep(), здесь не должно появиться ни одного обращения к
  платформе: приёмка стрелы держится на том, что её арифметику можно проверить
  без физического мира.
*/

#include "engine/gameplay/sources/CameraBoom.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>

namespace dfn::gameplay {
namespace {

// СМЕЩЕНИЕ КАМЕРЫ ОТ ГОЛОВЫ ПРИ СВОБОДНОМ ПУТИ. Та же формула, что стояла в
// App.cpp до коллизии (eye - fwd*back + up*lift), вынесенная сюда целиком —
// вторая копия расходится с первой ровно тогда, когда кто-нибудь подкрутит
// кадрирование вида (Rule 39).
[[nodiscard]] glm::vec3 boom_offset(float yaw, float pitch, const CameraBoomDesc& d) {
    const float cp = std::cos(pitch);
    const glm::vec3 fwd{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    return -fwd * d.back + glm::vec3{0.0f, d.lift, 0.0f};
}

} // namespace

CameraBoomAim camera_boom_aim(float yaw, float pitch, const CameraBoomDesc& d) {
    CameraBoomAim aim;
    const glm::vec3 off = boom_offset(yaw, pitch, d);
    aim.reach = std::sqrt(off.x * off.x + off.y * off.y + off.z * off.z);
    if (aim.reach <= 1e-6f) {
        // Вырожденной оснастки (нулевой отвод И нулевой подъём) у живого вида
        // не бывает, но ответ обязан быть единичным вектором: назад по взгляду.
        const float cp = std::cos(pitch);
        aim.direction = -glm::vec3{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
        aim.reach = 0.0f;
        return aim;
    }
    aim.direction = off / aim.reach;
    return aim;
}

float camera_boom_free_length(const platform::RayHit& sweep, float reach,
                              const CameraBoomDesc& d) {
    if (!sweep.hit) {
        return std::max(0.0f, reach);
    }
    return std::clamp(sweep.distance - d.margin, 0.0f, std::max(0.0f, reach));
}

platform::RayHit camera_boom_sweep(const platform::IPhysics& physics, const glm::vec3& head,
                                   const CameraBoomAim& aim, const CameraBoomDesc& d,
                                   platform::CollisionMask mask) {
    return physics.sphere_cast(head, aim.direction, d.probe_radius, aim.reach, mask);
}

CameraBoomPerch camera_boom_perch(const glm::vec3& eye, float pitch,
                                  const glm::vec3& perch, float pitch_cap,
                                  float weight) {
    CameraBoomPerch out{eye, pitch};
    const float w = std::clamp(weight, 0.0f, 1.0f);
    if (w <= 0.0f) {
        // НУЛЕВАЯ ДОЛЯ — ЭТО БИТ-В-БИТ ПРЕЖНЯЯ КАМЕРА, и проверяется это
        // тестом: стоящий игрок обязан получить ровно свой глаз и ровно свой
        // тангаж, иначе правка позы тихо переехала бы всему третьему лицу.
        return out;
    }
    const glm::vec3 above{perch.x, std::max(perch.y, eye.y), perch.z};
    out.origin = glm::mix(eye, above, w);
    out.pitch = glm::mix(pitch, std::min(pitch, pitch_cap), w);
    return out;
}

float camera_boom_step(CameraBoomState& state, float free_length, float dt,
                       const CameraBoomDesc& d) {
    if (state.length < 0.0f) {
        state.length = free_length; // первый кадр вида: без взлёта
        return state.length;
    }
    if (free_length <= state.length) {
        // УКОРОЧЕНИЕ МГНОВЕННО. Любая инерция здесь — это кадр, на котором
        // камера ещё за стеной, а приёмка запрещает такой кадр целиком.
        state.length = free_length;
        return state.length;
    }
    // Выпуск обратно — по экспоненте, независимо от частоты кадров.
    const float k = 1.0f - std::exp(-d.return_rate * std::max(0.0f, dt));
    state.length += (free_length - state.length) * k;
    return state.length;
}

} // namespace dfn::gameplay
