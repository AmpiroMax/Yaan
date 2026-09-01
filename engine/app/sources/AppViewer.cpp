/*
Module: engine/app
File: engine/app/sources/AppViewer.cpp

Responsibility:
- THE VIEWING STAND, wired: the half of the mode that needs an open map and a
  renderer. Which model is on the pedestal, how it got there (a shelf .dfo, a
  glTF, or a downloaded file converted and cached), and the orbiting eye.

Key items:
- App::viewer_enter() / viewer_leave(): the mode, opened by the map itself.
- App::viewer_show(): convert if needed, read, bake, upload — ONE scatter key,
  so the previous model's meshes are destroyed as the next are created.
- App::viewer_mouse() / on_viewer_cycle / on_viewer_turn / on_viewer_reset.
- App::viewer_camera_pose(): the frame's eye; App::viewer_draw(): the captions.

Dependencies:
- Uses: ModelViewer.h (every decision), ModelConvert.h (the cache), engine/render
  (ObjectRegistry, RenderSystem, PixelCanvas), UiFont, Localization, AppDoors.
- Used by: App.cpp (camera, HUD, doses), AppWorld.cpp (entering a map),
  AppInput.cpp (the three keys).

Notes:
- WHY THE MODEL IS BAKED INTO THE WORLD AND NOT DRAWN WITH A MATRIX. The
  registry's own road into a frame is upload_prebuilt_scatter: two streams,
  the same programs, the same leaf atlas, the same wind and the same light as
  every tree in the world. A second, viewer-only draw path would show a model
  the game does not draw — which is exactly the question this mode exists to
  answer, so answering it with a special case would answer nothing.
- AND THAT IS WHY SWITCHING CANNOT LEAK. upload_prebuilt_scatter drops the key
  before it uploads (RenderSystem.cpp), so create and destroy come in pairs by
  construction rather than by a rule somebody has to remember. The key is its
  own: not a chunk coordinate (the streamer would drop it) and not in the scene
  tiles' 1000+ band (a composition would collide with it).
- THE SKINNED MODELS ARE SHOWN IN THEIR REST POSE, and rest is the bind pose:
  the SKIN stream's vertices ARE the model with an identity palette. So there
  is no rig, no clip and no tick here — which is also why this file does not
  touch SkinnedCharacter.cpp at all.
- BLENDER WAS ORDERED AND BLENDER IS NOT INSTALLED. See ModelConvert.h: STL is
  read directly (deterministic, no install, cannot half-succeed) and glTF goes
  through our own dfn_import_gltf. Reported in the wave's report.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- User-facing text is a localization key (Rule 5). File names, category folders
  and SOURCE.txt lines are CONTENT read from the tree and printed verbatim.
- Every decision that can be wrong on its own belongs in ModelViewer.h, where a
  test can reach it. What stays here is what needs the window.
*/

#include "engine/app/sources/App.h"
#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/Localization.h"
#include "engine/app/sources/ModelConvert.h"
#include "engine/app/sources/UiFont.h"

#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace dfn::app {

