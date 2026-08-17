/*
Created: 17:08:2026 - 19:17:13
Last updated: 17:08:2026 - 20:00:35
Module: engine/editor
File: engine/editor/sources/EditorUi.h

Responsibility:
- THE EDITOR'S INTERFACE FRAME, and nothing above it. It owns the Dear ImGui
  context, feeds it our platform input, hands its draw lists to the bgfx
  backend, carries the shared style and the Cyrillic font, and holds the list
  of PANELS other agents write.

Key items:
- EditorUi: init / begin_frame / end_frame, the whole per-frame contract App
  needs. One hook in the loop, nothing else.
- EditorUi::wants_mouse() / wants_keyboard(): the two capture flags. SEPARATE
  on purpose — see below, it is the difference between a usable editor and one
  that walks the camera away while you type in a search box.
- EditorPanel + add_panel(): how a panel is declared by code that knows nothing
  about bgfx, ImGui windows, or App.
- EditorTexture + make_texture() / texture_of() / image(): how a panel shows a
  picture (part thumbnails) without ever naming a backend type.
- tr(): localized text as a null-terminated string ImGui can take (Rule 5).

WHY THIS EXISTS (user, 17.08.2026): «тут нужно imgui, надо добавлять... справа
должно окно рисоваться с его характеристиками, я должен уметь их менять».
Dear ImGui is the EDITOR'S interface only. The game menu, the HUD and the
controls screen stay on PixelCanvas and keep going through the palette and the
post chain; this layer draws at native resolution, after the upscale, and is
deliberately NOT part of the game's look.

THE TWO CAPTURE FLAGS ARE TWO DIFFERENT QUESTIONS, and answering them with one
boolean is the bug this note exists to prevent. `wants_mouse()` means the
pointer is over a panel: the camera must stop turning and the cursor must be
free, because the user asked to «стоять на месте и кликать мышкой по меню».
`wants_keyboard()` means a text field has focus: W/A/S/D must stop being
movement, because the search box in the object menu eats letters, and a search
for «дверь» that also walks the camera forward is a search nobody will use
twice. A panel can want the mouse and not the keyboard, and the reverse.

Dependencies:
- Uses: engine/platform/{input,render,window} interfaces, Dear ImGui, and the
  bgfx ImGui backend behind ImGuiBackend.h (which hides both bgfx and ImGui).
- Used by: engine/app (App: one hook per frame), EditorPalette, EditorBrush.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A PANEL NEVER CALLS ImGui::Begin/End FOR ITS OWN WINDOW. EditorUi opens the
  window, places it, sizes it and closes it; the callback draws CONTENT. That
  is what keeps twelve panels written by three agents looking like one tool,
  and it is what lets the layout change in one place later.
- User-facing strings go through tr() (Rule 5). A literal Russian string in a
  panel is a violation, and it will also be the string that has no glyph.
*/
/*
UPD:
- 17:08:2026 - 19:17:13: Создан — каркас интерфейса редактора (ImGui), контракт
  панелей, два признака захвата, отдача текстур.
- 17:08:2026 - 20:00:35: ПАНЕЛЬ НЕ ПОЯВЛЯЛАСЬ У ПОЛЬЗОВАТЕЛЯ, и виноват этот файл. Каркас
  имел ДВА выключателя — visible_ (общий) и open у панели, — и общий по
  умолчанию стоял в false. Лид открыл панель set_panel_open(), как и написано в
  контракте, и не нарисовалось НИЧЕГО: у пользователя рука строителя работала,
  камера послушно замирала по wants_mouse(), а меню не было — управление отняли,
  взамен не дали. Теперь visible_ по умолчанию ИСТИНЕН и означает общее
  СКРЫТИЕ (чистый кадр мира), а не общий показ.
  ПОЧЕМУ ЭТОГО НЕ ПОЙМАЛИ МОИ КАДРЫ — и это вторая половина находки: дверь
  DFN_UI_PROBE САМА ставила visible_ = true. То есть каждый мой снимок шёл путём,
  которым не идёт ни одна настоящая панель, и «интерфейс работает» было правдой
  ровно про пробу. Дверь больше не срезает угол: она делает то же и только то же,
  что делает сосед, — add_panel(open=false) плюс set_panel_open(id, true).
  Точка съёмки, которой доступен путь, недоступный настоящему вызывающему, не
  проверяет ничего (правило 27).
*/

