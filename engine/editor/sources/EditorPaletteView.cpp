/*
Created: 17:08:2026 - 19:22:11
Last updated: 17:08:2026 - 19:43:57
Module: engine/editor
File: engine/editor/sources/EditorPaletteView.cpp

Responsibility:
- The object menu's panel, declared in EditorPaletteView.h.

Dependencies:
- Uses: EditorPaletteView.h, EditorPalette.h, EditorUi.h, Dear ImGui.
- Used by: engine/app (App).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- NO DECISION LIVES HERE. Filtering, sorting, favourites and the parse are the
  model's; this file only asks and draws. If you are about to write an `if` that
  decides which parts to show, it belongs in PaletteModel where a test can see
  it.
- THE LIST IS CLIPPED, ALWAYS. ImGuiListClipper is not an optimisation here, it
  is the contract with the thumbnail hook: only visible rows may ask for a
  picture, and 2411 rows drawn eagerly would ask for 2411 offscreen renders.
*/
/*
UPD:
- 17:08:2026 - 19:22:11: Создан вместе с EditorPaletteView.h.
- 17:08:2026 - 19:22:54: Переезд в engine/editor. ARCHITECTURE.md разрешает Dear ImGui
  ТОЛЬКО в engine/editor, а слой editor не имеет права включать engine/app
  (LAYERS в tools/dag_check.py) — значит панель и её модель обязаны жить
  по одну сторону, и эта сторона — editor. Ни строки логики не тронуто.
- 17:08:2026 - 19:37:50: СТОЛКНОВЕНИЕ ID У ФИШЕК ФАСЕТОВ. ImGui берёт личность виджета из
  ПОДПИСИ, а разные группы предлагают одно слово: «brick» это и материал, и
  кладка, «door» — и семейство, и проём. Без PushID по группе две фишки были
  ОДНИМ виджетом, и нажатие любой отвечало за обе. Плюс полосы перешли на
  index_of вместо просмотра полки на каждый кадр.
- 17:08:2026 - 19:39:49: высота строки в стрижке МЕРЯЕТСЯ, а не вычисляется. Плитка это миниатюра
  плюс подпись плюс отступ рамки кнопки, а отступ живёт внутри
  EditorUi::image_button — то есть моя арифметика была копией чужого числа
  (правило 35) и разъехалась бы в день смены стиля, причём молча: список
  просто прокручивался бы не туда.
- 17:08:2026 - 19:43:57: ★ убрана из таблицы. Шрифт редактора грузит 0x0020-0x00FF и
  0x0400-0x045F, U+2605 не входит ни в один — отметка избранного рисовалась бы
  пустым квадратом. Найдено перебором ВСЕХ кодпойнтов таблицы по атласу
  (1012 штук, без глифа ровно один). И отметка выбранного перестала быть
  литералом в C++ — литерал не виден ни правилу 5, ни этой проверке.
*/