namespace {

namespace fs = std::filesystem;

/// WHERE THE PEDESTAL'S MESH LIVES IN THE SCATTER MAP. Its own key, far from
/// both the streamer's chunk coordinates (small integers) and the composition
/// tiles (SCENE_TILE_KEY_BASE + i). A key shared with either of them would be
/// dropped by somebody else's bookkeeping, and the model would vanish for a
/// reason nothing in this file could explain.
constexpr glm::ivec2 VIEWER_MESH_KEY{2000, 2000};

/// ONE STEP OF Q/E, radians. Fifteen degrees: twenty-four steps make a full
/// turn, which is enough to bring any side of a model to the light and few
/// enough that a full turn is a second of tapping rather than a minute.
constexpr float VIEWER_TURN_STEP = 0.2617994f;

constexpr render::Color VIEWER_INK{234, 228, 208};
constexpr render::Color VIEWER_DIM{178, 172, 156};
constexpr render::Color VIEWER_BAD{224, 138, 118};

/// The three source words, as localization keys.
[[nodiscard]] std::uint64_t source_key(ViewerSource s) {
    switch (s) {
    case ViewerSource::Character: return serialization::fnv1a64("viewer.source.character");
    case ViewerSource::Downloaded: return serialization::fnv1a64("viewer.source.downloaded");
    case ViewerSource::Shelf:
    case ViewerSource::Count: break;
    }
    return serialization::fnv1a64("viewer.source.shelf");
}

/// ГЛИНА СТЕНДА: чем красится модель, у которой в этом проходе нет своей
/// текстуры. Скин .dfo несёт БЕЛЫЙ цвет вершин — его альбедо живёт в текстуре,
/// а «prop», которым рисуется постамент, слота под неё не имеет. Белое альбедо
/// плюс лампы стенда плюс солнце дают ровно то, что показал первый приёмочный
/// кадр: белый силуэт без единой складки. Серая глина — то, чем скульптуру и
/// показывают: она оставляет место затенению, а затенение и есть форма.
constexpr glm::vec3 STAND_CLAY{0.60f, 0.58f, 0.55f};

/// SKINNED VERTICES INTO PLAIN ONES. The bind pose is the rest pose, so the
/// positions and normals carry over untouched and only the four palette slots
/// are dropped — they address a palette this draw does not have.
void append_skin(render::MeshData& dst, const render::SkinMesh& skin) {
    const auto base = static_cast<std::uint32_t>(dst.vertices.size());
    const std::uint32_t clay = render::pack(STAND_CLAY);
    dst.vertices.reserve(dst.vertices.size() + skin.vertices.size());
    for (const platform::SkinnedVertex& v : skin.vertices) {
        dst.vertices.push_back({v.position, v.normal, v.uv, clay});
    }
    dst.indices.reserve(dst.indices.size() + skin.indices.size());
    for (const std::uint32_t i : skin.indices) {
        dst.indices.push_back(base + i);
    }
}

/// THE BOUND OF EVERYTHING THE OBJECT WILL DRAW. Not render::measure_object:
/// that one answers "how much room does this take in a composition" and
/// deliberately ignores the streams a placer must not trip over. The stand
/// frames what is DRAWN, which includes the crown, the baked house pieces and
/// the skin.
void grow_bound(glm::vec3& lo, glm::vec3& hi, const render::MeshData& m) {
    for (const platform::Vertex& v : m.vertices) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
}

[[nodiscard]] std::string two_decimals(float v) {
    char buf[32] = {};
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return buf;
}

} // namespace

void App::viewer_enter() {
    viewer_mode_ = true;
    viewer_items_ = scan_viewer_items(ViewerRoots{});
    const ViewerTally tally = viewer_tally(viewer_items_);
    std::fprintf(stderr,
                 "[смотровая] моделей %d: полки %d, персонажи (glTF) %d, "
                 "скачано %d\n",
                 tally.total(), tally.shelf, tally.character, tally.downloaded);

    // WHERE THE EXHIBIT STANDS: the middle of the map, on the ground the
    // streamer built. Asked of the streamer rather than written into the
    // scene, for the same reason the composition's spawn is: a pad that is
    // re-cut must not leave the pedestal buried or floating.
    const float mid = static_cast<float>(config::CHUNK_SIZE) * 0.5f;
    const float ground = chunks_.height_at({mid, mid}).value_or(0.0f);
    viewer_pad_ = {mid, ground, mid};

    viewer_gltf_tool_ = find_gltf_tool(fs::path()).string();
    if (viewer_gltf_tool_.empty()) {
        std::fprintf(stderr, "[смотровая] dfn_import_gltf не найден: .glb/.gltf "
                             "показаны не будут (STL — будут)\n");
    }

    int start = 0;
    if (const char* want = door_value("DFN_VIEWER_ITEM");
        want != nullptr && want[0] != '\0') {
        const int found = viewer_find_item(viewer_items_, want);
        if (found < 0) {
            // LOUD, AND NO SUBSTITUTE. A named door that silently showed model
            // zero would hand back an acceptance frame that is plausible and
            // not the one asked for (the same ruling DFN_POSE took).
            std::fprintf(stderr, "[смотровая] DFN_VIEWER_ITEM=%s — такой модели "
                                 "нет среди %d\n", want, tally.total());
        } else {
            start = found;
        }
    }
    viewer_show(start);
}

