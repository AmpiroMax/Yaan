/*
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/ImGuiBackend.h

Responsibility:
- The bgfx rendering backend for Dear ImGui. bgfx ships none — its own lives in
  the examples folder and is not part of the library — so this is ours.

Key items:
- imgui_backend_init / shutdown / ready: the program, the vertex layout, the
  sampler and the font atlas texture.
- imgui_backend_submit(view, w, h): turns the CURRENT frame's ImGui draw lists
  into bgfx draw calls in one view. Called by BgfxRenderer::end_frame, TWICE —
  see the note below, it is the reason this function takes a view at all.
- imgui_backend_create_texture / update / destroy / wrap_native: pictures a
  panel wants to show, and pictures the renderer already owns.

WHY THE SUBMIT TAKES A VIEW AND IS CALLED TWICE. Every acceptance frame in this
project is read back from `capture_fb`, an internal-sized target the upscale is
submitted into a second time — never from the backbuffer, because a sleeping
display hands out no drawable and the saved image comes back black (see the
long note in BgfxRendererFrame.cpp). An interface drawn only into the window
would therefore appear on NO screenshot, and around here a panel that cannot be
photographed does not exist. The draw lists carry interface pixels and the
projection is built per call, so the same lists land correctly in both targets
with nothing copied and no second layout pass.

Dependencies:
- Uses: bgfx, Dear ImGui, the generated vs_imgui/fs_imgui headers.
- Used by: BgfxRendererFrame.cpp (submit), engine/app EditorUi (lifetime and
  textures).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. NO bgfx AND NO ImGui TYPE MAY APPEAR IN
  THIS HEADER (Rule 1): engine/app includes it, and engine/app does not link
  bgfx. Textures leave as an opaque uint64_t for exactly that reason.
- The functions read the ImGui context that engine/app created. There is one
  context because there is one linked copy of the library; do not create a
  second one here.
*/

#pragma once

#include <cstdint>

namespace dfn::platform {

/// Builds the program, the vertex layout, the sampler and the font atlas from
/// the ImGui context that already exists. Returns false if the shader program
/// is unavailable (non-Apple builds ship no embedded shaders this stage), in
/// which case every other call here is a no-op and the app runs without an
/// editor interface rather than crashing.
/// СЛОВООХОТЛИВОСТЬ ЗАПУСКА, и по умолчанию её НЕТ.
///
/// ПОТОМУ ЧТО ПУТЬ ИГРОКА ЧИСТ. Строка про атлас шрифта — это ответ на вопрос
/// «влезли ли глифы», который задаёт тот, кто правит панель редактора; игроку,
/// открывшему главное меню, она приезжает в терминал вместе с восемью строками
/// EditorUi и читается как сообщение об ошибке. Один выключатель на всю
/// диагностику интерфейса — здесь и в EditorUi::set_diagnostics, которая его
/// же и щёлкает: два выключателя на одно решение разошлись бы первым же.
void imgui_backend_set_diagnostics(bool on);

[[nodiscard]] bool imgui_backend_init();

void imgui_backend_shutdown();

[[nodiscard]] bool imgui_backend_ready();

/// Draws the current ImGui frame's lists into `view`, scaled to a target of
/// `target_width` x `target_height` pixels. Does nothing when the backend is
/// down or ImGui produced no lists this frame.
void imgui_backend_submit(uint16_t view, uint32_t target_width,
                          uint32_t target_height);

/// An RGBA8 picture owned by the caller. Returns 0 on failure.
[[nodiscard]] uint64_t imgui_backend_create_texture(uint32_t width, uint32_t height,
                                                    const uint8_t* rgba);
void imgui_backend_update_texture(uint64_t texture, uint32_t width, uint32_t height,
                                  const uint8_t* rgba);
void imgui_backend_destroy_texture(uint64_t texture);

/// Wraps a texture the RENDERER owns (IRenderer::native_texture_handle) so a
/// panel can draw it. Nothing is copied and nothing is owned.
[[nodiscard]] uint64_t imgui_backend_wrap_native(uint32_t native_handle);

} // namespace dfn::platform
