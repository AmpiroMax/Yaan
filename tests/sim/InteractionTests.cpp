/*
Created: 09:08:2026 - 18:56:32
Last updated: 13:08:2026 - 18:25:00
Module: tests
File: tests/sim/InteractionTests.cpp

Responsibility:
- The four verbs end to end on the null physics backend: LOOK (crosshair
  targeting writes HoverTarget), TAKE (into the inventory), OPEN/CLOSE (the
  door state machine, including the locked refusal), USE (content-declared
  action, repeatable and one-shot). Plus inventory semantics (has_item,
  stacking, shortfall, quest-item protection) and the save-section round trip.

Key items:
- Rig: world + null physics + a player with an inventory, camera and hover.

Dependencies:
- Uses: doctest, dfn_gameplay, dfn_platform_physics, dfn_core.
- Used by: ctest (sim_interaction).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- has_item semantics here are contract with story: total count of an ItemId.
*/
/*
UPD:
- 09:08:2026 - 18:56:32: Initial four-verb + inventory + save suite; torch
                         held/lit state.
- 09:08:2026 - 20:28:17: Carried-light bridge case, pinning the hand
                         offset above the feet (a "down from the origin"
                         offset would bury the light).
- 10:08:2026 - 21:06:27: UNBLOCKED. core's BinaryReader/BinaryWriter are
                         implemented (sim, under a lead carve), so the two
                         persistence cases below now LINK and RUN as authored —
                         DFN_SIM_HAVE_BINARY_IO defaults to 1. Not one
                         assertion in them was edited: they were written before
                         the implementation existed and by a different pass,
                         which is what makes them evidence rather than a
                         restatement of the code.
- 13:08:2026 - 18:25:00: The three spawn descs use DESIGNATED initialisers.
  They were positional, so `InteractableDesc` gaining a field in the middle
  (mesh_asset) silently re-aimed every argument after it -- the compiler caught
  it this time only because the types happened to disagree. A positional
  aggregate of nine fields is a trap that fires quietly the once it does not.
*/

#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "engine/core/components/sources/Components.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/GameplaySave.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"

namespace {

namespace gameplay = dfn::gameplay;
namespace platform = dfn::platform;
namespace serialization = dfn::serialization;
namespace config = dfn::config;
using dfn::components::HoverTarget;
using dfn::ecs::EntityId;

const gameplay::ItemId TORCH{serialization::fnv1a64("item.tool.torch")};
const gameplay::ItemId APPLE{serialization::fnv1a64("item.food.apple")};
const gameplay::ItemId COIN{serialization::fnv1a64("item.coin.septim")};
const gameplay::ItemId GRANT{serialization::fnv1a64("item.quest.crown_grant")};

// Item definitions as the content file declares them (the loader lands with
// core's JSON reader; the definitions themselves are what matter here).
gameplay::ItemDatabase make_items() {
    gameplay::ItemDatabase db;
    db.add(gameplay::ItemDef{TORCH, "item.tool.torch.name", "", 1, 1.0f, false, true});
    db.add(gameplay::ItemDef{APPLE, "item.food.apple.name", "", 8, 0.15f, false});
    db.add(gameplay::ItemDef{COIN, "item.coin.septim.name", "", 999, 0.01f, false});
    db.add(gameplay::ItemDef{GRANT, "item.quest.crown_grant.name", "", 1, 0.05f, true});
    return db;
}

struct Rig {
    dfn::ecs::World world;
    dfn::events::EventBus events;
    std::unique_ptr<platform::IPhysics> physics = platform::create_null_physics();
    EntityId player{};

    Rig() {
        REQUIRE(physics->init());
        world.add_resource(make_items());
        world.add_resource(HoverTarget{});

        player = world.spawn();
        world.add(player, gameplay::PlayerState{});
        world.add(player, gameplay::Inventory{});
        // Eye at origin looking down -Z (yaw 0), the convention the ray uses.
        world.add(player, dfn::components::CameraPose{{0.0f, 1.7f, 0.0f}, 0.0f, 0.0f});
    }

    [[nodiscard]] gameplay::Inventory& inventory() {
        return *world.get<gameplay::Inventory>(player);
    }

