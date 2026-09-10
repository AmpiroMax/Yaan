/*
Module: engine/app
File: engine/app/sources/CharacterFeet.cpp

Responsibility:
- Реализация физических стоп (CharacterFeet.h): создание тел по хитбоксам
  стоп, машина Swing/Plant по замкам, якорь и скольжение от тела физики.

Key items:
- CharacterFeet::tick(): см. заголовок; скольжение в корень — только XZ, по
  высоте якорь идёт полностью (стопа села на землю).

Dependencies:
- Uses: CharacterFeet.h, SkinnedCharacter.h, CollisionLayers.h, Constants.h.
- Used by: App, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Тело стопы не сталкивается с капсулой и хитбоксами хозяина — маска слоёв
  из контракта §12, не расширять.
*/
#include "engine/app/sources/CharacterFeet.h"

#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/physics/sources/CollisionLayers.h"

#include "engine/app/sources/AppDoors.h"

#include <cstdio>

#include <glm/gtc/quaternion.hpp>

namespace dfn::app {

void CharacterFeet::bind(platform::IPhysics* physics, uint64_t owner_bits) {
    shutdown();
    physics_ = physics;
    owner_ = owner_bits;
}

void CharacterFeet::set_enabled(bool on) {
    if (!on) {
        for (std::size_t side = 0; side < 2; ++side) {
            if (physics_ != nullptr && foot_[side].valid()) {
                physics_->destroy_body(foot_[side]);
            }
            foot_[side] = {};
            planted_[side] = false;
            report_[side] = {};
        }
    }
    enabled_ = on;
}

void CharacterFeet::shutdown() {
    for (std::size_t side = 0; side < 2; ++side) {
        if (physics_ != nullptr && foot_[side].valid()) {
            physics_->destroy_body(foot_[side]);
        }
        foot_[side] = {};
        planted_[side] = false;
        report_[side] = {};
    }
    physics_ = nullptr;
}

void CharacterFeet::tick(SkinnedCharacter& body, float dt) {
    (void)dt;
    if (physics_ == nullptr || !enabled_ || !body.ready()) {
        return;
    }
    for (std::size_t side = 0; side < 2; ++side) {
        glm::mat4 frame{1.0f};
        glm::vec3 half{0.0f};
        if (!body.foot_box_world(side, frame, half)) {
            continue;
        }
        const platform::BodyPose pose{glm::vec3{frame[3]}, glm::normalize(glm::quat_cast(frame))};
        FootPhysicsReport& r = report_[side];
        if (!foot_[side].valid()) {
            platform::FootBodyDesc desc;
            desc.half_extents = half;
            desc.position = pose.position;
            desc.rotation = pose.rotation;
            desc.mass_kg = static_cast<float>(config::FOOT_BODY_MASS_KG);
            desc.substance = core::find_substance(SOLE_SUBSTANCE);
            desc.layer = physics::LAYER_FOOT;
            desc.collides_with = physics::LAYER_STATIC | physics::LAYER_LOOSE
                                 | physics::LAYER_INTERACTABLE;
            desc.user_data = owner_;
            foot_[side] = physics_->create_foot_body(desc);
            if (!foot_[side].valid()) {
                std::fprintf(stderr, "[feet] тело стопы %zu не создано — физика отказала\n",
                             side);
                continue;
            }
            planted_[side] = false;
            r = {};
            r.has_body = true;
            std::fprintf(stderr,
                         "[feet] стопа %zu: коробка %.3f×%.3f×%.3f м, подошва %s, %.0f кг\n",
                         side, static_cast<double>(2.0f * half.x),
                         static_cast<double>(2.0f * half.y), static_cast<double>(2.0f * half.z),
                         SOLE_SUBSTANCE, static_cast<double>(config::FOOT_BODY_MASS_KG));
        }
        r.has_body = true;
        r.slip_delta = glm::vec3{0.0f};
        const platform::FootContact c = physics_->foot_contact(foot_[side]);
        r.touching = c.touching;
        r.holds = c.holds;
        // Скольжение — только у тела, которое ПРОШЛЫЙ шаг было динамическим:
        // у кинематического «скорость» — это перенос маха (до 16 м/с на стенде).
        r.slip_mps = planted_[side] ? c.slip_speed_mps : 0.0f;
        // ВДАВЛЕННАЯ СТОПА — НЕ ОПОРА: коробка, лёгшая на кромку лавки с
        // проникновением глубже FOOT_BODY_PLANT_DEPTH_MAX, выдавливается
        // решателем вбок по 8 мм за тик при «держит, скольжение 0» (владелец
        // 07.09 22:32: «поставил стопу на лавку — задёргало и отбросило»).
        // Такую стопу не ставим динамической, а поставленную — не слушаем.
        const bool embedded = c.touching
                              && c.depth > static_cast<float>(config::FOOT_BODY_PLANT_DEPTH_MAX);
        r.ground = c.ground;
        r.friction_pair = c.friction_pair;
        r.slope_tan = c.slope_tan;
        const anim::LocomotionOut& lo = body.locomotion();
        {
            // СТОПА — ДАТЧИК (§16.6): стоит, пока стоит по расписанию клипа.
            const bool sched = lo.valid && lo.verbatim && lo.planted[side];
            if (planted_[side]) {
                if (!sched) {
                    physics_->set_foot_mode(foot_[side], platform::FootMode::Swing);
                    planted_[side] = false;
                } else {
                    const platform::BodyPose now = physics_->body_pose(foot_[side]);
                    const glm::vec3 d = now.position - body_seen_[side];
                    if (embedded) {
                        // выдавливание из препятствия — не скольжение
                    } else if (glm::dot(d, d) > 1.0e-12f) {
                        body.add_root_slip(glm::vec3{d.x, 0.0f, d.z});
                        r.slip_delta = d;
                    }
                    body_seen_[side] = now.position;
                }
            }
            if (!planted_[side]) {
                if (sched && !embedded && c.touching) {
                    // постановка там, где тело есть (кинематический мах довёз)
                    physics_->set_foot_mode(foot_[side], platform::FootMode::Plant);
                    body_seen_[side] = physics_->body_pose(foot_[side]).position;
                    planted_[side] = true;
                } else {
                    physics_->set_foot_kinematic_pose(foot_[side], pose);
                }
            }
            r.planted = planted_[side];
            SkinnedCharacter::FootPhysicsNote note;
            note.sensed = true;
            note.planted = r.planted;
            note.holds = r.holds;
            note.slip_mps = r.slip_mps;
            body.note_foot_physics(side, note);
        }
    }
}

} // namespace dfn::app