#pragma once

#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/platform/window/interfaces/IWindow.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::app {

/// Where the editor's user-facing text comes from (Rule 5). A function rather
/// than an include because the localization table lives in engine/app, which
/// sits ABOVE this layer: the DAG allows app -> editor and never the reverse.
using EditorTextSource = std::string_view (*)(const char* key);

/// A picture a panel can show. Opaque on purpose: the value is the backend's
/// business and a panel must never take it apart. Zero means "no picture".
using EditorTexture = std::uint64_t;

/// Where EditorUi parks a panel's window. The editor's shape is decided HERE,
/// once, so three agents' panels cannot each invent their own corner.
enum class EditorPanelSide : std::uint8_t {
    Right,    ///< the properties column the user asked for («справа окно»)
    Left,     ///< the object menu / shelf side
    Bottom,   ///< timelines, logs, wide lists
    Floating, ///< the panel places itself; use only when it truly must move
};

/// One panel of the editor, declared by whoever owns its subject.
///
/// `draw` is called between ImGui::Begin and ImGui::End of a window EditorUi
/// owns: inside it, plain ImGui calls are the whole API. It is called ONLY when
/// the panel is open and the editor interface is visible, so a panel needs no
/// visibility test of its own.
struct EditorPanel {
    /// Stable ASCII identity: used for toggling, ordering and (later) for
    /// remembering open/closed between runs. Not shown to anybody.
    std::string id;
    /// Localization key for the title bar (Rule 5), e.g. "editor.panel.parts".
    std::string title_key;
    EditorPanelSide side = EditorPanelSide::Right;
    /// Width for Left/Right sides, height for Bottom, in interface pixels.
    /// A first size only — the user can drag it, and EditorUi will remember.
    float extent_px = 380.0f;
    /// Open at startup. Panels the user must ASK for start closed.
    bool open = false;
    /// The content. Called once per frame while open.
    std::function<void()> draw;
};

/// The editor's ImGui frame. One instance, owned by App.
class EditorUi {
public:
    EditorUi() = default;
    ~EditorUi();
    EditorUi(const EditorUi&) = delete;
    EditorUi& operator=(const EditorUi&) = delete;

    // -- lifetime -------------------------------------------------------------

    /// Creates the ImGui context, the style and the font, and brings up the
    /// bgfx backend. Returns false if the backend could not start; the editor
    /// then runs exactly as it did before this module existed (Rule 3's spirit:
    /// a missing interface must not be a crash).
    [[nodiscard]] bool init(platform::IRenderer& renderer);
    void shutdown();
    [[nodiscard]] bool ready() const { return ready_; }

    // -- the per-frame hook (this is App's ONLY duty) -------------------------

    /// Opens the interface frame: pumps input into ImGui and lays the panels
    /// out. Call once per frame, AFTER input->update() and BEFORE the panels'
    /// draw callbacks would want to run — begin_frame runs them itself.
    ///
    /// IT TAKES THE WINDOW, NOT A SIZE, because it needs TWO sizes: the
    /// framebuffer in pixels and the content box in logical units. On a Retina
    /// display those differ by two, the mouse is reported in the second, and an
    /// interface that guesses the ratio is an interface whose cursor is off by
    /// a factor of two.
    void begin_frame(platform::IInput& input, const platform::IWindow& window,
                     float dt);

    /// Closes the frame and hands the draw lists to the backend. Call before
    /// IRenderer::end_frame() — which, in this engine, happens inside
    /// RenderSystem::render(). So: begin_frame at the top of the loop,
    /// end_frame immediately before the render call.
    void end_frame();

    /// A MASTER HIDE, AND IT IS ON BY DEFAULT. Turning it off blanks every
    /// panel at once — for a clean screenshot of the world, the way DFN_HUD=0
    /// blanks the game's overlays. Panels keep their own open/closed state
    /// across it.
    ///
    /// IT DEFAULTS TO VISIBLE BECAUSE THE OTHER WAY ROUND COST A USER HIS
    /// EVENING. It started as an opt-in switch, so a panel opened with
    /// set_panel_open() drew NOTHING until somebody also called this — and the
    /// somebody could not know, because opening a panel is the obvious and
    /// complete-looking action. What the user got was the build hand active,
    /// the camera correctly frozen by wants_mouse(), and no menu: his controls
    /// taken away with nothing given back. Two switches where one will do is a
    /// trap for whoever wires the second one, and the person who pays is the
    /// one holding the tool.
    void set_visible(bool on) { visible_ = on; }
    [[nodiscard]] bool visible() const { return visible_; }

