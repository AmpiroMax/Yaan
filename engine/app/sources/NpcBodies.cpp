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
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/PlayerMovement.h"

#include <chrono>
#include <cstdio>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>

namespace dfn::app {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

uint64_t fnv1a_file(uint64_t h, const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return h;
    }
    char buf[1 << 16];
    while (in.read(buf, sizeof buf) || in.gcount() > 0) {
        const std::streamsize n = in.gcount();
        for (std::streamsize i = 0; i < n; ++i) {
            h ^= static_cast<unsigned char>(buf[i]);
            h *= 1099511628211ull;
        }
    }
    return h;
}

/// КЛЮЧ АССЕТА — ХЭШ ВЫПЕЧКИ (условие 3 синка): байты .dfo тела и наборов
/// частей/одежды рядом с ним (<тело>.parts.dfo, <тело>.clothes.dfo — те же
/// файлы, что берёт фабрика при пустых путях). Имя файла в ключ не входит:
/// два разных пресета под одним именем — разные тела, и наоборот.
uint64_t asset_key(const std::filesystem::path& body_path) {
    uint64_t h = 1469598103934665603ull;
    h = fnv1a_file(h, body_path);
    std::filesystem::path parts = body_path;
    parts.replace_extension(".parts.dfo");
    std::filesystem::path clothes = body_path;
    clothes.replace_extension(".clothes.dfo");
    std::error_code ec;
    if (std::filesystem::exists(parts, ec)) {
        h = fnv1a_file(h ^ 0x5041525453ull, parts);
    }
    if (std::filesystem::exists(clothes, ec)) {
        h = fnv1a_file(h ^ 0x434c4f544845ull, clothes);
    }
    return h;
}

} // namespace

double NpcCost::draw_ms_per_body_frame() const {
    return frames == 0 || bodies_ticks == 0 ? 0.0
                                            : draw_ms / static_cast<double>(frames)
                                                  / (static_cast<double>(bodies_ticks) / static_cast<double>(std::max<uint64_t>(ticks, 1)));
}

std::string NpcBodies::cost_report() const {
    char buf[320];
    const double bodies = cost_.ticks == 0 ? 0.0 : static_cast<double>(cost_.bodies_ticks) / static_cast<double>(cost_.ticks);
    std::snprintf(buf, sizeof buf,
                  "[npc] цена: тел %.1f, тиков %llu, кадров %llu; на одного НПС за тик: паром+машина+клип %.3f мс, "
                  "commit+стопы %.3f, поведения+исполнитель %.3f (Σ сим %.3f, бюджет %.1f); скиннинг за кадр %.3f мс",
                  bodies, static_cast<unsigned long long>(cost_.ticks), static_cast<unsigned long long>(cost_.frames),
                  cost_.bodies_ticks ? cost_.advance_ms / static_cast<double>(cost_.bodies_ticks) : 0.0,
                  cost_.bodies_ticks ? cost_.after_ms / static_cast<double>(cost_.bodies_ticks) : 0.0,
                  cost_.bodies_ticks ? cost_.executor_ms / static_cast<double>(cost_.bodies_ticks) : 0.0,
                  cost_.sim_ms_per_body(), static_cast<double>(config::NPC_TICK_BUDGET_MS),
                  cost_.draw_ms_per_body_frame());
    return buf;
}

