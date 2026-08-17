/*
Created: 09:08:2026 - 00:45:00
Last updated: 18:08:2026 - 00:24:58
Module: engine/platform/input
File: engine/platform/input/sources/glfw/GlfwInput.cpp

Responsibility:
- GlfwInput implementation + create_glfw_input factory. The only input file
  that includes GLFW headers (Rule 1). Maps engine Key/MouseButton codes to
  GLFW codes.

Key items:
- key table (engine Key -> GLFW key), GlfwInput methods, scroll callback,
  create_glfw_input().

Dependencies:
- Uses: GLFW 3.4, GlfwInput.h, CreateGlfwInput.h, GlfwWindow.h (factory cast).
- Used by: dfn_platform_input target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- The Key enum is append-only; extend to_glfw_key in the same order.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation (raw mouse motion
  when captured, scroll via callback, snapshot edge detection).
- 14:08:2026 - 16:59:44: Added the GLFW char callback feeding text_accum_, the
  per-frame text snapshot in update(), text_input() accessor, and the
  DFN_TEXT_INPUT_LOG=1 stderr door-probe.
- 18:08:2026 - 00:24:58: ПОВТОРНЫЙ ЗАПРОС ЗАХВАТА БОЛЬШЕ НЕ СОБЫТИЕ, и это та
  самая поломка, из-за которой камера редактора не поворачивалась ВОВСЕ.
  set_cursor_captured сбрасывал have_prev_pos_ на КАЖДЫЙ вызов. Для СМЕНЫ
  режима это верно: захват телепортирует курсор, и первая разность после
  телепорта — мусор. Но App держит захват УТВЕРЖДЕНИЕМ, а не событием: пока
  человек в редакторе, он просит захват каждым кадром. Признак «предыдущее
  положение известно» не доживал ни до одного кадра, update() честно отдавал
  ноль, и камера получала нулевое смещение всегда.
  ПОЧЕМУ ЭТО ЖИЛО ТРИ ЗАХОДА: все автоматические прогоны идут через двери, а
  двери зовут unattended_run() и захват НЕ ЗАПРАШИВАЮТ ни разу. Значит любой
  наш прибор — и мой собственный, заведённый часом раньше, — мерил здоровую
  руку. Отказ был доступен ровно одному наблюдателю: человеку за игрой.
  Теперь на него есть tests/platform/CursorCaptureTests.cpp — единственный
  рукав в дереве с НАСТОЯЩИМ окном GLFW; рука двигает курсор сама через
  glfwSetCursorPos. Три руки: захват один раз — 39 кадров со смещением из 40;
  захват каждым кадром до правки — 0 из 40; после правки — 39 из 40.
*/

#include "engine/platform/input/sources/glfw/GlfwInput.h"

#include "engine/platform/input/sources/glfw/CreateGlfwInput.h"
#include "engine/platform/window/sources/glfw/GlfwWindow.h"

#include <GLFW/glfw3.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace dfn::platform {

