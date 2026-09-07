/*
Module: engine/app
File: engine/app/sources/BodyFerry.cpp

Responsibility:
- Реализация парома «сим → тело» (BodyFerry.h).

Key items:
- ferry_body_drive(): тело читает, приложение пишет.

Dependencies:
- Uses: BodyFerry.h, glm.
- Used by: App, NpcBodies.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Рыск сим'а по часовой; мир = R(−рыск)·тело (см. commit_root).
*/
#include "engine/app/sources/BodyFerry.h"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dfn::app {

void ferry_body_drive(anim::BodyDrive& drive, gameplay::PlayerState& ps,
                      const platform::IPhysics* physics, const BodyView& view) {
    drive.stride_phase = ps.stride_phase;
    drive.step_length_m = gameplay::step_length(ps.stride_speed);
    drive.speed_mps = ps.stride_speed;
    // КОРПУС ДОВОРАЧИВАЕТСЯ К ХОДУ, А НЕ К КАМЕРЕ (§13): от третьего лица
    // рыск игрока и есть корпус (ThirdPersonRig); от первого — доворот в
    // PlayerMovement; стоя корпус поворачивает КЛИП (root_yaw_delta).
    if (view.third_person && !view.npc) {
        ps.body_yaw = ps.yaw;
    }
    drive.facing_yaw = ps.body_yaw;
    if (view.npc) {
        // НПС: взгляда нет (лид 07.09) — повороты на месте не стреляют,
        // корпус ведёт исполнитель очереди через рыск ходока.
        drive.view_yaw = ps.yaw;
        drive.view_valid = false;
    } else {
        drive.view_yaw = view.third_person ? view.cam_yaw : ps.yaw;
        drive.view_valid = true;
        if (view.stand_cam != 0) {
            // КАМЕРА СТЕНДА — орбита вокруг фигуры: её рыск смотрит НА тело.
            // Кадры походки — нейтральная голова; камера «лицо» (6) — взгляд
            // в объектив, то есть навстречу.
            drive.view_yaw = view.cam_yaw + glm::pi<float>();
            drive.view_valid = view.stand_cam == 6;
        }
    }
    // ТОЛЧОК (ярус 0): контакты капсулы, где мир двинул её (pushed_character).
    drive.push_mps_model = glm::vec3{0.0f};
    if (physics != nullptr && ps.character.valid()) {
        glm::vec3 push{0.0f};
        for (const platform::CharacterContact& c : physics->character_contacts(ps.character)) {
            // Упор в СТАТИКУ (масса ∞) — не толчок: стена не двигает, она
            // не пускает. Замер 07.09: бот, идущий в лавку, получал Stagger
            // каждый тик с «толчком» в свою же скорость хода.
            if (!c.pushed_character || !std::isfinite(c.mass_kg)) {
                continue;
            }
            const float along = glm::dot(c.relative_velocity, c.normal);
            if (along > 0.0f) {
                push += c.normal * along;
            }
        }
        push.y = 0.0f;
        drive.push_mps_model = glm::vec3{
            glm::rotate(glm::mat4{1.0f}, ps.body_yaw, glm::vec3{0.0f, 1.0f, 0.0f})
            * glm::vec4{push, 0.0f}};
    }
    drive.grounded = !ps.airborne;
    drive.vertical_velocity = ps.vertical_velocity;
    drive.crouch_blend = ps.crouch_blend;
    drive.want_speed_mps = ps.want_speed_mps;
    // НАПРАВЛЕНИЕ ХОДА В СИСТЕМЕ ТЕЛА (§9): ввод в мире → поворот на +рыск.
    if (glm::length(ps.want_dir) > 1.0e-4f) {
        const glm::vec3 world{ps.want_dir.x, 0.0f, ps.want_dir.y};
        const glm::mat4 to_model =
            glm::rotate(glm::mat4{1.0f}, ps.body_yaw, glm::vec3{0.0f, 1.0f, 0.0f});
        const glm::vec3 m = glm::vec3{to_model * glm::vec4{world, 0.0f}};
        const float len = glm::length(glm::vec2{m.x, m.z});
        if (len > 1.0e-4f) {
            drive.move_dir_model = glm::vec3{m.x / len, 0.0f, m.z / len};
        }
    }
    // ПЕРЕДАЧА — явным switch, не кастом (Rule 35/37).
    switch (ps.gait) {
    case gameplay::Gait::Walk: drive.gait = anim::Gait::Walk; break;
    case gameplay::Gait::Jog:  drive.gait = anim::Gait::Jog;  break;
    case gameplay::Gait::Run:  drive.gait = anim::Gait::Run;  break;
    }
}

} // namespace dfn::app