    // Null physics raycasts always miss, so hover is set directly where a test
    // is about the VERB rather than about the ray (the ray itself is covered
    // by the Jolt suite's targeting case).
    void look_at(EntityId target) {
        auto& hover = world.resource<HoverTarget>();
        const gameplay::InteractionOffer offer = gameplay::offer_for(world, target);
        hover.entity = target;
        hover.verb = static_cast<uint8_t>(offer.verb);
        hover.prompt_key = offer.prompt_key;
    }

    bool act() { return gameplay::interact(world, events, player); }
};

// --- LOOK --------------------------------------------------------------------

TEST_CASE("LOOK: offer_for resolves each verb and its prompt key") {
    Rig rig;
    const EntityId apple = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Pickup,
         .position = {0, 0, -2},
         .half_extents = {0.1f, 0.1f, 0.1f},
         .prompt_key = "interact.take",
         .item = APPLE,
         .count = 2});
    const EntityId chest = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Openable,
         .position = {2, 0, -2},
         .half_extents = {0.5f, 0.35f, 0.35f},
         .prompt_key = "interact.open"});
    const EntityId lever = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Usable,
         .position = {-2, 0, -2},
         .half_extents = {0.1f, 0.4f, 0.1f},
         .prompt_key = "interact.use",
         .action = serialization::fnv1a64("use.testbed.lever"),
         .repeatable = false});

    CHECK(gameplay::offer_for(rig.world, apple).verb == gameplay::InteractionVerb::Take);
    CHECK(gameplay::offer_for(rig.world, chest).verb == gameplay::InteractionVerb::Open);
    CHECK(gameplay::offer_for(rig.world, lever).verb == gameplay::InteractionVerb::Use);
    // The prompt travels as a hash of the localization KEY, never as text.
    CHECK(gameplay::offer_for(rig.world, apple).prompt_key ==
          serialization::fnv1a64("interact.take"));
    // A dead entity offers nothing rather than crashing.
    CHECK(gameplay::offer_for(rig.world, EntityId::null()).verb ==
          gameplay::InteractionVerb::None);
}

TEST_CASE("LOOK: update_hover clears the resource when nothing is targeted") {
    Rig rig;
    auto& hover = rig.world.resource<HoverTarget>();
    hover.entity = rig.player;
    hover.verb = 3;
    hover.prompt_key = 42;

    // Null physics never hits, so a tick must leave the resource cleared.
    gameplay::update_hover(rig.world, *rig.physics);
    CHECK(rig.world.resource<HoverTarget>().entity.is_null());
    CHECK(rig.world.resource<HoverTarget>().verb == 0);
    CHECK(rig.world.resource<HoverTarget>().prompt_key == 0);
}

// --- TAKE --------------------------------------------------------------------

TEST_CASE("TAKE: the item lands in the inventory and the pickup is gone") {
    Rig rig;
    const EntityId apple = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Pickup,
         .position = {0, 0, -2},
         .half_extents = {0.1f, 0.1f, 0.1f},
         .prompt_key = "interact.take",
         .item = APPLE,
         .count = 2});

    std::vector<gameplay::ItemTaken> taken;
    rig.events.subscribe<gameplay::ItemTaken>(
        [&](const gameplay::ItemTaken& e) { taken.push_back(e); });

    rig.look_at(apple);
    REQUIRE(rig.act());
    rig.events.pump();

    CHECK(gameplay::count_item(rig.inventory(), APPLE) == 2);
    REQUIRE(taken.size() == 1);
    CHECK(taken[0].item.value == APPLE.value);
    CHECK(taken[0].count == 2);
    CHECK(taken[0].actor == rig.player);

    rig.world.flush_destroyed();
    CHECK_FALSE(rig.world.alive(apple)); // the pickup entity is consumed
}

TEST_CASE("TAKE: an actor without an inventory fails cleanly") {
    Rig rig;
    const EntityId bystander = rig.world.spawn(); // no Inventory component
    const EntityId apple = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Pickup,
         .position = {0, 0, -2},
         .half_extents = {0.1f, 0.1f, 0.1f},
         .prompt_key = "interact.take",
         .item = APPLE,
         .count = 1});

    std::vector<gameplay::InteractionFailed> failures;
    rig.events.subscribe<gameplay::InteractionFailed>(
        [&](const gameplay::InteractionFailed& e) { failures.push_back(e); });

    rig.look_at(apple);
    CHECK_FALSE(gameplay::interact(rig.world, rig.events, bystander));
    rig.events.pump();
    REQUIRE(failures.size() == 1);
    CHECK(failures[0].reason == gameplay::InteractionFailure::NoInventory);
    CHECK(rig.world.alive(apple)); // nothing was consumed
}