namespace {

/// Engine Key -> GLFW key code. Indexed by Key value; order mirrors the enum.
int to_glfw_key(Key key) {
    switch (key) {
        case Key::A: return GLFW_KEY_A;
        case Key::B: return GLFW_KEY_B;
        case Key::C: return GLFW_KEY_C;
        case Key::D: return GLFW_KEY_D;
        case Key::E: return GLFW_KEY_E;
        case Key::F: return GLFW_KEY_F;
        case Key::G: return GLFW_KEY_G;
        case Key::H: return GLFW_KEY_H;
        case Key::I: return GLFW_KEY_I;
        case Key::J: return GLFW_KEY_J;
        case Key::K: return GLFW_KEY_K;
        case Key::L: return GLFW_KEY_L;
        case Key::M: return GLFW_KEY_M;
        case Key::N: return GLFW_KEY_N;
        case Key::O: return GLFW_KEY_O;
        case Key::P: return GLFW_KEY_P;
        case Key::Q: return GLFW_KEY_Q;
        case Key::R: return GLFW_KEY_R;
        case Key::S: return GLFW_KEY_S;
        case Key::T: return GLFW_KEY_T;
        case Key::U: return GLFW_KEY_U;
        case Key::V: return GLFW_KEY_V;
        case Key::W: return GLFW_KEY_W;
        case Key::X: return GLFW_KEY_X;
        case Key::Y: return GLFW_KEY_Y;
        case Key::Z: return GLFW_KEY_Z;
        case Key::NUM_0: return GLFW_KEY_0;
        case Key::NUM_1: return GLFW_KEY_1;
        case Key::NUM_2: return GLFW_KEY_2;
        case Key::NUM_3: return GLFW_KEY_3;
        case Key::NUM_4: return GLFW_KEY_4;
        case Key::NUM_5: return GLFW_KEY_5;
        case Key::NUM_6: return GLFW_KEY_6;
        case Key::NUM_7: return GLFW_KEY_7;
        case Key::NUM_8: return GLFW_KEY_8;
        case Key::NUM_9: return GLFW_KEY_9;
        case Key::ESCAPE: return GLFW_KEY_ESCAPE;
        case Key::ENTER: return GLFW_KEY_ENTER;
        case Key::TAB: return GLFW_KEY_TAB;
        case Key::BACKSPACE: return GLFW_KEY_BACKSPACE;
        case Key::SPACE: return GLFW_KEY_SPACE;
        case Key::LEFT: return GLFW_KEY_LEFT;
        case Key::RIGHT: return GLFW_KEY_RIGHT;
        case Key::UP: return GLFW_KEY_UP;
        case Key::DOWN: return GLFW_KEY_DOWN;
        case Key::LEFT_SHIFT: return GLFW_KEY_LEFT_SHIFT;
        case Key::RIGHT_SHIFT: return GLFW_KEY_RIGHT_SHIFT;
        case Key::LEFT_CONTROL: return GLFW_KEY_LEFT_CONTROL;
        case Key::RIGHT_CONTROL: return GLFW_KEY_RIGHT_CONTROL;
        case Key::LEFT_ALT: return GLFW_KEY_LEFT_ALT;
        case Key::RIGHT_ALT: return GLFW_KEY_RIGHT_ALT;
        case Key::LEFT_SUPER: return GLFW_KEY_LEFT_SUPER;
        case Key::RIGHT_SUPER: return GLFW_KEY_RIGHT_SUPER;
        case Key::INSERT: return GLFW_KEY_INSERT;
        case Key::DELETE: return GLFW_KEY_DELETE;
        case Key::HOME: return GLFW_KEY_HOME;
        case Key::END: return GLFW_KEY_END;
        case Key::PAGE_UP: return GLFW_KEY_PAGE_UP;
        case Key::PAGE_DOWN: return GLFW_KEY_PAGE_DOWN;
        case Key::F1: return GLFW_KEY_F1;
        case Key::F2: return GLFW_KEY_F2;
        case Key::F3: return GLFW_KEY_F3;
        case Key::F4: return GLFW_KEY_F4;
        case Key::F5: return GLFW_KEY_F5;
        case Key::F6: return GLFW_KEY_F6;
        case Key::F7: return GLFW_KEY_F7;
        case Key::F8: return GLFW_KEY_F8;
        case Key::F9: return GLFW_KEY_F9;
        case Key::F10: return GLFW_KEY_F10;
        case Key::F11: return GLFW_KEY_F11;
        case Key::F12: return GLFW_KEY_F12;
        case Key::GRAVE: return GLFW_KEY_GRAVE_ACCENT;
        case Key::MINUS: return GLFW_KEY_MINUS;
        case Key::EQUAL: return GLFW_KEY_EQUAL;
        case Key::LEFT_BRACKET: return GLFW_KEY_LEFT_BRACKET;
        case Key::RIGHT_BRACKET: return GLFW_KEY_RIGHT_BRACKET;
        case Key::SEMICOLON: return GLFW_KEY_SEMICOLON;
        case Key::APOSTROPHE: return GLFW_KEY_APOSTROPHE;
        case Key::COMMA: return GLFW_KEY_COMMA;
        case Key::PERIOD: return GLFW_KEY_PERIOD;
        case Key::SLASH: return GLFW_KEY_SLASH;
        case Key::BACKSLASH: return GLFW_KEY_BACKSLASH;
        case Key::UNKNOWN:
        case Key::COUNT: return GLFW_KEY_UNKNOWN;
    }
    return GLFW_KEY_UNKNOWN;
}

int to_glfw_button(MouseButton button) {
    switch (button) {
        case MouseButton::LEFT: return GLFW_MOUSE_BUTTON_LEFT;
        case MouseButton::RIGHT: return GLFW_MOUSE_BUTTON_RIGHT;
        case MouseButton::MIDDLE: return GLFW_MOUSE_BUTTON_MIDDLE;
        case MouseButton::COUNT: break;
    }
    return GLFW_MOUSE_BUTTON_LEFT;
}

} // namespace

