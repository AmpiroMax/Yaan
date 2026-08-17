/*
Created: 17:08:2026 - 19:17:13
Last updated: 17:08:2026 - 20:26:58
Module: engine/editor
File: engine/editor/sources/EditorUi.cpp

Responsibility:
- The editor interface frame declared in EditorUi.h: the ImGui context, the
  font with a Cyrillic range that actually covers Cyrillic, the input routing
  from OUR platform/input, the shared style, and the panel layout.

Dependencies:
- Uses: EditorUi.h, Dear ImGui, the bgfx ImGui backend behind ImGuiBackend.h,
  engine/platform/{input,window,render} interfaces. NOT engine/app: the DAG
  runs app -> editor, so the localization table arrives as a function pointer
  (set_text_source) instead of as an include.
- Used by: App (one hook per frame), EditorPalette, EditorBrush.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- INPUT COMES FROM platform::IInput AND NEVER FROM GLFW. ImGui ships GLFW
  backends; using one would give the editor a second, parallel input path that
  disagrees with the game's the first time a key is remapped.
*/
/*
UPD:
- 17:08:2026 - 19:17:13: Создан — контекст, шрифт с кириллицей (диапазон свой,
  встроенный в ImGui теряет строчную «ё»), маршрутизация ввода, стиль, раскладка
  панелей, два признака захвата.
- 17:08:2026 - 19:54:38: ЗНАКИ. Зона меню объектов упёрлась в мой диапазон: ★ (U+2605)
  рисовалась бы пустым квадратом, и звезду пришлось убрать из таблицы. Диапазон
  расширен — и сразу измерено, потому что ДИАПАЗОН ЭТО ЗАПРОС, А НЕ ОБЕЩАНИЕ:
  ImGui молча пропускает кодпойнты, которых нет в шрифте. Arial несёт ▼ → ×, и
  НЕ несёт ★ ☆ ✓, то есть одно расширение списка не дало бы ничего. Поэтому
  знаки домешиваются вторым и третьим шрифтом (Apple Symbols, Arial Unicode):
  буквы остаются в том начертании, под которое верстались панели, а из вторых
  берутся только те кодпойнты, которых первый не смог. report_mark_glyphs()
  печатает при старте каждый знак поимённо и держит СВОЙ контроль — U+4E2D,
  заведомо вне запроса, обязан отсутствовать; если он «есть», значит проверка
  отвечает из подменного глифа и все «есть» выше ничего не стоят.
  Числа: глифов 307 -> 327 (только расширение диапазона) -> 411 (с домесом),
  атлас 512x512 -> 512x1024. Все семь знаков на кадре, контроль отсутствует.
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
- 17:08:2026 - 20:26:58: РАСКЛАДКА ПОЛОСАМИ (заказ 17.08: «много элементов UI которые
  перекрываются... пусть рисуется вдоль экрана как приложухи старых
  операционных систем виндовс»). Полосы ПРИСТЫКОВАНЫ и ОТЪЕДАЮТ место, а не
  лежат на картинке: верхняя полоса инструментов во всю ширину, панели —
  колонками у краёв, мир в остатке. Наружу отдано insets(), world_rect() и
  world_rect_norm() (доли, а не пиксели: HUD компонуется во ВНУТРЕННЮЮ цель
  1920x1080, а не в кадровый буфер, и число в пикселях было бы верным для
  одного и тихо неверным для другого).
  Окна стали NoMove/NoResize с позицией Always: панель, которую можно
  перетащить, — это панель, которую можно перетащить ПОВЕРХ соседней, и тогда
  гарантия непересечения становится заботой пользователя, а не нашей.
  ДВЕ ПРЕДЫДУЩИЕ ВЕРСИИ ОДНОЙ СТРОКИ И ЕСТЬ ДОВОД ЗА РАСКЛАДКУ: сначала полоса
  села на плиту отладочного вывода, потом её подвинули на 44 единицы вниз.
  Подвигание — это то, что делают, когда раскладкой никто не владеет: число
  верно для того вывода в тот день, а следующий рисовальщик подвигал бы снова
  против числа, которого не видит.
  ПРИБОР С КОНТРОЛЕМ, а не «на глаз не налезает»: DFN_UI_LAYOUT_CHECK=1 меряет
  площадь пересечения каждой пары полос, =2 меряет ТО ЖЕ, сняв верхнюю
  резервацию. Рука 1: 0.0 кв.ед. Рука 2: 18744.0 кв.ед. Контроль обязателен —
  прибор, отвечающий «не пересекается» на раскладке, которая пересечься не
  может, продолжал бы отвечать так и после поломки.
  Числа кадра: отъедено сверху 44.0, справа 432.7 (колонка ужата до трети
  ширины), мир 847x676 от (0, 44).
*/

#include "engine/editor/sources/EditorUi.h"

#include "engine/platform/render/sources/bgfx/ImGuiBackend.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <optional>
#include <filesystem>