// --- OPEN / CLOSE ------------------------------------------------------------

TEST_CASE("OPEN: the door state machine toggles and reports each change") {
    Rig rig;
    const EntityId door = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Openable,
         .position = {0, 0, -2},
         .half_extents = {0.6f, 1.0f, 0.1f},
         .prompt_key = "interact.open"});

    std::vector<gameplay::OpenStateChanged> changes;
    rig.events.subscribe<gameplay::OpenStateChanged>(
        [&](const gameplay::OpenStateChanged& e) { changes.push_back(e); });

    rig.look_at(door);
    REQUIRE(rig.act());
    CHECK(rig.world.get<gameplay::Openable>(door)->open);
    // Now it offers CLOSE, not OPEN — the verb follows the state.
    CHECK(gameplay::offer_for(rig.world, door).verb == gameplay::InteractionVerb::Close);

    rig.look_at(door);
    REQUIRE(rig.act());
    CHECK_FALSE(rig.world.get<gameplay::Openable>(door)->open);

    rig.events.pump();
    REQUIRE(changes.size() == 2);
    CHECK(changes[0].open);
    CHECK_FALSE(changes[1].open);
}

TEST_CASE("OPEN: a locked door refuses before and after the press") {
    Rig rig;
    const EntityId door = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Openable,
         .position = {0, 0, -2},
         .half_extents = {0.6f, 1.0f, 0.1f},
         .prompt_key = "interact.open",
         .starts_open = false,
         .locked = true});

    // The refusal is visible in the OFFER, so the reticle can show it before
    // the player presses anything.
    const gameplay::InteractionOffer offer = gameplay::offer_for(rig.world, door);
    CHECK(offer.verb == gameplay::InteractionVerb::Open);
    CHECK(offer.blocked == gameplay::InteractionFailure::Locked);
    CHECK_FALSE(offer.available());

    std::vector<gameplay::InteractionFailed> failures;
    rig.events.subscribe<gameplay::InteractionFailed>(
        [&](const gameplay::InteractionFailed& e) { failures.push_back(e); });

    rig.look_at(door);
    CHECK_FALSE(rig.act());
    rig.events.pump();
    REQUIRE(failures.size() == 1);
    CHECK(failures[0].reason == gameplay::InteractionFailure::Locked);
    CHECK_FALSE(rig.world.get<gameplay::Openable>(door)->open); // still shut
}

// --- USE ---------------------------------------------------------------------

TEST_CASE("USE: a repeatable lever fires every time, carrying its action id") {
    Rig rig;
    const uint64_t action = serialization::fnv1a64("use.testbed.lever");
    const EntityId lever = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Usable,
         .position = {0, 0, -2},
         .half_extents = {0.1f, 0.4f, 0.1f},
         .prompt_key = "interact.use",
         .action = action,
         .repeatable = true});

    std::vector<gameplay::Used> uses;
    rig.events.subscribe<gameplay::Used>(
        [&](const gameplay::Used& e) { uses.push_back(e); });

    rig.look_at(lever);
    REQUIRE(rig.act());
    rig.look_at(lever);
    REQUIRE(rig.act());
    rig.events.pump();

    REQUIRE(uses.size() == 2);
    CHECK(uses[0].action == action); // content declares the meaning, not C++
    CHECK(uses[0].entity == lever);
}

TEST_CASE("USE: a one-shot usable refuses the second time") {
    Rig rig;
    const EntityId shrine = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Usable,
         .position = {0, 0, -2},
         .half_extents = {0.3f, 0.3f, 0.3f},
         .prompt_key = "interact.use",
         .action = serialization::fnv1a64("use.shrine.once"),
         .repeatable = false});

    rig.look_at(shrine);
    REQUIRE(rig.act());
    CHECK(gameplay::offer_for(rig.world, shrine).blocked ==
          gameplay::InteractionFailure::AlreadyUsed);
    rig.look_at(shrine);
    CHECK_FALSE(rig.act());
}