// Free function (befriended) so the GLFW C callback can reach the accumulator.
void glfw_input_scroll_callback(GLFWwindow* window, double dx, double dy) {
    auto* self = static_cast<GlfwInput*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->scroll_accum_ += glm::vec2(static_cast<float>(dx), static_cast<float>(dy));
    }
}

// Befriended char callback: GLFW hands us finished Unicode codepoints already
// resolved through the OS keyboard layout and IME (so Cyrillic arrives here
// directly — we never touch UTF-8 bytes or scancodes). Accumulate; update()
// snapshots and clears, exactly like scroll.
void glfw_input_char_callback(GLFWwindow* window, unsigned int codepoint) {
    auto* self = static_cast<GlfwInput*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }
    self->text_accum_.push_back(static_cast<uint32_t>(codepoint));
    // Door-probe (Rule 27): with DFN_TEXT_INPUT_LOG=1 set, echo every accepted
    // codepoint to stderr so a live keystroke is observable end-to-end.
    static const bool log_enabled = std::getenv("DFN_TEXT_INPUT_LOG") != nullptr;
    if (log_enabled) {
        std::fprintf(stderr, "[DFN_TEXT_INPUT] codepoint U+%04X (%u)\n",
                     codepoint, codepoint);
    }
}

GlfwInput::GlfwInput(GLFWwindow* window) : window_(window) {
    assert(window_ != nullptr);
    // Callback policy (zone-internal agreement): input owns the user pointer
    // and the scroll callback; GlfwWindow claims neither.
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, &glfw_input_scroll_callback);
    glfwSetCharCallback(window_, &glfw_input_char_callback);
}

GlfwInput::~GlfwInput() {
    // The window may already be destroyed by GlfwWindow::shutdown; only detach
    // if it is still alive (app wiring destroys input before window).
    if (window_ != nullptr) {
        glfwSetScrollCallback(window_, nullptr);
        glfwSetCharCallback(window_, nullptr);
        glfwSetWindowUserPointer(window_, nullptr);
    }
}

void GlfwInput::update() {
    keys_prev_ = keys_curr_;
    buttons_prev_ = buttons_curr_;

    for (size_t i = 0; i < KEY_COUNT; ++i) {
        const int glfw_key = to_glfw_key(static_cast<Key>(i));
        keys_curr_[i] =
            glfw_key != GLFW_KEY_UNKNOWN && glfwGetKey(window_, glfw_key) == GLFW_PRESS;
    }
    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        buttons_curr_[i] =
            glfwGetMouseButton(window_, to_glfw_button(static_cast<MouseButton>(i)))
            == GLFW_PRESS;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window_, &x, &y);
    const glm::vec2 pos(static_cast<float>(x), static_cast<float>(y));
    // First update has no previous position — report zero delta, not a jump.
    mouse_delta_ = have_prev_pos_ ? pos - mouse_pos_ : glm::vec2(0.0f);
    mouse_pos_ = pos;
    have_prev_pos_ = true;

    scroll_delta_ = scroll_accum_;
    scroll_accum_ = {0.0f, 0.0f};

    // Snapshot the codepoints entered since the previous update() (the char
    // callbacks fired during IWindow::poll_events), then clear for next frame.
    text_curr_.swap(text_accum_);
    text_accum_.clear();
}