namespace dfn::app {
namespace {

/// THE GLYPH RANGE, AND IT IS NOT ImGui's.
///
/// ImFontAtlas::GetGlyphRangesCyrillic() asks for 0x0400-0x044F, which stops
/// ONE CODEPOINT SHORT of lowercase «ё» (U+0451) — the letter sits in the
/// supplement block after «я», not with the rest of the alphabet. Uppercase «Ё»
/// (U+0401) is inside the range and renders fine, so the defect appears only in
/// running text and only in words like «ещё»: one blank box in a sentence, on a
/// font everyone already checked. 0x045F is the end of the block and costs 16
/// more glyphs.
const ImWchar* cyrillic_ranges() {
    // Static storage on purpose: ImGui keeps the pointer until the atlas is
    // built, which happens later, inside the backend.
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Latin + Latin-1 supplement (numbers, punctuation)
        0x0400, 0x045F, // Cyrillic INCLUDING Ё (0401) and ё (0451)
        0x2010, 0x2027, // dashes and quotes our strings actually use
        0x2116, 0x2116, // №
        // THE MARKS A TOOL PUTS BESIDE A ROW. Added because the object menu
        // needed a star for "favourite" and got an empty box: two ranges of
        // letters is exactly the font a panel with no marks in it needs, and
        // the first panel with marks in it proved it. Asking for a range the
        // font does not carry costs nothing — a missing glyph is simply not
        // rasterized — so the honest test is the probe below, not this list.
        0x2190, 0x2193, // ← ↑ → ↓
        0x25A0, 0x25FF, // ■ □ ▲ ▼ ● ○ and the rest of the geometric shapes
        0x2605, 0x2606, // ★ ☆ — the object menu's favourite mark
        0x2713, 0x2714, // ✓ ✔
        0,
    };
    return ranges;
}

/// The marks a panel is entitled to assume, checked against what the font
/// ACTUALLY carries once the atlas exists.
///
/// A RANGE IS A REQUEST, NOT A PROMISE. ImGui silently skips codepoints the
/// font has no outline for, so widening the list above proves nothing on its
/// own — and "I added the range" is precisely the kind of claim that reads as
/// done and ships an empty box. The neighbour who hit this found it by scanning
/// every codepoint of the string table against the atlas; this is the same
/// question asked from the other end, at startup, about the marks we tell
/// panels they may use.
void report_mark_glyphs() {
    struct Mark {
        ImWchar cp;
        const char* what;
    };
    static const Mark MARKS[] = {
        {0x2605, "★ избранное"}, {0x2606, "☆ не избранное"},
        {0x2713, "✓ отметка"},   {0x25B6, "▶ свёрнуто"},
        {0x25BC, "▼ раскрыто"},  {0x2192, "→ ведёт к"},
        {0x00D7, "× закрыть"},
        // THE CONTROL, and the probe is worthless without it: a codepoint
        // nobody could have loaded. If this one reports "есть", the check is
        // answering from a fallback glyph rather than from the font, and every
        // "есть" above is meaningless (Rule 30b).
        {0x4E2D, "中 КОНТРОЛЬ, обязан отсутствовать"},
    };
    const ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.empty()) {
        return;
    }
    ImFont* font = io.Fonts->Fonts[0];
    for (const Mark& m : MARKS) {
        const bool have = font->FindGlyphNoFallback(m.cp) != nullptr;
        std::fprintf(stderr, "[editor-ui] знак U+%04X %s: %s\n",
                     static_cast<unsigned>(m.cp), m.what, have ? "есть" : "НЕТ");
    }
}

/// UI fonts, in the order they are tried. A repo-local file wins so the look is
/// reproducible; the system ones are the fallback that keeps the tool usable
/// today. Rule 24 forbids installing anything, so nothing is downloaded here.
const char* const FONT_CANDIDATES[] = {
    "assets/fonts/editor.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/System/Library/Fonts/Geneva.ttf",
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/arial.ttf",
};

constexpr float FONT_POINTS = 16.0f;

/// platform::Key -> ImGuiKey. Only what an interface needs: text editing, the
/// arrows, the modifiers and the printable keys ImGui's own shortcuts use.
struct KeyPair {
    platform::Key ours;
    ImGuiKey theirs;
};

const KeyPair KEY_TABLE[] = {
    {platform::Key::TAB, ImGuiKey_Tab},
    {platform::Key::LEFT, ImGuiKey_LeftArrow},
    {platform::Key::RIGHT, ImGuiKey_RightArrow},
    {platform::Key::UP, ImGuiKey_UpArrow},
    {platform::Key::DOWN, ImGuiKey_DownArrow},
    {platform::Key::PAGE_UP, ImGuiKey_PageUp},
    {platform::Key::PAGE_DOWN, ImGuiKey_PageDown},
    {platform::Key::HOME, ImGuiKey_Home},
    {platform::Key::END, ImGuiKey_End},
    {platform::Key::INSERT, ImGuiKey_Insert},
    {platform::Key::DELETE, ImGuiKey_Delete},
    {platform::Key::BACKSPACE, ImGuiKey_Backspace},
    {platform::Key::SPACE, ImGuiKey_Space},
    {platform::Key::ENTER, ImGuiKey_Enter},
    {platform::Key::ESCAPE, ImGuiKey_Escape},
    {platform::Key::LEFT_CONTROL, ImGuiKey_LeftCtrl},
    {platform::Key::LEFT_SHIFT, ImGuiKey_LeftShift},
    {platform::Key::LEFT_ALT, ImGuiKey_LeftAlt},
    {platform::Key::LEFT_SUPER, ImGuiKey_LeftSuper},
    {platform::Key::RIGHT_CONTROL, ImGuiKey_RightCtrl},
    {platform::Key::RIGHT_SHIFT, ImGuiKey_RightShift},
    {platform::Key::RIGHT_ALT, ImGuiKey_RightAlt},
    {platform::Key::RIGHT_SUPER, ImGuiKey_RightSuper},
    {platform::Key::A, ImGuiKey_A}, {platform::Key::C, ImGuiKey_C},
    {platform::Key::V, ImGuiKey_V}, {platform::Key::X, ImGuiKey_X},
    {platform::Key::Y, ImGuiKey_Y}, {platform::Key::Z, ImGuiKey_Z},
};

[[nodiscard]] ImTextureID to_imgui_id(EditorTexture value) {
    return (ImTextureID)(uintptr_t)value; // NOLINT: see ImGuiBackend.cpp
}

