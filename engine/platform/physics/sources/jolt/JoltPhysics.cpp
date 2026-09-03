/*
Module: engine/platform/physics
File: engine/platform/physics/sources/jolt/JoltPhysics.cpp

Responsibility:
- Jolt backend of IPhysics: fixed-step world, static terrain (triangle mesh
  from heightmap samples) and boxes, kinematic capsule characters via
  CharacterVirtual (collide-and-slide, stair stepping, slope limit), masked
  raycasts, PHYSICAL FEET (kinematic in the swing, dynamic with friction when
  planted), character impulses and contacts, ragdolls (parts + swing-twist
  joints + motors). The ONLY translation unit that includes Jolt (Rule 1).

Key items:
- JoltPhysics (file-local): the IPhysics implementation.
- Object layers: STATIC / CHARACTER / CHARACTER_GHOST (ghost = the character's
  raycastable inner body; collides with nothing) / DYNAMIC (loose props) /
  FOOT (feet: world only, never the capsule) / RAGDOLL (parts).
- MaskBodyFilter: filters queries/contacts by the engine's opaque CollisionMask
  stored per body — the backend never interprets mask bits (IPhysics.h note).
- MaskContactListener: the same AND, applied to SOLVER pairs for the bodies
  that carry a collides_with (feet, ragdoll parts) — a foot never touches a
  hitbox even though both sit on Jolt layers that may collide.
- CharacterPushListener: CharacterVirtual contacts → CharacterContact records
  and the mass rule of who pushes whom (CHARACTER_PUSH_MASS_KG).

Dependencies:
- Uses: interfaces/IPhysics.h, Jolt (v5.2.0), generated constants (GRAVITY).
- Used by: engine/app wiring (via CreateJoltPhysics.h), jolt-backed tests.

Notes:
- Terrain is a static MeshShape built from the float samples, not a
  JPH::HeightFieldShape: Jolt's height field wants sample counts divisible by
  its block size, while our chunks are 257x257 (NUMBERS, 2^n+1 with shared
  edges — 129x129 until 18:08:2026, and the +1 is exactly what makes the
  count odd, so no power-of-two block size ever divides it at any
  resolution). The mesh triangulation matches the render mesher's grid
  exactly — the same quad diagonal, not merely the same vertices — so collision and visuals
  cannot disagree. THAT SENTENCE WAS FALSE FOR TWO DAYS while it sat here: the
  two zones split every quad on OPPOSITE diagonals, and the only test covering
  it used a flat chunk, where the two splits are identically equal. A claim of
  agreement between two zones is worth exactly the test that could refute it
  (Rule 35), which now exists and runs against real generated terrain.
- move_character() accumulates the desired displacement; step() converts it to
  a velocity over SIM_DT and runs ExtendedUpdate (slide + stairs + stick to
  floor). Queries return post-step state, per the interface contract.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Keep the null backend's observable API semantics identical where defined.
- Do not interpret CollisionMask bits — store and AND them, nothing more.
*/

#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"

#include "engine/core/config/sources/Constants.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <limits>
#include <thread>
#include <unordered_map>
#include <vector>

namespace dfn::platform {
namespace {

// Squared-area threshold below which an extracted triangle is treated as
// degenerate. 1e-12 m^4 is ~1e-6 m^2 of area: far below anything the 1 m voxel
// grid produces intentionally, far above float noise.
constexpr float DEGENERATE_AREA_EPSILON = 1e-12f;

// --- Conversions -------------------------------------------------------------

[[nodiscard]] JPH::Vec3 to_jph(const glm::vec3& v) { return {v.x, v.y, v.z}; }
[[nodiscard]] glm::vec3 to_glm(JPH::Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }

// --- Layers ------------------------------------------------------------------
// Backend-internal Jolt layers; engine semantics stay in the opaque
// CollisionMask stored per body (never interpreted here).

namespace object_layers {
constexpr JPH::ObjectLayer STATIC = 0;
constexpr JPH::ObjectLayer CHARACTER = 1;
constexpr JPH::ObjectLayer CHARACTER_GHOST = 2; // raycast-only inner bodies
constexpr JPH::ObjectLayer DYNAMIC = 3;         // loose props (jugs, stools)
constexpr JPH::ObjectLayer FOOT = 4;            // physical feet (world only)
constexpr JPH::ObjectLayer RAGDOLL = 5;         // ragdoll parts
constexpr JPH::uint COUNT = 6;
} // namespace object_layers

namespace broad_phase_layers {
constexpr JPH::BroadPhaseLayer STATIC{0};
constexpr JPH::BroadPhaseLayer MOVING{1};
constexpr JPH::uint COUNT = 2;
} // namespace broad_phase_layers

class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return broad_phase_layers::COUNT; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == object_layers::STATIC ? broad_phase_layers::STATIC
                                              : broad_phase_layers::MOVING;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == broad_phase_layers::STATIC ? "STATIC" : "MOVING";
    }
#endif
};

class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        // Characters query static geometry AND the loose props they shove
        // aside; ghosts collide with nothing.
        if (layer == object_layers::CHARACTER_GHOST) {
            return false;
        }
        if (layer == object_layers::CHARACTER) {
            return bp == broad_phase_layers::STATIC || bp == broad_phase_layers::MOVING;
        }
        // A loose prop falls on the world and lands on other props; a foot
        // and a ragdoll part stand on the world and on the props.
        if (layer == object_layers::DYNAMIC || layer == object_layers::FOOT
            || layer == object_layers::RAGDOLL) {
            return true;
        }
        return false; // static vs anything: static never queries
    }
};

class ObjectPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        const auto pair_is = [&](JPH::ObjectLayer x, JPH::ObjectLayer y) {
            return (a == x && b == y) || (a == y && b == x);
        };
        return pair_is(object_layers::CHARACTER, object_layers::STATIC)
            // A PROP FALLS ON THE WORLD, STACKS ON ITS OWN KIND, AND IS SHOVED
            // BY THE PLAYER. The third pair is what makes the capsule a body
            // in the world rather than a ghost that walks through the crockery.
            || pair_is(object_layers::DYNAMIC, object_layers::STATIC)
            || (a == object_layers::DYNAMIC && b == object_layers::DYNAMIC)
            || pair_is(object_layers::CHARACTER, object_layers::DYNAMIC)
            // A FOOT TOUCHES THE WORLD AND THE PROPS AND NOTHING OF ITS OWN
            // BODY: not the capsule (the capsule answers "where can I walk",
            // and a foot inside it would be a wall the walker carries) and
            // not the other foot. Hitboxes sit on STATIC and are kept off by
            // the engine mask (MaskContactListener), not by this table.
            || pair_is(object_layers::FOOT, object_layers::STATIC)
            || pair_is(object_layers::FOOT, object_layers::DYNAMIC)
            // A ragdoll lies on the world, on props, on other ragdolls, and
            // the walker shoves it aside like any loose body.
            || pair_is(object_layers::RAGDOLL, object_layers::STATIC)
            || pair_is(object_layers::RAGDOLL, object_layers::DYNAMIC)
            || (a == object_layers::RAGDOLL && b == object_layers::RAGDOLL)
            || pair_is(object_layers::RAGDOLL, object_layers::CHARACTER);
    }
};

// Filters bodies by the engine's opaque per-body CollisionMask (stored by the
// backend at creation): pass iff (body_mask & query_mask) != 0.
class MaskBodyFilter final : public JPH::BodyFilter {
public:
    MaskBodyFilter(const std::unordered_map<JPH::uint32, CollisionMask>& masks,
                   CollisionMask query_mask)
        : masks_(masks), query_mask_(query_mask) {}

    bool ShouldCollide(const JPH::BodyID& body_id) const override {
        const auto it = masks_.find(body_id.GetIndexAndSequenceNumber());
        const CollisionMask body_mask = it != masks_.end() ? it->second : COLLIDE_ALL;
        return (body_mask & query_mask_) != 0;
    }

private:
    const std::unordered_map<JPH::uint32, CollisionMask>& masks_;
    CollisionMask query_mask_;
};

