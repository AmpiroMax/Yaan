/*
Created: 17:08:2026 - 20:06:53
Last updated: 18:08:2026 - 01:29:51
Module: engine/editor
File: engine/editor/sources/EditorBrushView.cpp

Responsibility:
- The brush panel's content, declared in EditorBrushView.h.

Dependencies:
- Uses: EditorBrushView.h, EditorBrush.h, EditorUi.h, Dear ImGui.
- Used by: engine/app (App).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO Begin/End HERE. EditorUi owns the window; this draws content.
- EVERY VISIBLE STRING GOES THROUGH EditorUi::tr() (Rule 5).
- THE PANEL DECIDES NOTHING ABOUT THE WORLD. It moves numbers in two structs.
  A slider that also applied a stroke would fire the moment it was dragged,
  which is the exact accident this panel's existence is meant to prevent.
*/
/*
UPD:
- 17:08:2026 - 20:06:53: Создан — панель кисти рельефа и посадки.
- 18:08:2026 - 01:24:18: свотчи поверхностей: печь один раз, рисовать 24 px с уменьшением.
- 18:08:2026 - 01:29:51: три попытки выпечки свотча и голос при сдаче (см. заголовок; правка от
  хрупкости, не по воспроизведённому отказу).
*/

#include "engine/editor/sources/EditorBrushView.h"

#include "engine/editor/sources/EditorPaletteThumb.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

