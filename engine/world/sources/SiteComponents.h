/*
Created: 09:08:2026 - 11:05:22
Last updated: 14:08:2026 - 21:03:06
Module: engine/world
File: engine/world/sources/SiteComponents.h

Responsibility:
- Site-type ECS component attached to worldgen P4 entities (buildings, shrine,
  dungeon entrances, tower ruin) and the placeholder archetype table mapping
  SiteType -> content id string, provisional RenderMesh id and local bounds
  (silhouette boxes per LANDSCAPE.md §6 until real content data files exist).

Key items:
- SiteType, SiteMarker (component, Rule 8 plain data).
- SiteArchetype, site_archetype(): the placeholder archetype table.

Dependencies:
- Uses: glm.
- Used by: WorldgenSites (records), ChunkManager (spawn attachment), gameplay
  (site queries via ECS view), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PROVISIONAL placeholder mesh ids 1..7: RenderMesh ids are registry-assigned
  dense ids (stage-1 sync decision). The id table here is the worldgen-side
  half of that registry until the lead lands a shared one; render maps the
  same ids to placeholder prisms (communicated 09:08:2026). Do not reuse ids.
- Real content moves to data files (Rule 5) with the archetype/content sync;
  footprint numbers cite LANDSCAPE.md §6 and are design's to tune.
*/
/*
UPD:
- 09:08:2026 - 11:05:22: Stage 3b — SiteMarker component + placeholder
  archetype table (§6 silhouettes, provisional mesh ids).
- 09:08:2026 - 15:18:34: Castle (§6.1.3 hall-castle): SiteTypes CastleHall/Wall/Gatehouse/Solar with lead-blessed mesh ids 8..11; archetype lookup bound extended to the new types.
- 09:08:2026 - 19:33:58: Fortress revision: SiteType::CastleTower reinstated (mesh id 12); solar envelope raised to 20 m.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 14:08:2026 - 21:03:06: SiteArchetype.model_half_extents + site_placeholder_scale() — таблица наконец говорит, В КАКОМ ПРОСТРАНСТВЕ авторован меш, а не только какой у сайта ящик. Спавнер иначе не имел третьего варианта и рисовал КАЖДЫЙ плейсхолдер в масштабе 1: верно по везению для мешей 1..12 (ProcMesh.cpp авторует их в метрах против ровно этих границ) и неверно для меша 52, который InteractableMesh.h авторует в единичном кубе — сконс рисовался 5× шире и 1.8× выше собственных границ, и при WALL_TORCH_INSET 0.25 м его полуширина 1.0 м уводила 0.75 м предмета ВНУТРЬ породы. Это и есть «кривой рыжий кусочек наполовину в стене». Чинится МЕХАНИЗМ, а не экземпляр (правило 32): масштабирование одной плохой строки оставило бы дыру следующему мешу, взятому у другой зоны. Ноль в поле = «авторован в метрах против этих границ» — сигнальное значение, а не повтор полуразмеров, потому что повтор был бы теневой копией (правило 39). Границы факела заодно сделаны СИММЕТРИЧНЫМИ по y: рисуемый ящик центрирован на начале сущности (центр меша y=0), а −0.2..+0.9 описывал ящик, которого рендер никогда не рисовал (правило 43 — принуждать на той величине, на которой меряют контракт). Полный размер не тронут, 0.4×1.1×0.4 м; каким он должен СТАТЬ — вопрос NUMBERS/design (TORCH_MODEL_RESEARCH §5), и эта правка на него намеренно не отвечает. Контрольная рука из ЭТОГО бинарника: DFN_SITE_SCALE=0 возвращает прежнее поведение (все плейсхолдеры в масштабе 1), отвергается вслух.
*/

#pragma once

#include "engine/core/serialization/sources/ContentHash.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <glm/vec3.hpp>
#include <optional>

