/*
Created: 18:08:2026 - 12:06:30
Last updated: 18:08:2026 - 20:26:30
Module: engine/editor
File: engine/editor/sources/EditorToolsGround.cpp

Responsibility:
- The three tools that work on the ground: the height brush, the surface brush
  and PLANTING. Their click, their preview and their own settings.

Dependencies:
- Uses: EditorToolsBuiltin.h, EditorBrush.h (the mechanics and the outline),
  EditorBrushView.h (BrushSwatches — the pictures of the four surfaces),
  EditorUi.h (tr, image), Dear ImGui.
- Used by: engine/app (constructs them).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE BRUSH BELONGS TO THE TOOL. Do not reintroduce a shared TerrainBrush with
  a mode: two tools that can set each other's state is exactly the drift the
  audit measured.
*/
/*
UPD:
- 18:08:2026 - 12:06:30: Создан — кисть высоты, кисть поверхности и посадка как ТРИ
  инструмента с собственными настройками. Посадка перестала быть Shift-щелчком
  внутри чужого обработчика.
- 18:08:2026 - 13:08:07: draw_last_dab — числа последнего мазка в настройках обеих кистей
  (счётчик вернулся из удалённой панели кисти).
- 18:08:2026 - 20:26:30: То же для кисти и посадки.
*/

#include "engine/editor/sources/EditorToolsBuiltin.h"

#include "engine/editor/sources/EditorBrushView.h"
#include "engine/editor/sources/EditorUi.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace dfn::app {
namespace {

struct ModeRow {
    BrushMode mode;
    const char* key;
};
/// THE HEIGHT BRUSH'S MODES, and Paint is NOT among them. That absence is the
/// fix: the mode used to be settable from the panel while the bar claimed a
/// different tool, so «Поверхность» on the bar and «Поднять» in the panel could
/// both be true at once.
constexpr ModeRow HEIGHT_MODES[] = {
    {BrushMode::Raise, "brush.mode.raise"},
    {BrushMode::Lower, "brush.mode.lower"},
    {BrushMode::Smooth, "brush.mode.smooth"},
    {BrushMode::Flatten, "brush.mode.flatten"},
};

struct SurfaceRow {
    math::SurfaceClass surface;
    const char* key;
};
constexpr SurfaceRow SURFACES[] = {
    {math::SurfaceClass::Grass, "brush.surface.grass"},
    {math::SurfaceClass::GrassRockBlend, "brush.surface.blend"},
    {math::SurfaceClass::Rock, "brush.surface.rock"},
    {math::SurfaceClass::Sand, "brush.surface.sand"},
};
constexpr float SWATCH_DRAW_PX = 24.0f;

} // namespace

// ============================ THE GROUND BRUSHES ============================

void TerrainBrushToolBase::on_press(const ToolAim& aim, ToolWorld& world) {
    stroking_ = true;
    pad_written_ = false;
    on_drag(aim, 0.0f, world);
}

void TerrainBrushToolBase::on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) {
    const glm::vec2 centre{aim.point.x, aim.point.z};
    if (brush_.mode == BrushMode::Flatten) {
        // ONE PAD PER STROKE, not one per frame the button is held. A pad is a
        // STATEMENT the composer can move and re-read, and sixty of them a
        // second would bury the file he has to live in.
        if (!pad_written_ && world.add_pad) {
            pad_written_ = true;
            world.add_pad(flatten_pad(brush_, centre));
        }
        return;
    }
    if (world.terrain_dab) {
        (void)world.terrain_dab(brush_, centre, dt_s);
    }
}

void TerrainBrushToolBase::on_release(ToolWorld& world) {
    stroking_ = false;
    pad_written_ = false;
    if (world.finish_stroke) {
        world.finish_stroke();
    }
}

void TerrainBrushToolBase::on_deselected(ToolWorld& world) {
    // A HALF-DUG STROKE DIES WITH THE TOOL. It used to survive the switch and
    // bite the ground the next tool aimed at.
    if (stroking_) {
        on_release(world);
    }
}

void TerrainBrushToolBase::draw_last_dab() const {
    if (world_ == nullptr || !world_->last_dab) {
        return;
    }
    int samples = 0;
    float worst = 0.0f;
    world_->last_dab(samples, worst);
    ImGui::Spacing();
    if (samples > 0) {
        ImGui::Text("%s %d · %.3f m", EditorUi::tr("brush.lastdab"), samples,
                    static_cast<double>(worst));
    } else {
        // МОЛЧАНИЕ ЗДЕСЬ НЕОТЛИЧИМО ОТ ПОЛОМКИ. Кисть, наведённая за
        // подгруженное кольцо, не двигает ни отсчёта и выглядит ровно как
        // кисть, наведённая не туда, — эта строка их и различает.
        ImGui::TextDisabled("%s", EditorUi::tr("brush.lastdab.none"));
    }
}