// --- Inventory semantics (contract with story) -------------------------------

TEST_CASE("inventory: has_item counts totals across stacks") {
    Rig rig;
    const auto& items = rig.world.resource<gameplay::ItemDatabase>();
    // 20 coins with max_stack 999 stay one stack; 20 apples (max 8) split.
    gameplay::add_item(rig.inventory(), items, COIN, 20);
    gameplay::add_item(rig.inventory(), items, APPLE, 20);

    CHECK(gameplay::count_item(rig.inventory(), COIN) == 20);
    CHECK(gameplay::count_item(rig.inventory(), APPLE) == 20);
    CHECK(gameplay::count_item(rig.inventory(), TORCH) == 0); // has_item(x,==,0)

    int apple_stacks = 0;
    for (const auto& stack : rig.inventory().stacks) {
        if (stack.item.value == APPLE.value) {
            ++apple_stacks;
            CHECK(stack.count <= 8);
        }
    }
    CHECK(apple_stacks == 3); // 8 + 8 + 4
}

TEST_CASE("inventory: remove reports the shortfall instead of failing") {
    Rig rig;
    const auto& items = rig.world.resource<gameplay::ItemDatabase>();
    gameplay::add_item(rig.inventory(), items, APPLE, 3);

    // story's decision: remove what is there, report the difference, never
    // strand a quest state machine mid-transition.
    const uint32_t removed = gameplay::remove_item(rig.inventory(), APPLE, 5);
    CHECK(removed == 3);
    CHECK(gameplay::count_item(rig.inventory(), APPLE) == 0);
    CHECK(rig.inventory().stacks.empty()); // emptied stacks are erased
}

TEST_CASE("inventory: quest items cannot be dropped") {
    Rig rig;
    const auto& items = rig.world.resource<gameplay::ItemDatabase>();
    gameplay::add_item(rig.inventory(), items, GRANT, 1);

    CHECK_FALSE(gameplay::can_drop(items, GRANT)); // the crown grant is safe
    CHECK(gameplay::can_drop(items, APPLE));
    // Quest effects may still move it: removal is not dropping.
    CHECK(gameplay::remove_item(rig.inventory(), GRANT, 1) == 1);
}

// --- The torch (held + lit state) --------------------------------------------

TEST_CASE("torch: taken, held and lit; the light state is reported") {
    Rig rig;
    rig.world.add(rig.player, gameplay::HeldItem{});
    const EntityId torch = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Pickup,
         .position = {0, 0, -2},
         .half_extents = {0.15f, 0.3f, 0.15f},
         .prompt_key = "interact.take",
         .item = TORCH,
         .count = 1});

    std::vector<gameplay::HeldLightChanged> lights;
    rig.events.subscribe<gameplay::HeldLightChanged>(
        [&](const gameplay::HeldLightChanged& e) { lights.push_back(e); });

    // Pick it up, put it in hand, light it.
    rig.look_at(torch);
    REQUIRE(rig.act());
    REQUIRE(gameplay::hold_item(rig.world, rig.events, rig.player, TORCH));
    REQUIRE(gameplay::toggle_lit(rig.world, rig.events, rig.player));
    CHECK(rig.world.get<gameplay::HeldItem>(rig.player)->lit);

    rig.events.pump();
    REQUIRE(lights.size() == 1);
    CHECK(lights[0].lit);
    CHECK(lights[0].item.value == TORCH.value);

    // Douse it again.
    REQUIRE(gameplay::toggle_lit(rig.world, rig.events, rig.player));
    CHECK_FALSE(rig.world.get<gameplay::HeldItem>(rig.player)->lit);
}

TEST_CASE("torch: only content-declared light sources can be lit") {
    Rig rig;
    rig.world.add(rig.player, gameplay::HeldItem{});
    const auto& items = rig.world.resource<gameplay::ItemDatabase>();
    gameplay::add_item(rig.inventory(), items, APPLE, 1);

    REQUIRE(gameplay::hold_item(rig.world, rig.events, rig.player, APPLE));
    // An apple is not a torch: refused, not silently ignored.
    CHECK_FALSE(gameplay::toggle_lit(rig.world, rig.events, rig.player));
    CHECK_FALSE(rig.world.get<gameplay::HeldItem>(rig.player)->lit);
}

