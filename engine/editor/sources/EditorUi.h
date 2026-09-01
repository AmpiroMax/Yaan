/*
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

A DOOR THAT TAKES A SHORTCUT THE REAL CALLER CANNOT TAKE PROVES NOTHING, and
the doors below (DFN_UI_PROBE, DFN_UI_PROBE_MOUSE, DFN_UI_PROBE_CLICK,
DFN_UI_PROBE_KEYS, DFN_UI_PANEL) are written to obey that. Read this before you
add the next one.

The first version of DFN_UI_PROBE flipped `visible_` itself and then opened its
panel. Every frame shipped as evidence therefore came up through a path no real
panel uses, so "the interface works" was true of the probe and of nothing else.
The defect it hid — a panel opened by set_panel_open() drawing nothing — reached
the user, who got the build hand active, the camera correctly frozen, and no
menu: his controls taken away with nothing given back. Three sessions had
written a tool nobody could use, and four archived frames said it was fine.

So: a door may supply what a HAND supplies — a pointer position, a button going
down and up, a keystroke — and nothing else. It must never call the function the
hand's click would eventually reach. If you cannot see how to drive a feature
that way, that is a finding about the feature, not a licence to shortcut.

AND THE WORD TO WATCH FOR IS "SHOULD". "It should work now" is what stands in
for a measurement when the measurement is inconvenient, and it is what cost this
zone an evening on 17.08.2026.
*/

#pragma once

#include "engine/editor/sources/EditorToolbox.h"
#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/platform/window/interfaces/IWindow.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::app {

class ToolIconCache;

/// Where the editor's user-facing text comes from (Rule 5). A function rather
/// than an include because the localization table lives in engine/app, which
/// sits ABOVE this layer: the DAG allows app -> editor and never the reverse.
using EditorTextSource = std::string_view (*)(const char* key);

/// A picture a panel can show. Opaque on purpose: the value is the backend's
/// business and a panel must never take it apart. Zero means "no picture".
using EditorTexture = std::uint64_t;

/// ЧТО СЕЙЧАС В РУКЕ — вопрос, у которого ОДИН адресат: EditorToolbox
/// (EditorToolbox.h), и он держит указатель, а не ярлык. Перечисления
/// EditorTool здесь больше нет намеренно: пока оно было, «какой сейчас
/// инструмент» спрашивали семь мест и решали каждое по-своему, а два хозяина
/// одной кнопки были не ошибкой одного из них, а формой всей конструкции
/// (docs/audits/AUDIT_EDITOR_TOOLS.md).

/// HOW MUCH OF THE WINDOW THE INTERFACE HAS TAKEN, per edge, in ImGui's logical
/// units. Zero on every edge when nothing is docked or the interface is hidden.
///
/// WHY ANYBODY OUTSIDE NEEDS THIS (user, 17.08.2026: «у нас сейчас много
/// элементов UI которые перекрываются... пусть кнопки и прочее инструментов
/// рисуется вдоль экрана как приложухи старых операционных систем виндовс»).
/// Four independent painters share this frame: this toolbar, the ImGui panels,
/// the game HUD on PixelCanvas (compass, bars, crosshair) and the debug readout.
/// Each knows only about itself, so each finds a free corner by hand and finds
/// it wrong — this zone has already paid once, by seating the toolbar on the
/// readout's plate. One place counts the taken strips and everyone else asks;
/// a second set of coordinates would be a second thing to keep true.
struct EditorInsets {
    float top = 0.0f;
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

/// A rectangle in ImGui's logical units (or, for the _norm form, as fractions
/// of the display).
struct EditorRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

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

/// The id of the ONE settings window. It is a panel like any other — docked,
/// counted in the insets, closed by ESC — but its content is whichever tool's
/// triangle is down, so there is exactly one settings window in the editor and
/// never one per tool.
inline constexpr const char* TOOL_SETTINGS_PANEL_ID = "tool.settings";

/// The editor's ImGui frame. One instance, owned by App.
class EditorUi {
public:
    /// Both defined in the .cpp, not defaulted here: the icon cache is held by
    /// unique_ptr behind a forward declaration, and a constructor defaulted in
    /// the header would need its full definition in engine/app — which is the
    /// layer that may not see the bar's file at all.
    EditorUi();
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

    // -- the layout: what the interface took, what is left for the world ------

    /// The strips this interface occupies, in logical units. Valid after
    /// begin_frame; all zero while hidden.
    [[nodiscard]] EditorInsets insets() const { return insets_; }

    /// What is LEFT for the world after the strips, in logical units.
    [[nodiscard]] EditorRect world_rect() const;

    /// The same rectangle as FRACTIONS of the display (0..1). This is the form
    /// to use when placing anything on a surface that is not the window — the
    /// HUD is composited into the internal 1920x1080 target, not into the
    /// framebuffer, so a number in pixels would be right for one of them and
    /// quietly wrong for the other. The crosshair belongs at the centre of
    /// THIS, not at the centre of the window: with a panel open the two differ
    /// by half the panel's width.
    [[nodiscard]] EditorRect world_rect_norm() const;