#include "engine/editor/sources/EditorPaletteView.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace dfn::app {
namespace {

/// Thumbnail edge in interface pixels before the Retina scale. Big enough that
/// a wall panel and a floor deck are told apart at a glance, small enough that
/// a 380 px column still fits three across.
constexpr float THUMB_PX = 96.0f;
constexpr float ROW_PX = 34.0f;

/// LOCALIZED, OR THE TOKEN ITSELF. tr() returns a loud "?0x...?" marker for a
/// missing key, which is right for interface text and wrong for a part's own
/// vocabulary: a material baked this morning has no row yet, and showing the
/// builder "?0x8fa1?" instead of "clay" tells him the editor is broken when the
/// only thing missing is a translation.
[[nodiscard]] const char* token_text(const char* prefix, const std::string& token) {
    if (token.empty()) {
        return "";
    }
    const std::string key = std::string(prefix) + token;
    const char* text = EditorUi::tr(key.c_str());
    return (text != nullptr && text[0] == '?') ? token.c_str() : text;
}

/// The angle step a connector's facet count allows (HOUSES.md §4). Round posts
/// take any angle; a missing facet count means the part is not a connector.
void faces_line(char* out, std::size_t cap, int faces) {
    if (faces < 0) {
        out[0] = '\0';
    } else if (faces == 0) {
        std::snprintf(out, cap, "%s", EditorUi::tr("palette.faces.any"));
    } else {
        std::snprintf(out, cap, "%s %d · %d°", EditorUi::tr("palette.faces.label"), faces,
                      360 / faces);
    }
}

/// The size a row shows. The NAME's number when it states one, otherwise the
/// MEASURED span — and the two are labelled differently, because "the kit says
/// 3.25 m" and "this mesh is 3.27 m across" are different claims and the
/// difference is where a kit defect shows.
void size_line(char* out, std::size_t cap, const PartFacets& f, const PartMeasure& m) {
    if (f.span_m > 0.0f) {
        std::snprintf(out, cap, "%.2f %s", static_cast<double>(f.span_m),
                      EditorUi::tr("palette.unit.m"));
    } else if (m.known) {
        const float span = std::max({m.width_m, m.depth_m, m.height_m});
        std::snprintf(out, cap, "~%.2f %s", static_cast<double>(span),
                      EditorUi::tr("palette.unit.m"));
    } else {
        std::snprintf(out, cap, "%s", EditorUi::tr("palette.size.unknown"));
    }
}

void draw_tooltip(const PartFacets& f, const PartMeasure& m) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        return;
    }
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(f.name.c_str());
    ImGui::Separator();
    if (!f.parsed) {
        ImGui::TextDisabled("%s", EditorUi::tr("palette.unparsed"));
        ImGui::EndTooltip();
        return;
    }
    ImGui::Text("%s · %s", token_text("palette.family.", f.family),
                token_text("palette.material.", f.material));
    if (!f.style.empty()) {
        ImGui::TextDisabled("%s", token_text("palette.style.", f.style));
    }
    if (f.wear_pct >= 0) {
        ImGui::Text("%s %d%%", EditorUi::tr("palette.tip.wear"), f.wear_pct);
    }
    char buf[128];
    faces_line(buf, sizeof(buf), f.faces);
    if (buf[0] != '\0') {
        ImGui::TextUnformatted(buf);
    }
    if (f.steps > 0) {
        ImGui::Text("%s %d", EditorUi::tr("palette.tip.steps"), f.steps);
    }
    for (const std::string& t : f.tags) {
        ImGui::TextDisabled("%s", token_text("palette.tag.", t));
    }
    // THE MEASURED BOX, IN METRES, from render::measure_object — the same ruler
    // the judge and the build ghost use, so what the tooltip promises is what
    // the ghost will occupy.
    if (m.known) {
        ImGui::Separator();
        ImGui::Text("%s %.2f × %.2f × %.2f %s", EditorUi::tr("palette.tip.measured"),
                    static_cast<double>(m.width_m), static_cast<double>(m.height_m),
                    static_cast<double>(m.depth_m), EditorUi::tr("palette.unit.m"));
        ImGui::TextDisabled("%s %d", EditorUi::tr("palette.tip.tris"), m.triangles);
    } else {
        ImGui::Separator();
        ImGui::TextDisabled("%s", EditorUi::tr("palette.tip.unmeasured"));
    }
    ImGui::EndTooltip();
}