/// DFN_UI_PROBE_MOUSE=x,y — a pointer position for a run with no hand on the
/// mouse. Read once; empty when the door is shut, and then nothing here runs.
std::optional<ImVec2> probe_pointer() {
    static const std::optional<ImVec2> parsed = [] -> std::optional<ImVec2> {
        const char* v = std::getenv("DFN_UI_PROBE_MOUSE");
        if (v == nullptr || *v == '\0') {
            return std::nullopt;
        }
        float x = 0.0f;
        float y = 0.0f;
        if (std::sscanf(v, "%f,%f", &x, &y) != 2) {
            std::fprintf(stderr, "[editor-ui] DFN_UI_PROBE_MOUSE=\"%s\" не разобран; "
                                 "ожидается x,y в логических единицах\n", v);
            return std::nullopt;
        }
        std::fprintf(stderr, "[editor-ui] указатель подан дверью: %.1f, %.1f\n",
                     static_cast<double>(x), static_cast<double>(y));
        return ImVec2(x, y);
    }();
    return parsed;
}

/// DFN_UI_PROBE_CLICK=1 — presses and releases the left button over whatever
/// DFN_UI_PROBE_MOUSE is pointing at.
///
/// THIS IS THE USER'S PATH AND NOT A SHORTCUT AROUND IT, which is the whole
/// point: the tool could be switched from an unattended run by calling
/// set_tool() directly, and that would photograph a function nobody presses.
/// Here the pointer sits on a chip and the BUTTON GOES DOWN AND UP — every
/// line between the click and the mode change is the same code the hand runs.
/// The lesson is paid for: my first three frames all raised the interface
/// through a door that flipped a flag no real caller flips, and the defect it
/// hid cost the user his evening.
bool probe_wants_click() {
    static const bool on = [] {
        const char* v = std::getenv("DFN_UI_PROBE_CLICK");
        return v != nullptr && *v != '\0' && *v != '0';
    }();
    return on;
}

/// DFN_UI_LAYOUT_CHECK — 1 measures the strips, 2 measures them with the
/// toolbar's reservation DELIBERATELY switched off.
///
/// THE SECOND ARM IS THE POINT. An instrument that reports "no overlap" on a
/// layout that cannot overlap has measured nothing, and it would keep saying
/// "no overlap" after the day somebody broke the docking. Arm 2 removes exactly
/// the mechanism under test — the top inset — so the panels start at y = 0 and
/// MUST be reported as sitting on the toolbar. If arm 2 comes back clean, the
/// instrument is broken and arm 1 means nothing (Rule 30b, Rule 48).
int layout_check() {
    static const int mode = [] {
        const char* v = std::getenv("DFN_UI_LAYOUT_CHECK");
        if (v == nullptr || *v == '\0' || *v == '0') {
            return 0;
        }
        return std::atoi(v);
    }();
    return mode;
}

/// DFN_UI_PROBE_KEYS=1 — puts the caret in the probe panel's text field, which
/// is the only way an unattended run can photograph wants_keyboard() saying yes.
bool probe_wants_keys() {
    static const bool on = [] {
        const char* v = std::getenv("DFN_UI_PROBE_KEYS");
        return v != nullptr && *v != '\0' && *v != '0';
    }();
    return on;
}

EditorTextSource& text_source() {
    static EditorTextSource src = nullptr;
    return src;
}

/// The per-frame arena tr() hands out. A deque and not a vector because the
/// pointers must survive the arena growing while a panel is mid-draw.
std::deque<std::string>& text_arena() {
    static std::deque<std::string> arena;
    return arena;
}

/// THE STYLE, IN ONE PLACE. Three agents write panels; the tool has to look
/// like one tool. Dark, low-chroma, one accent — the world behind the panels is
/// the subject, and an interface competing with it for attention is an
/// interface the builder learns to look past.
void apply_style(float scale) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle{};
    ImGui::StyleColorsDark();
    s.WindowRounding = 4.0f;
    s.ChildRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.TabRounding = 3.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = ImVec2(10.0f, 10.0f);
    s.FramePadding = ImVec2(8.0f, 4.0f);
    s.ItemSpacing = ImVec2(8.0f, 6.0f);
    s.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    s.IndentSpacing = 18.0f;
    s.ScrollbarSize = 12.0f;
    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4* c = s.Colors;
    // Panels sit ON a lit world, so they are opaque enough to read against a
    // bright sky and a dark cellar alike. 0.92 was picked by eye against both
    // and is the one number here that is a look decision.
    c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.11f, 0.92f);
    c[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.12f, 0.13f, 0.60f);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.09f, 0.10f, 0.97f);
    c[ImGuiCol_Border] = ImVec4(0.28f, 0.30f, 0.32f, 0.70f);
    c[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.17f, 0.19f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.17f, 0.18f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.25f, 0.27f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.30f, 0.32f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.22f, 0.34f, 0.26f, 0.85f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.44f, 0.32f, 0.90f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.52f, 0.37f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.40f, 0.30f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.33f, 0.52f, 0.38f, 1.00f);
    // THE ACCENT IS THE VERDICT'S GREEN. The builder already reads green as
    // "allowed" from the ghost in his hand; using the same green for "this is
    // selected" costs nothing and means one colour has one meaning in the tool.
    c[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.85f, 0.50f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.72f, 0.45f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.85f, 0.55f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.28f, 0.30f, 0.32f, 0.60f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.54f, 0.56f, 1.00f);

    // ONE MULTIPLICATION FOR EVERY SIZE, and on this engine it is normally 1.
    //
    // THE FIRST VERSION OF THIS PASSED THE FRAMEBUFFER SCALE HERE AND THE FIRST
    // FRAME SHOWED WHY IT IS WRONG: text came out 32 units tall, three words
    // per line, the panel's own numbers clipped off its right edge. The style
    // and the layout are authored in ImGui's LOGICAL units — the same units the
    // mouse arrives in — and the backend's projection is what turns those into
    // a Retina display's pixels. Multiplying here scales the panel a SECOND
    // time, on top of the scaling that already happens for free.
    //
    // The parameter stays because a deliberate "make everything bigger" knob is
    // a real thing to want later; it is simply not the display's business.
    s.ScaleAllSizes(scale);
}

} // namespace

EditorUi::~EditorUi() {
    shutdown();
}

