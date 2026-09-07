/*
Module: engine/app
File: engine/app/sources/NpcBodies.cpp

Responsibility:
- Реализация тел НПС (NpcBodies.h): спавн через CharacterFactory, тик и кадр
  в том же порядке, что у игрока.

Key items:
- spawn(): полоса мешей с потолком (отказ вслух), spawn_npc + spawn_body +
  build_character, скрытие коробчатых сегментов, телеметрия, патруль.
- before_step()/after_step()/draws(): см. заголовок.

Dependencies:
- Uses: NpcBodies.h, BodyFerry.h, anim/Body.h, gameplay, ecs/World.
- Used by: App, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Тела — только build_character (один путь построения персонажа).
*/
#include "engine/app/sources/NpcBodies.h"

#include "engine/anim/sources/Body.h"
#include "engine/app/sources/BodyFerry.h"
#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/PlayerMovement.h"

#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

namespace dfn::app {

NpcBody* NpcBodies::spawn(ecs::World& world, platform::IPhysics& physics,
                          render::RenderSystem& render_system, platform::IRenderer& renderer,
                          const anim::Rig& rig, const std::filesystem::path& body_path,
                          const glm::vec3& at, bool telemetry, const std::string& csv) {
    if (bodies_.size() >= NPC_BODIES_MAX) {
        std::fprintf(stderr,
                     "[npc] тело НПС не создано — полоса номеров мешей исчерпана "
                     "(%u тел, NpcBodies.h)\n",
                     NPC_BODIES_MAX);
        return nullptr;
    }
    const uint32_t k = static_cast<uint32_t>(bodies_.size());
    auto npc = std::make_unique<NpcBody>();
    npc->id = gameplay::spawn_npc(world, physics, at);
    if (!world.alive(npc->id)) {
        return nullptr;
    }
    anim::spawn_body(world, npc->id, rig, /*hide_head=*/false);
    CharacterSpec spec;
    spec.proportions = &rig;
    spec.owner = npc->id;
    spec.make_capsule = false;
    spec.mesh_asset = NPC_MESH_ID_FIRST + k * NPC_MESH_ID_STRIDE;
    spec.blade_asset = spec.mesh_asset + 1;
    spec.parts_mesh_first = spec.mesh_asset + 2;
    spec.to_world = glm::translate(glm::mat4{1.0f}, at);
    if (!build_character(npc->body, npc->bodies, render_system, renderer, &physics, body_path,
                         spec)) {
        std::fprintf(stderr, "[npc] тело НПС %u: build_character отказал (%s)\n", k,
                     body_path.string().c_str());
        return nullptr;
    }
    // Коробчатые сегменты под скиннованным телом не рисуются — как у игрока.
    if (const auto* body_rig = world.get<anim::BodyRig>(npc->id)) {
        const auto segments = body_rig->segments;
        for (const ecs::EntityId seg : segments) {
            if (auto* rm = world.get<components::RenderMesh>(seg)) {
                rm->mesh_asset = 0;
            }
        }
    }
    if (telemetry) {
        npc->body.set_telemetry(true, csv);
    }
    world.add(npc->id, gameplay::WalkerLocomotion{});
    std::fprintf(stderr, "[npc] тело НПС %u в (%.1f %.1f %.1f): меши %u..%u\n", k,
                 static_cast<double>(at.x), static_cast<double>(at.y),
                 static_cast<double>(at.z), spec.mesh_asset,
                 spec.mesh_asset + NPC_MESH_ID_STRIDE - 1);
    bodies_.push_back(std::move(npc));
    return bodies_.back().get();
}

void NpcBodies::before_step(ecs::World& world, const platform::IPhysics* physics, float dt) {
    for (auto& npc : bodies_) {
        auto* drive = world.get<anim::BodyDrive>(npc->id);
        auto* ps = world.get<gameplay::PlayerState>(npc->id);
        const auto* tr = world.get<components::Transform>(npc->id);
        auto* walker = world.get<gameplay::WalkerLocomotion>(npc->id);
        if (drive == nullptr || ps == nullptr || tr == nullptr || walker == nullptr) {
            continue;
        }
        // ПАТРУЛЬ: очередь опустела — тот же круг заново (стенд).
        if (!npc->patrol.empty()) {
            if (auto* queue = world.get<gameplay::NpcActionQueue>(npc->id);
                queue != nullptr && queue->pending.empty()) {
                // пауза перед новым кругом: после PathBlocked не долбиться в
                // препятствие каждый тик
                gameplay::enqueue(*queue, gameplay::Wait{1.0f});
                for (const glm::vec3& p : npc->patrol) {
                    gameplay::enqueue(*queue, gameplay::MoveTo{p, 0.0f, npc->patrol_gait});
                }
            }
        }
        BodyView view;
        view.npc = true;
        ferry_body_drive(*drive, *ps, physics, view);
        walker->request = {};
        if (!npc->body.ready()) {
            continue;
        }
        npc->body.advance(*drive, tr->position, dt);
        const anim::LocomotionOut& lo = npc->body.locomotion();
        if (lo.valid && lo.root_yaw_delta != 0.0f) {
            ps->body_yaw += lo.root_yaw_delta;
        }
        if (lo.valid) {
            const float yaw = anim::body_root_for(*drive, tr->position).yaw;
            const glm::vec3 w = glm::vec3{
                glm::rotate(glm::mat4{1.0f}, -yaw, glm::vec3{0.0f, 1.0f, 0.0f})
                * glm::vec4{lo.root_delta_model, 0.0f}};
            walker->request.valid = true;
            walker->request.delta_xz = glm::vec2{w.x, w.z};
            walker->request.phase = lo.phase;
            walker->request.footfall_left = lo.footfall[0];
            walker->request.footfall_right = lo.footfall[1];
        }
    }
}

void NpcBodies::after_step(ecs::World& world, platform::IPhysics* physics, float dt) {
    for (auto& npc : bodies_) {
        const auto* drive = world.get<anim::BodyDrive>(npc->id);
        const auto* tr = world.get<components::Transform>(npc->id);
        if (drive == nullptr || tr == nullptr || !npc->body.ready()) {
            continue;
        }
        npc->body.commit_root(*drive, tr->position, dt);
        if (physics != nullptr && npc->feet.enabled()) {
            if (!npc->feet.bound()) {
                npc->feet.bind(physics, npc->id.packed());
            }
            npc->feet.tick(npc->body, dt);
        }
    }
}

void NpcBodies::draws(ecs::World& world, platform::IPhysics* physics, float alpha,
                      std::vector<render::RenderSystem::SkinnedDraw>& out) {
    (void)world;
    for (auto& npc : bodies_) {
        if (!npc->body.ready()) {
            continue;
        }
        const std::size_t base = out.size();
        out.push_back(npc->body.build_draw(/*hide_head=*/false, alpha));
        if (npc->body.blade_drawn()) {
            out.push_back(npc->body.blade_draw(out[base]));
        }
        npc->body.part_draws(out[base], out);
        static bool said_draw = false;
        if (!said_draw) {
            said_draw = true;
            const auto& d = out[base];
            const auto* tr = world.get<components::Transform>(npc->id);
            std::fprintf(stderr,
                         "[npc] первый кадр: дро с номера %zu (всего %zu), меш %u, лист %u, палитра %zu, "
                         "перенос (%.1f %.1f %.1f), сущность (%.1f %.1f %.1f)\n",
                         base, out.size(), d.mesh_asset, d.texture_asset, d.palette.size(),
                         static_cast<double>(d.transform[3].x), static_cast<double>(d.transform[3].y),
                         static_cast<double>(d.transform[3].z),
                         tr ? static_cast<double>(tr->position.x) : 0.0,
                         tr ? static_cast<double>(tr->position.y) : 0.0,
                         tr ? static_cast<double>(tr->position.z) : 0.0);
        }
        if (physics != nullptr) {
            if (!npc->bodies.hitboxes.live()) {
                npc->bodies.hitboxes.create(*physics, npc->id, npc->body.hitboxes(),
                                            npc->body.hitbox_pose(), out[base].transform);
            } else {
                npc->bodies.hitboxes.update(*physics, npc->body.hitbox_pose(),
                                            out[base].transform);
            }
        }
    }
}

void NpcBodies::shutdown(platform::IPhysics* physics) {
    for (auto& npc : bodies_) {
        npc->feet.shutdown();
        if (physics != nullptr) {
            npc->bodies.hitboxes.destroy(*physics);
        }
    }
    bodies_.clear();
}

} // namespace dfn::app