/// One part, wherever it appears. Returns true when the builder picked it.
/// Right-click stars it — a menu that needs a mode switch to mark a favourite
/// is a menu whose favourites stay empty.
bool draw_part(PaletteModel& model, const PaletteHooks& hooks, std::size_t index,
               bool grid, float thumb) {
    const PartFacets& f = model.part(index);
    const PartMeasure& m = model.measure(index);
    const bool picked_now = model.selected() == f.name;
    const bool fav = model.is_favourite(f.name);

    ImGui::PushID(static_cast<int>(index));
    bool took = false;

    const EditorTexture tex = hooks.thumbnail ? hooks.thumbnail(f.name) : 0;
    if (grid) {
        ImGui::BeginGroup();
        if (tex != 0) {
            if (EditorUi::image_button("##thumb", tex, thumb, thumb)) {
                took = true;
            }
        } else {
            // NO PICTURE YET IS NOT AN EMPTY SLOT. A blank square reads as a
            // broken part; the family's own word reads as "still drawing".
            if (ImGui::Button(token_text("palette.family.", f.family), ImVec2(thumb, thumb))) {
                took = true;
            }
        }
    } else {
        if (tex != 0) {
            EditorUi::image(tex, ROW_PX, ROW_PX);
            ImGui::SameLine();
        }
        // THE NAME IS THE SELECTABLE'S OWN LABEL, not a separate Text after it.
        // Drawn separately, every IsItemHovered/IsItemClicked below would refer
        // to the LABEL rather than to the row — the tooltip and the right-click
        // star would answer for a strip of text a few pixels wide.
        if (ImGui::Selectable(f.name.c_str(), picked_now, 0, ImVec2(0.0f, ROW_PX))) {
            took = true;
        }
    }

    draw_tooltip(f, m);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        model.toggle_favourite(f.name);
    }
    // CTRL+1..9 OVER A ROW BINDS THE SLOT. The alternative — a dialog — costs
    // more attention than the feature saves.
    if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl) {
        for (int slot = 1; slot <= PALETTE_QUICK_SLOTS; ++slot) {
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + slot))) {
                model.set_quick_slot(slot, f.name);
            }
        }
    }

    if (grid) {
        if (picked_now) {
            ImGui::TextUnformatted(EditorUi::tr("palette.picked"));
            ImGui::SameLine();
        }
        if (fav) {
            ImGui::TextDisabled("%s", EditorUi::tr("palette.star"));
            ImGui::SameLine();
        }
        char buf[64];
        size_line(buf, sizeof(buf), f, m);
        ImGui::TextDisabled("%s", buf);
        ImGui::EndGroup();
    } else {
        char buf[64];
        size_line(buf, sizeof(buf), f, m);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", buf);
        // THE FACET COUNT RIDES IN THE ROW, not only in the filter (lead's
        // ruling): it is the angle the next wall may leave the post at, and a
        // builder chooses a post BY it.
        char faces[64];
        faces_line(faces, sizeof(faces), f.faces);
        if (faces[0] != '\0') {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", faces);
        }
    }

    ImGui::PopID();
    if (took) {
        model.select(f.name);
        if (hooks.on_pick) {
            hooks.on_pick(f.name);
        }
    }
    return took;
}

/// A short horizontal run of parts by NAME (favourites, recents, quick slots).
/// Names rather than indices because these lists outlive a shelf swap.
void draw_strip(PaletteModel& model, const PaletteHooks& hooks,
                const std::vector<std::string>& names, const char* empty_key) {
    if (names.empty()) {
        ImGui::TextDisabled("%s", EditorUi::tr(empty_key));
        return;
    }
    const float thumb = 44.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    float used = 0.0f;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const std::size_t at = model.index_of(names[i]);
        // A KEPT NAME THAT IS NO LONGER ON THE SHELF is shown greyed rather than
        // dropped: the builder swapped map or the kit was re-baked, and silently
        // losing his favourites would look like the editor forgetting.
        if (at == model.part_count()) {
            ImGui::TextDisabled("%s", names[i].c_str());
            continue;
        }
        if (used + thumb > avail && i > 0) {
            break;
        }
        used += thumb + ImGui::GetStyle().ItemSpacing.x;
        draw_part(model, hooks, at, /*grid=*/true, thumb);
        if (i + 1 < names.size()) {
            ImGui::SameLine();
        }
    }
    ImGui::NewLine();
}