namespace dfn::world {

/// What a P4 site entity is. Values are stable (serialized via archetype ids,
/// not this enum — but keep append-only anyway).
enum class SiteType : uint8_t {
    Dwelling = 0,
    Trader = 1,
    Tavern = 2,
    Barn = 3,
    Shrine = 4,
    DungeonEntrance = 5,
    TowerRuin = 6,
    // Castle mass (LANDSCAPE §6.1.3 hall-castle revision), mesh ids 8..11
    // blessed by the lead. Horizontal-dominant: a long hall with ONE modest
    // vertical (the solar) — no keep, no corner towers.
    CastleHall = 7,
    CastleWall = 8,       ///< curtain enclosure — render draws it hollow
    CastleGatehouse = 9,
    CastleSolar = 10,     ///< the tall vertical on the oldest ward
    CastleTower = 11,     ///< corner tower, REINSTATED by the fortress revision
    /// A torch in a sconce on a carved corridor's wall (the user's ruling:
    /// «факела точно должны висеть в той пещере»). Placed by worldgen wherever
    /// a corridor is ENCLOSED — the same roof predicate the darkness gate uses,
    /// so a torch appears exactly where the place goes dark and nowhere else.
    /// It is a site entity because that is the only path a generated chunk has
    /// for placing things; ChunkManager gives this type its light.
    WallTorch = 12,
};

/// ECS component attached to every P4 site entity at chunk spawn. Derived
/// from the entity record's archetype hash (site_type_from_archetype).
struct SiteMarker {
    SiteType type = SiteType::Dwelling;
};

/// Placeholder archetype: content id (hashed into GeneratedEntityRecord),
/// provisional RenderMesh id, and model-space bounds (LANDSCAPE §6 footprint
/// x height; min.y = 0 at the pad surface).
struct SiteArchetype {
    SiteType type;
    const char* content_id;  ///< developer-facing id, hashed via fnv1a64
    uint32_t mesh_id;        ///< provisional dense RenderMesh id (see notice)
    glm::vec3 bounds_min;
    glm::vec3 bounds_max;
    /// THE HALF-EXTENTS OF THE SPACE THE MESH IS AUTHORED IN — the field whose
    /// absence let a 2 m cube be spawned as a wall sconce.
    ///
    /// The table used to say what a site's box IS and never what space its
    /// TRIANGLES live in, and the spawner had no third option: it drew every
    /// placeholder at scale 1. That was correct by luck for meshes 1..12,
    /// which `ProcMesh.cpp` authors in metres against these very bounds, and
    /// wrong for mesh 52, which `InteractableMesh.h` authors in the unit cube
    /// (its own header says so: "authored in the unit cube") — so the sconce
    /// was drawn 5x wider and 1.8x taller than its own bounds, and with
    /// `WALL_TORCH_INSET` 0.25 m its 1.0 m half-width put 0.75 m of it INSIDE
    /// the rock. That is the user's «кривой рыжий кусочек наполовину в стене».
    ///
    /// Naming the space is the mechanism (Rule 32); scaling the one bad row
    /// would have been the instance, and the next mesh borrowed from another
    /// zone would have re-opened the hole. Rule 43 in the same breath: the
    /// contract is measured on the DRAWN box, so this is what the drawn box is
    /// enforced against.
    ///
    /// ZERO (the default) means "authored in METRES against these very
    /// bounds", i.e. scale 1. It is a sentinel rather than a repeat of the
    /// half-extents on purpose: writing those twelve number pairs a second
    /// time would be a shadow copy, and Rule 39 is precisely about the copy
    /// that is correct on the day it is written.
    glm::vec3 model_half_extents{};
};

/// The scale a spawner must give this archetype's Transform so the drawn mesh
/// fills exactly its declared bounds. Exactly 1 for every metre-authored row,
/// so landing this moves those rows by not one pixel — the zero-dose control
/// is in the derivation rather than measured beside it.
[[nodiscard]] inline glm::vec3 site_placeholder_scale(const SiteArchetype& a) {
    // THE CONTROL ARM, OUT OF THIS BINARY (DFN_SITE_SCALE=0, Rule 47). The
    // before/after of a geometry fix is otherwise two builds an hour apart in
    // a tree seven zones are committing to, which measures the week and not
    // the change. 0 restores the old behaviour exactly: every placeholder at
    // scale 1. Anything else, or nothing, is shipping. Refused OUT LOUD.
    static const bool disabled = [] {
        const char* e = std::getenv("DFN_SITE_SCALE");
        const bool off = e != nullptr && *e == '0';
        if (off) {
            std::fprintf(stderr, "[world] DFN_SITE_SCALE=0: site placeholders "
                                 "drawn at scale 1 again (the defect arm)\n");
        }
        return off;
    }();
    if (disabled) {
        return glm::vec3{1.0f};
    }
    const glm::vec3 m = a.model_half_extents;
    if (m.x <= 0.0f || m.y <= 0.0f || m.z <= 0.0f) {
        return glm::vec3{1.0f}; // authored in metres against these bounds
    }
    return (a.bounds_max - a.bounds_min) * 0.5f / m;
}

/// The archetype for a site type. Footprints per LANDSCAPE.md §6.
[[nodiscard]] inline const SiteArchetype& site_archetype(SiteType type) {
    static constexpr SiteArchetype TABLE[] = {
        {SiteType::Dwelling, "site.dwelling", 1, {-3.0f, 0.0f, -4.0f}, {3.0f, 5.5f, 4.0f}},
        {SiteType::Trader, "site.trader", 2, {-4.0f, 0.0f, -5.0f}, {4.0f, 6.5f, 5.0f}},
        {SiteType::Tavern, "site.tavern", 3, {-5.0f, 0.0f, -7.0f}, {5.0f, 8.5f, 7.0f}},
        {SiteType::Barn, "site.barn", 4, {-4.0f, 0.0f, -6.0f}, {4.0f, 7.5f, 6.0f}},
        {SiteType::Shrine, "site.shrine", 5, {-2.5f, 0.0f, -2.5f}, {2.5f, 12.0f, 2.5f}},
        {SiteType::DungeonEntrance, "site.dungeon_entrance", 6, {-2.0f, 0.0f, -2.0f},
         {2.0f, 4.0f, 2.0f}},
        {SiteType::TowerRuin, "site.tower_ruin", 7, {-2.0f, 0.0f, -2.0f}, {2.0f, 12.0f, 2.0f}},
        // Castle mass (§6.1.3). Heights are the design MAX values; the actual
        // built heights are solved against R3 in WorldgenCastle and written
        // into each record — these bounds are the placeholder envelope.
        {SiteType::CastleHall, "site.castle_hall", 8, {-5.0f, 0.0f, -11.0f},
         {5.0f, 9.0f, 11.0f}},
        {SiteType::CastleWall, "site.castle_wall", 9, {-20.0f, 0.0f, -20.0f},
         {20.0f, 8.0f, 20.0f}},
        {SiteType::CastleGatehouse, "site.castle_gatehouse", 10, {-5.0f, 0.0f, -3.0f},
         {5.0f, 11.0f, 3.0f}},
        {SiteType::CastleSolar, "site.castle_solar", 11, {-4.0f, 0.0f, -4.0f},
         {4.0f, 20.0f, 4.0f}},
        {SiteType::CastleTower, "site.castle_tower", 12, {-3.5f, 0.0f, -3.5f},
         {3.5f, 15.0f, 3.5f}},
        // Mesh 52 is sim's placeholder torch, ferried into render's registry by
        // the lead (a976569). 32/33 are NOT built and the registrar refuses
        // them -- do not "fix" this id to those.
        // THE ONE ROW WHOSE MESH IS NOT IN METRES. 52 is gameplay's
        // interactable torch, and InteractableMesh.h authors it in the unit
        // cube [-1,1]^3 — so it needs a scale where every row above needs
        // none. Bounds made SYMMETRIC in y at the same time: the drawn box is
        // centred on the entity origin (the mesh's own centre is y = 0), and
        // -0.2..+0.9 described a box the renderer never drew. Total size is
        // unchanged at 0.4 x 1.1 x 0.4 m; where the sconce's size should
        // finally SIT is a NUMBERS/design question (TORCH_MODEL_RESEARCH §5),
        // and this change deliberately does not answer it — it only makes the
        // drawn thing and the declared thing the same thing.
        {SiteType::WallTorch, "site.wall_torch", 52, {-0.2f, -0.55f, -0.2f},
         {0.2f, 0.55f, 0.2f}, {1.0f, 1.0f, 1.0f}},
    };
    return TABLE[static_cast<uint8_t>(type)];
}

/// Inverse lookup: archetype content hash -> site type (used when spawning
/// chunk entities from GeneratedEntityRecords). nullopt for non-site
/// archetypes (future content kinds pass through untouched).
[[nodiscard]] inline std::optional<SiteType> site_type_from_archetype(uint64_t archetype) {
    for (uint8_t t = 0; t <= static_cast<uint8_t>(SiteType::WallTorch); ++t) {
        const SiteArchetype& a = site_archetype(static_cast<SiteType>(t));
        if (serialization::fnv1a64(a.content_id) == archetype) {
            return a.type;
        }
    }
    return std::nullopt;
}

} // namespace dfn::world