    // -- the two capture flags (read these, freeze accordingly) ---------------

    /// The pointer is over a panel, or a widget is being dragged. While true:
    /// the camera must not turn and the cursor must be free.
    /// False when the interface is hidden or not ready — the editor without an
    /// interface behaves exactly as it did before.
    [[nodiscard]] bool wants_mouse() const { return wants_mouse_; }

    /// A text field has focus. While true: keys are TEXT, not commands — no
    /// movement, no hotkeys, no build actions.
    [[nodiscard]] bool wants_keyboard() const { return wants_keyboard_; }

    // -- panels ---------------------------------------------------------------

    /// Declares a panel. Call once, at setup. Re-adding the same id replaces
    /// the previous declaration (so a panel can be re-registered on map change
    /// without leaking a stale callback).
    void add_panel(EditorPanel panel);

    [[nodiscard]] bool panel_open(const std::string& id) const;
    void set_panel_open(const std::string& id, bool open);
    void toggle_panel(const std::string& id);

    // -- pictures -------------------------------------------------------------

    /// Uploads an RGBA8 image and returns a handle a panel can draw. `pixels`
    /// must be width*height*4 bytes. Returns 0 on failure.
    /// The caller owns it: call drop_texture when the picture is gone. Meant
    /// for thumbnails a panel builds itself.
    [[nodiscard]] EditorTexture make_texture(std::uint32_t width, std::uint32_t height,
                                             const std::uint8_t* rgba);

    /// Replaces the pixels of a texture made by make_texture, same dimensions.
    void update_texture(EditorTexture texture, std::uint32_t width,
                        std::uint32_t height, const std::uint8_t* rgba);

    void drop_texture(EditorTexture texture);

    /// A picture the RENDERER already owns (an offscreen target a thumbnail was
    /// drawn into, an atlas page). Nothing is copied and nothing is owned:
    /// destroying the renderer's texture invalidates this. Returns 0 for an
    /// invalid handle or a backend that cannot answer.
    [[nodiscard]] EditorTexture texture_of(platform::TextureHandle handle) const;

    /// Draws a picture at `w` x `h` interface pixels. Use this rather than
    /// ImGui::Image so a panel never has to name ImTextureID.
    static void image(EditorTexture texture, float w, float h);

    /// The same as a button. `id` is an ASCII widget id, not shown.
    [[nodiscard]] static bool image_button(const char* id, EditorTexture texture,
                                           float w, float h);

    // -- text (Rule 5) ---------------------------------------------------------

    /// The localized string for `key`, null-terminated and valid until the next
    /// begin_frame. This is how a panel says anything to a human: ImGui takes
    /// const char*, the table gives a string_view, and the gap between them is
    /// exactly where literal Russian sneaks into C++.
    ///
    /// Falls back to the KEY ITSELF when no source is wired. That is visibly
    /// wrong on screen ("editor.panel.parts" in a title bar) and therefore
    /// safe — a blank title would look like a finished panel with no name.
    [[nodiscard]] static const char* tr(const char* key);

    /// Wires the text table. App calls this once at startup; engine/editor must
    /// not include engine/app (the DAG runs the other way), so the string table
    /// arrives as a function rather than as an include.
    static void set_text_source(EditorTextSource source);

    /// The interface's own pixel scale (Retina). Panels sizing anything in
    /// pixels should multiply by it; most panels should size in em instead.
    [[nodiscard]] float scale() const { return scale_; }

private:
    void layout_panels();
    /// The built-in self-check panel (door DFN_UI_PROBE=1). It exists to answer
    /// "does the font actually cover our alphabet" with a photograph rather
    /// than with a promise, and to state the frame's numbers beside it.
    void draw_probe_panel();

    platform::IRenderer* renderer_ = nullptr;
    bool ready_ = false;
    bool visible_ = true; // master HIDE, not a master show — see set_visible()
    bool wants_mouse_ = false;
    bool wants_keyboard_ = false;
    bool frame_open_ = false;
    float scale_ = 1.0f;
    glm::uvec2 framebuffer_{0, 0};
    std::vector<EditorPanel> panels_;
};

} // namespace dfn::app