void draw_facet_group(PaletteModel& model, FacetKind kind, const char* title_key,
                      const char* value_prefix) {
    const std::vector<FacetValue>& values = model.facet_values(kind);
    if (values.empty()) {
        return;
    }
    if (!ImGui::CollapsingHeader(EditorUi::tr(title_key))) {
        return;
    }
    // ONE ID SCOPE PER GROUP, and it is a correctness fix rather than tidiness:
    // ImGui derives a widget's identity from its LABEL, and two groups can offer
    // the same word — "brick" is both a material and a bond, "door" is both a
    // family and an opening. Without this, the two chips would be ONE widget and
    // ticking either would answer for both.
    ImGui::PushID(static_cast<int>(kind));
    const float avail = ImGui::GetContentRegionAvail().x;
    float used = 0.0f;
    for (const FacetValue& v : values) {
        char label[128];
        // THE COUNT IS ON THE CHIP, and it is the count that would REMAIN.
        // A chip reading 0 is a dead end and says so instead of being hidden:
        // a filter that quietly drops options teaches the builder that the
        // menu is unpredictable.
        std::snprintf(label, sizeof(label), "%s (%zu)",
                      value_prefix == nullptr ? v.value.c_str()
                                              : token_text(value_prefix, v.value),
                      v.count);
        const float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 4.0f;
        if (used > 0.0f && used + w < avail) {
            ImGui::SameLine();
        } else {
            used = 0.0f;
        }
        used += w + ImGui::GetStyle().ItemSpacing.x;
        bool on = v.on;
        if (ImGui::Checkbox(label, &on)) {
            model.set_facet(kind, v.value, on);
        }
    }
    ImGui::PopID();
}

} // namespace

// ---------------------------------------------------------------------------

