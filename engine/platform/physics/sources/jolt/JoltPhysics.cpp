/*
Created: 09:08:2026 - 00:45:08
Last updated: 21:08:2026 - 14:35:00
Module: engine/platform/physics
File: engine/platform/physics/sources/jolt/JoltPhysics.cpp

Responsibility:
- Jolt backend of IPhysics: fixed-step world, static terrain (triangle mesh
  from heightmap samples) and boxes, kinematic capsule characters via
  CharacterVirtual (collide-and-slide, stair stepping, slope limit), masked
  raycasts. The ONLY translation unit that includes Jolt (Rule 1).

Key items:
- JoltPhysics (file-local): the IPhysics implementation.
- Object layers: STATIC / CHARACTER / CHARACTER_GHOST (ghost = the character's
  raycastable inner body; collides with nothing).
- MaskBodyFilter: filters queries/contacts by the engine's opaque CollisionMask
  stored per body — the backend never interprets mask bits (IPhysics.h note).

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
/*
UPD:
- 09:08:2026 - 00:45:08: Stage 2 — initial Jolt backend (terrain mesh, boxes,
                         CharacterVirtual with inner body, masked raycast).
- 09:08:2026 - 15:08:24: Reject layer == 0 (and character collides_with == 0)
                         with an invalid handle — a body no mask can select is
                         never intentional; a silently accepted layer-0 terrain
                         let the player fall through the world (fixed in app
                         37f1e1c; this is the backend-side guard).
- 09:08:2026 - 16:51:22: create_terrain_mesh: static MeshShape from the
                         extracted voxel surface — the terrain path that
                         supports tunnels and overhangs. Degenerate and
                         out-of-range triangles are dropped (Jolt rejects a
                         whole mesh over one bad triangle).
- 09:08:2026 - 22:18:17: set_character_height/character_height: the
                         capsule is rebuilt via a shared make_capsule()
                         and stays anchored at its BOTTOM point, so
                         crouching moves the head and not the feet.
- 13:08:2026 - 18:20:00: set_body_transform: a static body may be MOVED, for a
                         door leaf that carries its own ray target. Static, not
                         kinematic, on purpose — nothing reads a door leaf's
                         velocity, and giving it one would put it in the
                         solver's integration for nothing.
- 18:08:2026 - 12:06:07: Comment only, no code: chunks are 257x257 after
                         HEIGHTMAP_RESOLUTION moved 129 -> 257 in NUMBERS.
                         The MeshShape reasoning is UNCHANGED and still the
                         reason — 2^n+1 is odd by construction, so a bigger
                         chunk does not bring the sample count any closer to
                         Jolt's block size. Stamped so nobody reads the stale
                         129 as "the shape choice was tied to that number"
                         and re-opens a settled decision on a wrong premise.
- 21:08:2026 - 14:35:00: Отладочная дверь DFN_CHAR_TRACE=1 - раз в полсекунды
  вход/исход CharacterVirtual (позиция, GroundState, нормаль опоры, pending,
  скорость) и разовая перепись всех тел с габаритами. Ею найден невидимый
  куб кроны Гилдергрина, о который бился бот Вайтрана: расхождение живого
  конвейера с голой репродукцией можно было поймать только цифрами изнутри.
*/

#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"

#include "engine/core/config/sources/Constants.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <glm/geometric.hpp>
#include <thread>
#include <unordered_map>

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
constexpr JPH::uint COUNT = 3;
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
        // Characters query static geometry; ghosts collide with nothing.
        if (layer == object_layers::CHARACTER_GHOST) {
            return false;
        }
        if (layer == object_layers::CHARACTER) {
            return bp == broad_phase_layers::STATIC;
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
        return pair_is(object_layers::CHARACTER, object_layers::STATIC);
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
        return true;
    }

    void shutdown() override {
        for (auto& [id, character] : characters_) {
            character.virtual_character = nullptr;
        }
        characters_.clear();
        bodies_.clear();
        body_masks_.clear();
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

        // Characters first: collide-and-slide against the current static world.
        const JPH::Vec3 gravity{0.0f, -static_cast<float>(dfn::config::GRAVITY), 0.0f};
        for (auto& [id, character] : characters_) {
            JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
            update_settings.mWalkStairsStepUp = {0.0f, character.step_height, 0.0f};
            update_settings.mStickToFloorStepDown = {0.0f, -character.step_height, 0.0f};

            const glm::vec3 pending = character.pending;
            character.virtual_character->SetLinearVelocity(to_jph(character.pending / dt));
            const MaskBodyFilter body_filter{body_masks_, character.collides_with};
            character.virtual_character->ExtendedUpdate(
                dt, gravity, update_settings,
                system_->GetDefaultBroadPhaseLayerFilter(object_layers::CHARACTER),
                system_->GetDefaultLayerFilter(object_layers::CHARACTER), body_filter,
                {}, *temp_allocator_);
            character.pending = glm::vec3{0.0f};
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
                    std::fprintf(stderr,
                                 "[char] pos=(%.2f %.2f %.2f) ground=%d "
                                 "n=(%.2f %.2f %.2f) pend=(%.3f %.3f %.3f) "
                                 "v=(%.2f %.2f %.2f)\n",
                                 static_cast<double>(p.GetX()), static_cast<double>(p.GetY()),
                                 static_cast<double>(p.GetZ()),
                                 static_cast<int>(character.virtual_character->GetGroundState()),
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
                               JPH::Quat::sIdentity(), desc.layer, desc.user_data);
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
                               JPH::Quat::sIdentity(), desc.layer, desc.user_data);
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
                               rotation, desc.layer, desc.user_data);
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

    void destroy_body(PhysicsBodyHandle body) override {
        const auto it = bodies_.find(body.id);
        if (it == bodies_.end()) {
            return;
        }
        auto& body_interface = system_->GetBodyInterface();
        body_masks_.erase(it->second.GetIndexAndSequenceNumber());
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

        Character character;
        character.virtual_character = new JPH::CharacterVirtual(
            &settings, JPH::RVec3(to_jph(desc.position)), JPH::Quat::sIdentity(),
            desc.user_data, system_.get());
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

private:
    struct Character {
        JPH::Ref<JPH::CharacterVirtual> virtual_character;
        glm::vec3 pending{0.0f};
        float step_height = 0.0f;
        CollisionMask collides_with = COLLIDE_ALL;
        float radius = 0.0f; // kept so a resize can rebuild the capsule
        float height = 0.0f;
    };

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
                                      uint64_t user_data) {
        JPH::BodyCreationSettings settings(shape, position, rotation,
                                           JPH::EMotionType::Static,
                                           object_layers::STATIC);
        settings.mUserData = user_data;
        const JPH::BodyID body_id = system_->GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);
        if (body_id.IsInvalid()) {
            return {};
        }
        const PhysicsBodyHandle handle{next_id_++};
        bodies_.emplace(handle.id, body_id);
        body_masks_[body_id.GetIndexAndSequenceNumber()] = mask;
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
    // BodyID (index+sequence) -> engine mask, for query/contact filtering.
    std::unordered_map<JPH::uint32, CollisionMask> body_masks_;
    bool broad_phase_dirty_ = false;
};

} // namespace

std::unique_ptr<IPhysics> create_jolt_physics() {
    return std::make_unique<JoltPhysics>();
}

} // namespace dfn::platform