ToolPreview TerrainBrushToolBase::preview(const ToolAim& aim) const {
    // ЗА ПРЕДЕЛОМ ДАЛЬНОСТИ ЭТОТ ИНСТРУМЕНТ НЕ РИСУЕТ НИЧЕГО: вся его картинка —
    // обещание щелчка, а щелчок туда не достанет.
    if (!aim.in_reach) {
        return ToolPreview{};
    }

    (void)aim;
    ToolPreview out;
    out.ring_brush = &brush_;
    return out;
}

HeightBrushTool::HeightBrushTool() {
    brush_.mode = BrushMode::Raise;
}

ToolIdentity HeightBrushTool::identity() const {
    return ToolIdentity{"height", "editor.tool.height", "tool.hint.height",
                        ToolIcon::Height};
}

void HeightBrushTool::draw_settings() {
    ImGui::TextUnformatted(EditorUi::tr("brush.section.ground"));
    ImGui::Separator();
    for (const ModeRow& row : HEIGHT_MODES) {
        if (ImGui::RadioButton(EditorUi::tr(row.key), brush_.mode == row.mode)) {
            brush_.mode = row.mode;
        }
    }
    ImGui::Spacing();
    ImGui::SliderFloat(EditorUi::tr("brush.size"), &brush_.radius_m, BRUSH_MIN_RADIUS_M,
                       64.0f, "%.1f m");
    if (brush_.radius_m <= BRUSH_MIN_RADIUS_M + 0.01f) {
        ImGui::TextDisabled("%s", EditorUi::tr("brush.size.floor"));
    }
    ImGui::SliderFloat(EditorUi::tr("brush.strength"), &brush_.strength_m_s, 0.1f, 10.0f,
                       "%.2f m/s");
    ImGui::SliderFloat(EditorUi::tr("brush.hardness"), &brush_.hardness, 0.0f,
                       BRUSH_HARDNESS_MAX, "%.2f");
    if (brush_.mode == BrushMode::Flatten) {
        ImGui::InputFloat(EditorUi::tr("brush.flatten.height"), &brush_.flatten_height_m,
                          0.25f, 1.0f, "%.2f m");
        ImGui::TextDisabled("%s", EditorUi::tr("brush.flatten.note"));
    }
    draw_last_dab();
}

SurfacePaintTool::SurfacePaintTool(EditorUi& ui)
    : swatches_(std::make_unique<BrushSwatches>(ui)) {
    // PERMANENTLY PAINT. Not "set on selection" — set here, once, and never
    // written again: a mode that is assigned when the tool is picked up is a
    // mode somebody else can assign in between.
    brush_.mode = BrushMode::Paint;
}

SurfacePaintTool::~SurfacePaintTool() = default;

ToolIdentity SurfacePaintTool::identity() const {
    return ToolIdentity{"surface", "editor.tool.paint", "tool.hint.paint",
                        ToolIcon::Surface};
}

void SurfacePaintTool::draw_settings() {
    ImGui::SliderFloat(EditorUi::tr("brush.size"), &brush_.radius_m, BRUSH_MIN_RADIUS_M,
                       64.0f, "%.1f m");
    if (brush_.radius_m <= BRUSH_MIN_RADIUS_M + 0.01f) {
        ImGui::TextDisabled("%s", EditorUi::tr("brush.size.floor"));
    }
    ImGui::Spacing();
    ImGui::TextUnformatted(EditorUi::tr("brush.surface"));
    for (const SurfaceRow& row : SURFACES) {
        // THE PICTURE BEFORE THE NAME, composed from the same cells and splat
        // weights the ground is drawn with.
        if (swatches_ != nullptr) {
            if (const EditorTexture tex = swatches_->surface(row.surface); tex != 0) {
                EditorUi::image(tex, SWATCH_DRAW_PX, SWATCH_DRAW_PX);
                ImGui::SameLine();
            }
        }
        if (ImGui::RadioButton(EditorUi::tr(row.key), brush_.paint == row.surface)) {
            brush_.paint = row.surface;
        }
    }
    draw_last_dab();
}

// ================================ PLANTING ==================================

PlantTool::PlantTool(SpeciesSource species) : species_(std::move(species)) {
    plant_.species.clear();
    ring_.mode = BrushMode::Paint; // no digging: the ring is a footprint, not a dig
    ring_.hardness = BRUSH_HARDNESS_MAX;
}

ToolIdentity PlantTool::identity() const {
    return ToolIdentity{"plant", "editor.tool.plant", "tool.hint.plant", ToolIcon::Plant};
}

