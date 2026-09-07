/*
Module: engine/anim
File: engine/anim/sources/Inertializer.h

Responsibility:
- ИНЕРЦИАЛИЗАЦИЯ СТЫКОВ (LOCOMOTION_GROUNDED.md §13.7; Bollo, GDC 2016/2018):
  при смене клипа новый играет СРАЗУ с полным весом, а разница «старая поза −
  новая» запоминается на каждый сустав вместе со скоростью старой позы и
  гасится квинтикой за blend_s: скорость на стыке непрерывна по построению,
  чего линейный кроссфейд не даёт (он рвёт скорость на разницу поз / время
  фейда: замер 07.09 — колено 2 100…4 800 рад/с² на стыке старт → цикл).
- Работает на локальных ориентациях и смещениях суставов, скелета не знает:
  сустав к суставу, в любом порядке.

Key items:
- Quintic: одномерная гасящая кривая x(0)=x0, x'(0)=v0, x(t1)=x'(t1)=x''(t1)=0.
- Inertializer::capture(): снять разницу на стыке по двум прошлым позам и свежей.
- Inertializer::apply(): наложить остаток разницы на позу в момент t.
- weight(): та же кривая от 1 к 0 — для того, что раньше ослаблялось весом фейда.

Dependencies:
- Uses: SkinnedBody.h (JointLocal), glm.
- Used by: SkinnedCharacter (тик и кадр), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Позы на входе — «как показаны» (после прошлого стыка), иначе цепочка стыков
  подряд щёлкнет на второй.
*/
#pragma once

#include "engine/anim/sources/SkinnedBody.h"

#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace dfn::anim {

/// Гасящая квинтика Болло: x(0)=x0, x'(0)=v0, x''(0)=a0 ≥ 0 (не разгоняет
/// разницу), в t1 — ноль по значению, скорости и ускорению. Если разница уже
/// закрывается (v0 < 0), t1 укорачивается до −5·x0/v0, чтобы не перелететь ноль.
struct Quintic {
    float x0 = 0.0f;
    float v0 = 0.0f;
    float a0 = 0.0f;
    float t1 = 0.0f;
    float a = 0.0f, b = 0.0f, c = 0.0f; ///< коэффициенты при t⁵, t⁴, t³
    static Quintic fit(float x0, float v0, float t1);
    [[nodiscard]] float at(float t) const;
};

struct InertialTrack {
    glm::vec3 axis{0.0f, 1.0f, 0.0f}; ///< ось разницы ориентации (локально)
    Quintic rot;                       ///< угол разницы, рад
    glm::vec3 dir{0.0f};               ///< направление разницы смещения
    Quintic pos;                       ///< длина разницы, м
    bool has_rot = false;
    bool has_pos = false;
};

struct Inertializer {
    std::vector<InertialTrack> tracks;
    float t_s = 0.0f;
    float blend_s = 0.0f;

    /// Снять разницу на стыке. `older` и `last` — две прошлые показанные позы
    /// (тик −2 и −1), `fresh` — новая роль на этом тике, dt — шаг тика.
    /// После снятия t = 0; вызывающий сразу делает advance(dt).
    void capture(std::span<const JointLocal> older, std::span<const JointLocal> last,
                 std::span<const JointLocal> fresh, float dt, float blend_s);
    void advance(float dt) { t_s += dt; }
    [[nodiscard]] bool active() const { return blend_s > 0.0f && t_s < blend_s; }
    [[nodiscard]] float time() const { return t_s; }
    /// Остаток стыка от 1 к 0 по той же кривой (x0 = 1, v0 = 0).
    [[nodiscard]] float weight() const;
    /// Наложить остаток разницы в момент t (с, от стыка) на позу.
    void apply(float t, std::span<JointLocal> pose) const;
};

} // namespace dfn::anim