void App::viewer_leave() {
    if (!viewer_mode_) {
        return;
    }
    if (renderer_ != nullptr) {
        render_system_.drop_scatter(*renderer_, VIEWER_MESH_KEY);
    }
    viewer_mode_ = false;
    viewer_items_.clear();
    viewer_index_ = 0;
    viewer_error_.clear();
    viewer_triangles_ = 0;
}

void App::viewer_show(int index) {
    if (renderer_ == nullptr) {
        return;
    }
    if (viewer_items_.empty()) {
        render_system_.drop_scatter(*renderer_, VIEWER_MESH_KEY);
        viewer_error_ = std::string(localized(serialization::fnv1a64("viewer.empty")));
        return;
    }
    const int n = static_cast<int>(viewer_items_.size());
    viewer_index_ = ((index % n) + n) % n;
    const ViewerItem& item = viewer_items_[static_cast<std::size_t>(viewer_index_)];
    viewer_error_.clear();
    viewer_triangles_ = 0;
    ++viewer_swaps_;

    const ConvertResult conv = convert_model(item.path, fs::path(ViewerRoots{}.downloads),
                                             fs::path(viewer_gltf_tool_));
    if (conv.dfo.empty()) {
        // THE PEDESTAL IS CLEARED, not left holding the previous model: a
        // caption that says "не показано" over a model that IS shown is the
        // worst of the three possible frames.
        render_system_.drop_scatter(*renderer_, VIEWER_MESH_KEY);
        viewer_error_ = conv.reason;
        std::fprintf(stderr, "[смотровая] %s: %s\n", item.path.c_str(),
                     conv.reason.c_str());
        return;
    }
    const auto obj = render::read_object(conv.dfo);
    if (!obj) {
        render_system_.drop_scatter(*renderer_, VIEWER_MESH_KEY);
        viewer_error_ = ".dfo отвергнут";
        std::fprintf(stderr, "[смотровая] %s отвергнут (см. [dfo] выше)\n",
                     conv.dfo.string().c_str());
        return;
    }

    // The bound FIRST, in the model's own space: the display scale and the
    // framing both come out of it, and the bake needs both.
    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
    grow_bound(lo, hi, obj->wood);
    grow_bound(lo, hi, obj->cards);
    grow_bound(lo, hi, obj->bark);
    grow_bound(lo, hi, obj->ground);
    for (const render::HouseSubmesh& hs : obj->house) {
        grow_bound(lo, hi, hs.mesh);
    }
    render::MeshData skin;
    if (!obj->skin.empty()) {
        append_skin(skin, obj->skin);
        grow_bound(lo, hi, skin);
    }
    if (hi.x < lo.x) {
        render_system_.drop_scatter(*renderer_, VIEWER_MESH_KEY);
        viewer_error_ = "в файле нет ни одного треугольника";
        return;
    }
    viewer_lo_ = lo;
    viewer_hi_ = hi;
    viewer_scale_ = viewer_display_scale(item.source, lo, hi);

    // STANDING ON THE PAD, not hanging from its origin: some of this tree is
    // modelled from the feet (trees, kit parts) and some from the middle
    // (imported figures), and only the measured bottom reconciles the two.
    const glm::vec3 at{viewer_pad_.x, viewer_pad_.y - lo.y * viewer_scale_,
                       viewer_pad_.z};
    const float yaw = viewer_view_.model_yaw;
    render::MeshData wood;
    render::MeshData cards;
    render::append_transformed(wood, obj->wood, at, yaw, viewer_scale_);
    render::append_transformed(wood, skin, at, yaw, viewer_scale_);
    for (const render::HouseSubmesh& hs : obj->house) {
        // Baked building pieces ride the plain-wood program here rather than
        // the kit's tiled one: the stand shows the SHAPE of an object, and the
        // house road would need a whole interior's material state to draw one
        // bed on an empty pad.
        render::append_transformed(wood, hs.mesh, at, yaw, viewer_scale_);
    }
    render::append_transformed(cards, obj->cards, at, yaw, viewer_scale_);
    render::append_transformed(cards, obj->bark, at, yaw, viewer_scale_);
    render::append_transformed(cards, obj->ground, at, yaw, viewer_scale_);
    viewer_triangles_ = wood.triangle_count() + cards.triangle_count();

    // ONE KEY, AND THE UPLOAD DROPS IT FIRST. That is what makes switching a
    // pair of create/destroy rather than a slow leak (RenderSystem.cpp:1731).
    render_system_.upload_prebuilt_scatter(*renderer_, VIEWER_MESH_KEY, wood, cards);

    // The framing is re-derived for every model; the ORBIT ANGLES are kept, so
    // walking the shelf shows every model from the same portrait pose and the
    // frames of two models are comparable.
    const float keep_yaw = viewer_view_.orbit_yaw;
    const float keep_pitch = viewer_view_.orbit_pitch;
    const float keep_model_yaw = viewer_view_.model_yaw;
    viewer_view_ = viewer_reset(item.source, lo, hi, camera_.fov_y(),
                                camera_.aspect_ratio());
    viewer_view_.orbit_yaw = keep_yaw;
    viewer_view_.orbit_pitch = keep_pitch;
    viewer_view_.model_yaw = keep_model_yaw;

    const ViewerSize size = viewer_size(item.source, lo, hi);
    std::fprintf(stderr,
                 "[смотровая] %d/%d %s (%s): %zu тр., %.2f x %.2f x %.2f м, "
                 "масштаб %.3f%s\n",
                 viewer_index_ + 1, n, item.name.c_str(), item.category.c_str(),
                 viewer_triangles_, static_cast<double>(size.width_m),
                 static_cast<double>(size.height_m), static_cast<double>(size.depth_m),
                 static_cast<double>(size.scale),
                 conv.from_cache ? "" : " (преобразовано)");
}