bool EditorUi::init(platform::IRenderer& renderer) {
    if (ready_) {
        return true;
    }
    renderer_ = &renderer;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // panel layout is ours to persist, not ImGui's
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendPlatformName = "dfn_platform_input";
    io.BackendRendererName = "dfn_bgfx";

    // THE FONT IS LOADED AT THE PIXEL SIZE IT WILL BE DRAWN AT and then scaled
    // back down globally. Rasterizing at 16 and stretching to 32 on a Retina
    // display is the difference between crisp text and text that looks like a
    // screenshot of text.
    scale_ = 1.0f; // corrected on the first begin_frame, once the window is known
    bool font_loaded = false;
    for (const char* path : FONT_CANDIDATES) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            continue;
        }
        if (io.Fonts->AddFontFromFileTTF(path, FONT_POINTS * 2.0f, nullptr,
                                         cyrillic_ranges()) != nullptr) {
            io.FontGlobalScale = 0.5f;
            std::fprintf(stderr, "[editor-ui] шрифт: %s (%.0f px, диапазон "
                                 "0x0400-0x045F — со строчной «ё»)\n",
                         path, static_cast<double>(FONT_POINTS * 2.0f));
            font_loaded = true;
            break;
        }
    }
    if (!font_loaded) {
        // LOUD, because the failure mode is silent: the built-in font has no
        // Cyrillic at all, so every panel would show boxes and the first guess
        // would be "the backend is broken".
        std::fprintf(stderr, "[editor-ui] НИ ОДНОГО ШРИФТА С КИРИЛЛИЦЕЙ НЕ НАЙДЕНО "
                             "— интерфейс будет в квадратиках. Искали: ");
        for (const char* path : FONT_CANDIDATES) {
            std::fprintf(stderr, "%s ", path);
        }
        std::fprintf(stderr, "\n");
        io.Fonts->AddFontDefault();
    }

    // THE MARKS COME FROM A SECOND FONT, MERGED IN. Measured, not assumed:
    // Arial carries ▼, → and ×, and carries NEITHER ★ nor ☆ nor ✓. Widening
    // the range did nothing for those three, because a range is a request and
    // the font is the answer — which is the whole reason report_mark_glyphs()
    // exists and prints every mark by name.
    //
    // Merging beats switching the text font to one that has everything (Arial
    // Unicode, 23 MB): the letters keep the face the panels were laid out in,
    // and only the codepoints the first font could not serve are taken from
    // the second.
    if (font_loaded) {
        static const ImWchar SYMBOL_RANGES[] = {
            0x2190, 0x2193, 0x25A0, 0x25FF, 0x2605, 0x2606, 0x2713, 0x2714, 0,
        };
        for (const char* path : {"/System/Library/Fonts/Apple Symbols.ttf",
                                 "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"}) {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) {
                continue;
            }
            ImFontConfig cfg;
            cfg.MergeMode = true;
            cfg.PixelSnapH = true;
            // NO `break`: BOTH are merged, in order, because neither alone is
            // enough — Apple Symbols has ★ ☆ ▶ and no ✓ (U+2713), Arial
            // Unicode has the check. Merging is additive per codepoint, so the
            // second font only fills what the first left empty, and the probe
            // below is what says whether it did.
            if (io.Fonts->AddFontFromFileTTF(path, FONT_POINTS * 2.0f, &cfg,
                                             SYMBOL_RANGES) != nullptr) {
                std::fprintf(stderr, "[editor-ui] знаки домешаны из %s\n", path);
            }
        }
    }

    if (!platform::imgui_backend_init()) {
        ImGui::DestroyContext();
        renderer_ = nullptr;
        return false;
    }
    report_mark_glyphs();
    apply_style(1.0f);
    ready_ = true;

    // THE SELF-CHECK PANEL, and it is not decoration. It answers the one
    // question about a font that is otherwise discovered by a user, mid-work,
    // in the middle of a word: does the glyph range actually cover the alphabet
    // we write in. It also states the frame's numbers, so a screenshot of it is
    // a MEASUREMENT and not a picture of a window. Off unless asked for; the
    // door opens it without a keypress so an unattended run can photograph it
    // (Rule 27: a feature only a human hand can reach is a feature nobody can
    // prove works).
    add_panel(EditorPanel{
        "ui.probe", "editor.ui.probe.title", EditorPanelSide::Right, 460.0f, false,
        [this] { draw_probe_panel(); }});
    if (const char* door = std::getenv("DFN_UI_PROBE");
        door != nullptr && *door != '\0' && *door != '0') {
        // NOTHING BUT set_panel_open, and that is now part of what this door
        // tests. It used to also flip visible_, which is exactly what hid the
        // defect: every frame I shot came up through a path no other panel
        // uses, so "the interface works" was true only of the probe. A door
        // that takes a shortcut the real caller cannot take is a door that
        // photographs a feature nobody else has (Rule 27).
        set_panel_open("ui.probe", true);
    }
    return true;
}