namespace dfn::app {
namespace {

/// The modes, in the order a sculptor reaches for them, each with its key.
/// A TABLE AND NOT A CHAIN OF IFS: a mode added without a caption would then be
/// a button with no name, and this way it cannot compile without one.
struct ModeRow {
    BrushMode mode;
    const char* key;
};
constexpr ModeRow MODES[] = {
    {BrushMode::Raise, "brush.mode.raise"},
    {BrushMode::Lower, "brush.mode.lower"},
    {BrushMode::Smooth, "brush.mode.smooth"},
    {BrushMode::Flatten, "brush.mode.flatten"},
    {BrushMode::Paint, "brush.mode.paint"},
};

struct SurfaceRow {
    math::SurfaceClass surface;
    const char* key;
};
/// THE FOUR THE WORLD CAN ACTUALLY DRAW. The user asked for «трава, земля,
/// камень, песок» and there is no earth/dirt CLASS in math::SurfaceClass — the
/// bare trodden ground in this world is path wear, which is a different claim
/// about the ground and carries the no-building rule with it. Offering a
/// "земля" swatch that painted a road would be worse than not offering it, so
/// the panel shows what exists and the gap is a request to render, not a lie
/// on screen.
constexpr SurfaceRow SURFACES[] = {
    {math::SurfaceClass::Grass, "brush.surface.grass"},
    {math::SurfaceClass::GrassRockBlend, "brush.surface.blend"},
    {math::SurfaceClass::Rock, "brush.surface.rock"},
    {math::SurfaceClass::Sand, "brush.surface.sand"},
};

/// How big a swatch is baked and drawn. 32 texels is the smallest square in
/// which the 4x4 Bayer dither of the blend band still reads as two materials
/// rather than as noise — the band is exactly what a builder cannot picture
/// from the word «смесь», so a swatch too small to show it would be showing
/// him the one thing he already knew.
constexpr int SWATCH_PX = 32;
/// Drawn slightly smaller than baked, so the picture is minified rather than
/// magnified: a magnified swatch shows the dither's own pixels as a pattern
/// that exists nowhere on the ground.
constexpr float SWATCH_DRAW_PX = 24.0f;

} // namespace

BrushSwatches::~BrushSwatches() {
    if (ui_ == nullptr) {
        return;
    }
    for (const auto& [surface, slot] : tex_) {
        (void)surface;
        if (slot.texture != 0) {
            ui_->drop_texture(slot.texture);
        }
    }
}

EditorTexture BrushSwatches::surface(math::SurfaceClass surface) {
    // BAKED ONCE PER CLASS AND KEPT. The panel asks every frame for every row;
    // baking here would be four proc-texture composes per frame for a picture
    // that cannot change. A FAILED bake is cached as 0 too, deliberately —
    // retrying a bake that already failed, sixty times a second, turns one
    // missing picture into a stutter.
    Slot& slot = tex_[surface];
    if (slot.texture != 0 || slot.given_up) {
        return slot.texture;
    }
    // ТРИ ПОПЫТКИ, А НЕ ОДНА И НЕ БЕСКОНЕЧНО. Одна попытка — то, что было, и
    // это стоило картинок целиком: неудача запоминалась как готовый ноль, так
    // что панель до конца сессии рисовала одни названия, а код выглядел
    // рабочим. Бесконечные попытки — другая крайность: одна недоступная
    // картинка превратилась бы в композицию текстуры шестьдесят раз в секунду.
    constexpr int MAX_ATTEMPTS = 3;
    ++slot.attempts;
    std::vector<std::uint8_t> rgba;
    if (ui_ != nullptr && bake_surface_swatch(surface, SWATCH_PX, rgba)) {
        slot.texture = ui_->make_texture(static_cast<std::uint32_t>(SWATCH_PX),
                                         static_cast<std::uint32_t>(SWATCH_PX), rgba.data());
    }
    if (slot.texture == 0 && slot.attempts >= MAX_ATTEMPTS) {
        slot.given_up = true;
        // СДАЁМСЯ ВСЛУХ. Молчащий отказ здесь неотличим от «так и задумано»:
        // на экране в обоих случаях просто название без картинки.
        std::fprintf(stderr,
                     "[кисть] свотч поверхности %d не выпекся за %d попытки — "
                     "в панели останется одно название\n",
                     static_cast<int>(surface), MAX_ATTEMPTS);
    }
    return slot.texture;
}

namespace {

} // namespace

void draw_brush_panel(TerrainBrush& terrain, PlantBrush& plant, const BrushHooks& hooks,
                      BrushSwatches* swatches) {
    // ---------------------------------------------------------------- ground
    ImGui::TextUnformatted(EditorUi::tr("brush.section.ground"));
    ImGui::Separator();

    for (const ModeRow& row : MODES) {
        const bool active = terrain.mode == row.mode;
        if (ImGui::RadioButton(EditorUi::tr(row.key), active)) {
            terrain.mode = row.mode;
        }
    }

    ImGui::Spacing();
    ImGui::SliderFloat(EditorUi::tr("brush.size"), &terrain.radius_m, BRUSH_MIN_RADIUS_M,
                       64.0f, "%.1f m");
    // THE FLOOR IS EXPLAINED, NOT JUST ENFORCED. A slider that stops for no
    // stated reason reads as a broken slider; a builder who is told WHY it
    // stops keeps trusting the tool. This is the 2 m the terrain lattice can
    // hold, and no brush can be finer than the ground it paints on.
    if (terrain.radius_m <= BRUSH_MIN_RADIUS_M + 0.01f) {
        ImGui::TextDisabled("%s", EditorUi::tr("brush.size.floor"));
    }

    if (terrain.mode != BrushMode::Paint) {
        ImGui::SliderFloat(EditorUi::tr("brush.strength"), &terrain.strength_m_s, 0.1f,
                           10.0f, "%.2f m/s");
        ImGui::SliderFloat(EditorUi::tr("brush.hardness"), &terrain.hardness, 0.0f,
                           BRUSH_HARDNESS_MAX, "%.2f");
    }
    if (terrain.mode == BrushMode::Flatten) {
        ImGui::InputFloat(EditorUi::tr("brush.flatten.height"), &terrain.flatten_height_m,
                          0.25f, 1.0f, "%.2f m");
        // The flatten brush does not paint samples — it writes a [pad] into the
        // composition. Saying so here is the difference between a tool the
        // composer can reason about and one whose file surprises him later.
        ImGui::TextDisabled("%s", EditorUi::tr("brush.flatten.note"));
    }
    if (terrain.mode == BrushMode::Paint) {
        ImGui::Spacing();
        ImGui::TextUnformatted(EditorUi::tr("brush.surface"));
        for (const SurfaceRow& row : SURFACES) {
            // THE PICTURE BEFORE THE NAME. A picker that only names its classes
            // asks the builder to remember what «смесь» looks like; the swatch
            // is composed from the same cells and the same splat weights the
            // ground itself is drawn with, so what he picks is what he gets.
            if (swatches != nullptr) {
                if (const EditorTexture tex = swatches->surface(row.surface); tex != 0) {
                    EditorUi::image(tex, SWATCH_DRAW_PX, SWATCH_DRAW_PX);
                    ImGui::SameLine();
                }
            }
            if (ImGui::RadioButton(EditorUi::tr(row.key), terrain.paint == row.surface)) {
                terrain.paint = row.surface;
            }
        }
    }

    // WHAT THE LAST DAB ACTUALLY DID. A brush that has quietly stopped biting —
    // aimed past the world, or at a chunk that is not resident — looks exactly
    // like a brush working on ground the builder cannot see moving. The numbers
    // tell the two apart at a glance.
    if (hooks.last_dab) {
        int samples = 0;
        float worst = 0.0f;
        hooks.last_dab(samples, worst);
        ImGui::Spacing();
        if (samples > 0) {
            ImGui::Text("%s %d · %.3f m", EditorUi::tr("brush.lastdab"), samples,
                        static_cast<double>(worst));
        } else {
            ImGui::TextDisabled("%s", EditorUi::tr("brush.lastdab.none"));
        }
    }

    // ------------------------------------------------------------ vegetation
    ImGui::Spacing();
    ImGui::TextUnformatted(EditorUi::tr("brush.section.plants"));
    ImGui::Separator();

    ImGui::SliderFloat(EditorUi::tr("brush.plant.radius"), &plant.radius_m, 0.0f, 40.0f,
                       "%.1f m");
    ImGui::SliderInt(EditorUi::tr("brush.plant.count"), &plant.count, 1, 40);
    ImGui::SliderFloat(EditorUi::tr("brush.plant.spacing"), &plant.min_spacing_m, 0.0f,
                       10.0f, "%.2f m");
    ImGui::DragFloatRange2(EditorUi::tr("brush.plant.scale"), &plant.scale_min,
                           &plant.scale_max, 0.01f, 0.2f, 3.0f, "%.2f", "%.2f");
    ImGui::Checkbox(EditorUi::tr("brush.plant.yaw"), &plant.random_yaw);

    if (hooks.species) {
        const std::vector<std::string>& shelf = hooks.species();
        ImGui::Spacing();
        ImGui::Text("%s %zu", EditorUi::tr("brush.plant.species"), plant.species.size());
        // A DAB DRAWS FROM SEVERAL SPECIES ON PURPOSE: one object at one size
        // repeated eleven times reads as a texture, not as vegetation. So this
        // is a multi-select, and the count above says how many are armed.
        if (ImGui::BeginChild("brush.species", ImVec2(0.0f, 220.0f),
                              ImGuiChildFlags_Border)) {
            for (const std::string& name : shelf) {
                const auto at = std::find(plant.species.begin(), plant.species.end(), name);
                bool on = at != plant.species.end();
                if (ImGui::Checkbox(name.c_str(), &on)) {
                    if (on) {
                        plant.species.push_back(name);
                    } else {
                        plant.species.erase(at);
                    }
                }
            }
        }
        ImGui::EndChild();
        if (plant.species.empty()) {
            // An empty selection plants NOTHING, and silence there is exactly
            // what makes a tool feel broken. It says so instead.
            ImGui::TextDisabled("%s", EditorUi::tr("brush.plant.none"));
        }
    }
}

EditorPanel make_brush_panel(EditorUi& ui, TerrainBrush& terrain, PlantBrush& plant,
                             BrushHooks hooks, EditorPanelSide side) {
    EditorPanel panel;
    panel.id = "brush";
    panel.title_key = "editor.panel.brush";
    panel.side = side;
    panel.extent_px = 360.0f;
    // CLOSED AT STARTUP. A panel the builder has to ask for is a panel that is
    // not in his way while he is doing something else.
    panel.open = false;
    // THE CACHE LIVES AS LONG AS THE CALLBACK DOES, and dies with it: a panel
    // re-registered on a map change destroys the old lambda, and a swatch that
    // outlived it would be a texture leaked once per map. shared_ptr because
    // EditorPanel::draw is a std::function and must stay copyable.
    auto swatches = std::make_shared<BrushSwatches>(ui);
    panel.draw = [&terrain, &plant, hooks = std::move(hooks), swatches] {
        draw_brush_panel(terrain, plant, hooks, swatches.get());
    };
    return panel;
}

} // namespace dfn::app