// SOLVER PAIRS FILTERED BY THE ENGINE'S MASKS. Jolt's layer table above says
// which KINDS of body may meet; the engine's masks say which INSTANCES may.
// Only bodies that carry a `collides_with` (feet, ragdoll parts) are checked
// — a foot's mask keeps it off the hitboxes, which are Jolt-static and would
// otherwise stop it. Read-only during Update (called from worker threads);
// both maps are written only between steps.
class MaskContactListener final : public JPH::ContactListener {
public:
    MaskContactListener(const std::unordered_map<JPH::uint32, CollisionMask>& masks,
                        const std::unordered_map<JPH::uint32, CollisionMask>& collides_with)
        : masks_(masks), collides_with_(collides_with) {}

    JPH::ValidateResult OnContactValidate(const JPH::Body& body1, const JPH::Body& body2,
                                          JPH::RVec3Arg base_offset,
                                          const JPH::CollideShapeResult& result) override {
        (void)base_offset;
        (void)result;
        return allowed(body1.GetID(), body2.GetID()) && allowed(body2.GetID(), body1.GetID())
                   ? JPH::ValidateResult::AcceptAllContactsForThisBodyPair
                   : JPH::ValidateResult::RejectAllContactsForThisBodyPair;
    }

private:
    [[nodiscard]] bool allowed(const JPH::BodyID& self, const JPH::BodyID& other) const {
        const auto cw = collides_with_.find(self.GetIndexAndSequenceNumber());
        if (cw == collides_with_.end()) {
            return true; // a body without a collides_with meets whatever the layers allow
        }
        const auto m = masks_.find(other.GetIndexAndSequenceNumber());
        const CollisionMask other_mask = m != masks_.end() ? m->second : COLLIDE_ALL;
        return (other_mask & cw->second) != 0;
    }

    const std::unordered_map<JPH::uint32, CollisionMask>& masks_;
    const std::unordered_map<JPH::uint32, CollisionMask>& collides_with_;
};

// THE CAPSULE'S CONTACTS, AND WHO PUSHES WHOM. CharacterVirtual asks this for
// every body it touches during ExtendedUpdate (on the calling thread, one
// character at a time): the answer is the mass rule — lighter than
// CHARACTER_PUSH_MASS_KG the body is shoved and cannot shove back; heavier or
// static it shoves the capsule and the capsule cannot shove it. Each contact
// is also written down for character_contacts().
class CharacterPushListener final : public JPH::CharacterContactListener {
public:
    std::vector<CharacterContact>* sink = nullptr;    // set before each update
    const JPH::BodyLockInterface* locks = nullptr;

    void OnContactAdded(const JPH::CharacterVirtual* character, const JPH::BodyID& body_id,
                        const JPH::SubShapeID& sub_shape, JPH::RVec3Arg position,
                        JPH::Vec3Arg normal, JPH::CharacterContactSettings& settings) override {
        (void)sub_shape;
        CharacterContact contact;
        contact.mass_kg = std::numeric_limits<float>::infinity();
        contact.point = to_glm(JPH::Vec3(position));
        // Jolt hands the normal pointing from the character INTO the body
        // (measured: a crate on the −X side reports −X). The contract wants
        // the direction the character is pushed — the same axis, flipped.
        contact.normal = -to_glm(normal);
        JPH::Vec3 body_velocity = JPH::Vec3::sZero();
        if (locks != nullptr) {
            JPH::BodyLockRead lock(*locks, body_id);
            if (lock.Succeeded()) {
                const JPH::Body& body = lock.GetBody();
                contact.user_data = body.GetUserData();
                if (body.IsDynamic()) {
                    const float inverse_mass =
                        body.GetMotionProperties()->GetInverseMassUnchecked();
                    if (inverse_mass > 0.0f) {
                        contact.mass_kg = 1.0f / inverse_mass;
                    }
                }
                if (!body.IsStatic()) {
                    body_velocity = body.GetPointVelocity(position);
                }
            }
        }
        contact.relative_velocity = to_glm(body_velocity - character->GetLinearVelocity());
        const bool heavy =
            contact.mass_kg >= static_cast<float>(dfn::config::CHARACTER_PUSH_MASS_KG);
        settings.mCanPushCharacter = heavy;
        settings.mCanReceiveImpulses = !heavy;
        contact.pushed_character = heavy;
        contact.pushed_body = !heavy;
        if (sink != nullptr) {
            sink->push_back(contact);
        }
    }
};

// --- Backend -----------------------------------------------------------------

class JoltPhysics final : public IPhysics {
public:
    bool init() override {
        // Process-global Jolt bootstrap; idempotent across instances (tests).
        static bool jolt_registered = [] {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
            return true;
        }();
        (void)jolt_registered;

        // Backend capacities (engine internals, not gameplay constants):
        // enough for the testbed's chunks, props and characters (NUMBERS
        // TESTBED_*); revisit at the streaming-budget sync.
        constexpr JPH::uint MAX_BODIES = 16384;
        constexpr JPH::uint BODY_MUTEXES = 0; // 0 = Jolt default
        constexpr JPH::uint MAX_BODY_PAIRS = 16384;
        constexpr JPH::uint MAX_CONTACTS = 8192;
        constexpr JPH::uint TEMP_ALLOC_BYTES = 16 * 1024 * 1024;

        temp_allocator_ = std::make_unique<JPH::TempAllocatorImpl>(TEMP_ALLOC_BYTES);
        job_system_ = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
            std::max(1u, std::thread::hardware_concurrency() - 1u));