void EditorUi::draw_probe_panel() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::TextWrapped("%s", tr("editor.ui.probe.pangram"));
    ImGui::TextDisabled("%s", tr("editor.ui.probe.note"));
    // The marks a panel may use, ON THE FRAME. stderr says which codepoints the
    // font carries; only a frame says they are legible at the size they will be
    // read at.
    ImGui::TextWrapped("%s", tr("editor.ui.probe.marks"));
    ImGui::Separator();

    if (ImGui::CollapsingHeader(tr("editor.ui.probe.numbers"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
        ImGui::Text("framebuffer %ux%u  content %.0fx%.0f  scale %.2f", framebuffer_.x,
                    framebuffer_.y, static_cast<double>(io.DisplaySize.x),
                    static_cast<double>(io.DisplaySize.y),
                    static_cast<double>(scale_));
        const int glyphs = io.Fonts->Fonts.empty() ? 0 : io.Fonts->Fonts[0]->Glyphs.Size;
        ImGui::Text("atlas %dx%d  glyphs %d", io.Fonts->TexWidth, io.Fonts->TexHeight,
                    glyphs);
        ImGui::Text("%.1f FPS  %.2f ms", static_cast<double>(io.Framerate),
                    static_cast<double>(1000.0f / (io.Framerate > 0.0f ? io.Framerate
                                                                       : 1.0f)));
        // Last frame's answer: the flags are settled after the panels draw, so
        // reading them here is one frame behind by construction. Said out loud
        // because a number that is quietly stale is worse than no number.
        ImGui::Text(tr("editor.ui.probe.capture"),
                    tr(wants_mouse_ ? "editor.ui.yes" : "editor.ui.no"),
                    tr(wants_keyboard_ ? "editor.ui.yes" : "editor.ui.no"));
    }
    ImGui::Separator();

    // Live widgets: they prove the input routing, not the drawing. A panel that
    // renders but does not take a click is a picture of a tool.
    static char query[128] = "";
    if (probe_wants_keys()) {
        // Once, on the first frame the panel exists: after that the field keeps
        // the caret on its own, and re-focusing every frame would fight a human
        // who clicked elsewhere.
        static bool focused = false;
        if (!focused) {
            focused = true;
            ImGui::SetKeyboardFocusHere();
        }
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##probe.query", tr("editor.ui.probe.field"), query,
                             sizeof(query));
    static float brush = 4.0f;
    ImGui::SliderFloat(tr("editor.ui.probe.slider"), &brush, 0.5f, 32.0f, "%.1f");
    static bool snap = true;
    ImGui::Checkbox(tr("editor.ui.probe.check"), &snap);
    static int clicks = 0;
    if (ImGui::Button(tr("editor.ui.probe.button"))) {
        ++clicks;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%d", clicks);
}

void EditorUi::shutdown() {
    if (!ready_) {
        return;
    }
    if (frame_open_) {
        ImGui::EndFrame();
        frame_open_ = false;
    }
    platform::imgui_backend_shutdown();
    ImGui::DestroyContext();
    panels_.clear();
    text_arena().clear();
    ready_ = false;
    wants_mouse_ = false;
    wants_keyboard_ = false;
    renderer_ = nullptr;
}

void EditorUi::begin_frame(platform::IInput& input, const platform::IWindow& window,
                           float dt) {
    if (!ready_) {
        return;
    }
    // A FRAME LEFT OPEN IS CLOSED HERE, NOT ASSERTED ON. App's loop has several
    // exits that skip the render call (menu mode, a resize, a paused tick), and
    // ImGui answers an unmatched NewFrame with an assert — i.e. the tool would
    // die on a path that has nothing to do with the tool. Discarding last
    // frame's lists is the honest recovery: they were never shown.
    if (frame_open_) {
        ImGui::EndFrame();
        frame_open_ = false;
    }
    const glm::uvec2 fb = window.framebuffer_size();
    const glm::uvec2 content = window.content_size();
    if (fb.x == 0 || fb.y == 0 || content.x == 0 || content.y == 0) {
        return;
    }
    framebuffer_ = fb;
    const float new_scale =
        static_cast<float>(fb.x) / static_cast<float>(content.x);
    // THE SCALE IS REPORTED, NOT APPLIED. It is the number the backend needs
    // (DisplayFramebufferScale, below) and a number worth showing in the probe;
    // the style and the font must NOT be multiplied by it, because the panels
    // are authored in logical units and the projection already does the work.
    // Applying it cost one frame to learn — see the note in apply_style.
    scale_ = new_scale;

    ImGuiIO& io = ImGui::GetIO();
    // ImGui WORKS IN LOGICAL UNITS. The mouse arrives in them, the panels are
    // authored in them, and the backend's projection is the only place pixels
    // appear — which is also why one frame's draw lists can be shown in a
    // window and in a differently-sized capture target.
    io.DisplaySize = ImVec2(static_cast<float>(content.x),
                            static_cast<float>(content.y));
    io.DisplayFramebufferScale = ImVec2(scale_, scale_);
    io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;

    // -- input, all of it from OUR platform layer -----------------------------
    // THE POINTER DOOR (DFN_UI_PROBE_MOUSE=x,y in logical units). A capture flag
    // is worth exactly as much as the run that can demonstrate it, and an
    // unattended run has no hand on the mouse — so without this, wants_mouse()
    // could only ever be photographed answering "no", which is the answer a
    // completely broken implementation also gives (Rule 27: a vantage that
    // cannot fail is not evidence; Rule 48: the arm that matters is the one
    // where the dose is non-zero). Two arms of one binary: a coordinate over a
    // panel and a coordinate over the world.
    if (const auto pointer = probe_pointer(); pointer.has_value()) {
        io.AddMousePosEvent(pointer->x, pointer->y);
    } else if (!input.is_cursor_captured()) {
        const glm::vec2 m = input.mouse_position();
        io.AddMousePosEvent(m.x, m.y);
    } else {
        // A CAPTURED CURSOR HAS NO POSITION worth reporting: it is pinned to the
        // window centre while raw deltas drive the camera. Telling ImGui "off
        // screen" is the truth and stops a panel from lighting up under a
        // pointer nobody can see.
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
    io.AddMouseButtonEvent(0, input.is_down(platform::MouseButton::LEFT));
    io.AddMouseButtonEvent(1, input.is_down(platform::MouseButton::RIGHT));
    io.AddMouseButtonEvent(2, input.is_down(platform::MouseButton::MIDDLE));
    if (probe_wants_click()) {
        // Down for ten frames, then up. A button fires on RELEASE in ImGui, so
        // a door that only held the button down would photograph a chip that
        // looks pressed and never acts — the same "looks right, does nothing"
        // this file has already shipped once.
        static uint32_t frame = 0;
        ++frame;
        io.AddMouseButtonEvent(0, frame >= 30 && frame < 40);
    }
    const glm::vec2 wheel = input.scroll_delta();
    if (wheel.x != 0.0f || wheel.y != 0.0f) {
        io.AddMouseWheelEvent(wheel.x, wheel.y);
    }
    for (const KeyPair& k : KEY_TABLE) {
        if (input.was_pressed(k.ours)) {
            io.AddKeyEvent(k.theirs, true);
        }
        if (input.was_released(k.ours)) {
            io.AddKeyEvent(k.theirs, false);
        }
    }
    io.AddKeyEvent(ImGuiMod_Ctrl, input.is_down(platform::Key::LEFT_CONTROL)
                                      || input.is_down(platform::Key::RIGHT_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, input.is_down(platform::Key::LEFT_SHIFT)
                                       || input.is_down(platform::Key::RIGHT_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, input.is_down(platform::Key::LEFT_ALT)
                                     || input.is_down(platform::Key::RIGHT_ALT));
    io.AddKeyEvent(ImGuiMod_Super, input.is_down(platform::Key::LEFT_SUPER)
                                       || input.is_down(platform::Key::RIGHT_SUPER));
    // TEXT COMES FROM text_input(), NOT FROM THE KEY ENUM. That is the channel
    // the OS layout and the IME already resolved, so a search box takes «дверь»
    // on a Russian layout — which the physical Key enum cannot express at all.
    for (const uint32_t cp : input.text_input()) {
        io.AddInputCharacter(cp);
    }

    text_arena().clear();
    tool_changed_ = false; // set by set_tool during this frame, read after it
    // THE STRIPS ARE RECOMPUTED FROM SCRATCH EVERY FRAME, never accumulated: a
    // panel closed on frame N must give its column back on frame N, and an
    // inset that only ever grows is the bug that would make the world shrink a
    // little every time somebody opened and closed the parts menu.
    insets_ = EditorInsets{};
    placed_.clear();
    ImGui::NewFrame();
    frame_open_ = true;
    if (visible_) {
        if (toolbar_) {
            draw_toolbar();
        }
        layout_panels();
    }
    if (const int mode = layout_check(); mode != 0) {
        // ON ONE SETTLED FRAME, not every frame: the first frame has no measured
        // toolbar height yet, and a report per frame is a log nobody reads.
        static uint32_t f = 0;
        if (++f == 60) {
            std::fprintf(stderr, "[editor-ui] проверка раскладки, рука %d%s\n", mode,
                         mode == 2 ? " (КОНТРОЛЬ: верхняя полоса ничего не отъедает,"
                                     " пересечение ОБЯЗАНО быть)" : "");
            report_layout_overlaps();
        }
    }
    // READ THE FLAGS AFTER THE PANELS DREW. WantCaptureMouse is decided by what
    // is under the pointer, and what is under the pointer is not known until
    // the windows for this frame exist.
    wants_mouse_ = visible_ && io.WantCaptureMouse;
    wants_keyboard_ = visible_ && io.WantTextInput;
}

const char* EditorUi::tool_name(EditorTool tool) {
    switch (tool) {
    case EditorTool::HeightBrush:  return tr("editor.tool.height");
    case EditorTool::SurfacePaint: return tr("editor.tool.paint");
    case EditorTool::SelectObject: return tr("editor.tool.select");
    case EditorTool::PlaceObject:  return tr("editor.tool.place");
    case EditorTool::Look:
    case EditorTool::Count:
        break;
    }
    return tr("editor.tool.look");
}

void EditorUi::set_tool(EditorTool tool) {
    if (tool == tool_ || tool == EditorTool::Count) {
        return;
    }
    tool_ = tool;
    tool_changed_ = true;
}

void EditorUi::draw_toolbar() {
    // THE ORDER IS THE USER'S OWN NUMBERING, so "press 3" and "the third chip"
    // are the same thing. Look is his 5 and sits last, even though it is the
    // default: a default that moves to the front would renumber the other four
    // every time somebody re-read his message.
    struct Chip {
        EditorTool tool;
        const char* digit;
    };
    static const Chip CHIPS[] = {
        {EditorTool::HeightBrush, "1"},  {EditorTool::SurfacePaint, "2"},
        {EditorTool::SelectObject, "3"}, {EditorTool::PlaceObject, "4"},
        {EditorTool::Look, "5"},
    };

    const ImGuiIO& io = ImGui::GetIO();
    // A FULL-WIDTH STRIP PINNED TO THE TOP EDGE, and it RESERVES that strip:
    // the world starts underneath it, and so does everything else that asks.
    //
    // TWO EARLIER VERSIONS OF THIS LINE ARE THE ARGUMENT FOR THE WHOLE LAYOUT.
    // The first floated at top centre and landed ON the debug readout's plate;
    // the second was nudged down 44 units to clear it. Nudging is what you do
    // when nobody owns the layout — the number was right for that readout, on
    // that day, and the next painter would have had to nudge again against a
    // number he could not see. Now the height is MEASURED after drawing and
    // published as insets().top.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("##editor.toolbar", nullptr, flags)) {
        for (std::size_t i = 0; i < std::size(CHIPS); ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            const bool active = CHIPS[i].tool == tool_;
            // WORDS, NOT ICONS. Five glyphs nobody has learned yet are five
            // guesses; the strip is wide enough for the words, and the day it
            // is not, the words are what the tooltip would have said anyway.
            char label[96];
            std::snprintf(label, sizeof(label), "%s  %s##tool%zu", CHIPS[i].digit,
                          tool_name(CHIPS[i].tool), i);
            if (active) {
                // The verdict's green again — one colour, one meaning across
                // the whole tool.
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.27f, 0.44f, 0.32f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImVec4(0.33f, 0.52f, 0.38f, 1.0f));
            }
            if (ImGui::Button(label)) {
                set_tool(CHIPS[i].tool);
            }
            if (active) {
                ImGui::PopStyleColor(2);
            }
        }
    }
    // MEASURED, NOT ASSUMED. The strip's height follows the font, the style's
    // padding and the display scale; a constant here would be right until the
    // first of those moved and would then hand everyone else a wrong number
    // with total confidence.
    toolbar_height_ = ImGui::GetWindowSize().y;
    {
        const ImVec2 pos = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        record_rect("toolbar", pos.x, pos.y, size.x, size.y);
    }
    ImGui::End();
}

void EditorUi::record_rect(const std::string& id, float x, float y, float w,
                           float h) {
    placed_.push_back(PlacedRect{id, x, y, w, h});
}

void EditorUi::report_layout_overlaps() const {
    // AREA OF INTERSECTION, IN SQUARE LOGICAL UNITS, for every pair. "Nothing
    // looks like it overlaps" survives exactly until the first window size
    // nobody photographed; a number does not care what the screen is.
    double worst = 0.0;
    std::string worst_pair;
    for (std::size_t i = 0; i < placed_.size(); ++i) {
        for (std::size_t j = i + 1; j < placed_.size(); ++j) {
            const PlacedRect& a = placed_[i];
            const PlacedRect& b = placed_[j];
            const float ox = std::max(0.0f, std::min(a.x + a.w, b.x + b.w)
                                                - std::max(a.x, b.x));
            const float oy = std::max(0.0f, std::min(a.y + a.h, b.y + b.h)
                                                - std::max(a.y, b.y));
            const double area = static_cast<double>(ox) * oy;
            std::fprintf(stderr, "[editor-ui] раскладка: %s x %s = %.1f кв.ед.\n",
                         a.id.c_str(), b.id.c_str(), area);
            if (area > worst) {
                worst = area;
                worst_pair = a.id + " x " + b.id;
            }
        }
    }
    const EditorRect wr = world_rect();
    std::fprintf(stderr,
                 "[editor-ui] раскладка: полос %zu, худшее пересечение %.1f кв.ед. "
                 "(%s); отъедено сверху %.1f слева %.1f справа %.1f снизу %.1f; "
                 "мир %.0fx%.0f от (%.0f, %.0f)\n",
                 placed_.size(), worst,
                 worst_pair.empty() ? "-" : worst_pair.c_str(), insets_.top,
                 insets_.left, insets_.right, insets_.bottom, wr.w, wr.h, wr.x, wr.y);
}

void EditorUi::layout_panels() {
    // DOCKED STRIPS, NOT FLOATING WINDOWS (user, 17.08.2026: «пусть кнопки и
    // прочее инструментов рисуется вдоль экрана как приложухи старых
    // операционных систем виндовс»). Every open panel is assigned a rectangle
    // computed HERE, and the world gets what is left — which is the whole
    // difference between "nothing overlaps today" and "nothing can overlap".
    //
    // The windows are therefore NoMove and NoResize with position and size set
    // Always, not FirstUseEver. A panel the user can drag is a panel the user
    // can drag ON TOP of another one, and then the guarantee this function
    // exists to provide is his problem instead of ours.
    const ImGuiIO& io = ImGui::GetIO();
    const float w = io.DisplaySize.x;
    const float h = io.DisplaySize.y;
    constexpr float GAP = 6.0f; // between strips, so borders never share a pixel

    // FIRST PASS: how wide is each column, and how many panels share it. A
    // column is as wide as its widest panel asked to be, because a narrower
    // neighbour looks like a mistake while a clipped one IS one.
    float left_w = 0.0f;
    float right_w = 0.0f;
    float bottom_h = 0.0f;
    int left_n = 0;
    int right_n = 0;
    int bottom_n = 0;
    for (const EditorPanel& p : panels_) {
        if (!p.open || !p.draw) {
            continue;
        }
        switch (p.side) {
        case EditorPanelSide::Left:
            left_w = std::max(left_w, p.extent_px);
            ++left_n;
            break;
        case EditorPanelSide::Right:
            right_w = std::max(right_w, p.extent_px);
            ++right_n;
            break;
        case EditorPanelSide::Bottom:
            bottom_h = std::max(bottom_h, p.extent_px);
            ++bottom_n;
            break;
        case EditorPanelSide::Floating:
            break;
        }
    }
    // A COLUMN NEVER EATS MORE THAN A THIRD OF THE WINDOW. On a small window two
    // 380-unit columns would leave the world a slit, and the world is the thing
    // being edited.
    const float max_side = w / 3.0f;
    left_w = std::min(left_w, max_side);
    right_w = std::min(right_w, max_side);
    bottom_h = std::min(bottom_h, h / 3.0f);

    // ARM 2 OF THE INSTRUMENT drops the top reservation and nothing else, so
    // the panels climb onto the toolbar and the overlap becomes measurable.
    insets_.top = layout_check() == 2 ? 0.0f : toolbar_height_;
    insets_.left = left_n > 0 ? left_w + GAP : 0.0f;
    insets_.right = right_n > 0 ? right_w + GAP : 0.0f;
    insets_.bottom = bottom_n > 0 ? bottom_h + GAP : 0.0f;

    // SECOND PASS: place. The columns run from under the toolbar to the bottom
    // strip, and each panel takes an equal share of its column's height, so two
    // panels on one edge meet at a border instead of on top of each other.
    const float col_top = insets_.top;
    const float col_bottom = h - insets_.bottom;
    const float col_h = std::max(col_bottom - col_top, 0.0f);
    const float left_share = left_n > 0 ? col_h / static_cast<float>(left_n) : 0.0f;
    const float right_share = right_n > 0 ? col_h / static_cast<float>(right_n) : 0.0f;
    const float bottom_share =
        bottom_n > 0 ? (w - insets_.left - insets_.right) / static_cast<float>(bottom_n)
                     : 0.0f;
    float left_y = col_top;
    float right_y = col_top;
    float bottom_x = insets_.left;
    int index = 0;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                                   | ImGuiWindowFlags_NoCollapse
                                   | ImGuiWindowFlags_NoSavedSettings;
    for (EditorPanel& p : panels_) {
        if (!p.open || !p.draw) {
            continue;
        }
        ImGuiWindowFlags f = flags;
        switch (p.side) {
        case EditorPanelSide::Right:
            ImGui::SetNextWindowPos(ImVec2(w - right_w, right_y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(right_w, right_share - GAP),
                                     ImGuiCond_Always);
            right_y += right_share;
            break;
        case EditorPanelSide::Left:
            ImGui::SetNextWindowPos(ImVec2(0.0f, left_y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(left_w, left_share - GAP), ImGuiCond_Always);
            left_y += left_share;
            break;
        case EditorPanelSide::Bottom:
            ImGui::SetNextWindowPos(ImVec2(bottom_x, h - bottom_h), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(bottom_share - GAP, bottom_h),
                                     ImGuiCond_Always);
            bottom_x += bottom_share;
            break;
        case EditorPanelSide::Floating:
            // THE ONE KIND THAT MAY OVERLAP, and it reserves nothing. Use it
            // only for something that genuinely must move (a colour picker
            // following the cursor); anything permanent belongs on an edge.
            f = ImGuiWindowFlags_NoSavedSettings;
            break;
        }
        // THE WINDOW IS OPENED HERE, NOT BY THE PANEL. One place decides what a
        // panel of this tool looks like; a panel that opened its own window
        // would be free to disagree, and twelve of them written by three agents
        // certainly would.
        bool open = true;
        // The title carries the id after ## so two panels may share a title and
        // still keep separate positions — ImGui identifies windows by name.
        const std::string label = std::string(tr(p.title_key.c_str())) + "###" + p.id;
        if (ImGui::Begin(label.c_str(), &open, f)) {
            p.draw();
        }
        {
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            record_rect(p.id, pos.x, pos.y, size.x, size.y);
        }
        ImGui::End();
        p.open = open; // the title bar's X closes it, like any other tool
        ++index;
    }
    (void)index;
}

EditorRect EditorUi::world_rect() const {
    const ImGuiIO& io = ImGui::GetIO();
    const float w = io.DisplaySize.x;
    const float h = io.DisplaySize.y;
    EditorRect r;
    r.x = insets_.left;
    r.y = insets_.top;
    r.w = std::max(w - insets_.left - insets_.right, 0.0f);
    r.h = std::max(h - insets_.top - insets_.bottom, 0.0f);
    return r;
}

EditorRect EditorUi::world_rect_norm() const {
    const ImGuiIO& io = ImGui::GetIO();
    const float w = io.DisplaySize.x;
    const float h = io.DisplaySize.y;
    if (w <= 0.0f || h <= 0.0f) {
        return EditorRect{0.0f, 0.0f, 1.0f, 1.0f};
    }
    const EditorRect r = world_rect();
    return EditorRect{r.x / w, r.y / h, r.w / w, r.h / h};
}

void EditorUi::end_frame() {
    if (!ready_ || !frame_open_) {
        return;
    }
    // Render ALWAYS, even hidden: ImGui must see a matched NewFrame/Render pair
    // or the next frame asserts. Hidden simply means no windows were declared,
    // so the draw data is empty and the backend does nothing.
    ImGui::Render();
    frame_open_ = false;
}

void EditorUi::add_panel(EditorPanel panel) {
    for (EditorPanel& p : panels_) {
        if (p.id == panel.id) {
            const bool was_open = p.open;
            p = std::move(panel);
            p.open = was_open; // re-registering must not slam a panel shut
            return;
        }
    }
    panels_.push_back(std::move(panel));
}

bool EditorUi::panel_open(const std::string& id) const {
    for (const EditorPanel& p : panels_) {
        if (p.id == id) {
            return p.open;
        }
    }
    return false;
}

void EditorUi::set_panel_open(const std::string& id, bool open) {
    for (EditorPanel& p : panels_) {
        if (p.id == id) {
            p.open = open;
            return;
        }
    }
}

void EditorUi::toggle_panel(const std::string& id) {
    for (EditorPanel& p : panels_) {
        if (p.id == id) {
            p.open = !p.open;
            return;
        }
    }
}

EditorTexture EditorUi::make_texture(std::uint32_t width, std::uint32_t height,
                                     const std::uint8_t* rgba) {
    return ready_ ? platform::imgui_backend_create_texture(width, height, rgba) : 0;
}

void EditorUi::update_texture(EditorTexture texture, std::uint32_t width,
                              std::uint32_t height, const std::uint8_t* rgba) {
    if (ready_) {
        platform::imgui_backend_update_texture(texture, width, height, rgba);
    }
}

void EditorUi::drop_texture(EditorTexture texture) {
    if (ready_) {
        platform::imgui_backend_destroy_texture(texture);
    }
}

EditorTexture EditorUi::texture_of(platform::TextureHandle handle) const {
    if (!ready_ || renderer_ == nullptr || !handle.valid()) {
        return 0;
    }
    const uint32_t native = renderer_->native_texture_handle(handle);
    return native == 0xFFFFFFFFu ? 0 : platform::imgui_backend_wrap_native(native);
}

void EditorUi::image(EditorTexture texture, float w, float h) {
    if (texture == 0) {
        ImGui::Dummy(ImVec2(w, h));
        return;
    }
    ImGui::Image(to_imgui_id(texture), ImVec2(w, h));
}

bool EditorUi::image_button(const char* id, EditorTexture texture, float w, float h) {
    if (texture == 0) {
        // A BUTTON WITH NO PICTURE IS STILL A BUTTON. A thumbnail that has not
        // been drawn yet must not make the part unclickable — that would make
        // the menu feel broken exactly while it is busy.
        return ImGui::Button(id, ImVec2(w, h));
    }
    return ImGui::ImageButton(id, to_imgui_id(texture), ImVec2(w, h));
}

void EditorUi::set_text_source(EditorTextSource source) {
    text_source() = source;
}

const char* EditorUi::tr(const char* key) {
    const EditorTextSource src = text_source();
    text_arena().emplace_back(src != nullptr ? std::string(src(key))
                                             : std::string(key));
    return text_arena().back().c_str();
}

} // namespace dfn::app
