/*
Created: 09:08:2026 - 00:16:00
Last updated: 27:08:2026 - 14:00:00
Module: engine/platform/window
File: engine/platform/window/interfaces/IWindow.h

Responsibility:
- The platform window contract (Rule 0): lifecycle, OS event pump, native handle
  for the renderer, framebuffer size and resize/close signalling. GLFW lives only
  behind it.

Key items:
- IWindow: init/shutdown, poll_events, should_close, native_handle,
  framebuffer_size, consume_resize.
- WindowInitParams: logical size, title, fullscreen/resizable flags.

Dependencies:
- Uses: C++ stdlib, glm (Rule 2). Nothing else.
- Used by: engine/app (owns the window, feeds RendererInitParams), engine/render
  (tour shutdown via request_close), tests (null backend).

Notes:
- Polling model, no callbacks: the app calls poll_events() once per frame, then
  reads state. Keeps the contract trivially implementable by the null backend and
  by any windowing library (Rule 4).
- Backends (stage 2): sources/glfw/ (real), sources/null/ (headless; runnable
  mode per Rule 3 — never a crash, sensible inert values).
- native_handle() returns the OS-level handle RendererInitParams expects
  (NSWindow* on macOS, HWND on Windows); null backend returns nullptr and the
  null renderer accepts that.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Public contract, frozen for the stage (Rule 26): changes only via group sync.
- Do not add GLFW types, includes, or assumptions to this header.
*/
/*
UPD:
- 09:08:2026 - 00:16:00: Initial stage-1 contract (render zone).
- 17:08:2026 - 16:27:55: set_fullscreen/is_fullscreen — полный экран во время работы, а не только при рождении окна.
- 17:08:2026 - 19:17:13: content_size() — размер содержимого в ЛОГИЧЕСКИХ единицах ОС, тех самых, в которых сообщается мышь. На Retina он вдвое меньше кадрового буфера, и всё, что обязано положить УКАЗАТЕЛЬ и КАРТИНКУ в одну систему координат, нуждается в обоих числах; интерфейс, который угадывает отношение, — это интерфейс с курсором, промахивающимся вдвое. Добавление с телом по умолчанию (правило 26).
- 18:08:2026 - 00:24:58: focus() — дать окну фокус. Добавление к контракту (правило 26) ради
  того же прибора: окно без фокуса не получает положения курсора, и стенд молча
  перестаёт мерить. Первая версия стенда именно так себя и вела — все три руки
  дали ноль, включая контрольную, и это единственное, что спасло от вывода
  «поломки нет».
- 27:08:2026 - 14:00:00: set_size(w, h) — размер окна во время работы
  (заказ владельца 27.08: страница настроек меняет разрешение окна). Ровно та
  же причина, по которой 17.08 появился set_fullscreen: настройка, которую
  нельзя применить сейчас, выглядит на экране как неработающая.
*/

#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <string>

namespace dfn::platform {

struct WindowInitParams {
    uint32_t width = 0;        // logical size, pixels (framebuffer may differ on HiDPI)
    uint32_t height = 0;
    std::string title;         // window title; caller resolves it via localization (Rule 5)
    bool fullscreen = false;
    bool resizable = true;
};

class IWindow {
public:
    virtual ~IWindow() = default;

    // Lifecycle ----------------------------------------------------------------
    [[nodiscard]] virtual bool init(const WindowInitParams& params) = 0;
    virtual void shutdown() = 0;

    // Pumps OS events. Call exactly once per frame, before IInput::update().
    virtual void poll_events() = 0;

    // Close signalling ---------------------------------------------------------
    // True once the user (or request_close) asked to close; the app owns the exit.
    [[nodiscard]] virtual bool should_close() const = 0;
    virtual void request_close() = 0; // e.g. the tour after its last screenshot

    // ДАЁТ ОКНУ ФОКУС. Заведено ради прибора: окно без фокуса не получает
    // положения курсора, и стенд, проверяющий смещение мыши, молча перестаёт
    // мерить -- первая его версия именно так и вела себя, все руки дали ноль.
    virtual void focus() = 0;

    // Renderer handoff ---------------------------------------------------------
    // OS-level handle for RendererInitParams::native_window_handle.
    // NSWindow* on macOS, HWND on Windows; nullptr from the null backend.
    [[nodiscard]] virtual void* native_handle() const = 0;

    // Framebuffer size in physical pixels (HiDPI-aware). Feeds RendererInitParams
    // and IRenderer::resize.
    [[nodiscard]] virtual glm::uvec2 framebuffer_size() const = 0;

    // CONTENT SIZE IN LOGICAL (OS) UNITS — what the mouse is measured in.
    //
    // On a Retina display these two differ by a factor of two, and everything
    // that has to put a POINTER and a PICTURE in the same coordinate system
    // needs both: IInput::mouse_position() reports logical units, the renderer
    // works in pixels, and an interface that guesses the ratio is an interface
    // whose cursor is off by 2x — the single most common way a tool feels
    // broken. Defaulted to the framebuffer size so this is purely additive
    // (Rule 26): a backend without a notion of scale answers "they are the
    // same", which is true for it.
    [[nodiscard]] virtual glm::uvec2 content_size() const {
        return framebuffer_size();
    }

    // Returns true if the framebuffer size changed since the previous call and
    // clears the flag (one consumer: the app, which forwards to IRenderer::resize).
    [[nodiscard]] virtual bool consume_resize() = 0;

    // FULLSCREEN AT RUNTIME. `fullscreen` at init only chose the mode the
    // window was BORN in; a player who wants the whole screen has to be able
    // to ask mid-game. Toggling is expected to change the framebuffer size, so
    // the next consume_resize() reports it like any other resize — no separate
    // path, and the renderer needs to know nothing about fullscreen.
    // A backend that cannot do it (the null window) answers false forever,
    // which is honest: nothing on screen, nothing to make full.
    virtual void set_fullscreen(bool on) = 0;
    [[nodiscard]] virtual bool is_fullscreen() const = 0;

    // РАЗМЕР ОКНА ВО ВРЕМЯ РАБОТЫ, в ЛОГИЧЕСКИХ единицах — тех же, что
    // WindowInitParams и content_size(), а не в пикселях кадрового буфера: на
    // Retina это разные числа вдвое, и настройка, заданная в одних и
    // прочитанная в других, удваивала бы окно с каждым применением.
    //
    // ЗАЧЕМ ОНА ПОЯВИЛАСЬ (заказ владельца 27.08: «в меню настроек добавь
    // возможность менять графику… разрешение окна»). До неё размер окна был
    // свойством РОЖДЕНИЯ окна, ровно как полный экран до 17.08, и по той же
    // причине это не годится: настройка, которую нельзя применить сейчас,
    // выглядит на экране как неработающая.
    //
    // В ПОЛНОМ ЭКРАНЕ НЕ ДЕЛАЕТ НИЧЕГО и не притворяется: размер окна там
    // задаёт монитор. Вызывающий обязан прочитать content_size() ПОСЛЕ вызова
    // и поверить ему, а не тому, что просил, — бэкенд имеет право зажать
    // размер по монитору.
    virtual void set_size(uint32_t width, uint32_t height) = 0;
};

} // namespace dfn::platform
