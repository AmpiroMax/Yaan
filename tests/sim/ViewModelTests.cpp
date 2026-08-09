/*
Created: 09:08:2026 - 22:34:38
Last updated: 09:08:2026 - 22:34:38
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
/*
UPD:
- 09:08:2026 - 22:34:38: Created with the visible hands and inventory screen.
*/

#include <doctest/doctest.h>

#include <cmath>

#include <glm/geometric.hpp>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/Inventory.h"
#include "engine/gameplay/sources/InventoryScreen.h"
#include "engine/gameplay/sources/Item.h"
#include "engine/gameplay/sources/ViewModel.h"

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
        // The light rides the item itself, so it needs no offset of its own —
        // the anchor already put it at the grip. A non-zero offset here would
        // be the flame drifting off the stick again.
        if (light != nullptr) {
            CHECK(glm::length(light->offset) == doctest::Approx(0.0f));
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

} // namespace