void draw_parts_panel(PaletteModel& model, const PaletteHooks& hooks) {
    if (hooks.measure) {
        model.set_measure_source(hooks.measure);
    }

    // -- the search box, and it takes focus the frame the panel appears -------
    static char query[128] = {};
    static bool focus_next = true;
    if (ImGui::IsWindowAppearing()) {
        focus_next = true;
        // The box is re-seeded from the model, not the other way round: the
        // model is what survived the restart, and a search box that silently
        // disagreed with the list it filters is the worst kind of wrong.
        std::snprintf(query, sizeof(query), "%s", model.search().c_str());
    }
    if (focus_next) {
        ImGui::SetKeyboardFocusHere();
        focus_next = false;
    }
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("XXXX").x);
    if (ImGui::InputTextWithHint("##palette.search", EditorUi::tr("palette.search.hint"), query,
                                 sizeof(query))) {
        model.set_search(query);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(EditorUi::tr("palette.clear"))) {
        query[0] = '\0';
        model.set_search("");
        model.clear_facets();
        model.set_only_favourites(false);
    }

    // -- what the builder is holding -----------------------------------------
    if (!model.selected().empty()) {
        ImGui::Text("%s %s", EditorUi::tr("palette.selected"), model.selected().c_str());
    } else {
        ImGui::TextDisabled("%s", EditorUi::tr("palette.nothing_picked"));
    }

    // -- the count, and the honest empty state --------------------------------
    ImGui::Text("%s %zu / %zu", EditorUi::tr("palette.found"), model.result_count(),
                model.part_count());
    if (model.empty_result()) {
        // A LIST THAT IS EMPTY WITHOUT A SENTENCE READS AS A BROKEN MENU. The
        // builder starts clicking to find out what he did wrong, and what he
        // did wrong was type one letter too many.
        ImGui::TextWrapped("%s", EditorUi::tr("palette.none"));
    }

    // -- the kept lists, first because they are what a session actually uses --
    if (ImGui::CollapsingHeader(EditorUi::tr("palette.favourites"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_strip(model, hooks, model.favourites(), "palette.favourites.empty");
        bool only = model.only_favourites();
        if (ImGui::Checkbox(EditorUi::tr("palette.only_fav"), &only)) {
            model.set_only_favourites(only);
        }
    }
    if (ImGui::CollapsingHeader(EditorUi::tr("palette.recents"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_strip(model, hooks, model.recents(), "palette.recents.empty");
    }

    // -- sort and view --------------------------------------------------------
    static const char* const SORT_KEYS[] = {"palette.sort.name", "palette.sort.size",
                                            "palette.sort.size_desc", "palette.sort.recent"};
    int sort_at = static_cast<int>(model.sort());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    if (ImGui::BeginCombo("##palette.sort", EditorUi::tr(SORT_KEYS[sort_at]))) {
        for (int i = 0; i < 4; ++i) {
            if (ImGui::Selectable(EditorUi::tr(SORT_KEYS[i]), sort_at == i)) {
                model.set_sort(static_cast<PaletteSort>(i));
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const bool grid = model.view() == PaletteView::Grid;
    if (ImGui::SmallButton(EditorUi::tr(grid ? "palette.view.list" : "palette.view.grid"))) {
        model.set_view(grid ? PaletteView::List : PaletteView::Grid);
    }

    // -- the facets -----------------------------------------------------------
    draw_facet_group(model, FacetKind::Family, "palette.facet.family", "palette.family.");
    draw_facet_group(model, FacetKind::Material, "palette.facet.material", "palette.material.");
    draw_facet_group(model, FacetKind::Style, "palette.facet.style", "palette.style.");
    draw_facet_group(model, FacetKind::Wear, "palette.facet.wear", "palette.wear.");
    draw_facet_group(model, FacetKind::Faces, "palette.facet.faces", "palette.faces.");
    draw_facet_group(model, FacetKind::Tag, "palette.facet.tag", "palette.tag.");

    // -- the list -------------------------------------------------------------
    ImGui::Separator();
    const std::vector<std::size_t>& rows = model.results();
    const float thumb = THUMB_PX;
    ImGui::BeginChild("##palette.rows", ImVec2(0.0f, 0.0f));
    if (model.view() == PaletteView::Grid) {
        const float step = thumb + ImGui::GetStyle().ItemSpacing.x;
        const int columns =
            std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / step));
        const int lines = static_cast<int>((rows.size() + columns - 1) / columns);
        ImGuiListClipper clipper;
        // THE HEIGHT IS MEASURED, NOT COMPUTED (the -1 default). A tile is a
        // thumbnail plus its caption plus a button's frame padding, and that
        // padding lives inside EditorUi::image_button where this file cannot
        // see it — so any arithmetic here would be a copy of a number owned by
        // another module, wrong the day the style changes and wrong silently
        // (the list would simply scroll to the wrong place). Let ImGui measure.
        clipper.Begin(lines);
        while (clipper.Step()) {
            for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line) {
                for (int c = 0; c < columns; ++c) {
                    const std::size_t at =
                        static_cast<std::size_t>(line) * static_cast<std::size_t>(columns) +
                        static_cast<std::size_t>(c);
                    if (at >= rows.size()) {
                        break;
                    }
                    if (c > 0) {
                        ImGui::SameLine();
                    }
                    draw_part(model, hooks, rows[at], /*grid=*/true, thumb);
                }
            }
        }
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size())); // measured, same reason
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                draw_part(model, hooks, rows[static_cast<std::size_t>(i)], /*grid=*/false,
                          thumb);
            }
        }
    }
    ImGui::EndChild();
}

EditorPanel make_parts_panel(PaletteModel& model, PaletteHooks hooks, EditorPanelSide side) {
    EditorPanel panel;
    panel.id = PALETTE_PANEL_ID;
    panel.title_key = "editor.panel.parts";
    panel.side = side;
    panel.extent_px = 420.0f;
    // CLOSED AT STARTUP: the user asked to open it with a key and stand still
    // while it is up. A menu that is already covering a third of the world when
    // the editor opens is a menu he closes before he ever uses it.
    panel.open = false;
    panel.draw = [&model, hooks]() { draw_parts_panel(model, hooks); };
    return panel;
}

} // namespace dfn::app