void PlantTool::on_press(const ToolAim& aim, ToolWorld& world) {
    if (!world.plant_dab) {
        return;
    }
    ensure_default_species();
    last_planted_ = world.plant_dab(plant_, {aim.point.x, aim.point.z});
}

ToolPreview PlantTool::preview(const ToolAim& aim) const {
    // ЗА ПРЕДЕЛОМ ДАЛЬНОСТИ ЭТОТ ИНСТРУМЕНТ НЕ РИСУЕТ НИЧЕГО: вся его картинка —
    // обещание щелчка, а щелчок туда не достанет.
    if (!aim.in_reach) {
        return ToolPreview{};
    }

    (void)aim;
    ToolPreview out;
    // THE SAME OUTLINE CODE draws this ring; only the radius is the plant
    // brush's own. A second ring drawer would drift from the brush's.
    ring_.radius_m = std::max(plant_.radius_m, BRUSH_MIN_RADIUS_M);
    out.ring_brush = &ring_;
    return out;
}

/// ПЕРВАЯ ПОРОДА ОТМЕЧАЕТСЯ САМА, если человек не выбрал ни одной.
///
/// Пользователь 18.08: «инструмент посадки не работает». И он был прав: список
/// пород начинался ПУСТЫМ, посадка честно отказывалась, подпись честно
/// говорила почему — а инструмент из коробки не делал НИЧЕГО. Отказ был
/// правильным по механике и неправильным по смыслу: инструмент, который надо
/// сначала настроить, чтобы он вообще что-то умел, читается как сломанный, и
/// прочитан он был именно так.
///
/// Умолчание, а не выбор за человека: список открыт соседней кнопкой, галочки
/// снимаются, и первый же его заход в настройки всё переопределит. Ставится
/// ЛЕНИВО, при первой попытке посадить, а не в конструкторе — полка читается с
/// диска и на момент рождения инструмента может быть ещё не прочитана.
void PlantTool::ensure_default_species() {
    if (!plant_.species.empty() || !species_) {
        return;
    }
    const std::vector<std::string>& shelf = species_();
    if (shelf.empty()) {
        return;
    }
    plant_.species.push_back(shelf.front());
    std::fprintf(stderr, "[посадка] порода не была выбрана — беру первую с полки: %s\n",
                 shelf.front().c_str());
}

ToolStatus PlantTool::status(const ToolAim& aim) const {
    (void)aim;
    if (plant_.species.empty()) {
        // WHY NOTHING WOULD APPEAR, before the click rather than after it. This
        // is the exact question the user asked — «что за порода выбирается... она
        // ни на что не влияет» — and now the answer is on screen.
        return ToolStatus{"brush.plant.none", {}, false};
    }
    return ToolStatus{identity().hint_key, {}, true};
}

void PlantTool::draw_settings() {
    ImGui::SliderFloat(EditorUi::tr("brush.plant.radius"), &plant_.radius_m, 0.0f, 40.0f,
                       "%.1f m");
    ImGui::SliderInt(EditorUi::tr("brush.plant.count"), &plant_.count, 1, 40);
    ImGui::SliderFloat(EditorUi::tr("brush.plant.spacing"), &plant_.min_spacing_m, 0.0f,
                       10.0f, "%.2f m");
    ImGui::DragFloatRange2(EditorUi::tr("brush.plant.scale"), &plant_.scale_min,
                           &plant_.scale_max, 0.01f, 0.2f, 3.0f, "%.2f", "%.2f");
    ImGui::Checkbox(EditorUi::tr("brush.plant.yaw"), &plant_.random_yaw);
    if (!species_) {
        return;
    }
    const std::vector<std::string>& shelf = species_();
    ImGui::Spacing();
    ImGui::Text("%s %zu", EditorUi::tr("brush.plant.species"), plant_.species.size());
    if (ImGui::BeginChild("plant.species", ImVec2(0.0f, 260.0f), ImGuiChildFlags_Border)) {
        for (const std::string& name : shelf) {
            const auto at = std::find(plant_.species.begin(), plant_.species.end(), name);
            bool on = at != plant_.species.end();
            if (ImGui::Checkbox(name.c_str(), &on)) {
                if (on) {
                    plant_.species.push_back(name);
                } else {
                    plant_.species.erase(at);
                }
            }
        }
    }
    ImGui::EndChild();
    if (plant_.species.empty()) {
        ImGui::TextDisabled("%s", EditorUi::tr("brush.plant.none"));
    }
    if (last_planted_ > 0) {
        ImGui::TextDisabled("%s %d", EditorUi::tr("brush.plant.last"), last_planted_);
    }
}

} // namespace dfn::app
