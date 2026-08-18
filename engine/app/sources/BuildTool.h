/*
Created: 17:08:2026 - 19:05:00
Last updated: 18:08:2026 - 12:08:00
Module: engine/app
File: engine/app/sources/BuildTool.h

Responsibility:
- THE HAND THAT PLACES THINGS. Holds what the builder picked, where the ghost
  stands, and — the whole point — whether putting it there is ALLOWED.

Key items:
- BuildPalette: the shelves' objects, grouped, with a selection.
- BuildGhost: the candidate placement (object, position, yaw).
- BuildVerdict: allowed or not, and WHY not, in the builder's language.
- snap_to_grid() / verdict_from_findings(): the two decisions worth testing.

WHY THIS EXISTS (user, 17.08.2026): «необходимо в редакторе добавить меню
выбора любого из объектов, что есть для строительства и добавить механику
строительства для меня, человека. Я должен уметь ставить полы и крепить к
нужным местам, где нельзя что-то ставить я должен видеть красным отметены, где
можно зеленым и прочее».

THE ONE DESIGN DECISION THAT MATTERS: green and red are decided by the SCENE
JUDGE ITSELF (world::check_scene), not by a second implementation of the rules
living here. The candidate is appended to a copy of the composition, the judge
runs, and the findings that name the candidate ARE the red. This costs a judge
pass per frame and buys the only property worth having — the editor cannot
allow what the judge forbids, and cannot forbid what it allows, because there
is one set of rules and this is it (Rule 32: single definition).

A second copy of the rules would drift within a week, and it would drift
SILENTLY: a builder placing a wall the editor showed green, and the judge later
calling it red, learns to distrust the colour — at which point the colour is
worse than nothing.

Dependencies:
- Uses: engine/world (SceneDoc, check_scene), glm.
- Used by: engine/app (App, editor mode), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NEVER answer "allowed" from a rule written in this file. If a new kind of
  refusal is needed, it belongs in the judge, where the tools and the agents
  see it too.
- The reason text is for a HUMAN standing in the world, so it names what he
  can do about it, not the rule's enum.
*/
/*
UPD:
- 17:08:2026 - 19:05:00: Создан — рука строителя и её зелёное/красное (заказ 17.08).
- 18:08:2026 - 12:08:00: place_support_y() — НА ЧТО САДИТСЯ ТО, ЧТО В РУКЕ. Заказ 18.08:
  «сейчас я могу ставить блок строительные только на землю... не могу ставить
  объекты друг на друга». Судья тут ни при чём: OnGround разрешает опору на
  другую расстановку (Scene.cpp), но ТОЛЬКО внутри одной группы. Виноват был
  инструмент — он безусловно сажал призрак на грунт (`at.y = height_at(...)`),
  то есть ВНУТРЬ того, на что человек целился, и судья честно отвечал «buried
  in». Теперь опора — САМАЯ ВЫСОКАЯ из двух: земля под прицелом и верх той
  расстановки, в которую прицел упирается.
*/

#pragma once

#include "engine/world/sources/Scene.h"

#include <glm/vec3.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace dfn::app {

/// THE BUILD GRID. The kit is authored on 0.25 m (BUILD_GRID_M in PartForge),
/// so a hand-placed part that ignored it could never seat on a joint — the
/// grid is not a convenience here, it is what makes parts meet at all.
inline constexpr float BUILD_GRID_M = 0.25f;

/// One shelf's worth of objects the builder can pick from, kept as a group so
/// the menu can show "стены" without the app knowing what a wall is.
struct BuildGroup {
    std::string title;               ///< shown in the menu (content, Rule 5)
    std::vector<std::string> names;  ///< registry object names
};

/// What the builder picked and where he is pointing it.
struct BuildGhost {
    std::string object;      ///< registry name; empty = nothing picked
    glm::vec3 position{0.0f};
    float yaw = 0.0f;        ///< radians, same convention as Placement::yaw
    [[nodiscard]] bool valid() const { return !object.empty(); }
};

/// The answer the builder sees as a colour, with the sentence behind it.
struct BuildVerdict {
    bool allowed = false;
    /// Why not, for a human: "дерево на тропе", "внутри дома". Empty when
    /// allowed. NOT the rule's name — the builder cannot act on an enum.
    std::string reason;
};

/// Rounds a position onto the build grid. Y is NOT snapped: the ground is
/// continuous and a part that jumped 25 cm up would float or sink for no
/// reason the builder can see.
[[nodiscard]] glm::vec3 snap_to_grid(glm::vec3 world_position);

/// Turns the judge's findings into the builder's answer. `candidate_index` is
/// where the ghost sits in the doc that was judged; findings about ANY OTHER
/// placement are somebody else's problem and must not colour this ghost red —
/// a scene that is already imperfect elsewhere would otherwise forbid every
/// new placement, which is how a build tool becomes unusable.
[[nodiscard]] BuildVerdict verdict_from_findings(
    const std::vector<world::SceneFinding>& findings, std::size_t candidate_index);

/// НА КАКОЙ ВЫСОТЕ СТОИТ ТО, ЧТО СТАВЯТ, — земля или верх того, на что целятся.
///
/// Returns the Y the placed part's ORIGIN should take. `aim` is where the
/// crosshair met the world (its Y is the ceiling: nothing is lifted ABOVE the
/// point the builder pointed at), `ground_y` is the terrain under the same x/z,
/// and `world` supplies the same measurements the judge uses — never a second
/// ruler (Rule 32).
///
/// WHY THE AIM'S OWN HEIGHT IS A CEILING. Without it, standing beside a house
/// and pointing at the GROUND next to a wall would snap the part to the roof,
/// because the wall's footprint covers that spot too. The rule is "the highest
/// support AT OR BELOW where you are pointing", which is what a builder means
/// when he points at a deck.
///
/// `on_what`, when given, receives the object's name, or "the ground".
[[nodiscard]] float place_support_y(const world::SceneDoc& doc, glm::vec3 aim,
                                    float ground_y, const world::SceneWorld& world,
                                    std::string* on_what = nullptr);

/// Builds the palette from the shelves a map declares (the same
/// semicolon-separated list the .map's `objects` key carries), grouping by the
/// leading token of the object name — "wall-log-..." lands in "wall". Reading
/// the SHELF rather than a hand-written list is deliberate: a list would go
/// stale the first time the kit grew, and it would go stale silently.
[[nodiscard]] std::vector<BuildGroup> build_palette(const std::string& shelves);

} // namespace dfn::app
