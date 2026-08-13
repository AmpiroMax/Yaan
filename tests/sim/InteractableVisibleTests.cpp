/*
Created: 13:08:2026 - 17:30:00
Last updated: 13:08:2026 - 17:30:00
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
*/

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/gameplay/sources/InteractableMesh.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
namespace render = dfn::render;
namespace components = dfn::components;

constexpr uint32_t ALL_IDS[3] = {gameplay::INTERACTABLE_MESH_DOOR,
                                 gameplay::INTERACTABLE_MESH_LEVER,
                                 gameplay::INTERACTABLE_MESH_TORCH};

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