TEST_CASE("torch: the hand can only hold what is carried, and switching douses") {
    Rig rig;
    rig.world.add(rig.player, gameplay::HeldItem{});
    const auto& items = rig.world.resource<gameplay::ItemDatabase>();

    // Not carried => cannot be held.
    CHECK_FALSE(gameplay::hold_item(rig.world, rig.events, rig.player, TORCH));

    gameplay::add_item(rig.inventory(), items, TORCH, 1);
    gameplay::add_item(rig.inventory(), items, APPLE, 1);
    REQUIRE(gameplay::hold_item(rig.world, rig.events, rig.player, TORCH));
    REQUIRE(gameplay::toggle_lit(rig.world, rig.events, rig.player));
    rig.events.pump(); // drain the "lit" event so only the douse is observed

    std::vector<gameplay::HeldLightChanged> lights;
    rig.events.subscribe<gameplay::HeldLightChanged>(
        [&](const gameplay::HeldLightChanged& e) { lights.push_back(e); });

    // Switching to the apple must douse the torch — no lit torch in a pocket.
    REQUIRE(gameplay::hold_item(rig.world, rig.events, rig.player, APPLE));
    rig.events.pump();
    REQUIRE(lights.size() == 1);
    CHECK_FALSE(lights[0].lit);
    CHECK_FALSE(rig.world.get<gameplay::HeldItem>(rig.player)->lit);

    gameplay::stow_item(rig.world, rig.events, rig.player);
    CHECK_FALSE(rig.world.get<gameplay::HeldItem>(rig.player)->item.valid());
}

TEST_CASE("torch: the carried light mirrors the held state for render") {
    Rig rig;
    rig.world.add(rig.player, gameplay::HeldItem{});
    const auto& items = rig.world.resource<gameplay::ItemDatabase>();
    gameplay::add_item(rig.inventory(), items, TORCH, 1);
    REQUIRE(gameplay::hold_item(rig.world, rig.events, rig.player, TORCH));

    // Held but unlit: no light component is created at all.
    gameplay::update_carried_lights(rig.world);
    CHECK(rig.world.get<dfn::components::CarriedLight>(rig.player) == nullptr);

    REQUIRE(gameplay::toggle_lit(rig.world, rig.events, rig.player));
    gameplay::update_carried_lights(rig.world);
    const auto* light = rig.world.get<dfn::components::CarriedLight>(rig.player);
    REQUIRE(light != nullptr);
    CHECK(light->active);
    CHECK(light->radius_m == 0.0f);  // 0 = render's default torch
    CHECK(light->color_rgb == 0u);

    // THE BUG THIS PINS: the offset is measured from Transform.position, which
    // is the capsule BOTTOM. A flame "0.25 m down from the origin" would sit
    // underground and light nothing; it belongs at hand height above the feet.
    CHECK(light->offset.y > 1.0f);
    CHECK(light->offset.y ==
          doctest::Approx(static_cast<float>(config::PLAYER_EYE_HEIGHT) - 0.25f));
    CHECK(light->offset.x > 0.0f); // local +X is the carrier's right hand

    // Dousing clears it without destroying the component.
    REQUIRE(gameplay::toggle_lit(rig.world, rig.events, rig.player));
    gameplay::update_carried_lights(rig.world);
    CHECK_FALSE(rig.world.get<dfn::components::CarriedLight>(rig.player)->active);

    // Stowing a lit torch also goes dark.
    REQUIRE(gameplay::toggle_lit(rig.world, rig.events, rig.player));
    gameplay::update_carried_lights(rig.world);
    REQUIRE(rig.world.get<dfn::components::CarriedLight>(rig.player)->active);
    gameplay::stow_item(rig.world, rig.events, rig.player);
    gameplay::update_carried_lights(rig.world);
    CHECK_FALSE(rig.world.get<dfn::components::CarriedLight>(rig.player)->active);
}

