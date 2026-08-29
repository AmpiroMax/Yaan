/*
Module: tests
File: tests/render/NullBackendTests.cpp

Responsibility:
- Smoke tests for the null window/input/renderer backends: every IWindow /
  IInput / IRenderer postcondition holds headless (Rule 3).

Key items:
- doctest cases over the three null factories.

Dependencies:
- Uses: doctest, null backends of window/input/render.
- Used by: ctest (render_null_backends).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/

#include "engine/platform/input/sources/null/CreateNullInput.h"
#include "engine/platform/render/sources/null/CreateNullRenderer.h"
#include "engine/platform/window/sources/null/CreateNullWindow.h"

#include <doctest/doctest.h>

#include <array>
#include <vector>

using namespace dfn::platform;

TEST_CASE("null window: full lifecycle without an OS") {
    auto window = create_null_window();
    REQUIRE(window != nullptr);

    WindowInitParams params;
    params.width = 1280;
    params.height = 720;
    params.title = "test";
    REQUIRE(window->init(params));

    CHECK(window->native_handle() == nullptr);
    CHECK(window->framebuffer_size() == glm::uvec2{1280, 720});
    CHECK_FALSE(window->consume_resize());
    CHECK_FALSE(window->should_close());
    window->poll_events();
    window->request_close();
    CHECK(window->should_close());
    window->shutdown();
}

TEST_CASE("null input: nothing pressed, zero deltas, capture remembered") {
    auto input = create_null_input();
    REQUIRE(input != nullptr);
    input->update();
    CHECK_FALSE(input->is_down(Key::W));
    CHECK_FALSE(input->was_pressed(Key::ESCAPE));
    CHECK_FALSE(input->is_down(MouseButton::LEFT));
    CHECK(input->mouse_delta() == glm::vec2{0.0f, 0.0f});
    CHECK(input->scroll_delta() == glm::vec2{0.0f, 0.0f});
    CHECK_FALSE(input->is_cursor_captured());
    input->set_cursor_captured(true);
    CHECK(input->is_cursor_captured());
}

TEST_CASE("null renderer: valid-but-inert handles, screenshot returns false") {
    auto renderer = create_null_renderer();
    REQUIRE(renderer != nullptr);

    RendererInitParams params; // null native handle is fine here (Rule 3)
    params.framebuffer_width = 640;
    params.framebuffer_height = 360;
    params.internal_width = 320;
    params.internal_height = 180;
    REQUIRE(renderer->init(params));

    const std::array<Vertex, 3> verts{};
    const std::array<uint32_t, 3> idx{0, 1, 2};
    const MeshHandle mesh = renderer->create_mesh(verts, idx);
    CHECK(mesh.valid());

    const std::vector<uint8_t> pixels(4 * 4 * 4, 0xFF);
    const TextureHandle tex =
        renderer->create_texture(4, 4, TextureFormat::RGBA8, pixels);
    CHECK(tex.valid());
    CHECK(tex.id != mesh.id); // ids never collide across live resources

    const ProgramHandle prog = renderer->load_program("terrain");
    CHECK(prog.valid());

    renderer->begin_frame(glm::mat4(1.0f), glm::mat4(1.0f));
    renderer->submit(mesh, prog, glm::mat4(1.0f));
    renderer->submit(mesh, prog, glm::mat4(1.0f), tex);
    renderer->debug_line({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0xFFFFFFFFu);
    renderer->end_frame();

    CHECK_FALSE(renderer->save_screenshot("/nonexistent/nope.png"));
    renderer->reload_shaders();
    renderer->resize(1920, 1080);

    renderer->destroy_mesh(mesh);
    renderer->destroy_texture(tex);
    renderer->destroy_program(prog);
    renderer->shutdown();
}
