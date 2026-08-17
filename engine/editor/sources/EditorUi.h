/*
Created: 17:08:2026 - 19:17:13
Last updated: 17:08:2026 - 20:21:23
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
DFN_UI_PROBE_KEYS) are written to obey that. Read this before you add the next
one.

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
- 17:08:2026 - 20:17:55: ПАНЕЛЬ ИНСТРУМЕНТОВ (заказ 17.08: «надо добавить панель с выбором
  этих инструментов»). EditorTool: пять РЕЖИМОВ, ровно один активен. Это не
  украшение списка клавиш: сегодня ЛКМ значит «поставить деталь», а кисть
  завтра тоже захочет ЛКМ, потому что рисование это протяжка, — и драка двух
  хозяев за кнопку невидима, щелчок делает то одно, то другое. С режимом у
  кнопки один хозяин в любой момент, и какой именно — видно на полосе.
  Look («просто смотрю») стоит в списке пятым, как у пользователя, и является
  режимом ПО УМОЛЧАНИЮ: инструмент, который нельзя отложить, делает каждый
  щелчок риском.
  Наружу отдано ровно три вещи: tool(), set_tool(), tool_changed() — последняя
  истинна один кадр и нужна владельцу режима, чтобы бросить недоделанное
  (мазок кисти и недопоставленная деталь обязаны кончиться при уходе).
  Полоса всегда видна и стоит ПОД отладочным выводом, а не рядом: его плита
  во всю ширину и в три строки, и полоса на y=6 садилась ровно на неё —
  два оверлея в одном углу этот проект уже оплачивал.
  Проверено ТЕМ ЖЕ ПУТЁМ, ЧТО У ПОЛЬЗОВАТЕЛЯ: указатель на фишке, кнопка вниз
  и вверх (DFN_UI_PROBE_CLICK), а не вызовом set_tool из двери. Вызов из двери
  сфотографировал бы функцию, которую никто не нажимает, — ошибка, стоившая
  сегодня вечера.
- 17:08:2026 - 20:21:23: В шапку записан урок про ДВЕРИ (по указанию лида, чтобы
  следующий прочитал его раньше, чем отчитается кадром): дверь может подавать
  то же, что подаёт РУКА — положение указателя, кнопку вниз и вверх, нажатие
  клавиши, — и ничего больше; звать функцию, до которой щелчок руки только
  добрался бы, ей нельзя. И слово, за которым надо следить, — «должен»: оно
  становится на место измерения ровно тогда, когда измерение неудобно.
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

/// WHAT THE EDITOR IS DOING RIGHT NOW. Exactly one at a time.
///
/// WHY A MODE AND NOT FIVE INDEPENDENT SWITCHES (user, 17.08.2026: «надо
/// добавить панель с выбором этих инструментов»). Today each tool is a key that
/// turns something on beside everything else, and the left mouse button already
/// means "place a part". The moment the terrain brush wants the left button too
/// — and it will, because painting is a drag — the two owners fight, and the
/// fight is invisible: the click does both, or the wrong one, depending on
/// which test ran first. With a mode the button has ONE owner at all times, and
/// which one is a thing the user chose and can see.
///
/// AND `Look` IS A REAL MODE, NOT THE ABSENCE OF ONE. It is the default on
/// entering the editor, because a tool you cannot put down makes every click a
/// risk: the user asked for «пустой курсор, словно ничего не делаем, просто как
/// играем» in the same breath as the other four, which is exactly the right
/// instinct.
///
/// The numbers in the comments are the user's own numbering, and the toolbar
/// shows them, so "press 3" and "the third chip" are the same thing.
enum class EditorTool : std::uint8_t {
    Look = 0,      ///< 5 — просто смотрю. THE DEFAULT.
    HeightBrush,   ///< 1 — кисть высоты ландшафта
    SurfacePaint,  ///< 2 — кисть поверхности (скала/трава/песок/тропа)
    SelectObject,  ///< 3 — прицел выбирает объект
    PlaceObject,   ///< 4 — прицел ставит объект
    Count
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

    // -- the tool (mode) ------------------------------------------------------

    /// What the editor is doing. Poll it; it is cheap and it never lies about a
    /// frame that has already been drawn.
    [[nodiscard]] EditorTool tool() const { return tool_; }

    /// Switches tools. Safe to call from a key handler, from a panel, or from
    /// anywhere else — the toolbar and the label follow on the same frame.
    void set_tool(EditorTool tool);

    /// True for the ONE frame in which the tool changed, whoever changed it.
    /// The owner of a tool uses this to drop whatever it was holding: a brush
    /// mid-stroke and a half-placed part must both end when the user leaves.
    [[nodiscard]] bool tool_changed() const { return tool_changed_; }

    /// The tool's name for a human, localized (Rule 5). Valid until the next
    /// begin_frame, like tr().
    [[nodiscard]] static const char* tool_name(EditorTool tool);

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
    /// The always-on strip of tool chips (top centre).
    void draw_toolbar();
    /// The built-in self-check panel (door DFN_UI_PROBE=1). It exists to answer
    /// "does the font actually cover our alphabet" with a photograph rather
    /// than with a promise, and to state the frame's numbers beside it.
    void draw_probe_panel();

    platform::IRenderer* renderer_ = nullptr;
    bool ready_ = false;
    bool visible_ = true; // master HIDE, not a master show — see set_visible()
    bool toolbar_ = true;
    EditorTool tool_ = EditorTool::Look; // «просто смотрю» is where you start
    bool tool_changed_ = false;
    bool wants_mouse_ = false;
    bool wants_keyboard_ = false;
    bool frame_open_ = false;
    float scale_ = 1.0f;
    glm::uvec2 framebuffer_{0, 0};
    std::vector<EditorPanel> panels_;
};

} // namespace dfn::app