void App::viewer_mouse() {
    if (!viewer_mode_ || input_ == nullptr) {
        return;
    }
    const glm::vec2 d = input_->mouse_delta();
    viewer_orbit(viewer_view_, d.x, d.y, static_cast<float>(config::MOUSE_SENSITIVITY));
    // THE WHEEL IS THE ZOOM, and it is the only consumer of the wheel in this
    // mode: the editor's reach and the carried prop's distance both belong to
    // hands the stand does not have.
    viewer_zoom(viewer_view_, input_->scroll_delta().y);
}

bool App::viewer_camera_pose(components::CameraPose& out) const {
    if (!viewer_mode_) {
        return false;
    }
    const glm::vec3 target = viewer_target(viewer_pad_, viewer_lo_, viewer_hi_,
                                           viewer_scale_);
    out.position = viewer_eye(target, viewer_view_);
    out.yaw = viewer_view_.orbit_yaw;
    out.pitch = viewer_view_.orbit_pitch;
    out.fov_scale = 1.0f;
    return true;
}

void App::on_viewer_cycle() {
    if (!viewer_mode_) {
        return;
    }
    // WHICH ARROW WAS PRESSED IS ASKED OF THE KEYBOARD, not carried by the
    // action: the row binds a PAIR (Controls.cpp), exactly as the pose cycle
    // does, and the modifier is a condition rather than a third row.
    const bool back = input_ != nullptr && input_->was_pressed(platform::Key::LEFT);
    const bool shift = input_ != nullptr
                       && (input_->is_down(platform::Key::LEFT_SHIFT)
                           || input_->is_down(platform::Key::RIGHT_SHIFT));
    viewer_show(viewer_step_index(viewer_items_, viewer_index_, back ? -1 : 1, shift));
}

void App::on_viewer_turn() {
    if (!viewer_mode_) {
        return;
    }
    const bool back = input_ != nullptr && input_->was_pressed(platform::Key::Q);
    viewer_view_.model_yaw += back ? -VIEWER_TURN_STEP : VIEWER_TURN_STEP;
    // The turn is baked into the vertices, so it is a re-show of the same
    // index rather than a camera change. One re-bake per press, never per
    // frame — see Controls.h at Action::ViewerTurn.
    viewer_show(viewer_index_);
}

