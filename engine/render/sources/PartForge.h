/*
Created: 16:08:2026 - 20:52:00
Last updated: 16:08:2026 - 20:52:00
Module: engine/render
File: engine/render/sources/PartForge.h

Responsibility:
- THE BUILDING KIT: forges construction PARTS — beams, posts, planks, wall
  panels, gables, roof slopes, stairs, doors, windows, footings, fences — into
  the object registry (.dfo), so an agent builds a house by PLACING PIECES
  instead of writing a house-shaped function.

Key items:
- PartKind / PartParams / forge_part(): one part, made to size.
- kit_catalogue(): the whole numbered kit, expanded from a few families.

WHY PARTS AND NOT HOUSES (user, 16.08.2026: «надо чтобы агент мог сделать себе
несколько видов разных палок, стен, лестниц и тд, чтобы был набор из 500-та
различных строй материалов и их конфигураций, чтобы агент строил разные дома»):
a house generator produces the houses its author imagined; a KIT produces the
houses its USER imagines. The reference frames he supplied are Nordic timber
frame — posts and beams carrying the load, infill between them, a steep roof,
a stair to a raised floor — which is a kit by construction: the same dozen
pieces, cut to a few lengths, repeated.

EVERY PART SNAPS. Sizes are whole multiples of BUILD_GRID_M and origins sit at
a piece's natural joint (a beam's origin is its END, not its middle), so pieces
placed on the grid meet exactly instead of nearly. Placement by eye is what
makes assembled buildings look assembled.

Dependencies:
- Uses: ObjectRegistry.h (RegistryObject), FloraCards.h (bark/wood tiles).
- Used by: tools/forge_parts.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- PURE AND DETERMINISTIC: same params, same bytes, same content hash.
- Rule 52 applies to every part: closed volumes, never single planes. A wall
  you can see through the edge of is a wall nobody can build with.
*/
/*
UPD:
- 16:08:2026 - 20:52:00: Создан — набор строительных деталей для агента-строителя.
*/

#pragma once

#include "engine/render/sources/ObjectRegistry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::render {

/// The build grid. Every part's size is a whole multiple of it and every
/// origin sits on it, which is what lets an agent place by integer counts and
/// have the pieces MEET. 0.25 m: fine enough for a step's rise and a plank's
/// width, coarse enough that a whole house is a few dozen integers.
inline constexpr float BUILD_GRID_M = 0.25f;

enum class PartKind : uint8_t {
    Beam,       ///< horizontal timber, origin at its near end
    Post,       ///< vertical timber, origin at its foot
    Plank,      ///< thin board for cladding and decks
    WallPanel,  ///< timber frame + infill, one bay wide
    Gable,      ///< the triangular end wall under a roof
    RoofSlope,  ///< one pitched roof plane with its ridge beam
    Stair,      ///< a flight, origin at the foot of the lowest step
    DoorFrame,  ///< opening with jambs and lintel (the door leaf is its own part)
    DoorLeaf,
    WindowFrame,
    Footing,    ///< the stone block a timber building stands on
    Fence,      ///< one section of rail fence
};

/// What a part is made OF, in the reference's own terms: the frames the user
/// gave show three materials and nothing else — weathered timber, pale infill
/// plaster, and grey stone footings — plus thatch on the roofs.
enum class PartMaterial : uint8_t {
    Timber,
    TimberDark,
    Plaster,
    Stone,
    Thatch,
    Shingle,
};

struct PartParams {
    uint64_t seed = 1;
    std::string name = "part";
    PartKind kind = PartKind::Beam;
    PartMaterial material = PartMaterial::Timber;
    /// Size in GRID UNITS, not metres — the unit an agent counts in.
    int length_u = 8;  ///< along the part's own axis
    int width_u = 1;
    int height_u = 1;
    /// 0 = crisp and new, 1 = weathered: axe marks, sag, split ends. The
    /// reference is old wood, so the kit's default is not zero.
    float wear = 0.5f;
};

/// The kit's naming rule: kind-material-LxWxH-wNN, e.g. "beam-timber-8x2x1-w06".
/// Derived from the params alone, so a part's file name states its size and an
/// agent can ASK for a part by describing it instead of reading an index.
[[nodiscard]] std::string part_name(const PartParams& params);

/// Forges one part. Ready for write_object().
[[nodiscard]] RegistryObject forge_part(const PartParams& params);

/// THE KIT: every part the catalogue declares, expanded from its families
/// (kind x size x material x wear). This is the "500 pieces" — produced by
/// rule rather than typed out, so adding a length adds a row everywhere.
[[nodiscard]] std::vector<PartParams> kit_catalogue();

} // namespace dfn::render
