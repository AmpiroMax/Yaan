/*
Module: tests
File: tests/sim/ViewModelTests.cpp

Responsibility:
- The two claims the hand and the inventory screen rest on: the hand is
  attached to the VIEW (so it follows pitch, not just yaw), and the inventory
  selection follows the ITEM across a refresh, not the row index.

Key items:
- View-model anchor placement, pitch tracking, item mesh and carried light.
- Inventory selection stability, merging, and ordering.

Dependencies:
- Uses: doctest, dfn_gameplay, dfn_core, generated constants.
- Used by: ctest (sim_view_model).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Each case names the implementation it exists to reject.
*/

#include <doctest/doctest.h>

#include <cmath>

#include <glm/geometric.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/PlayerActions.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/Inventory.h"
#include "engine/gameplay/sources/InventoryScreen.h"
#include "engine/gameplay/sources/Item.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/ViewModel.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

namespace {

namespace config = dfn::config;
namespace gameplay = dfn::gameplay;
namespace components = dfn::components;
namespace serialization = dfn::serialization;
using dfn::ecs::EntityId;
using dfn::ecs::World;

constexpr float RIGHT = static_cast<float>(config::TORCH_HAND_OFFSET_RIGHT);
constexpr float BELOW = static_cast<float>(config::TORCH_HAND_OFFSET_BELOW_EYE);
constexpr float FORWARD = static_cast<float>(config::HAND_OFFSET_FORWARD);

// A carrier with an eye pose and a view model.
EntityId make_carrier(World& world, float yaw = 0.0f, float pitch = 0.0f) {
    const EntityId id = world.spawn();
    world.add(id, components::CameraPose{.position = {10.0f, 5.0f, -3.0f},
                                         .yaw = yaw,
                                         .pitch = pitch});
    world.add(id, gameplay::HeldItem{});
    gameplay::spawn_view_model(world, id);
    return id;
}

// The item half of the view model (the half that holds things).
const components::Transform* item_transform(World& world) {
    for (auto [id, part, xf] : world.view<gameplay::ViewModelPart, components::Transform>()) {
        (void)id;
        if (part.is_item) {
            return &xf;
        }
    }
    return nullptr;
}

TEST_CASE("view model: the hand sits right, below and in front of the eye") {
    World world;
    const EntityId carrier = make_carrier(world);
    gameplay::update_view_model(world);

    const auto* eye = world.get<components::CameraPose>(carrier);
    const auto* hand = item_transform(world);
    REQUIRE(eye != nullptr);
    REQUIRE(hand != nullptr);

    // At yaw 0 the view faces -Z, right is +X, up is +Y.
    CHECK(hand->position.x == doctest::Approx(eye->position.x + RIGHT).epsilon(1e-3));
    CHECK(hand->position.y == doctest::Approx(eye->position.y - BELOW).epsilon(1e-3));
    CHECK(hand->position.z == doctest::Approx(eye->position.z - FORWARD).epsilon(1e-3));
}

TEST_CASE("view model: the hand follows PITCH, not only yaw") {
    World world;
    const EntityId level = make_carrier(world, 0.0f, 0.0f);
    gameplay::update_view_model(world);
    const glm::vec3 level_position = item_transform(world)->position;
    const auto* eye = world.get<components::CameraPose>(level);
    const float eye_y = eye->position.y;

    World pitched_world;
    (void)make_carrier(pitched_world, 0.0f, 1.0f); // look sharply up
    gameplay::update_view_model(pitched_world);
    const glm::vec3 pitched_position = item_transform(pitched_world)->position;

    // Looking up must LIFT the hand: the forward offset now points upward.
    // CONTROL: a body-attached hand (yaw only, the way the carried light is
    // attached) gives the identical height for both poses, and fails here.
    CHECK(pitched_position.y > level_position.y + 0.1f);
    CHECK(pitched_position.y > eye_y - BELOW);
}

TEST_CASE("view model: the item slot shows what is held, and carries its light") {
    World world;
    const EntityId carrier = make_carrier(world);

    gameplay::ItemDatabase items;
    gameplay::ItemDef torch;
    torch.id = {serialization::fnv1a64("item.tool.torch")};
    torch.display_name_key = "item.tool.torch.name";
    torch.light_source = true;
    torch.mesh_id = 77;
    items.add(torch);
    world.add_resource(std::move(items));

    // Empty-handed: nothing to draw and no light anywhere.
    gameplay::update_view_model(world);
    for (auto [id, part, mesh] :
         world.view<gameplay::ViewModelPart, components::RenderMesh>()) {
        (void)id;
        if (part.is_item) {
            CHECK(mesh.mesh_asset == 0);
        }
    }

    // Holding it lit: the slot shows the torch mesh and gains an active light.
    auto* held = world.get<gameplay::HeldItem>(carrier);
    REQUIRE(held != nullptr);
    held->item = torch.id;
    held->lit = true;
    gameplay::update_view_model(world);

    bool saw_mesh = false;
    bool saw_light = false;
    for (auto [id, part] : world.view<gameplay::ViewModelPart>()) {
        if (!part.is_item) {
            continue;
        }
        const auto* mesh = world.get<components::RenderMesh>(id);
        REQUIRE(mesh != nullptr);
        saw_mesh = mesh->mesh_asset == torch.mesh_id;
        const auto* light = world.get<components::CarriedLight>(id);
        saw_light = light != nullptr && light->active;
        // The flame belongs at the HEAD of the torch. CONTROL: a zero offset
        // (the obvious implementation, and what this shipped as at first)
        // burns at the wrist, and render's shadows then look detached from the
        // stick they are supposedly cast by.
        if (light != nullptr) {
            CHECK(light->offset.y ==
                  doctest::Approx(static_cast<float>(config::TORCH_FLAME_ABOVE_GRIP)));
            CHECK(light->offset.x == doctest::Approx(0.0f));
            CHECK(light->offset.z == doctest::Approx(0.0f));
        }
    }
    CHECK(saw_mesh);
    CHECK(saw_light);

    // Dousing it puts the light out without removing the wood.
    held->lit = false;
    gameplay::update_view_model(world);
    for (auto [id, part] : world.view<gameplay::ViewModelPart>()) {
        if (!part.is_item) {
            continue;
        }
        const auto* light = world.get<components::CarriedLight>(id);
        CHECK((light == nullptr || !light->active));
        CHECK(world.get<components::RenderMesh>(id)->mesh_asset == torch.mesh_id);
    }
}

TEST_CASE("view model: spawning twice does not build a second pair of hands") {
    World world;
    const EntityId carrier = make_carrier(world);
    gameplay::spawn_view_model(world, carrier);
    gameplay::spawn_view_model(world, carrier);

    int parts = 0;
    for (auto [id, part] : world.view<gameplay::ViewModelPart>()) {
        (void)id;
        (void)part;
        ++parts;
    }
    CHECK(parts == 2); // the hand and the item slot, once
}

// --- Inventory screen --------------------------------------------------------

EntityId make_owner_with_items(World& world, gameplay::ItemDatabase& items,
                               const std::vector<const char*>& ids) {
    const EntityId owner = world.spawn();
    gameplay::Inventory inventory;
    for (const char* id : ids) {
        gameplay::ItemDef def;
        def.id = {serialization::fnv1a64(id)};
        def.display_name_key = std::string(id) + ".name";
        items.add(def);
        inventory.stacks.push_back(gameplay::ItemStack{def.id, 1});
    }
    world.add(owner, std::move(inventory));
    return owner;
}

TEST_CASE("inventory screen: the selection follows the ITEM, not the row") {
    World world;
    gameplay::ItemDatabase items;
    // Chosen so the new item sorts BEFORE the selected one and therefore shifts
    // every later row down by one — which is what an index-based selection
    // would silently follow.
    const EntityId owner =
        make_owner_with_items(world, items, {"item.a.sword", "item.b.shield"});
    world.add_resource(std::move(items));

    gameplay::refresh_inventory_screen(world, owner);
    auto& screen = world.resource<gameplay::InventoryScreen>();
    REQUIRE(screen.entries.size() == 2);

    // Select whichever item is currently in the second row.
    gameplay::move_selection(screen, 1);
    const auto* before = gameplay::selected_entry(screen);
    REQUIRE(before != nullptr);
    const uint64_t chosen = before->item.value;
    const uint32_t row_before = screen.selected;

    // Pick something up. Its id is forced below both existing ids so that it
    // must land in row 0 and push the selected item down a row.
    auto* inventory = world.get<gameplay::Inventory>(owner);
    REQUIRE(inventory != nullptr);
    uint64_t low = chosen;
    for (const auto& e : screen.entries) {
        low = std::min(low, e.item.value);
    }
    inventory->stacks.push_back(gameplay::ItemStack{dfn::gameplay::ItemId{low - 1}, 1});

    gameplay::refresh_inventory_screen(world, owner);
    const auto* after = gameplay::selected_entry(screen);
    REQUIRE(after != nullptr);

    // CONTROL: an index-based selection keeps the same row and therefore ends
    // up pointing at a DIFFERENT item. The row must have moved, and the item
    // must not have.
    CHECK(screen.entries.size() == 3);
    CHECK(screen.selected == row_before + 1);
    CHECK(after->item.value == chosen);
}

TEST_CASE("inventory screen: split stacks become one row, and rows are ordered") {
    World world;
    gameplay::ItemDatabase items;
    gameplay::ItemDef arrow;
    arrow.id = {serialization::fnv1a64("item.ammo.arrow")};
    arrow.display_name_key = "item.ammo.arrow.name";
    arrow.max_stack = 20;
    items.add(arrow);
    world.add_resource(std::move(items));

    const EntityId owner = world.spawn();
    gameplay::Inventory inventory;
    inventory.stacks.push_back(gameplay::ItemStack{arrow.id, 20});
    inventory.stacks.push_back(gameplay::ItemStack{arrow.id, 7});
    world.add(owner, std::move(inventory));

    gameplay::refresh_inventory_screen(world, owner);
    const auto& screen = world.resource<gameplay::InventoryScreen>();

    // CONTROL: a row-per-stack list shows "Arrow 20" and "Arrow 7" as two
    // separate lines, which is the icon-grid behaviour the user rejected.
    REQUIRE(screen.entries.size() == 1);
    CHECK(screen.entries[0].count == 27);
    CHECK(screen.entries[0].name_key ==
          serialization::fnv1a64(std::string("item.ammo.arrow.name")));
}

TEST_CASE("inventory screen: selection clamps and the preview pitch is limited") {
    World world;
    gameplay::ItemDatabase items;
    const EntityId owner = make_owner_with_items(world, items, {"item.a.one", "item.b.two"});
    world.add_resource(std::move(items));
    gameplay::refresh_inventory_screen(world, owner);
    auto& screen = world.resource<gameplay::InventoryScreen>();

    gameplay::move_selection(screen, -5);
    CHECK(screen.selected == 0); // no wraparound onto the last row
    gameplay::move_selection(screen, 99);
    CHECK(screen.selected == screen.entries.size() - 1);

    // Yaw is free (turning an item all the way round is the point); pitch is not.
    gameplay::rotate_preview(screen, 100.0f, 100.0f);
    CHECK(screen.preview_yaw == doctest::Approx(100.0f));
    CHECK(screen.preview_pitch ==
          doctest::Approx(static_cast<float>(config::CAMERA_PITCH_LIMIT)));
}

TEST_CASE("inventory screen: the mouse turns the item only while it is open") {
    World world;
    gameplay::ItemDatabase items;
    const EntityId owner = make_owner_with_items(world, items, {"item.a.one"});
    world.add_resource(std::move(items));
    world.add(owner, gameplay::PlayerState{});
    world.add(owner, components::Transform{});
    world.add(owner, components::PreviousTransform{});
    world.add(owner, components::CameraPose{});
    world.add(owner, components::PreviousCameraPose{});
    gameplay::refresh_inventory_screen(world, owner);

    auto physics = dfn::platform::create_null_physics();
    REQUIRE(physics->init());
    dfn::platform::CharacterDesc desc;
    desc.radius = static_cast<float>(config::PLAYER_CAPSULE_RADIUS);
    desc.height = static_cast<float>(config::PLAYER_CAPSULE_HEIGHT);
    desc.layer = dfn::physics::LAYER_CHARACTER;
    desc.collides_with = dfn::physics::LAYER_STATIC;
    world.get<gameplay::PlayerState>(owner)->character = physics->create_character(desc);

    auto& screen = world.resource<gameplay::InventoryScreen>();

    dfn::events::EventBus bus;

    // CLOSED: the mouse turns the HEAD and the preview does not move.
    screen.open = false;
    world.get<gameplay::PlayerState>(owner)->pending_look = {40.0f, 0.0f};
    gameplay::player_actions_step(world, bus, *physics);
    gameplay::player_pre_step(world, *physics);
    CHECK(screen.preview_yaw == doctest::Approx(0.0f));
    CHECK(world.get<gameplay::PlayerState>(owner)->yaw != doctest::Approx(0.0f));

    // OPEN: the same motion turns the ITEM and leaves the head alone. Without
    // this diversion the preview is not rotatable at all, since the mouse is
    // the only rotation input there is.
    //
    // It happens in player_actions_step and NOT in the movement path on
    // purpose: the world pauses behind the screen, so movement does not run,
    // and a preview that turned there would freeze exactly when it is needed.
    screen.open = true;
    const float yaw_before = world.get<gameplay::PlayerState>(owner)->yaw;
    world.get<gameplay::PlayerState>(owner)->pending_look = {40.0f, 0.0f};
    gameplay::player_actions_step(world, bus, *physics);
    CHECK(screen.preview_yaw != doctest::Approx(0.0f));
    CHECK(world.get<gameplay::PlayerState>(owner)->yaw == doctest::Approx(yaw_before));
}

TEST_CASE("drop: the item leaves the bag and appears in the world at the hand") {
    World world;
    gameplay::ItemDatabase items;
    gameplay::ItemDef stone;
    stone.id = {serialization::fnv1a64("item.junk.stone")};
    stone.display_name_key = "item.junk.stone.name";
    items.add(stone);
    world.add_resource(std::move(items));

    auto physics = dfn::platform::create_null_physics();
    REQUIRE(physics->init());

    const EntityId owner = world.spawn();
    gameplay::Inventory inv;
    inv.stacks.push_back(gameplay::ItemStack{stone.id, 2});
    world.add(owner, std::move(inv));
    world.add(owner, gameplay::HeldItem{});
    world.add(owner, components::CameraPose{.position = {5.0f, 2.0f, 5.0f}});
    world.add(owner, gameplay::PlayerState{});
    gameplay::refresh_inventory_screen(world, owner);

    auto& screen = world.resource<gameplay::InventoryScreen>();
    screen.open = true;
    dfn::events::EventBus bus;

    int dropped_events = 0;
    bus.subscribe<gameplay::ItemDropped>(
        [&](const gameplay::ItemDropped& e) {
            ++dropped_events;
            CHECK(e.item.value == stone.id.value);
            CHECK(e.count == 1);
        });

    world.get<gameplay::PlayerState>(owner)->drop_pressed = true;
    gameplay::player_actions_step(world, bus, *physics);
    bus.pump();

    // One left the bag, not the whole stack.
    CHECK(gameplay::count_item(*world.get<gameplay::Inventory>(owner), stone.id) == 1);
    CHECK(dropped_events == 1);

    // ... and it is now a real loose item, at the hand rather than at the feet.
    int pickups = 0;
    glm::vec3 where{0.0f};
    for (auto [id, pickup, xf] :
         world.view<gameplay::Pickup, components::Transform>()) {
        (void)id;
        CHECK(pickup.item.value == stone.id.value);
        where = xf.position;
        ++pickups;
    }
    REQUIRE(pickups == 1);
    const glm::vec3 hand = gameplay::hand_anchor_position(
        *world.get<components::CameraPose>(owner));
    CHECK(glm::length(where - hand) == doctest::Approx(0.0f).epsilon(1e-3));

    // CONTROL: with the screen SHUT the same latch drops nothing — the key is
    // an inventory action, not a world action, and Q while walking must not
    // scatter your belongings behind you.
    world.resource<gameplay::InventoryScreen>().open = false;
    world.get<gameplay::PlayerState>(owner)->drop_pressed = true;
    gameplay::player_actions_step(world, bus, *physics);
    bus.pump();
    CHECK(gameplay::count_item(*world.get<gameplay::Inventory>(owner), stone.id) == 1);
    CHECK(dropped_events == 1);
}

TEST_CASE("drop: a quest item is refused, loudly") {
    World world;
    gameplay::ItemDatabase items;
    gameplay::ItemDef crown;
    crown.id = {serialization::fnv1a64("item.quest.crown_grant")};
    crown.display_name_key = "item.quest.crown_grant.name";
    crown.quest_item = true; // story's integrity rule
    items.add(crown);
    world.add_resource(std::move(items));

    auto physics = dfn::platform::create_null_physics();
    REQUIRE(physics->init());

    const EntityId owner = world.spawn();
    gameplay::Inventory inv;
    inv.stacks.push_back(gameplay::ItemStack{crown.id, 1});
    world.add(owner, std::move(inv));
    world.add(owner, gameplay::HeldItem{});
    world.add(owner, components::CameraPose{});
    world.add(owner, gameplay::PlayerState{});
    gameplay::refresh_inventory_screen(world, owner);
    world.resource<gameplay::InventoryScreen>().open = true;

    dfn::events::EventBus bus;
    int refusals = 0;
    int drops = 0;
    bus.subscribe<gameplay::InteractionFailed>([&](const gameplay::InteractionFailed& e) {
        if (e.reason == gameplay::InteractionFailure::Undroppable) {
            ++refusals;
        }
    });
    bus.subscribe<gameplay::ItemDropped>([&](const gameplay::ItemDropped&) { ++drops; });

    world.get<gameplay::PlayerState>(owner)->drop_pressed = true;
    gameplay::player_actions_step(world, bus, *physics);
    bus.pump();

    // Still carried, nothing spawned, and the refusal was ANNOUNCED rather than
    // swallowed: a silent refusal is indistinguishable from a broken key.
    CHECK(gameplay::count_item(*world.get<gameplay::Inventory>(owner), crown.id) == 1);
    CHECK(drops == 0);
    CHECK(refusals == 1);

    // CONTROL: the identical flow on a NON-quest item does drop. Without this,
    // "nothing dropped" would also pass for a drop that never works at all.
    gameplay::ItemDef rag;
    rag.id = {serialization::fnv1a64("item.junk.rag")};
    rag.display_name_key = "item.junk.rag.name";
    world.resource<gameplay::ItemDatabase>().add(rag);
    world.get<gameplay::Inventory>(owner)->stacks.push_back(
        gameplay::ItemStack{rag.id, 1});
    gameplay::refresh_inventory_screen(world, owner);
    auto& screen = world.resource<gameplay::InventoryScreen>();
    for (uint32_t i = 0; i < screen.entries.size(); ++i) {
        if (screen.entries[i].item.value == rag.id.value) {
            screen.selected = i;
        }
    }
    world.get<gameplay::PlayerState>(owner)->drop_pressed = true;
    gameplay::player_actions_step(world, bus, *physics);
    bus.pump();
    CHECK(drops == 1);
    CHECK(gameplay::count_item(*world.get<gameplay::Inventory>(owner), rag.id) == 0);
}

TEST_CASE("drop: letting go of the last one empties the hand") {
    World world;
    gameplay::ItemDatabase items;
    gameplay::ItemDef torch;
    torch.id = {serialization::fnv1a64("item.tool.torch")};
    torch.display_name_key = "item.tool.torch.name";
    torch.light_source = true;
    items.add(torch);
    world.add_resource(std::move(items));

    auto physics = dfn::platform::create_null_physics();
    REQUIRE(physics->init());

    const EntityId owner = world.spawn();
    gameplay::Inventory inv;
    inv.stacks.push_back(gameplay::ItemStack{torch.id, 1});
    world.add(owner, std::move(inv));
    world.add(owner, gameplay::HeldItem{});
    world.add(owner, components::CameraPose{});
    world.add(owner, gameplay::PlayerState{});

    dfn::events::EventBus bus;
    REQUIRE(gameplay::hold_item(world, bus, owner, torch.id));
    REQUIRE(gameplay::toggle_lit(world, bus, owner));
    REQUIRE(world.get<gameplay::HeldItem>(owner)->lit);

    gameplay::refresh_inventory_screen(world, owner);
    world.resource<gameplay::InventoryScreen>().open = true;
    world.get<gameplay::PlayerState>(owner)->drop_pressed = true;
    gameplay::player_actions_step(world, bus, *physics);
    bus.pump();

    // The hand cannot show something the bag does not have. A lit torch left
    // burning in an empty hand is the bug this exists to reject.
    const auto* held = world.get<gameplay::HeldItem>(owner);
    REQUIRE(held != nullptr);
    CHECK_FALSE(held->item.valid());
    CHECK_FALSE(held->lit);
}

} // namespace