    // -- the tools ------------------------------------------------------------

    /// THE ONE OWNER OF «что в руке». The frame holds it because the frame
    /// draws the bar; everyone else asks it questions and never keeps an answer
    /// of their own.
    [[nodiscard]] EditorToolbox& toolbox() { return toolbox_; }
    [[nodiscard]] const EditorToolbox& toolbox() const { return toolbox_; }

    /// WHAT A TOOL MAY DO TO THE WORLD. Filled ONCE by whoever owns the world
    /// (App) and passed to every dispatch from here, so a tool cannot be handed
    /// two different worlds by two callers.
    [[nodiscard]] ToolWorld& tool_world() { return tool_world_; }

    /// The always-on strip of tool chips. On by default: the user must be able
    /// to see WHICH tool has the mouse without opening anything, because an
    /// editor whose state lives only inside the code is an editor that freezes
    /// the camera for reasons nobody on the outside can name — which is exactly
    /// what happened on 17.08 and is why this sentence is here.
    void set_toolbar_visible(bool on) { toolbar_ = on; }
    [[nodiscard]] bool toolbar_visible() const { return toolbar_; }

    // -- panels ---------------------------------------------------------------

    /// Declares a panel. Call once, at setup. Re-adding the same id replaces
    /// the previous declaration (so a panel can be re-registered on map change
    /// without leaking a stale callback).
    void add_panel(EditorPanel panel);

    [[nodiscard]] bool panel_open(const std::string& id) const;
    void set_panel_open(const std::string& id, bool open);
    void toggle_panel(const std::string& id);

    /// Открыта ли ХОТЬ ОДНА панель, и закрыть ВСЕ. Пара нужна для ESC (заказ
    /// 18.08: «esc будет закрывать открытое окно объектов / кистей, не важно
    /// что открыто»). Спрашивать по именам панелей нельзя: имена знает тот, кто
    /// панели объявил, а ESC живёт в кадровом цикле и обязан работать для
    /// панели, которую заведут завтра. Поэтому вопрос задаётся КАРКАСУ, а не
    /// списку идентификаторов, который пришлось бы дописывать при каждой новой
    /// панели — и однажды не дописать.
    [[nodiscard]] bool any_panel_open() const;
    /// Возвращает, закрылось ли что-нибудь: ESC обязан знать, съеден ли он
    /// панелью, иначе тем же нажатием откроется меню паузы.
    bool close_all_panels();

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
    /// The always-on strip of tool chips, docked to the top edge.
    void draw_toolbar();
    /// Remembers where a strip actually landed, for the overlap instrument.
    /// PLAIN FLOATS AND NOT ImVec2: engine/app includes this header and is not
    /// allowed to see Dear ImGui (Rule 1, and tools/dag_check.py enforces it) —
    /// naming an ImGui type here would either drag the library up a layer or,
    /// as it did on the first attempt, silently declare a DIFFERENT empty type
    /// with the same name.
    void record_rect(const std::string& id, float x, float y, float w, float h);
    /// PROVES THE STRIPS DO NOT OVERLAP, with a number and not with a look.
    /// Door DFN_UI_LAYOUT_CHECK=1 measures; =2 measures with the toolbar's
    /// reservation deliberately switched off, which MUST report an overlap —
    /// an instrument that cannot report one is measuring nothing (Rule 30b).
    void report_layout_overlaps() const;
    /// The built-in self-check panel (door DFN_UI_PROBE=1). It exists to answer
    /// "does the font actually cover our alphabet" with a photograph rather
    /// than with a promise, and to state the frame's numbers beside it.
    void draw_probe_panel();

    platform::IRenderer* renderer_ = nullptr;
    bool ready_ = false;
    bool visible_ = true; // master HIDE, not a master show — see set_visible()
    bool toolbar_ = true;
    EditorToolbox toolbox_;
    ToolWorld tool_world_;
    /// Baked on first draw, dropped with the frame. unique_ptr because the
    /// cache names ImGui-free types but lives beside them, and because this
    /// header is included by engine/app, which may not see the bar's file.
    std::unique_ptr<ToolIconCache> icons_;
    EditorInsets insets_;
    float toolbar_height_ = 0.0f; // measured after drawing; 0 on the first frame
    /// Where each strip landed this frame — the instrument's raw data.
    struct PlacedRect {
        std::string id;
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };
    std::vector<PlacedRect> placed_;
    bool wants_mouse_ = false;
    bool wants_keyboard_ = false;
    bool frame_open_ = false;
    float scale_ = 1.0f;
    glm::uvec2 framebuffer_{0, 0};
    std::vector<EditorPanel> panels_;
};

} // namespace dfn::app