NpcBody* NpcBodies::spawn(ecs::World& world, platform::IPhysics& physics,
                          render::RenderSystem& render_system, platform::IRenderer& renderer,
                          const anim::Rig& rig, const std::filesystem::path& body_path,
                          const glm::vec3& at, bool telemetry, const std::string& csv) {
    const auto bodies_max = static_cast<std::size_t>(config::NPC_BODIES_MAX);
    if (bodies_.size() >= bodies_max) {
        std::fprintf(stderr,
                     "[npc] тело НПС не создано — тел уже NPC_BODIES_MAX (%zu, реестр)\n",
                     bodies_max);
        return nullptr;
    }
    // БЮДЖЕТ ТИКА ПО ЗАМЕРУ: средняя сим-цена одного тела × (тел + 1) больше
    // NPC_TICK_BUDGET_MS — отказ вслух с числами, не молчаливое торможение.
    if (cost_.ticks >= 60 && !bodies_.empty()) {
        const double per = cost_.sim_ms_per_body();
        const double next = per * static_cast<double>(bodies_.size() + 1);
        if (next > static_cast<double>(config::NPC_TICK_BUDGET_MS)) {
            std::fprintf(stderr,
                         "[npc] тело НПС не создано — бюджет тика: %.3f мс на тело × %zu = %.2f > "
                         "NPC_TICK_BUDGET_MS %.1f\n",
                         per, bodies_.size() + 1, next, static_cast<double>(config::NPC_TICK_BUDGET_MS));
            return nullptr;
        }
    }
    // АССЕТ: общий меш по хэшу выпечки; новый ассет — новая полоса мешей
    const uint64_t key = asset_key(body_path);
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < assets_.size(); ++i) {
        if (assets_[i].key == key) {
            slot = i;
            break;
        }
    }
    const bool new_asset = slot == UINT32_MAX;
    if (new_asset) {
        if (assets_.size() >= NPC_ASSETS_MAX) {
            std::fprintf(stderr,
                         "[npc] тело НПС не создано — полоса номеров мешей вмещает %u разных ассетов, "
                         "это был бы %zu-й (\"%s\")\n",
                         NPC_ASSETS_MAX, assets_.size() + 1, body_path.string().c_str());
            return nullptr;
        }
        slot = static_cast<uint32_t>(assets_.size());
    }
    const uint32_t k = static_cast<uint32_t>(bodies_.size());
    auto npc = std::make_unique<NpcBody>();
    npc->id = gameplay::spawn_npc(world, physics, at);
    if (!world.alive(npc->id)) {
        return nullptr;
    }
    npc->asset_slot = slot;
    anim::spawn_body(world, npc->id, rig, /*hide_head=*/false);
    CharacterSpec spec;
    spec.proportions = &rig;
    spec.owner = npc->id;
    spec.make_capsule = false;
    spec.mesh_asset = NPC_MESH_ID_FIRST + slot * NPC_MESH_ID_STRIDE;
    spec.blade_asset = spec.mesh_asset + 1;
    spec.parts_mesh_first = spec.mesh_asset + 2;
    spec.reuse_meshes = !new_asset;
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
    if (new_asset) {
        assets_.push_back(AssetSlot{key, 0});
    }
    ++assets_[slot].bodies;
    std::fprintf(stderr, "[npc] тело НПС %u в (%.1f %.1f %.1f): ассет %u (%s, хэш %016llx), меши %u..%u\n", k,
                 static_cast<double>(at.x), static_cast<double>(at.y),
                 static_cast<double>(at.z), slot, new_asset ? "новый, регистрирую" : "общий",
                 static_cast<unsigned long long>(key), spec.mesh_asset,
                 spec.mesh_asset + NPC_MESH_ID_STRIDE - 1);
    bodies_.push_back(std::move(npc));
    return bodies_.back().get();
}

void NpcBodies::before_step(ecs::World& world, const platform::IPhysics* physics, float dt) {
    const Clock::time_point t0 = Clock::now();
    ++cost_.ticks;
    cost_.bodies_ticks += bodies_.size();
    struct Stop {
        NpcCost& c;
        Clock::time_point t;
        ~Stop() { c.advance_ms += ms_since(t); }
    } stop{cost_, t0};
    for (auto& npc : bodies_) {
        auto* drive = world.get<anim::BodyDrive>(npc->id);
        auto* ps = world.get<gameplay::PlayerState>(npc->id);
        const auto* tr = world.get<components::Transform>(npc->id);
        auto* walker = world.get<gameplay::WalkerLocomotion>(npc->id);
        if (drive == nullptr || ps == nullptr || tr == nullptr || walker == nullptr) {
            continue;
        }
        // Патруль стенда уехал в поведение gameplay::Patrol (NpcBehaviour.h,
        // NPC_NAVIGATION.md §5): приложение действий не заказывает.
        BodyView view;
        view.npc = true;
        view.root_track = npc->body.ready() && npc->body.root_track();
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
        walker->request =
            ferry_locomotion_request(lo, anim::body_root_for(*drive, tr->position).yaw);
    }
}

void NpcBodies::after_step(ecs::World& world, platform::IPhysics* physics, float dt) {
    struct Stop {
        NpcCost& c;
        Clock::time_point t;
        ~Stop() { c.after_ms += ms_since(t); }
    } stop{cost_, Clock::now()};
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
    ++cost_.frames;
    struct Stop {
        NpcCost& c;
        Clock::time_point t;
        ~Stop() { c.draw_ms += ms_since(t); }
    } stop{cost_, Clock::now()};
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
    if (cost_.ticks > 0 && !bodies_.empty()) {
        std::fprintf(stderr, "%s\n", cost_report().c_str());
    }
    // ОБРАТНЫЙ ПОРЯДОК: первое тело ассета владеет мешами, общие тела — нет;
    // снимать общие раньше владельца. (Тела при жизни мира не убывают —
    // деспавн с передачей владения мешами — хвост §8 записки.)
    for (auto it = bodies_.rbegin(); it != bodies_.rend(); ++it) {
        (*it)->feet.shutdown();
        if (physics != nullptr) {
            (*it)->bodies.hitboxes.destroy(*physics);
        }
    }
    bodies_.clear();
    assets_.clear();
}

} // namespace dfn::app