        system_ = std::make_unique<JPH::PhysicsSystem>();
        system_->Init(MAX_BODIES, BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACTS,
                      bp_layer_map_, object_vs_bp_filter_, pair_filter_);
        // The registry's gravity, for the dynamic bodies (feet, props,
        // ragdolls) — the same number the characters are handed each step.
        system_->SetGravity({0.0f, -static_cast<float>(dfn::config::GRAVITY), 0.0f});
        system_->SetContactListener(&mask_contact_listener_);
        return true;
    }

    void shutdown() override {
        for (auto& [id, character] : characters_) {
            character.virtual_character = nullptr;
        }
        characters_.clear();
        // Constraints are released before the system that owns their bodies.
        for (auto& [id, ragdoll] : ragdolls_) {
            release_ragdoll(ragdoll);
        }
        ragdolls_.clear();
        feet_.clear();
        bodies_.clear();
        body_masks_.clear();
        body_collides_with_.clear();
        body_substances_.clear();
        system_.reset();
        job_system_.reset();
        temp_allocator_.reset();
    }

    // Simulation ---------------------------------------------------------------
    void step(float dt) override {
        if (!system_) {
            return;
        }
        if (broad_phase_dirty_) {
            system_->OptimizeBroadPhase();
            broad_phase_dirty_ = false;
        }

        // Feet in the swing are driven to the animation's pose over this step
        // (a velocity, so a cup in the way is shoved rather than skipped).
        // Without a new pose the last one stands, which is a velocity of zero.
        for (auto& [id, foot] : feet_) {
            if (foot.mode == FootMode::Swing) {
                system_->GetBodyInterface().MoveKinematic(
                    foot.body, JPH::RVec3(to_jph(foot.target.position)),
                    JPH::Quat{foot.target.rotation.x, foot.target.rotation.y,
                              foot.target.rotation.z, foot.target.rotation.w},
                    dt);
            }
        }

        // Characters first: collide-and-slide against the current static world.
        const JPH::Vec3 gravity{0.0f, -static_cast<float>(dfn::config::GRAVITY), 0.0f};
        for (auto& [id, character] : characters_) {
            JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
            update_settings.mWalkStairsStepUp = {0.0f, character.step_height, 0.0f};
            update_settings.mStickToFloorStepDown = {0.0f, -character.step_height, 0.0f};

            const glm::vec3 pending = character.pending;
            // THE SHOVE RIDES ON TOP OF THE WALK: the displacement the
            // controller asked for, plus the velocity an impulse left behind.
            character.virtual_character->SetLinearVelocity(
                to_jph(character.pending / dt + character.push_velocity));
            character.contacts.clear();
            push_listener_.sink = &character.contacts;
            push_listener_.locks = &system_->GetBodyLockInterface();
            const MaskBodyFilter body_filter{body_masks_, character.collides_with};
            character.virtual_character->ExtendedUpdate(
                dt, gravity, update_settings,
                system_->GetDefaultBroadPhaseLayerFilter(object_layers::CHARACTER),
                system_->GetDefaultLayerFilter(object_layers::CHARACTER), body_filter,
                {}, *temp_allocator_);
            push_listener_.sink = nullptr;
            character.pending = glm::vec3{0.0f};
            character.velocity = to_glm(character.virtual_character->GetLinearVelocity());
            // A standing body catches itself: the shove decays with
            // CHARACTER_PUSH_DECAY_S while there is ground to push against.
            // In the air nothing stops it but the landing.
            if (character.virtual_character->GetGroundState() ==
                JPH::CharacterBase::EGroundState::OnGround) {
                character.push_velocity *= std::exp(
                    -dt / static_cast<float>(dfn::config::CHARACTER_PUSH_DECAY_S));
            }
            // ОТЛАДОЧНАЯ ДВЕРЬ DFN_CHAR_TRACE=1: раз в полсекунды — вход и
            // исход контроллера. Заведена на охоте за ботом Вайтрана, который
            // намертво вставал на ступени, где голая капсула с теми же
            // параметрами проходит: расхождение можно поймать только цифрами
            // из ЖИВОГО конвейера (21.08).
            static const bool char_trace = std::getenv("DFN_CHAR_TRACE") != nullptr;
            // Разовая перепись ВСЕХ тел (отладочная дверь): охота на стену,
            // которой нет ни в одном из известных источников (21.08).
            static bool bodies_dumped = false;
            if (char_trace && !bodies_dumped) {
                bodies_dumped = true;
                JPH::BodyIDVector ids;
                system_->GetBodies(ids);
                std::fprintf(stderr, "[тела] всего %zu\n", ids.size());
                for (const JPH::BodyID id : ids) {
                    JPH::AABox box = system_->GetBodyInterface()
                                         .GetTransformedShape(id)
                                         .GetWorldSpaceBounds();
                    std::fprintf(stderr,
                                 "[тело] id=%u x %.1f..%.1f y %.1f..%.1f "
                                 "z %.1f..%.1f\n",
                                 id.GetIndex(),
                                 static_cast<double>(box.mMin.GetX()),
                                 static_cast<double>(box.mMax.GetX()),
                                 static_cast<double>(box.mMin.GetY()),
                                 static_cast<double>(box.mMax.GetY()),
                                 static_cast<double>(box.mMin.GetZ()),
                                 static_cast<double>(box.mMax.GetZ()));
                }
            }
            if (char_trace) {
                static int trace_tick = 0;
                if (++trace_tick % 30 == 0) {
                    const JPH::RVec3 p = character.virtual_character->GetPosition();
                    const JPH::Vec3 n = character.virtual_character->GetGroundNormal();
                    const JPH::Vec3 v = character.virtual_character->GetLinearVelocity();
                    // СОСТОЯНИЕ ОПОРЫ — СЛОВОМ, НЕ ЧИСЛОМ. `ground=%d` печатал
                    // сырой Jolt EGroundState, где OnGround == 0: приёмка
                    // 22.08 прочла «ground=0 все 160 раз» как непрерывный срыв
                    // и завела претензию на землю, которая всё время держала.
                    // Два прибора одного прогона (эта трасса и rec.log с
                    // булевым ground=) печатали одно имя с противоположной
                    // конвенцией — таких имён больше нет.
                    const auto gs = character.virtual_character->GetGroundState();
                    const char* gs_name =
                        gs == JPH::CharacterBase::EGroundState::OnGround ? "on"
                        : gs == JPH::CharacterBase::EGroundState::OnSteepGround
                            ? "steep"
                        : gs == JPH::CharacterBase::EGroundState::NotSupported
                            ? "unsupported"
                            : "air";
                    std::fprintf(stderr,
                                 "[char] pos=(%.2f %.2f %.2f) ground=%s "
                                 "n=(%.2f %.2f %.2f) pend=(%.3f %.3f %.3f) "
                                 "v=(%.2f %.2f %.2f)\n",
                                 static_cast<double>(p.GetX()), static_cast<double>(p.GetY()),
                                 static_cast<double>(p.GetZ()),
                                 gs_name,
                                 static_cast<double>(n.GetX()), static_cast<double>(n.GetY()),
                                 static_cast<double>(n.GetZ()),
                                 static_cast<double>(pending.x), static_cast<double>(pending.y),
                                 static_cast<double>(pending.z),
                                 static_cast<double>(v.GetX()), static_cast<double>(v.GetY()),
                                 static_cast<double>(v.GetZ()));
                }
            }
        }

        constexpr int COLLISION_STEPS = 1; // one fixed step per call (Rule 12)
        system_->Update(dt, COLLISION_STEPS, temp_allocator_.get(), job_system_.get());
    }

    // Static bodies ------------------------------------------------------------
    PhysicsBodyHandle create_terrain_mesh(const TerrainMeshDesc& desc) override {
        if (desc.layer == 0 || desc.indices.size() < 3) {
            return {}; // empty mesh: nothing to collide, not an error
        }

        JPH::VertexList vertices;
        vertices.reserve(desc.positions.size());
        for (const glm::vec3& p : desc.positions) {
            vertices.push_back(JPH::Float3(p.x, p.y, p.z));
        }

        // Extraction can emit degenerate (zero-area) triangles at cell corners;
        // Jolt rejects the whole mesh on those, so drop them here instead.
        JPH::IndexedTriangleList triangles;
        triangles.reserve(desc.indices.size() / 3);
        const auto vertex_count = static_cast<uint32_t>(desc.positions.size());
        for (size_t i = 0; i + 2 < desc.indices.size(); i += 3) {
            const uint32_t a = desc.indices[i];
            const uint32_t b = desc.indices[i + 1];
            const uint32_t c = desc.indices[i + 2];
            if (a >= vertex_count || b >= vertex_count || c >= vertex_count) {
                continue; // malformed index: skip rather than corrupt the shape
            }
            if (a == b || b == c || a == c) {
                continue; // degenerate by index
            }
            const glm::vec3 e1 = desc.positions[b] - desc.positions[a];
            const glm::vec3 e2 = desc.positions[c] - desc.positions[a];
            const glm::vec3 cross = glm::cross(e1, e2);
            if (glm::dot(cross, cross) <= DEGENERATE_AREA_EPSILON) {
                continue; // degenerate by area (no GTX: dot(cross, cross))
            }
            triangles.push_back(JPH::IndexedTriangle(a, b, c));
        }
        if (triangles.empty()) {
            return {};
        }

        auto shape_result = JPH::MeshShapeSettings(vertices, triangles).Create();
        if (shape_result.HasError()) {
            return {};
        }
        return add_static_body(shape_result.Get(), JPH::RVec3::sZero(),
                               JPH::Quat::sIdentity(), desc.layer, desc.user_data,
                               desc.substance);
    }

    PhysicsBodyHandle create_terrain(const TerrainDesc& desc) override {
        // layer == 0 is unreachable by construction (see the contract note in
        // IPhysics.h): a body no mask can select is never intentional.
        if (desc.layer == 0 || desc.sample_count_x < 2 || desc.sample_count_z < 2 ||
            desc.heights.size() <
                static_cast<size_t>(desc.sample_count_x) * desc.sample_count_z) {
            return {};
        }

        // Grid triangulation identical to the render mesher — and identical is
        // now a TESTED claim rather than a comment (sim_jolt_physics, "render
        // and collision split every quad on the same diagonal"). It was false
        // for two days: this loop split each quad on i01-i10 while the render
        // mesher splits on i00-i11, so the drawn ground and the solid ground
        // were different surfaces wherever the four corners were not coplanar.
        JPH::VertexList vertices;
        vertices.reserve(desc.heights.size());
        for (uint32_t z = 0; z < desc.sample_count_z; ++z) {
            for (uint32_t x = 0; x < desc.sample_count_x; ++x) {
                vertices.push_back(JPH::Float3(
                    desc.origin.x + static_cast<float>(x) * desc.sample_spacing,
                    desc.heights[static_cast<size_t>(z) * desc.sample_count_x + x],
                    desc.origin.z + static_cast<float>(z) * desc.sample_spacing));
            }
        }
        JPH::IndexedTriangleList triangles;
        triangles.reserve(static_cast<size_t>(desc.sample_count_x - 1) *
                          (desc.sample_count_z - 1) * 2);
        for (uint32_t z = 0; z + 1 < desc.sample_count_z; ++z) {
            for (uint32_t x = 0; x + 1 < desc.sample_count_x; ++x) {
                const JPH::uint32 i00 = z * desc.sample_count_x + x;
                const JPH::uint32 i10 = i00 + 1;
                const JPH::uint32 i01 = i00 + desc.sample_count_x;
                const JPH::uint32 i11 = i01 + 1;
                // Split on i00-i11, the diagonal the render mesher uses, in the
                // winding that makes cross(v1-v0, v2-v0) point at +Y (up-facing
                // for both triangles). Which diagonal is arbitrary; that the
                // two zones pick THE SAME one is not, and the choice belongs to
                // whoever draws the ground — collision follows it.
                triangles.push_back(JPH::IndexedTriangle(i00, i11, i10));
                triangles.push_back(JPH::IndexedTriangle(i00, i01, i11));
            }
        }

        auto shape_result = JPH::MeshShapeSettings(vertices, triangles).Create();
        if (shape_result.HasError()) {
            return {};
        }
        return add_static_body(shape_result.Get(), JPH::RVec3::sZero(),
                               JPH::Quat::sIdentity(), desc.layer, desc.user_data,
                               desc.substance);
    }

    PhysicsBodyHandle create_static_box(const StaticBoxDesc& desc) override {
        const JPH::Vec3 half = to_jph(desc.half_extents);
        if (desc.layer == 0 || half.GetX() <= 0.0f || half.GetY() <= 0.0f ||
            half.GetZ() <= 0.0f) {
            return {};
        }
        const JPH::Quat rotation{desc.rotation.x, desc.rotation.y, desc.rotation.z,
                                 desc.rotation.w};
        return add_static_body(new JPH::BoxShape(half), JPH::RVec3(to_jph(desc.center)),
                               rotation, desc.layer, desc.user_data, desc.substance);
    }

    void set_body_transform(PhysicsBodyHandle body, const glm::vec3& position,
                            const glm::quat& rotation) override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end()) {
            return; // a caller with no body has nothing to move; not an error
        }
        // DontActivate: these are static bodies, and there is nothing to wake.
        // The broad phase is told about the move by SetPositionAndRotation
        // itself, which is the whole reason it must not be done by hand.
        system_->GetBodyInterface().SetPositionAndRotation(
            it->second, JPH::RVec3(to_jph(position)),
            JPH::Quat{rotation.x, rotation.y, rotation.z, rotation.w},
            JPH::EActivation::DontActivate);
    }

    // Dynamic bodies -----------------------------------------------------------
    PhysicsBodyHandle create_dynamic_body(const DynamicBodyDesc& desc) override {
        if (!system_ || desc.layer == 0 || desc.mass_kg <= 0.0f
            || desc.points.size() < 4) {
            return {};
        }
        JPH::Array<JPH::Vec3> points;
        points.reserve(desc.points.size());
        for (const glm::vec3& p : desc.points) {
            points.push_back(to_jph(p));
        }
        // CONVEX RADIUS ZERO. Jolt's default 0.05 m shrinks the hull by five
        // centimetres and rounds it back — invisible on a crate, HALF of a
        // 0.11 m cup. The prop must touch the table where it is drawn touching
        // it, so the hull is the hull.
        JPH::ConvexHullShapeSettings hull(points, 0.0f);
        auto shape_result = hull.Create();
        if (shape_result.HasError()) {
            return {}; // coplanar / degenerate: no volume, no body
        }
        JPH::BodyCreationSettings settings(
            shape_result.Get(), JPH::RVec3(to_jph(desc.position)),
            JPH::Quat{desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w},
            JPH::EMotionType::Dynamic, object_layers::DYNAMIC);
        settings.mUserData = desc.user_data;
        // ТРЕНИЕ И УПРУГОСТЬ — ИЗ ЗАПИСИ ВЕЩЕСТВА, не из полей вызова. Пара
        // складывается умолчаниями Jolt (sqrt для трения, max для упругости),
        // и это правило названо один раз — в PhysicsSubstance.h.
        const core::PhysicsSubstance& what = core::substance(desc.substance);
        settings.mFriction = what.friction;
        settings.mRestitution = what.restitution;
        settings.mLinearDamping = desc.linear_damping;
        settings.mAngularDamping = desc.angular_damping;
        // MASS IS THE CALLER'S (volume x density), and the INERTIA is scaled
        // from the hull's own shape to match it. Letting Jolt compute the mass
        // from the shape at unit density would make a clay jug and a wooden
        // stool of the same size weigh the same, which is the whole point of
        // the substance table on the other side of this call.
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.mass_kg;
        // ALLOW SLEEPING is Jolt's default and is stated here on purpose: the
        // stack acceptance ("three bowls stand for ten seconds without a
        // shiver") is measured as sleep, and a body that may not sleep cannot
        // pass it however still it looks.
        settings.mAllowSleeping = true;
        const JPH::BodyID body_id = system_->GetBodyInterface().CreateAndAddBody(
            settings, desc.start_asleep ? JPH::EActivation::DontActivate
                                        : JPH::EActivation::Activate);
        if (body_id.IsInvalid()) {
            return {};
        }
        const PhysicsBodyHandle handle{next_id_++};
        bodies_.emplace(handle.id, body_id);
        body_masks_[body_id.GetIndexAndSequenceNumber()] = desc.layer;
        body_substances_[body_id.GetIndexAndSequenceNumber()] = desc.substance;
        return handle;
    }

    BodyPose body_pose(PhysicsBodyHandle body) const override {
        BodyPose pose;
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end() || !system_) {
            return pose;
        }
        JPH::RVec3 position;
        JPH::Quat rotation;
        system_->GetBodyInterface().GetPositionAndRotation(it->second, position, rotation);
        pose.position = to_glm(JPH::Vec3(position));
        pose.rotation = glm::quat{rotation.GetW(), rotation.GetX(), rotation.GetY(),
                                  rotation.GetZ()};
        return pose;
    }

    glm::vec3 body_velocity(PhysicsBodyHandle body) const override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end() || !system_) {
            return glm::vec3{0.0f};
        }
        return to_glm(system_->GetBodyInterface().GetLinearVelocity(it->second));
    }

    void set_body_velocity(PhysicsBodyHandle body, const glm::vec3& linear,
                           const glm::vec3& angular) override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end() || !system_) {
            return;
        }
        auto& bi = system_->GetBodyInterface();
        // WAKE FIRST, then write: a velocity written to a sleeping body is
        // dropped by the solver, and the prop the player is dragging would
        // stay put for exactly as long as it took to notice.
        bi.ActivateBody(it->second);
        bi.SetLinearAndAngularVelocity(it->second, to_jph(linear), to_jph(angular));
    }

    void set_body_gravity_factor(PhysicsBodyHandle body, float factor) override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end() || !system_) {
            return;
        }
        system_->GetBodyInterface().SetGravityFactor(it->second, factor);
    }

    bool body_asleep(PhysicsBodyHandle body) const override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end() || !system_) {
            return true; // nothing there is not moving
        }
        return !system_->GetBodyInterface().IsActive(it->second);
    }

    void activate_body(PhysicsBodyHandle body) override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end() || !system_) {
            return;
        }
        system_->GetBodyInterface().ActivateBody(it->second);
    }

    void destroy_body(PhysicsBodyHandle body) override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end()) {
            return;
        }
        auto& body_interface = system_->GetBodyInterface();
        const JPH::uint32 key = it->second.GetIndexAndSequenceNumber();
        body_masks_.erase(key);
        body_collides_with_.erase(key);
        body_substances_.erase(key);
        feet_.erase(body.id); // a foot is a body; its record goes with it
        body_interface.RemoveBody(it->second);
        body_interface.DestroyBody(it->second);
        bodies_.erase(it);
    }

    // Character controller -----------------------------------------------------
    CharacterHandle create_character(const CharacterDesc& desc) override {
        // layer == 0: the character's own body would be unhittable by any
        // raycast. collides_with == 0: it would walk through the world.
        if (!system_ || desc.layer == 0 || desc.collides_with == 0) {
            return {};
        }
        const JPH::ShapeRefC shape = make_capsule(desc.radius, desc.height);
        if (shape == nullptr) {
            return {};
        }

        JPH::CharacterVirtualSettings settings;
        settings.mShape = shape;
        settings.mMaxSlopeAngle = desc.max_slope_radians;
        settings.mInnerBodyShape = shape; // raycastable ghost body
        settings.mInnerBodyLayer = object_layers::CHARACTER_GHOST;
        // THE WALKING SHOVE (28.08). Jolt's CharacterVirtual pushes the
        // dynamic bodies it touches with a force capped at mMaxStrength; that
        // cap IS the engine's "how heavy a thing does the player's body move".
        settings.mMaxStrength = desc.push_force_n;
        // THE WALKING BODY'S WEIGHT: the m of dv = J / m, and what the capsule
        // sets against a body that shoves it. 0 = the registry's number.
        settings.mMass = desc.mass_kg > 0.0f
                             ? desc.mass_kg
                             : static_cast<float>(dfn::config::CHARACTER_MASS_KG);

        Character character;
        character.virtual_character = new JPH::CharacterVirtual(
            &settings, JPH::RVec3(to_jph(desc.position)), JPH::Quat::sIdentity(),
            desc.user_data, system_.get());
        character.virtual_character->SetListener(&push_listener_);
        character.mass = settings.mMass;
        character.step_height = desc.step_height;
        character.collides_with = desc.collides_with;
        character.radius = desc.radius;
        character.height = desc.height;

        const JPH::BodyID inner = character.virtual_character->GetInnerBodyID();
        if (!inner.IsInvalid()) {
            system_->GetBodyInterface().SetUserData(inner, desc.user_data);
            body_masks_[inner.GetIndexAndSequenceNumber()] = desc.layer;
        }

        const CharacterHandle handle{next_id_++};
        characters_.emplace(handle.id, std::move(character));
        return handle;
    }

    void destroy_character(CharacterHandle character) override {
        const auto it = characters_.find(character.id);
        if (it == characters_.end()) {
            return;
        }
        const JPH::BodyID inner = it->second.virtual_character->GetInnerBodyID();
        if (!inner.IsInvalid()) {
            body_masks_.erase(inner.GetIndexAndSequenceNumber());
        }
        characters_.erase(it); // Ref<> releases the CharacterVirtual (+ inner body)
    }

    void move_character(CharacterHandle character, const glm::vec3& displacement) override {
        if (auto it = characters_.find(character.id); it != characters_.end()) {
            it->second.pending += displacement;
        }
    }

    glm::vec3 character_position(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end()
                   ? to_glm(it->second.virtual_character->GetPosition())
                   : glm::vec3{0.0f};
    }

    bool character_grounded(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() &&
               it->second.virtual_character->GetGroundState() ==
                   JPH::CharacterVirtual::EGroundState::OnGround;
    }

    // Crouch. The capsule is rebuilt at the new height and stays anchored at
    // its BOTTOM point (the offset inside make_capsule), so the feet do not
    // move and only the head does — standing up grows into the space above,
    // which is exactly the space the caller must have checked is free.
    void set_character_height(CharacterHandle character, float height) override {
        auto it = characters_.find(character.id);
        if (it == characters_.end() || !system_) {
            return;
        }
        const JPH::ShapeRefC shape = make_capsule(it->second.radius, height);
        if (shape == nullptr) {
            return; // dimensions cannot form a capsule: change nothing
        }
        // max_penetration_depth 0 + lock_bodies false: SetShape is called from
        // the fixed tick, outside step(), so no body locks are held.
        it->second.virtual_character->SetShape(shape, FLT_MAX, {}, {}, {}, {},
                                               *temp_allocator_);
        it->second.virtual_character->SetInnerBodyShape(shape);
        it->second.height = height;
    }

    float character_height(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() ? it->second.height : 0.0f;
    }

    void teleport_character(CharacterHandle character, const glm::vec3& position) override {
        if (auto it = characters_.find(character.id); it != characters_.end()) {
            it->second.virtual_character->SetPosition(JPH::RVec3(to_jph(position)));
            it->second.pending = glm::vec3{0.0f};
        }
    }

    // Queries ------------------------------------------------------------------
    RayHit raycast(const glm::vec3& origin, const glm::vec3& direction,
                   float max_distance, CollisionMask mask) const override {
        RayHit result;
        if (!system_ || max_distance <= 0.0f) {
            return result;
        }
        const JPH::RRayCast ray{JPH::RVec3(to_jph(origin)),
                                to_jph(direction * max_distance)};
        JPH::RayCastResult hit;
        const MaskBodyFilter body_filter{body_masks_, mask};
        if (!system_->GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, body_filter)) {
            return result;
        }

        result.hit = true;
        result.distance = hit.mFraction * max_distance;
        result.position = origin + direction * result.distance;
        JPH::BodyLockRead lock(system_->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            result.normal = to_glm(body.GetWorldSpaceSurfaceNormal(
                hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction)));
            result.user_data = body.GetUserData();
        }
        return result;
    }

    RayHit sphere_cast(const glm::vec3& origin, const glm::vec3& direction, float radius,
                       float max_distance, CollisionMask mask) const override {
        RayHit result;
        if (!system_ || max_distance <= 0.0f || radius <= 0.0f) {
            return result;
        }
        const JPH::SphereShape sphere(radius);
        const JPH::RShapeCast cast(&sphere, JPH::Vec3::sReplicate(1.0f),
                                   JPH::RMat44::sTranslation(JPH::RVec3(to_jph(origin))),
                                   to_jph(direction * max_distance));
        // BACK FACES ARE SOLID. Interior shells and terrain are one-sided
        // meshes; with the default (ignore back faces) a sweep that starts on
        // the outside — or a wall whose winding faces away — reports clear, and
        // the camera goes exactly where this call exists to stop it.
        JPH::ShapeCastSettings settings;
        settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
        settings.mBackFaceModeConvex = JPH::EBackFaceMode::CollideWithBackFaces;
        // Jolt's own "started penetrating" answer, kept rather than filtered:
        // the contract says an overlapping start reports distance 0.
        settings.mReturnDeepestPoint = true;
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const MaskBodyFilter body_filter{body_masks_, mask};
        system_->GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(),
                                                 collector, {}, {}, body_filter);
        if (!collector.HadHit()) {
            return result;
        }
        result.hit = true;
        // mFraction is along the sweep, so the distance is to the sphere CENTRE
        // at contact -- which is what the interface promises the caller.
        result.distance = std::max(0.0f, collector.mHit.mFraction * max_distance);
        result.position = to_glm(collector.mHit.mContactPointOn2);
        result.normal = -to_glm(collector.mHit.mPenetrationAxis.Normalized());
        JPH::BodyLockRead lock(system_->GetBodyLockInterface(), collector.mHit.mBodyID2);
        if (lock.Succeeded()) {
            result.user_data = lock.GetBody().GetUserData();
        }
        return result;
    }


    // Character impulses and contacts -----------------------------------------
    void character_add_impulse(CharacterHandle character, const glm::vec3& impulse_ns) override {
        if (auto it = characters_.find(character.id); it != characters_.end()) {
            it->second.push_velocity += impulse_ns / it->second.mass;
        }
    }

    glm::vec3 character_velocity(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() ? it->second.velocity : glm::vec3{0.0f};
    }

    float character_mass(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        return it != characters_.end() ? it->second.mass : 0.0f;
    }

    std::span<const CharacterContact> character_contacts(CharacterHandle character) const override {
        const auto it = characters_.find(character.id);
        if (it == characters_.end()) {
            return {};
        }
        return {it->second.contacts.data(), it->second.contacts.size()};
    }

    // Physical feet ---------------------------------------------------------------
    PhysicsBodyHandle create_foot_body(const FootBodyDesc& desc) override {
        const JPH::Vec3 half = to_jph(desc.half_extents);
        if (!system_ || desc.layer == 0 || desc.collides_with == 0 || desc.mass_kg <= 0.0f
            || half.GetX() <= 0.0f || half.GetY() <= 0.0f || half.GetZ() <= 0.0f) {
            return {};
        }
        // A sole is thin: the convex radius must fit inside the smallest half
        // extent, and a rounded sole would stand a hair above the drawn one.
        const float convex_radius = std::min(JPH::cDefaultConvexRadius, 0.5f * half.ReduceMin());
        JPH::ShapeRefC shape = new JPH::BoxShape(half, convex_radius);
        JPH::BodyCreationSettings settings(
            shape, JPH::RVec3(to_jph(desc.position)),
            JPH::Quat{desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w},
            JPH::EMotionType::Kinematic, object_layers::FOOT);
        settings.mUserData = desc.user_data;
        settings.mAllowDynamicOrKinematic = true; // Swing <-> Plant
        // TRANSLATION ONLY. The foot's orientation is the animation's, in
        // both modes: a planted foot on a slope keeps the angle it landed at
        // and slides as a block; it does not tumble like a crate.
        settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY
                                | JPH::EAllowedDOFs::TranslationZ;
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.mass_kg;
        const core::PhysicsSubstance& sole = core::substance(desc.substance);
        settings.mFriction = sole.friction;
        settings.mRestitution = sole.restitution;
        // NO AIR DRAG ON A FOOT: the ground's friction is its only brake, and
        // the slide law g*(sin - mu*cos) is measured against exactly that.
        settings.mLinearDamping = 0.0f;
        settings.mAngularDamping = 0.0f;
        settings.mAllowSleeping = true;
        const JPH::BodyID body_id = system_->GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::Activate);
        if (body_id.IsInvalid()) {
            return {};
        }
        const PhysicsBodyHandle handle{next_id_++};
        bodies_.emplace(handle.id, body_id);
        const JPH::uint32 key = body_id.GetIndexAndSequenceNumber();
        body_masks_[key] = desc.layer;
        body_collides_with_[key] = desc.collides_with;
        body_substances_[key] = desc.substance;
        Foot foot;
        foot.body = body_id;
        foot.shape = shape;
        foot.mode = FootMode::Swing;
        foot.target = BodyPose{desc.position, desc.rotation};
        foot.collides_with = desc.collides_with;
        foot.substance = desc.substance;
        feet_.emplace(handle.id, std::move(foot));
        return handle;
    }

    void set_foot_kinematic_pose(PhysicsBodyHandle foot, const BodyPose& pose) override {
        auto it = feet_.find(foot.id);
        if (it == feet_.end() || it->second.mode != FootMode::Swing) {
            return; // physics owns a planted foot
        }
        it->second.target = pose;
    }

    void set_foot_mode(PhysicsBodyHandle foot, FootMode mode) override {
        auto it = feet_.find(foot.id);
        if (it == feet_.end() || !system_ || it->second.mode == mode) {
            return;
        }
        auto& bi = system_->GetBodyInterface();
        if (mode == FootMode::Plant) {
            // Dynamic, AT REST: the swing's residual velocity is dropped. The
            // animation said "down"; a foot that lands is not thrown.
            bi.SetMotionType(it->second.body, JPH::EMotionType::Dynamic,
                             JPH::EActivation::Activate);
            bi.SetLinearAndAngularVelocity(it->second.body, JPH::Vec3::sZero(),
                                           JPH::Vec3::sZero());
        } else {
            bi.SetMotionType(it->second.body, JPH::EMotionType::Kinematic,
                             JPH::EActivation::Activate);
            bi.SetLinearAndAngularVelocity(it->second.body, JPH::Vec3::sZero(),
                                           JPH::Vec3::sZero());
            JPH::RVec3 position;
            JPH::Quat rotation;
            bi.GetPositionAndRotation(it->second.body, position, rotation);
            it->second.target.position = to_glm(JPH::Vec3(position));
            it->second.target.rotation = glm::quat{rotation.GetW(), rotation.GetX(),
                                                   rotation.GetY(), rotation.GetZ()};
        }
        it->second.mode = mode;
    }

    FootMode foot_mode(PhysicsBodyHandle foot) const override {
        const auto it = feet_.find(foot.id);
        return it != feet_.end() ? it->second.mode : FootMode::Swing;
    }

    FootContact foot_contact(PhysicsBodyHandle foot) const override {
        FootContact contact;
        const auto it = feet_.find(foot.id);
        if (it == feet_.end() || !system_) {
            return contact;
        }
        const Foot& record = it->second;
        auto& bi = system_->GetBodyInterface();
        JPH::RVec3 position;
        JPH::Quat rotation;
        bi.GetPositionAndRotation(record.body, position, rotation);

        // The foot's own shape, where the foot is, against everything it may
        // touch — plus FOOT_BODY_SKIN_M of air so a foot resting on the
        // surface (the solver keeps it at penetration zero ± residual) counts.
        JPH::CollideShapeSettings settings;
        settings.mMaxSeparationDistance = static_cast<float>(dfn::config::FOOT_BODY_SKIN_M);
        // A foot pushed through a one-sided terrain triangle still stands on
        // it rather than on nothing (the same choice sphere_cast made).
        settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        const FootQueryFilter body_filter{body_masks_, record.collides_with, record.body};
        system_->GetNarrowPhaseQuery().CollideShape(
            record.shape.GetPtr(), JPH::Vec3::sReplicate(1.0f),
            JPH::RMat44::sRotationTranslation(rotation, position), settings, JPH::RVec3::sZero(),
            collector, system_->GetDefaultBroadPhaseLayerFilter(object_layers::FOOT),
            system_->GetDefaultLayerFilter(object_layers::FOOT), body_filter);
        if (!collector.HadHit()) {
            return contact;
        }
        // The deepest contact is the ground; a brush against a crate's side
        // while standing on the floor does not become "standing on the crate".
        const JPH::CollideShapeResult* deepest = &collector.mHits[0];
        for (const JPH::CollideShapeResult& hit : collector.mHits) {
            if (hit.mPenetrationDepth > deepest->mPenetrationDepth) {
                deepest = &hit;
            }
        }
        contact.touching = true;
        contact.depth = deepest->mPenetrationDepth;
        contact.point = to_glm(JPH::Vec3(deepest->mContactPointOn2));
        const JPH::Vec3 axis = deepest->mPenetrationAxis;
        const JPH::Vec3 normal = axis.LengthSq() > 0.0f ? -axis.Normalized() : JPH::Vec3::sAxisY();
        contact.normal = to_glm(normal);
        const JPH::uint32 ground_key = deepest->mBodyID2.GetIndexAndSequenceNumber();
        if (const auto s = body_substances_.find(ground_key); s != body_substances_.end()) {
            contact.ground = s->second;
        }
        contact.ground_user_data = bi.GetUserData(deepest->mBodyID2);

        // COULOMB: the slope from the normal, the pair from the table, the
        // verdict from the comparison. tan(theta) <= sqrt(mu_sole * mu_ground).
        const float ny = std::clamp(normal.GetY(), -1.0f, 1.0f);
        const float horizontal = std::sqrt(std::max(0.0f, 1.0f - ny * ny));
        contact.slope_tan = ny > 1e-5f ? horizontal / ny : std::numeric_limits<float>::infinity();
        const float sole_friction = core::substance(record.substance).friction;
        const float ground_friction = core::substance(contact.ground).friction;
        contact.friction_pair = std::sqrt(std::max(0.0f, sole_friction * ground_friction));
        contact.holds = contact.slope_tan <= contact.friction_pair;

        // The solver's own answer: how fast the foot moves along the ground.
        const JPH::Vec3 foot_velocity = bi.GetLinearVelocity(record.body);
        const JPH::Vec3 ground_velocity =
            bi.GetMotionType(deepest->mBodyID2) == JPH::EMotionType::Static
                ? JPH::Vec3::sZero()
                : bi.GetPointVelocity(deepest->mBodyID2, deepest->mContactPointOn2);
        const JPH::Vec3 relative = foot_velocity - ground_velocity;
        const JPH::Vec3 tangent = relative - normal * relative.Dot(normal);
        contact.slip_velocity = to_glm(tangent);
        contact.slip_speed_mps = tangent.Length();
        return contact;
    }

    // Ragdoll -----------------------------------------------------------------
    RagdollHandle create_ragdoll(const RagdollDesc& desc) override {
        if (!system_ || desc.layer == 0 || desc.collides_with == 0 || desc.parts.empty()) {
            return {};
        }
        for (size_t i = 0; i < desc.parts.size(); ++i) {
            const RagdollPartDesc& part = desc.parts[i];
            const bool has_volume = part.radius > 0.0f
                                    || (part.half_extents.x > 0.0f && part.half_extents.y > 0.0f
                                        && part.half_extents.z > 0.0f);
            if (part.mass_kg <= 0.0f || !has_volume || part.parent >= static_cast<int32_t>(i)) {
                return {}; // parents before children; every part has volume and mass
            }
        }
        Ragdoll ragdoll;
        ragdoll.collides_with = desc.collides_with;
        // PARENT AND CHILD NEVER COLLIDE: a thigh overlaps its own hip at the
        // joint by construction, and a contact there would fight the joint.
        ragdoll.group = new JPH::GroupFilterTable(static_cast<JPH::uint>(desc.parts.size()));
        for (size_t i = 0; i < desc.parts.size(); ++i) {
            if (desc.parts[i].parent >= 0) {
                ragdoll.group->DisableCollision(static_cast<JPH::CollisionGroup::SubGroupID>(i),
                                                static_cast<JPH::CollisionGroup::SubGroupID>(
                                                    desc.parts[i].parent));
            }
        }
        const JPH::CollisionGroup::GroupID group_id = next_ragdoll_group_++;
        auto& bi = system_->GetBodyInterface();
        for (size_t i = 0; i < desc.parts.size(); ++i) {
            const RagdollPartDesc& part = desc.parts[i];
            JPH::ShapeRefC shape;
            if (part.radius > 0.0f) {
                shape = new JPH::SphereShape(part.radius);
            } else {
                const JPH::Vec3 half = to_jph(part.half_extents);
                shape = new JPH::BoxShape(half, std::min(JPH::cDefaultConvexRadius,
                                                         0.5f * half.ReduceMin()));
            }
            JPH::BodyCreationSettings settings(
                shape, JPH::RVec3(to_jph(part.pose.position)),
                JPH::Quat{part.pose.rotation.x, part.pose.rotation.y, part.pose.rotation.z,
                          part.pose.rotation.w},
                JPH::EMotionType::Dynamic, object_layers::RAGDOLL);
            settings.mUserData = part.user_data;
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = part.mass_kg;
            const core::PhysicsSubstance& what = core::substance(part.substance);
            settings.mFriction = what.friction;
            settings.mRestitution = what.restitution;
            settings.mCollisionGroup = JPH::CollisionGroup(
                ragdoll.group, group_id, static_cast<JPH::CollisionGroup::SubGroupID>(i));
            settings.mAllowSleeping = true;
            const JPH::BodyID body_id = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);
            if (body_id.IsInvalid()) {
                release_ragdoll(ragdoll);
                return {};
            }
            ragdoll.bodies.push_back(body_id);
            ragdoll.parent.push_back(part.parent);
            const JPH::uint32 key = body_id.GetIndexAndSequenceNumber();
            body_masks_[key] = desc.layer;
            body_collides_with_[key] = desc.collides_with;
            body_substances_[key] = part.substance;
        }
        // Joints: a swing-twist cone at the rig joint, twist along the child.
        for (size_t i = 0; i < desc.parts.size(); ++i) {
            const RagdollPartDesc& part = desc.parts[i];
            JPH::Ref<JPH::SwingTwistConstraint> joint;
            if (part.parent >= 0) {
                JPH::SwingTwistConstraintSettings st;
                st.mSpace = JPH::EConstraintSpace::WorldSpace;
                st.mPosition1 = st.mPosition2 = JPH::RVec3(to_jph(part.joint_position));
                const JPH::Vec3 twist = to_jph(part.twist_axis).NormalizedOr(JPH::Vec3::sAxisY());
                JPH::Vec3 plane = to_jph(part.plane_axis);
                plane -= twist * plane.Dot(twist);
                plane = plane.NormalizedOr(twist.GetNormalizedPerpendicular());
                st.mTwistAxis1 = st.mTwistAxis2 = twist;
                st.mPlaneAxis1 = st.mPlaneAxis2 = plane;
                st.mNormalHalfConeAngle = part.swing_limit_rad;
                st.mPlaneHalfConeAngle = part.swing_limit_rad;
                st.mTwistMinAngle = part.twist_min_rad;
                st.mTwistMaxAngle = part.twist_max_rad;
                const JPH::MotorSettings motor(static_cast<float>(dfn::config::RAGDOLL_MOTOR_HZ),
                                               static_cast<float>(dfn::config::RAGDOLL_MOTOR_DAMPING));
                st.mSwingMotorSettings = motor;
                st.mTwistMotorSettings = motor;
                const JPH::BodyID ids[2] = {ragdoll.bodies[static_cast<size_t>(part.parent)],
                                            ragdoll.bodies[i]};
                JPH::BodyLockMultiWrite lock(system_->GetBodyLockInterface(), ids, 2);
                JPH::Body* parent_body = lock.GetBody(0);
                JPH::Body* child_body = lock.GetBody(1);
                if (parent_body == nullptr || child_body == nullptr) {
                    release_ragdoll(ragdoll);
                    return {};
                }
                joint = static_cast<JPH::SwingTwistConstraint*>(st.Create(*parent_body, *child_body));
                system_->AddConstraint(joint);
            }
            ragdoll.joints.push_back(joint);
        }
        const RagdollHandle handle{next_id_++};
        ragdolls_.emplace(handle.id, std::move(ragdoll));
        return handle;
    }

    void destroy_ragdoll(RagdollHandle handle) override {
        auto it = ragdolls_.find(handle.id);
        if (it == ragdolls_.end()) {
            return;
        }
        release_ragdoll(it->second);
        ragdolls_.erase(it);
    }

    void set_ragdoll_pose(RagdollHandle handle, std::span<const BodyPose> parts) override {
        auto it = ragdolls_.find(handle.id);
        if (it == ragdolls_.end() || !system_) {
            return;
        }
        auto& bi = system_->GetBodyInterface();
        const size_t count = std::min(parts.size(), it->second.bodies.size());
        for (size_t i = 0; i < count; ++i) {
            bi.SetPositionAndRotation(
                it->second.bodies[i], JPH::RVec3(to_jph(parts[i].position)),
                JPH::Quat{parts[i].rotation.x, parts[i].rotation.y, parts[i].rotation.z,
                          parts[i].rotation.w},
                JPH::EActivation::Activate);
            bi.SetLinearAndAngularVelocity(it->second.bodies[i], JPH::Vec3::sZero(),
                                           JPH::Vec3::sZero());
        }
        // Yesterday's joint impulses belong to yesterday's pose.
        for (auto& joint : it->second.joints) {
            if (joint != nullptr) {
                joint->ResetWarmStart();
            }
        }
    }

    void ragdoll_pose(RagdollHandle handle, std::span<BodyPose> out) const override {
        const auto it = ragdolls_.find(handle.id);
        if (it == ragdolls_.end() || !system_) {
            return;
        }
        auto& bi = system_->GetBodyInterface();
        const size_t count = std::min(out.size(), it->second.bodies.size());
        for (size_t i = 0; i < count; ++i) {
            JPH::RVec3 position;
            JPH::Quat rotation;
            bi.GetPositionAndRotation(it->second.bodies[i], position, rotation);
            out[i].position = to_glm(JPH::Vec3(position));
            out[i].rotation = glm::quat{rotation.GetW(), rotation.GetX(), rotation.GetY(),
                                        rotation.GetZ()};
        }
    }

    void ragdoll_add_impulse(RagdollHandle handle, uint32_t part, const glm::vec3& impulse_ns,
                             const glm::vec3& at_world) override {
        const auto it = ragdolls_.find(handle.id);
        if (it == ragdolls_.end() || !system_ || part >= it->second.bodies.size()) {
            return;
        }
        system_->GetBodyInterface().AddImpulse(it->second.bodies[part], to_jph(impulse_ns),
                                               JPH::RVec3(to_jph(at_world)));
    }

    void ragdoll_drive_to_pose(RagdollHandle handle, std::span<const BodyPose> target,
                               float strength) override {
        auto it = ragdolls_.find(handle.id);
        if (it == ragdolls_.end() || !system_) {
            return;
        }
        Ragdoll& ragdoll = it->second;
        const float torque = std::clamp(strength, 0.0f, 1.0f)
                             * static_cast<float>(dfn::config::RAGDOLL_MOTOR_TORQUE_NM);
        for (size_t i = 0; i < ragdoll.joints.size(); ++i) {
            JPH::SwingTwistConstraint* joint = ragdoll.joints[i].GetPtr();
            if (joint == nullptr) {
                continue;
            }
            const auto parent = static_cast<size_t>(ragdoll.parent[i]);
            if (torque <= 0.0f || i >= target.size() || parent >= target.size()) {
                joint->SetSwingMotorState(JPH::EMotorState::Off);
                joint->SetTwistMotorState(JPH::EMotorState::Off);
                continue;
            }
            // Only the RELATIVE orientation parent->child is a joint target:
            // R_child = R_parent * q, so q = conj(R_parent) * R_child.
            const glm::quat q = glm::conjugate(target[parent].rotation) * target[i].rotation;
            joint->SetTargetOrientationBS(JPH::Quat{q.x, q.y, q.z, q.w});
            joint->GetSwingMotorSettings().SetTorqueLimit(torque);
            joint->GetTwistMotorSettings().SetTorqueLimit(torque);
            joint->SetSwingMotorState(JPH::EMotorState::Position);
            joint->SetTwistMotorState(JPH::EMotorState::Position);
        }
        for (const JPH::BodyID& id : ragdoll.bodies) {
            system_->GetBodyInterface().ActivateBody(id);
        }
    }

    bool ragdoll_asleep(RagdollHandle handle) const override {
        const auto it = ragdolls_.find(handle.id);
        if (it == ragdolls_.end() || !system_) {
            return true;
        }
        for (const JPH::BodyID& id : it->second.bodies) {
            if (system_->GetBodyInterface().IsActive(id)) {
                return false;
            }
        }
        return true;
    }

