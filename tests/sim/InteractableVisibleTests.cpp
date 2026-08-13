/*
Created: 13:08:2026 - 17:30:00
Last updated: 13:08:2026 - 18:59:13
Module: tests
File: tests/sim/InteractableVisibleTests.cpp

Responsibility:
- Proves that a prop the crosshair can name is a prop the player can SEE, and
  that what is drawn and what the ray hits are the same object.

Key items:
- "the spawn attaches everything render's view selects on": the case that would
  have caught the original defect, written against render's ACTUAL selector.
- "nothing is drawn outside the box the ray hits" / "every face of the box has
  geometry on it": the two halves of form-vs-collision, both measured.
- coverage_from(): what fraction of the box's silhouette, seen from the player,
  has drawn geometry behind it. The number the lead asked to be named.

Dependencies:
- Uses: doctest, dfn_gameplay, dfn_render (the geometry the expectations are
  read from), dfn_platform_physics (null backend), dfn_core.
- Used by: ctest (sim_interactable_visible).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The selector this file asserts against is render's, not a restatement of it:
  if render's ECS pass changes which components it needs, this file must be the
  thing that goes red.
*/
/*
UPD:
- 13:08:2026 - 17:30:00: Created with the three placeholder meshes.
- 13:08:2026 - 18:10:00: The entity-{0,0} case: the first prop a world spawns
                         packs to user_data 0 and used to be untargetable.
- 13:08:2026 - 18:15:00: The ray box must die with its prop.
- 13:08:2026 - 18:25:00: The verb must have a VISIBLE consequence — the door
                         swings, the lever throws, and the ray target follows.
- 13:08:2026 - 18:40:00: A settled leaf must have prev == curr, or it sweeps
                         between its last two frames for ever (the run smear).
- 13:08:2026 - 18:55:00: A mesh authored in metres must still fill its own ray
                         box (scale = half_extents / mesh_model_half_extents).
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/InteractableMesh.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
namespace render = dfn::render;
namespace components = dfn::components;
namespace physics_layer = dfn::physics;

constexpr uint32_t ALL_IDS[3] = {gameplay::INTERACTABLE_MESH_DOOR,
                                 gameplay::INTERACTABLE_MESH_LEVER,
                                 gameplay::INTERACTABLE_MESH_TORCH};

// Angle between two orientations, in degrees. Written out rather than reached
// for in glm/gtx: that header is an EXPERIMENTAL extension and including it
// would put a compile-time gate on this suite that has nothing to do with what
// it measures.
[[nodiscard]] float turn_degrees(const glm::quat& from, const glm::quat& to) {
    const glm::quat d = glm::normalize(to * glm::inverse(from));
    return 2.0f * std::acos(std::clamp(std::abs(d.w), 0.0f, 1.0f)) * 57.29577951f;
}

// Moller-Trumbore. Returns the ray parameter of the nearest hit, or -1, and
// (optionally) the winding normal of the triangle that was hit first.
[[nodiscard]] float ray_mesh(const render::MeshData& m, glm::vec3 origin, glm::vec3 dir,
                             glm::vec3* out_normal = nullptr) {
    float best = -1.0f;
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const glm::vec3 a = m.vertices[m.indices[i]].position;
        const glm::vec3 b = m.vertices[m.indices[i + 1]].position;
        const glm::vec3 c = m.vertices[m.indices[i + 2]].position;
        const glm::vec3 e1 = b - a;
        const glm::vec3 e2 = c - a;
        const glm::vec3 p = glm::cross(dir, e2);
        const float det = glm::dot(e1, p);
        if (std::abs(det) < 1e-9f) {
            continue;
        }
        const float inv = 1.0f / det;
        const glm::vec3 t = origin - a;
        const float u = glm::dot(t, p) * inv;
        if (u < 0.0f || u > 1.0f) {
            continue;
        }
        const glm::vec3 q = glm::cross(t, e1);
        const float v = glm::dot(dir, q) * inv;
        if (v < 0.0f || u + v > 1.0f) {
            continue;
        }
        const float hit = glm::dot(e2, q) * inv;
        if (hit > 1e-5f && (best < 0.0f || hit < best)) {
            best = hit;
            if (out_normal != nullptr) {
                *out_normal = glm::normalize(glm::cross(e1, e2));
            }
        }
    }
    return best;
}

// THE NUMBER THE LEAD ASKED FOR, and it is deliberately measured the way the
// player experiences it: from the eye, through the box the crosshair can hit.
// Sample a grid across the box's silhouette as seen from `eye`; report the
// fraction of those aim points that have drawn geometry behind them.
//
// 1.0 means "wherever inside its target box you aim, you see the prop". Less
// than 1.0 is solid air: the crosshair names a prop and the screen shows
// nothing, which is the complaint in miniature.
[[nodiscard]] float coverage_from(const render::MeshData& mesh, glm::vec3 half_extents,
                                  glm::vec3 prop_center, glm::vec3 eye, int grid = 41) {
    // The box, in model space, is [-1, 1]^3; the world box is half_extents
    // around prop_center. Work in MODEL space: put the eye there.
    const glm::vec3 eye_model{(eye.x - prop_center.x) / half_extents.x,
                              (eye.y - prop_center.y) / half_extents.y,
                              (eye.z - prop_center.z) / half_extents.z};
    // Aim at a grid over the face of the box nearest the eye. Which face that
    // is follows the eye's dominant axis, which is what "the side you are
    // looking at" means.
    const glm::vec3 a{std::abs(eye_model.x), std::abs(eye_model.y), std::abs(eye_model.z)};
    const int axis = (a.x >= a.y && a.x >= a.z) ? 0 : (a.y >= a.z ? 1 : 2);
    const float sign = (axis == 0 ? eye_model.x : axis == 1 ? eye_model.y : eye_model.z) >= 0.0f
                           ? 1.0f
                           : -1.0f;
    int hits = 0;
    int total = 0;
    for (int i = 0; i < grid; ++i) {
        for (int j = 0; j < grid; ++j) {
            const float u = -1.0f + 2.0f * (static_cast<float>(i) + 0.5f) / grid;
            const float v = -1.0f + 2.0f * (static_cast<float>(j) + 0.5f) / grid;
            glm::vec3 target{0.0f};
            if (axis == 0) {
                target = {sign, u, v};
            } else if (axis == 1) {
                target = {u, sign, v};
            } else {
                target = {u, v, sign};
            }
            const glm::vec3 dir = glm::normalize(target - eye_model);
            ++total;
            if (ray_mesh(mesh, eye_model, dir) > 0.0f) {
                ++hits;
            }
        }
    }
    return total > 0 ? static_cast<float>(hits) / static_cast<float>(total) : 0.0f;
}

} // namespace

TEST_CASE("the spawn attaches everything render's ECS view selects on") {
    // THE CASE THAT WOULD HAVE CAUGHT THE ORIGINAL DEFECT. Render draws
    // `world.view<Transform, PreviousTransform, RenderMesh>()`. The spawn used
    // to attach the first of those three, so every prop in the world was
    // invisible while the crosshair, the hover target and the prompt all worked
    // perfectly around it. Note that a fix adding ONLY RenderMesh would still
    // have drawn nothing — which is why all three are asserted, not just the
    // one that was obviously missing.
    auto physics = platform::create_null_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;

    for (const auto kind : {gameplay::InteractableKind::Pickup,
                            gameplay::InteractableKind::Openable,
                            gameplay::InteractableKind::Usable}) {
        gameplay::InteractableDesc desc;
        desc.kind = kind;
        desc.position = {1.0f, 2.0f, 3.0f};
        desc.prompt_key = "prompt.test";
        const dfn::ecs::EntityId id = gameplay::spawn_interactable(world, *physics, desc);
        REQUIRE(world.alive(id));

        const auto* transform = world.get<components::Transform>(id);
        const auto* previous = world.get<components::PreviousTransform>(id);
        const auto* mesh = world.get<components::RenderMesh>(id);
        REQUIRE(transform != nullptr);
        REQUIRE(previous != nullptr);
        REQUIRE(mesh != nullptr);
        // 0 is render's documented "draw nothing". It must never be what a prop
        // gets by default: the failure mode has to be a GENERIC door, not no
        // door at all.
        CHECK(mesh->mesh_asset != 0);
        CHECK(gameplay::build_interactable_mesh(mesh->mesh_asset).indices.size() >= 3);
        // A static prop interpolates from itself, so it does not smear toward
        // an origin it never occupied on the first frame.
        CHECK(previous->position == transform->position);
        CHECK(previous->scale == transform->scale);
        // The scale IS the half-extents: one set of numbers for both shapes.
        CHECK(transform->scale.x == doctest::Approx(desc.half_extents.x));
        CHECK(transform->scale.y == doctest::Approx(desc.half_extents.y));
        CHECK(transform->scale.z == doctest::Approx(desc.half_extents.z));
        // And the prop is still a ray target with its prompt (the half that
        // already worked must not have been traded for the half that did not).
        CHECK(world.get<gameplay::Highlightable>(id) != nullptr);
    }
}

TEST_CASE("content may name its own mesh, and that wins") {
    auto physics = platform::create_null_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;
    gameplay::InteractableDesc desc;
    desc.kind = gameplay::InteractableKind::Openable;
    desc.mesh_asset = gameplay::INTERACTABLE_MESH_TORCH;
    const dfn::ecs::EntityId id = gameplay::spawn_interactable(world, *physics, desc);
    CHECK(world.get<components::RenderMesh>(id)->mesh_asset ==
          gameplay::INTERACTABLE_MESH_TORCH);
}

TEST_CASE("nothing is drawn outside the box the ray hits") {
    // The direction that matters most: geometry outside the collision box is a
    // thing the player can see, aim at, and never hit — a prop with a lip you
    // walk your crosshair over and nothing happens.
    for (const uint32_t id : ALL_IDS) {
        const render::MeshData mesh = gameplay::build_interactable_mesh(id);
        REQUIRE_FALSE(mesh.vertices.empty());
        for (const platform::Vertex& v : mesh.vertices) {
            CHECK(std::abs(v.position.x) <= 1.0f + 1e-5f);
            CHECK(std::abs(v.position.y) <= 1.0f + 1e-5f);
            CHECK(std::abs(v.position.z) <= 1.0f + 1e-5f);
        }
    }
}

TEST_CASE("every face of the box has geometry standing on it") {
    // The other direction: a face of the target box with nothing drawn on it is
    // a face where the crosshair reports a prop and the screen shows air. This
    // is the property that lets the coverage numbers below be high at all.
    for (const uint32_t id : ALL_IDS) {
        const render::MeshData mesh = gameplay::build_interactable_mesh(id);
        float lo[3] = {2.0f, 2.0f, 2.0f};
        float hi[3] = {-2.0f, -2.0f, -2.0f};
        for (const platform::Vertex& v : mesh.vertices) {
            const float p[3] = {v.position.x, v.position.y, v.position.z};
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[k]);
                hi[k] = std::max(hi[k], p[k]);
            }
        }
        for (int k = 0; k < 3; ++k) {
            CHECK(lo[k] == doctest::Approx(-1.0f).epsilon(0.001));
            CHECK(hi[k] == doctest::Approx(1.0f).epsilon(0.001));
        }
    }
}

TEST_CASE("no face is wound inward: the silhouette has no holes in it") {
    // The six orderings in box() are typed once and trusted forever, and an
    // inward-wound quad is a face that VANISHES under backface culling — a hole
    // in the prop that looks exactly like the bug this whole file is about.
    //
    // Tested where it would show: fire a grid of parallel rays at the mesh from
    // outside, along each of the six axes, and require the FIRST surface each
    // one meets to face back at it. A "normal points away from the origin"
    // check would be simpler and wrong — these solids are unions of boxes that
    // do not straddle the origin, and it called 10 of the lever's 60 triangles
    // inward when every one of them was correct.
    for (const uint32_t id : ALL_IDS) {
        const render::MeshData mesh = gameplay::build_interactable_mesh(id);
        const glm::vec3 axes[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                   {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        int tested = 0;
        int facing = 0;
        for (const glm::vec3& dir : axes) {
            for (int i = 0; i < 17; ++i) {
                for (int j = 0; j < 17; ++j) {
                    const float u = -0.97f + 1.94f * (static_cast<float>(i) + 0.5f) / 17.0f;
                    const float v = -0.97f + 1.94f * (static_cast<float>(j) + 0.5f) / 17.0f;
                    glm::vec3 origin{0.0f};
                    if (std::abs(dir.x) > 0.5f) {
                        origin = {-dir.x * 4.0f, u, v};
                    } else if (std::abs(dir.y) > 0.5f) {
                        origin = {u, -dir.y * 4.0f, v};
                    } else {
                        origin = {u, v, -dir.z * 4.0f};
                    }
                    glm::vec3 normal{0.0f};
                    if (ray_mesh(mesh, origin, dir, &normal) <= 0.0f) {
                        continue;
                    }
                    ++tested;
                    if (glm::dot(normal, dir) < 0.0f) {
                        ++facing;
                    }
                }
            }
        }
        CHECK(tested > 500); // the mesh must actually be in the way
        CHECK(facing == tested);
    }
}

TEST_CASE("aiming anywhere in a prop's box shows the prop") {
    // THE MEASURED FORM-VS-COLLISION NUMBER, taken from where the player
    // actually stands: the app puts the door 2.5 m ahead of the spawn at eye
    // height, the lever and the torch 2 m to either side.
    const auto eye_h = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const glm::vec3 eye{0.0f, eye_h, 0.0f};

    struct Case {
        const char* name;
        uint32_t id;
        glm::vec3 center;
        glm::vec3 half_extents;
        float floor; // the coverage this prop must reach
    };
    // Centres and half-extents are the app's placement, relative to the spawn.
    const Case cases[3] = {
        {"door", gameplay::INTERACTABLE_MESH_DOOR, {0.0f, 1.0f, -2.5f}, {0.9f, 1.0f, 0.1f},
         0.99f},
        {"lever", gameplay::INTERACTABLE_MESH_LEVER, {-2.0f, 0.5f, 0.0f}, {0.25f, 0.25f, 0.25f},
         0.75f},
        {"torch", gameplay::INTERACTABLE_MESH_TORCH, {2.0f, 0.5f, 0.0f}, {0.25f, 0.25f, 0.25f},
         0.70f},
    };

    for (const Case& c : cases) {
        const render::MeshData mesh = gameplay::build_interactable_mesh(c.id);
        const float coverage = coverage_from(mesh, c.half_extents, c.center, eye);
        MESSAGE(std::string(c.name)
                << ": " << coverage * 100.0f
                << "% of the target box's silhouette, from the spawn eye, has "
                   "geometry behind it");
        CHECK(coverage >= c.floor);
        // The door is the one that can be exact, and it is: a slab that fills
        // its cube. Anything less would mean the prop standing dead ahead of
        // the spawn has holes in its own target box.
        if (c.id == gameplay::INTERACTABLE_MESH_DOOR) {
            CHECK(coverage == doctest::Approx(1.0f));
        }
    }
}

TEST_CASE("the ferry offers every id, and every id has geometry") {
    // A registry ferry that silently offered nothing would reproduce the
    // original defect exactly: props with mesh ids nothing uploaded.
    const std::vector<gameplay::InteractableMesh> meshes = gameplay::interactable_meshes();
    CHECK(meshes.size() == 3);
    for (const gameplay::InteractableMesh& m : meshes) {
        CHECK(m.mesh_asset != 0);
        CHECK_FALSE(m.vertices.empty());
        CHECK(m.indices.size() >= 3);
        CHECK(m.indices.size() % 3 == 0);
        for (const uint32_t i : m.indices) {
            CHECK(i < m.vertices.size());
        }
    }
    // Every verb's default mesh is one the ferry actually uploads. A verb whose
    // placeholder is not in the ferry is a prop that draws as nothing, which is
    // the whole defect wearing a different hat.
    for (const auto kind : {gameplay::InteractableKind::Pickup,
                            gameplay::InteractableKind::Openable,
                            gameplay::InteractableKind::Usable}) {
        const uint32_t id = gameplay::interactable_mesh_for(kind);
        const bool ferried = std::any_of(meshes.begin(), meshes.end(),
                                         [id](const gameplay::InteractableMesh& m) {
                                             return m.mesh_asset == id;
                                         });
        CHECK(ferried);
    }
}

TEST_CASE("the ids stay inside the range render blessed for this zone") {
    // 50..63, per the id map in engine/render/sources/ProcMesh.h. Two zones
    // picking the same number is a bug nobody finds until it draws — as the
    // wrong thing.
    for (const uint32_t id : ALL_IDS) {
        CHECK(id >= 50);
        CHECK(id <= 63);
    }
}

TEST_CASE("the FIRST entity a world spawns can still be interacted with") {
    // `EntityId::packed()` is `index << 32 | generation`, so entity {0, 0} packs
    // to exactly 0 — and update_hover used to discard every ray hit whose
    // user_data was 0 as "no entity". The first prop in a fresh world was
    // therefore untargetable: the ray found its box, the hit was thrown away,
    // and no prompt appeared however carefully the player aimed.
    //
    // The running game escaped it only because the app spawns the player and
    // hundreds of chunk entities before any prop. This case removes the luck.
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;
    world.add_resource(components::HoverTarget{});

    gameplay::InteractableDesc desc;
    desc.kind = gameplay::InteractableKind::Usable;
    desc.position = {2.0f, 1.5f, 0.0f};
    desc.prompt_key = "prompt.use";
    const dfn::ecs::EntityId first = gameplay::spawn_interactable(world, *physics, desc);
    // The precondition the case is about. If the ECS ever stops handing out
    // index 0 first, this REQUIRE says so instead of the case quietly passing
    // for a reason that has nothing to do with the bug.
    REQUIRE(first.index == 0u);
    REQUIRE(first.generation == 0u);
    REQUIRE(first.packed() == 0u);

    const dfn::ecs::EntityId player = world.spawn();
    world.add(player, gameplay::PlayerState{});
    world.add(player, components::CameraPose{{0.0f, 1.5f, 0.0f}, 1.5707963f, 0.0f});

    gameplay::update_hover(world, *physics);
    CHECK(world.resource<components::HoverTarget>().entity == first);
    CHECK(world.resource<components::HoverTarget>().verb ==
          static_cast<uint8_t>(gameplay::InteractionVerb::Use));
}

TEST_CASE("a prop's ray box dies with the prop") {
    // The handle used to be discarded at creation — `(void)create_static_box` —
    // so no prop's box could ever be destroyed. Taking an item despawned its
    // ENTITY and left an invisible ray target standing where it had been, which
    // stops the crosshair before whatever is actually behind it; every dropped
    // item added another, for the length of the session, against a world sized
    // for 16 384 bodies.
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;
    world.add_resource(components::HoverTarget{});

    gameplay::InteractableDesc desc;
    desc.kind = gameplay::InteractableKind::Pickup;
    desc.position = {2.0f, 1.5f, 0.0f};
    desc.prompt_key = "prompt.take";
    desc.item = gameplay::ItemId{1234};
    const dfn::ecs::EntityId prop = gameplay::spawn_interactable(world, *physics, desc);
    REQUIRE(world.has_resource<gameplay::InteractableBodies>());
    REQUIRE(world.resource<gameplay::InteractableBodies>().bodies.size() == 1);

    // A ray finds the box while the prop lives.
    const glm::vec3 eye{0.0f, 1.5f, 0.0f};
    const glm::vec3 dir{1.0f, 0.0f, 0.0f};
    REQUIRE(physics->raycast(eye, dir, 5.0f, physics_layer::LAYER_INTERACTABLE).hit);

    // The prop dies the way taking one kills it: deferred, then flushed.
    world.destroy_deferred(prop);
    world.flush_destroyed();
    REQUIRE_FALSE(world.alive(prop));
    // Still standing until something reaps it — that IS the defect, stated so
    // the case cannot pass by the box having never existed.
    CHECK(physics->raycast(eye, dir, 5.0f, physics_layer::LAYER_INTERACTABLE).hit);

    gameplay::reap_interactable_bodies(world, *physics);
    CHECK(world.resource<gameplay::InteractableBodies>().bodies.empty());
    CHECK_FALSE(physics->raycast(eye, dir, 5.0f, physics_layer::LAYER_INTERACTABLE).hit);
}

TEST_CASE("pressing the verb has a VISIBLE consequence") {
    // The user's complaint, stated as a test: «ни с чем взаимодействовать не
    // могу, хотя текст появляется». The chain was intact all the way to a last
    // step that changed a boolean nothing read — press, state changed, screen
    // identical. This asserts the screen is not identical.
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;
    world.add_resource(components::HoverTarget{});
    dfn::events::EventBus bus;

    gameplay::InteractableDesc door;
    door.kind = gameplay::InteractableKind::Openable;
    door.position = {0.0f, 1.0f, -2.5f};
    door.half_extents = {0.9f, 1.0f, 0.1f};
    door.prompt_key = "prompt.open";
    const dfn::ecs::EntityId leaf = gameplay::spawn_interactable(world, *physics, door);

    const glm::vec3 shut_pos = world.get<components::Transform>(leaf)->position;
    const glm::quat shut_rot = world.get<components::Transform>(leaf)->rotation;

    // CONTROL ARM (Rule 30): ticks with the door left shut must move nothing.
    for (int t = 0; t < 60; ++t) {
        gameplay::update_interactable_motion(world, *physics);
    }
    CHECK(glm::length(world.get<components::Transform>(leaf)->position - shut_pos) <
          1.0e-4f);

    // Open it the way the verb does.
    world.get<gameplay::Openable>(leaf)->open = true;
    for (int t = 0; t < 60; ++t) { // a second, well past the swing time
        gameplay::update_interactable_motion(world, *physics);
    }
    const components::Transform& now = *world.get<components::Transform>(leaf);

    // IT MOVED, and by an amount a player cannot miss. A door 1.8 m wide
    // hinged on its edge sweeps its centre through most of a metre.
    const float travel = glm::length(now.position - shut_pos);
    const float turned = turn_degrees(shut_rot, now.rotation);
    MESSAGE("the leaf's centre travelled " << travel << " m and turned " << turned
                                           << " deg");
    CHECK(travel > 0.5f);
    // AND IT TURNED A QUARTER: a door that slid sideways without turning would
    // pass the check above.
    CHECK(turned == doctest::Approx(90.0f).epsilon(0.02));

    // THE RAY TARGET WENT WITH IT. A door you can see but cannot aim at is the
    // same defect as a trunk you can see but walk through.
    const glm::vec3 eye{0.0f, 1.0f, 0.0f};
    const platform::RayHit at_the_doorway =
        physics->raycast(eye, {0.0f, 0.0f, -1.0f}, 3.0f, physics_layer::LAYER_INTERACTABLE);
    const glm::vec3 to_leaf = now.position - eye;
    const platform::RayHit at_the_leaf =
        physics->raycast(eye, glm::normalize(to_leaf), 4.0f, physics_layer::LAYER_INTERACTABLE);
    CHECK(at_the_leaf.hit);
    CHECK_FALSE(at_the_doorway.hit); // the leaf is not across the opening any more
    (void)bus;
}

TEST_CASE("a lever throws its handle when used") {
    auto physics = platform::create_jolt_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;

    gameplay::InteractableDesc lever;
    lever.kind = gameplay::InteractableKind::Usable;
    lever.position = {-2.0f, 1.3f, 0.0f};
    lever.prompt_key = "prompt.use";
    const dfn::ecs::EntityId id = gameplay::spawn_interactable(world, *physics, lever);
    const glm::quat rest = world.get<components::Transform>(id)->rotation;

    world.get<gameplay::Usable>(id)->used = true;
    for (int t = 0; t < 60; ++t) {
        gameplay::update_interactable_motion(world, *physics);
    }
    const glm::quat thrown = world.get<components::Transform>(id)->rotation;
    const float turned = turn_degrees(rest, thrown);
    MESSAGE("the handle turned " << turned << " deg");
    CHECK(turned > 30.0f);
    // A lever turns on its own body: its centre stays put, unlike a door leaf.
    CHECK(glm::length(world.get<components::Transform>(id)->position - lever.position) <
          1.0e-4f);
}

TEST_CASE("a settled door is STILL, not sweeping between its last two frames") {
    // The run-smear defect, one component over
    // (docs/FINDING_RUN_SMEAR.md). Render interpolates PreviousTransform ->
    // Transform with alpha sweeping 0..1 inside every tick, so a pair left
    // DISAGREEING is not a still object: it is an object that sweeps between
    // two poses for ever, at whatever the frame rate is. The swing's final tick
    // leaves exactly such a pair, and the at-rest branch used to return before
    // reconciling it.
    auto physics = platform::create_null_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;

    gameplay::InteractableDesc door;
    door.kind = gameplay::InteractableKind::Openable;
    door.position = {0.0f, 1.0f, -2.5f};
    door.half_extents = {0.9f, 1.0f, 0.1f};
    door.prompt_key = "prompt.open";
    const dfn::ecs::EntityId leaf = gameplay::spawn_interactable(world, *physics, door);

    world.get<gameplay::Openable>(leaf)->open = true;
    for (int t = 0; t < 120; ++t) { // two seconds: long past the 0.28 s swing
        gameplay::update_interactable_motion(world, *physics);
    }
    const components::Transform& curr = *world.get<components::Transform>(leaf);
    const components::PreviousTransform& prev =
        *world.get<components::PreviousTransform>(leaf);
    CHECK(glm::length(curr.position - prev.position) < 1.0e-6f);
    CHECK(std::abs(curr.rotation.w - prev.rotation.w) < 1.0e-6f);
    CHECK(std::abs(curr.rotation.y - prev.rotation.y) < 1.0e-6f);
    // And the same must hold for a door nobody ever touched.
    const dfn::ecs::EntityId shut = gameplay::spawn_interactable(world, *physics, door);
    for (int t = 0; t < 10; ++t) {
        gameplay::update_interactable_motion(world, *physics);
    }
    CHECK(glm::length(world.get<components::Transform>(shut)->position -
                      world.get<components::PreviousTransform>(shut)->position) < 1.0e-6f);
}

TEST_CASE("a mesh authored in metres still fills its own ray box") {
    // The promise this file exists for — the drawn prop and the box the
    // crosshair hits are the SAME object — held today only because every
    // placeholder is authored in the cube [-1,1]^3. A content mesh authored at
    // its real size would be scaled by half_extents too and come out stretched
    // by whatever those metres happen to be.
    //
    // The scale is half_extents / mesh_model_half_extents, so both cases are
    // one rule rather than two sets of numbers that have to agree.
    auto physics = platform::create_null_physics();
    REQUIRE(physics->init());
    dfn::ecs::World world;

    // (a) A placeholder: mesh space IS the unit cube, so scale == half_extents,
    //     exactly as before this field existed.
    gameplay::InteractableDesc unit;
    unit.kind = gameplay::InteractableKind::Openable;
    unit.half_extents = {0.9f, 1.0f, 0.1f};
    unit.prompt_key = "prompt.open";
    const dfn::ecs::EntityId a = gameplay::spawn_interactable(world, *physics, unit);
    const glm::vec3 sa = world.get<components::Transform>(a)->scale;
    CHECK(sa.x == doctest::Approx(0.9f));
    CHECK(sa.y == doctest::Approx(1.0f));
    CHECK(sa.z == doctest::Approx(0.1f));

    // (b) A mesh already modelled at 1.8 x 2.0 x 0.2 m declares its own
    //     half-extents and comes out at scale 1: drawn at its authored size,
    //     and the box is that size because it is the same set of numbers.
    gameplay::InteractableDesc metres = unit;
    metres.mesh_asset = 4321; // an id from outside this zone's range
    metres.mesh_model_half_extents = {0.9f, 1.0f, 0.1f};
    const dfn::ecs::EntityId b = gameplay::spawn_interactable(world, *physics, metres);
    const glm::vec3 sb = world.get<components::Transform>(b)->scale;
    CHECK(sb.x == doctest::Approx(1.0f));
    CHECK(sb.y == doctest::Approx(1.0f));
    CHECK(sb.z == doctest::Approx(1.0f));

    // (c) HALF the box, and the drawn thing halves with it — the ratio is what
    //     makes the two shapes one object rather than two that agree today.
    gameplay::InteractableDesc half = metres;
    half.half_extents = {0.45f, 0.5f, 0.05f};
    const dfn::ecs::EntityId c = gameplay::spawn_interactable(world, *physics, half);
    const glm::vec3 sc = world.get<components::Transform>(c)->scale;
    CHECK(sc.x == doctest::Approx(0.5f));
    CHECK(sc.y == doctest::Approx(0.5f));
    CHECK(sc.z == doctest::Approx(0.5f));
}