// --- Persistence -------------------------------------------------------------
//
// WAS BLOCKED, NOW LIVE. These two cases were written, reviewed and compiled
// out for two days because core's BinaryReader/BinaryWriter were declaration
// only. The IO landed and the switch below is now 1; the cases run exactly as
// they were authored, with no assertion touched. That ordering is the point:
// they are a check written against the CONTRACT before any implementation
// existed to be accidentally described.
//
// The round trip's Rule 30 control is not here but in SaveFormatTests.cpp — a
// second payload that must restore to DIFFERENT values, because a reader that
// returned a fixed struct would sail through the case below.
#ifndef DFN_SIM_HAVE_BINARY_IO
#define DFN_SIM_HAVE_BINARY_IO 1
#endif

#if DFN_SIM_HAVE_BINARY_IO

TEST_CASE("save sections round-trip inventory and interactable state") {
    Rig rig;
    const auto& items = rig.world.resource<gameplay::ItemDatabase>();
    gameplay::add_item(rig.inventory(), items, COIN, 17);
    gameplay::add_item(rig.inventory(), items, TORCH, 1);

    const EntityId door = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Openable,
         .position = {0, 0, -2},
         .half_extents = {0.6f, 1.0f, 0.1f},
         .prompt_key = "interact.open"});
    const EntityId lever = gameplay::spawn_interactable(
        rig.world, *rig.physics,
        {.kind = gameplay::InteractableKind::Usable,
         .position = {2, 0, -2},
         .half_extents = {0.1f, 0.4f, 0.1f},
         .prompt_key = "interact.use",
         .action = 7,
         .repeatable = false});
    rig.look_at(door);
    REQUIRE(rig.act()); // door now open
    rig.look_at(lever);
    REQUIRE(rig.act()); // lever now spent

    serialization::BinaryWriter writer;
    writer.begin_file(serialization::make_tag('D', 'F', 'S', 'V'), 1);
    writer.begin_section(gameplay::SECTION_INVENTORY,
                         gameplay::INVENTORY_SECTION_VERSION);
    gameplay::write_inventory_section(writer, rig.world);
    writer.end_section();
    writer.begin_section(gameplay::SECTION_INTERACTABLES,
                         gameplay::INTERACTABLES_SECTION_VERSION);
    gameplay::write_interactables_section(writer, rig.world);
    writer.end_section();

    // Corrupt the live state, then restore it from the bytes.
    rig.inventory().stacks.clear();
    rig.world.get<gameplay::Openable>(door)->open = false;
    rig.world.get<gameplay::Usable>(lever)->used = false;

    serialization::BinaryReader reader;
    REQUIRE(reader.open(writer.buffer(), serialization::make_tag('D', 'F', 'S', 'V')));
    while (const auto section = reader.next_section()) {
        if (section->tag == gameplay::SECTION_INVENTORY) {
            REQUIRE(gameplay::read_inventory_section(reader, rig.world, section->version));
        } else if (section->tag == gameplay::SECTION_INTERACTABLES) {
            REQUIRE(gameplay::read_interactables_section(reader, rig.world,
                                                         section->version));
        }
    }

    CHECK(gameplay::count_item(rig.inventory(), COIN) == 17);
    CHECK(gameplay::count_item(rig.inventory(), TORCH) == 1);
    CHECK(rig.world.get<gameplay::Openable>(door)->open);
    CHECK(rig.world.get<gameplay::Usable>(lever)->used);
}

TEST_CASE("save sections refuse a section written by a newer build") {
    Rig rig;
    serialization::BinaryWriter writer;
    writer.begin_file(serialization::make_tag('D', 'F', 'S', 'V'), 1);
    writer.begin_section(gameplay::SECTION_INVENTORY, 1);
    gameplay::write_inventory_section(writer, rig.world);
    writer.end_section();

    serialization::BinaryReader reader;
    REQUIRE(reader.open(writer.buffer(), serialization::make_tag('D', 'F', 'S', 'V')));
    REQUIRE(reader.next_section().has_value());
    // A future version must abort the load, not silently misread fields.
    CHECK_FALSE(gameplay::read_inventory_section(
        reader, rig.world, gameplay::INVENTORY_SECTION_VERSION + 1));
}

#endif // DFN_SIM_HAVE_BINARY_IO

} // namespace