private:
    struct Character {
        JPH::Ref<JPH::CharacterVirtual> virtual_character;
        glm::vec3 pending{0.0f};
        float step_height = 0.0f;
        CollisionMask collides_with = COLLIDE_ALL;
        float radius = 0.0f; // kept so a resize can rebuild the capsule
        float height = 0.0f;
        float mass = 0.0f;
        glm::vec3 push_velocity{0.0f};        // what impulses left behind, m/s
        glm::vec3 velocity{0.0f};             // post-step, after collide-and-slide
        std::vector<CharacterContact> contacts; // last step's
    };

    struct Foot {
        JPH::BodyID body;
        JPH::ShapeRefC shape;                 // reused by the contact query
        FootMode mode = FootMode::Swing;
        BodyPose target;                      // where the swing is headed
        CollisionMask collides_with = COLLIDE_ALL;
        core::SubstanceId substance = core::SUBSTANCE_DEFAULT;
    };

    struct Ragdoll {
        std::vector<JPH::BodyID> bodies;
        std::vector<int32_t> parent;
        std::vector<JPH::Ref<JPH::SwingTwistConstraint>> joints; // null on the root
        JPH::Ref<JPH::GroupFilterTable> group;
        CollisionMask collides_with = COLLIDE_ALL;
    };

    // The foot's contact query must skip the foot itself and honour its mask.
    class FootQueryFilter final : public JPH::BodyFilter {
    public:
        FootQueryFilter(const std::unordered_map<JPH::uint32, CollisionMask>& masks,
                        CollisionMask collides_with, JPH::BodyID self)
            : masks_(masks), collides_with_(collides_with), self_(self) {}
        bool ShouldCollide(const JPH::BodyID& body_id) const override {
            if (body_id == self_) {
                return false;
            }
            const auto it = masks_.find(body_id.GetIndexAndSequenceNumber());
            const CollisionMask mask = it != masks_.end() ? it->second : COLLIDE_ALL;
            return (mask & collides_with_) != 0;
        }

    private:
        const std::unordered_map<JPH::uint32, CollisionMask>& masks_;
        CollisionMask collides_with_;
        JPH::BodyID self_;
    };

    void release_ragdoll(Ragdoll& ragdoll) {
        if (!system_) {
            return;
        }
        for (auto& joint : ragdoll.joints) {
            if (joint != nullptr) {
                system_->RemoveConstraint(joint);
            }
        }
        ragdoll.joints.clear();
        auto& bi = system_->GetBodyInterface();
        for (const JPH::BodyID& id : ragdoll.bodies) {
            const JPH::uint32 key = id.GetIndexAndSequenceNumber();
            body_masks_.erase(key);
            body_collides_with_.erase(key);
            body_substances_.erase(key);
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
        ragdoll.bodies.clear();
    }

    // Builds the bottom-anchored capsule shape used by both create_character
    // and set_character_height. Null when the dimensions cannot form a capsule.
    [[nodiscard]] static JPH::ShapeRefC make_capsule(float radius, float height) {
        const float cylinder_half = 0.5f * height - radius;
        if (radius <= 0.0f || cylinder_half <= 0.0f) {
            return {};
        }
        // Offset so the character position is the capsule BOTTOM point.
        auto result = JPH::RotatedTranslatedShapeSettings(
                          {0.0f, 0.5f * height, 0.0f}, JPH::Quat::sIdentity(),
                          new JPH::CapsuleShape(cylinder_half, radius))
                          .Create();
        if (result.HasError()) {
            return {};
        }
        return result.Get();
    }

    PhysicsBodyHandle add_static_body(const JPH::ShapeRefC& shape, JPH::RVec3Arg position,
                                      JPH::QuatArg rotation, CollisionMask mask,
                                      uint64_t user_data,
                                      core::SubstanceId substance) {
        JPH::BodyCreationSettings settings(shape, position, rotation,
                                           JPH::EMotionType::Static,
                                           object_layers::STATIC);
        settings.mUserData = user_data;
        // ПОЛ — ПОЛОВИНА ВСЯКОЙ ПАРЫ ТРЕНИЯ. До этой волны статичное тело
        // молча несло умолчание Jolt (0.2), и это ровно то, что даёт
        // SUBSTANCE_DEFAULT: мир, в котором никто не назвал вещества, стоит
        // на прежних числах бит в бит.
        const core::PhysicsSubstance& what = core::substance(substance);
        settings.mFriction = what.friction;
        settings.mRestitution = what.restitution;
        const JPH::BodyID body_id = system_->GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);
        if (body_id.IsInvalid()) {
            return {};
        }
        const PhysicsBodyHandle handle{next_id_++};
        bodies_.emplace(handle.id, body_id);
        body_masks_[body_id.GetIndexAndSequenceNumber()] = mask;
        body_substances_[body_id.GetIndexAndSequenceNumber()] = substance;
        broad_phase_dirty_ = true;
        return handle;
    }

    BroadPhaseLayerMap bp_layer_map_;
    ObjectVsBroadPhaseFilter object_vs_bp_filter_;
    ObjectPairFilter pair_filter_;

    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator_;
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_;
    std::unique_ptr<JPH::PhysicsSystem> system_;

    uint32_t next_id_ = 1; // 0 is the invalid handle
    std::unordered_map<uint32_t, JPH::BodyID> bodies_;
    std::unordered_map<uint32_t, Character> characters_;
    std::unordered_map<uint32_t, Foot> feet_;       // by the same id as bodies_
    std::unordered_map<uint32_t, Ragdoll> ragdolls_;
    JPH::CollisionGroup::GroupID next_ragdoll_group_ = 1;
    // BodyID (index+sequence) -> engine mask, for query/contact filtering.
    std::unordered_map<JPH::uint32, CollisionMask> body_masks_;
    // BodyID -> what that body may touch (feet, ragdoll parts only).
    std::unordered_map<JPH::uint32, CollisionMask> body_collides_with_;
    // BodyID -> substance, so a foot can name what it stands on.
    std::unordered_map<JPH::uint32, core::SubstanceId> body_substances_;
    MaskContactListener mask_contact_listener_{body_masks_, body_collides_with_};
    CharacterPushListener push_listener_;
    bool broad_phase_dirty_ = false;
};

} // namespace

std::unique_ptr<IPhysics> create_jolt_physics() {
    return std::make_unique<JoltPhysics>();
}

} // namespace dfn::platform