bool GlfwInput::is_down(Key key) const {
    return keys_curr_[static_cast<size_t>(key)];
}
bool GlfwInput::was_pressed(Key key) const {
    const auto i = static_cast<size_t>(key);
    return keys_curr_[i] && !keys_prev_[i];
}
bool GlfwInput::was_released(Key key) const {
    const auto i = static_cast<size_t>(key);
    return !keys_curr_[i] && keys_prev_[i];
}

bool GlfwInput::is_down(MouseButton button) const {
    return buttons_curr_[static_cast<size_t>(button)];
}
bool GlfwInput::was_pressed(MouseButton button) const {
    const auto i = static_cast<size_t>(button);
    return buttons_curr_[i] && !buttons_prev_[i];
}
bool GlfwInput::was_released(MouseButton button) const {
    const auto i = static_cast<size_t>(button);
    return !buttons_curr_[i] && buttons_prev_[i];
}

glm::vec2 GlfwInput::mouse_position() const {
    return mouse_pos_;
}
glm::vec2 GlfwInput::mouse_delta() const {
    return mouse_delta_;
}
glm::vec2 GlfwInput::scroll_delta() const {
    return scroll_delta_;
}

void GlfwInput::set_cursor_captured(bool captured) {
    // ПОВТОРНЫЙ ВЫЗОВ С ТЕМ ЖЕ ЗНАЧЕНИЕМ — НЕ СОБЫТИЕ, И ЭТО НЕ ПРИДИРКА, А
    // РАЗБОР ОТКАЗА. Строка have_prev_pos_ = false ниже верна для СМЕНЫ режима:
    // захват телепортирует курсор, и первая разность после телепорта — мусор.
    // Но App держит захват УТВЕРЖДЕНИЕМ, а не событием: пока человек в
    // редакторе, он зовёт set_cursor_captured(true) КАЖДЫЙ КАДР. При безусловном
    // сбросе признак «предыдущее положение известно» не доживал ни до одного
    // кадра, и update() ниже честно отдавал НОЛЬ:
    //     mouse_delta_ = have_prev_pos_ ? pos - mouse_pos_ : vec2(0)
    // Камера получала нулевое смещение всегда и не поворачивалась вовсе —
    // пользователь писал «в режиме редактора не работает камера» три захода
    // подряд. Прогон через дверь DFN_EDITOR=1 эту поломку НЕ ВИДЕЛ: там
    // непритязательный прогон, и захват не запрашивается ни разу, поэтому
    // признак доживал и смещения доходили. Мой прибор мерил здоровый путь.
    const bool changed = captured_ != captured;
    captured_ = captured;
    glfwSetInputMode(window_, GLFW_CURSOR,
                     captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION,
                         captured ? GLFW_TRUE : GLFW_FALSE);
    }
    if (changed) {
        have_prev_pos_ = false; // смена режима телепортирует курсор: гасим скачок
    }
}

bool GlfwInput::is_cursor_captured() const {
    return captured_;
}

void GlfwInput::place_cursor(const glm::vec2& pos) {
    glfwSetCursorPos(window_, static_cast<double>(pos.x), static_cast<double>(pos.y));
    // Признак «предыдущее положение известно» НЕ трогаем: рукав ставит указатель
    // именно затем, чтобы следующее update() посчитало от него разность.
}

const std::vector<uint32_t>& GlfwInput::text_input() const {
    return text_curr_;
}

std::unique_ptr<IInput> create_glfw_input(IWindow& window) {
    auto* glfw_window = dynamic_cast<GlfwWindow*>(&window);
    assert(glfw_window != nullptr && "create_glfw_input needs a GlfwWindow");
    if (glfw_window == nullptr || glfw_window->glfw_handle() == nullptr) {
        return nullptr;
    }
    return std::make_unique<GlfwInput>(glfw_window->glfw_handle());
}

} // namespace dfn::platform