void App::on_viewer_reset() {
    if (!viewer_mode_) {
        return;
    }
    const float keep = viewer_view_.model_yaw;
    viewer_view_ = viewer_reset(viewer_items_[static_cast<std::size_t>(viewer_index_)].source,
                                viewer_lo_, viewer_hi_, camera_.fov_y(),
                                camera_.aspect_ratio());
    if (keep != 0.0f) {
        // R RETURNS THE EYE, AND THE MODEL WITH IT. Leaving the model turned
        // would make "reset" mean two different pictures depending on what the
        // person did before pressing it.
        viewer_view_.model_yaw = 0.0f;
        viewer_show(viewer_index_);
    }
}

bool App::viewer_draw(render::PixelCanvas& hud) {
    if (!viewer_mode_ || viewer_items_.empty()) {
        return false;
    }
    const int w = static_cast<int>(hud.width());
    const int h = static_cast<int>(hud.height());
    const int px = ui_px(h, UiText::Caption);
    const int small = ui_px(h, UiText::Small);
    if (px <= 0 || small <= 0) {
        return false; // no baked font: the block font is an instrument, not the game
    }
    const ViewerItem& item = viewer_items_[static_cast<std::size_t>(viewer_index_)];
    const ViewerSize size = viewer_size(item.source, viewer_lo_, viewer_hi_);

    const int margin = std::max(8, h / 24);
    int y = margin;
    const auto line = [&](const std::string& text, render::Color ink, int size_px) {
        if (text.empty()) {
            return;
        }
        const int tw = ui_text_width(text, size_px);
        draw_text_plate(hud, margin, y, tw, ui_cap_height(size_px));
        ui_draw_text(hud, margin, y, text, ink, size_px, /*shadow=*/true);
        y += ui_line_height(size_px);
    };

    // Line 1: what is on the pedestal, and where in the list it stands.
    char counter[48] = {};
    std::snprintf(counter, sizeof(counter), "  [%d/%d]", viewer_index_ + 1,
                  static_cast<int>(viewer_items_.size()));
    line(item.name + counter, VIEWER_INK, px);

    // Line 2: which of the three sources, and which folder inside it.
    std::string where(localized(source_key(item.source)));
    where += " ";
    where += std::string(localized(serialization::fnv1a64("viewer.of")));
    where += " ";
    where += item.category;
    line(where, VIEWER_DIM, small);

    // Line 3: the SIZE, in metres of the file, plus the factor it is drawn at
    // when that is not one. The raw number stays even when scaled — a caption
    // that only showed the scaled size would hide the very fact it exists for.
    if (viewer_error_.empty()) {
        std::string sz(localized(serialization::fnv1a64("viewer.size")));
        sz += " " + two_decimals(size.width_m) + " x " + two_decimals(size.height_m)
            + " x " + two_decimals(size.depth_m) + " "
            + std::string(localized(serialization::fnv1a64("viewer.metres")));
        if (std::fabs(size.scale - 1.0f) > 1e-3f) {
            char sc[48] = {};
            std::snprintf(sc, sizeof(sc), " · %s %.3f",
                          std::string(localized(serialization::fnv1a64("viewer.scale"))).c_str(),
                          static_cast<double>(size.scale));
            sz += sc;
        }
        line(sz, VIEWER_DIM, small);
    } else {
        line(std::string(localized(serialization::fnv1a64("viewer.failed"))) + ": "
                 + viewer_error_,
             VIEWER_BAD, small);
    }

    // Line 4: provenance. The SOURCE.txt line as it was written, or the path.
    line(item.origin, VIEWER_DIM, small);

    // The keys, along the bottom, where the interaction prompt lives: it is the
    // same kind of sentence — what this frame will do if you press something.
    const std::string keys(localized(serialization::fnv1a64("viewer.keys")));
    const int kw = ui_text_width(keys, small);
    const int kx = (w - kw) / 2;
    const int ky = h - std::max(8, h / 12);
    draw_text_plate(hud, kx, ky, kw, ui_cap_height(small));
    ui_draw_text(hud, kx, ky, keys, VIEWER_DIM, small, /*shadow=*/true);
    return true;
}

} // namespace dfn::app
