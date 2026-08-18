/*
Created: 18:08:2026 - 12:06:10
Last updated: 18:08:2026 - 12:06:10
Module: engine/editor
File: engine/editor/sources/EditorToolsBuiltin.h

Responsibility:
- THE FIVE TOOLS THEMSELVES, one class each: the height brush, the surface
  brush, selection, building, and planting. Each owns its own settings, its own
  reach, its own preview and its own click. Nothing about a tool lives outside
  its class.

Key items:
- HeightBrushTool / SurfacePaintTool: two SEPARATE brushes, not one brush with
  a mode shown twice.
- SelectTool / PlaceTool: the crosshair's two object hands.
- PlantTool: PLANTING NOW HAS AN OWNER.

WHY PLANTING IS A TOOL (docs/AUDIT_EDITOR_TOOLS.md, and the user found it by
using the editor): «что за порода выбирается, когда я открываю меню кисти? она
ни на что не влияет». It did not: the species list lived in the BRUSH panel
while the only call to plant a dab sat inside the BUILD tool's click handler,
behind Shift. Controls at one owner, action at another, and no owner for the
tool itself. It is a tool now — its own button, its own species list, its own
click — and the Shift path is gone.

WHY THE TWO GROUND BRUSHES ARE TWO OBJECTS. They used to be one TerrainBrush
with `mode`, drawn as two chips: the bar set the mode and the panel's mode
switch set it again, from a second place. Picking «Поверхность» then opening the
panel and choosing «Поднять» left the bar saying one thing and the ground doing
another. Two objects cannot disagree about which mode they are in.

Dependencies:
- Uses: EditorTool.h, EditorBrush.h (the mechanics), EditorPalette/PropsView
  (the two lists that were already panels), Dear ImGui inside draw_settings().
- Used by: engine/app (constructs them and hands them the world hooks).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The tool's settings are drawn by the tool. Do not add a second control for
  the same state anywhere else — that is the defect this file replaced.
*/
/*
UPD:
- 18:08:2026 - 12:06:10: Созданы пять инструментов по IEditorTool. Посадка стала
  отдельным инструментом со своим хозяином (заказ/разбор 18.08).
*/

#pragma once

#include "engine/editor/sources/EditorPaletteView.h"
#include "engine/editor/sources/EditorPropsView.h"
#include "engine/editor/sources/EditorTool.h"

#include <memory>
#include <string>
#include <vector>

namespace dfn::app {

class EditorUi;
class BrushSwatches;

/// Both ground brushes: press starts a stroke, drag keeps dabbing, release
/// spends the stroke's budget. The two subclasses differ in their MODE SET and
/// in their settings, and in nothing else — the biting is one implementation.
class TerrainBrushToolBase : public IEditorTool {
public:
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) override;
    void on_release(ToolWorld& world) override;
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    [[nodiscard]] float max_reach_m() const override { return 60.0f; }
    void on_deselected(ToolWorld& world) override;

    /// The brush this tool digs with. Public because the acceptance door and
    /// the tests read it; it is this tool's own and no one else's.
    [[nodiscard]] TerrainBrush& brush() { return brush_; }
    [[nodiscard]] const TerrainBrush& brush() const { return brush_; }

protected:
    TerrainBrush brush_;
    bool stroking_ = false;
    bool pad_written_ = false; ///< one [pad] per stroke, not per frame
};

/// 1 — кисть высоты. Modes: raise, lower, smooth, level. NEVER paint.
class HeightBrushTool final : public TerrainBrushToolBase {
public:
    HeightBrushTool();
    [[nodiscard]] ToolIdentity identity() const override;
    void draw_settings() override;
};

/// 2 — кисть поверхности. Its brush is permanently in Paint mode, so there is
/// no mode control here at all: what it paints is the only question it has.
class SurfacePaintTool final : public TerrainBrushToolBase {
public:
    explicit SurfacePaintTool(EditorUi& ui);
    ~SurfacePaintTool() override;
    [[nodiscard]] ToolIdentity identity() const override;
    void draw_settings() override;

private:
    std::unique_ptr<BrushSwatches> swatches_;
};

/// 3 — выбор. Its settings ARE the properties column: what you selected is
/// what you are about to change, so the two are one window.
class SelectTool final : public IEditorTool {
public:
    SelectTool(PropsModel& model, PropsHooks hooks);
    [[nodiscard]] ToolIdentity identity() const override;
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim&, float, ToolWorld&) override {}
    void on_release(ToolWorld&) override {}
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    void draw_settings() override;
    [[nodiscard]] float max_reach_m() const override { return 40.0f; }
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const override;

    /// The world hooks status() reads OUTSIDE a click («есть ли что-то под
    /// прицелом»). Handed over once, by the same code that fills ToolWorld.
    void set_world(const ToolWorld* world) { world_ = world; }

private:
    PropsModel* model_ = nullptr;
    PropsHooks hooks_;
    const ToolWorld* world_ = nullptr;
};

/// 4 — постройка. Owns the ghost's lifetime through the world hooks: putting
/// this tool down clears the part in hand, which is where «деталь остаётся в
/// руках» used to come from.
class PlaceTool final : public IEditorTool {
public:
    PlaceTool(PaletteModel& palette, PaletteHooks hooks);
    [[nodiscard]] ToolIdentity identity() const override;
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim&, float, ToolWorld&) override {}
    void on_release(ToolWorld&) override {}
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    void draw_settings() override;
    [[nodiscard]] float max_reach_m() const override { return 30.0f; }
    void on_deselected(ToolWorld& world) override;
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const override;
    [[nodiscard]] bool wants_part_rotation() const override { return true; }

    /// The world hooks the status/preview need OUTSIDE a click. They are handed
    /// over once, by the same code that fills ToolWorld.
    void set_world(const ToolWorld* world) { world_ = world; }

private:
    PaletteModel* palette_ = nullptr;
    PaletteHooks hooks_;
    const ToolWorld* world_ = nullptr;
};

/// 5 — посадка. Its own button, its own species list, its own click.
class PlantTool final : public IEditorTool {
public:
    /// `species` reports the shelf of things that may be planted. A function
    /// rather than a copy because the shelf is read lazily, on first ask.
    using SpeciesSource = std::function<const std::vector<std::string>&()>;

    explicit PlantTool(SpeciesSource species);
    [[nodiscard]] ToolIdentity identity() const override;
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim&, float, ToolWorld&) override {}
    void on_release(ToolWorld&) override {}
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    void draw_settings() override;
    [[nodiscard]] float max_reach_m() const override { return 40.0f; }
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const override;

    [[nodiscard]] PlantBrush& plant() { return plant_; }

private:
    PlantBrush plant_;
    SpeciesSource species_;
    /// The ring the planting dab covers, expressed as a TerrainBrush purely so
    /// the SAME outline code draws it (Rule 32) — its radius follows plant_.
    /// Mutable because preview() is const and the radius is this frame's.
    mutable TerrainBrush ring_;
    int last_planted_ = 0;
};

} // namespace dfn::app
