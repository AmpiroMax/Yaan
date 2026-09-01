/*
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/ImGuiBackend.cpp

Responsibility:
- The bgfx rendering backend for Dear ImGui declared in ImGuiBackend.h.

Dependencies:
- Uses: bgfx, Dear ImGui, generated vs_imgui_mtl.h / fs_imgui_mtl.h.
- Used by: BgfxRendererFrame.cpp, engine/app EditorUi.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE VERTICES CARRY INTERFACE PIXELS, NOT CLIP SPACE. The projection is built
  per submit from the target's size, which is what lets one frame's lists be
  drawn into the window and into the capture target with nothing recomputed.
  If you ever bake the projection into the vertices, the screenshot path loses
  the interface and nobody will notice for a week.
*/

#include "engine/platform/render/sources/bgfx/ImGuiBackend.h"

#include <bgfx/bgfx.h>
#include <imgui.h>

#include <cstdio>
#include <cstring>

#if defined(__APPLE__)
#include <vs_imgui_mtl.h>
#include <fs_imgui_mtl.h>
#endif

namespace dfn::platform {
namespace {

struct State {
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sampler = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle font = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout;
    bool ready = false;
};

State& state() {
    static State s;
    return s;
}

/// ImTextureID is `void*` on this pin and an integer on later ones. Both survive
/// a trip through uintptr_t, and going through one named pair of helpers means
/// the day the pin moves there is exactly one place to look.
[[nodiscard]] ImTextureID to_imgui_id(uint64_t value) {
    return (ImTextureID)(uintptr_t)value; // NOLINT: deliberate, see above
}

[[nodiscard]] uint64_t from_imgui_id(ImTextureID id) {
    return (uint64_t)(uintptr_t)id; // NOLINT: deliberate, see above
}

/// The low 16 bits are the bgfx texture index — the whole of a bgfx handle.
/// Bit 16 says "this is a texture at all", which a bare index cannot: bgfx
/// index 0 IS a valid handle, so without it a legitimate texture would be
/// indistinguishable from ImGui's "no texture" zero and would silently draw the
/// font atlas. Bit 17 marks a handle THIS FILE created (and therefore may
/// destroy), so a renderer-owned texture from wrap_native can never be freed
/// here.
constexpr uint64_t TEX_VALID_BIT = 1ull << 16;
constexpr uint64_t TEX_OWNED_BIT = 1ull << 17;

[[nodiscard]] bgfx::TextureHandle texture_of(uint64_t packed) {
    return bgfx::TextureHandle{static_cast<uint16_t>(packed & 0xFFFFull)};
}

bool g_diagnostics = false;

} // namespace

void imgui_backend_set_diagnostics(bool on) { g_diagnostics = on; }

bool imgui_backend_init() {
    State& s = state();
    if (s.ready) {
        return true;
    }
#if defined(__APPLE__)
    const bgfx::ShaderHandle vs =
        bgfx::createShader(bgfx::makeRef(vs_imgui_mtl, sizeof(vs_imgui_mtl)));
    const bgfx::ShaderHandle fs =
        bgfx::createShader(bgfx::makeRef(fs_imgui_mtl, sizeof(fs_imgui_mtl)));
    s.program = bgfx::createProgram(vs, fs, true);
#endif
    if (!bgfx::isValid(s.program)) {
        // SAID OUT LOUD. A silent failure here looks exactly like an editor
        // whose panels were never written, and that is a day of the wrong
        // search.
        std::fprintf(stderr, "[imgui] программа imgui недоступна — интерфейс "
                             "редактора не будет рисоваться\n");
        return false;
    }
    s.sampler = bgfx::createUniform("s_imguiTex", bgfx::UniformType::Sampler);

    s.layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    // THE FONT ATLAS. engine/app has already added the glyph ranges it needs by
    // the time this runs — the atlas is built here, once, from whatever it
    // asked for.
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (pixels != nullptr && width > 0 && height > 0) {
        s.font = bgfx::createTexture2D(
            static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_NONE,
            bgfx::copy(pixels, static_cast<uint32_t>(width) * height * 4));
        io.Fonts->SetTexID(
            to_imgui_id(static_cast<uint64_t>(s.font.idx) | TEX_VALID_BIT));
        if (g_diagnostics) {
            std::fprintf(stderr, "[imgui] атлас шрифта %dx%d, глифов %d\n", width,
                         height,
                         io.Fonts->Fonts.empty() ? 0
                                                 : io.Fonts->Fonts[0]->Glyphs.Size);
        }
    }
    s.ready = true;
    return true;
}

void imgui_backend_shutdown() {
    State& s = state();
    if (bgfx::isValid(s.font)) {
        bgfx::destroy(s.font);
        s.font = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s.sampler)) {
        bgfx::destroy(s.sampler);
        s.sampler = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s.program)) {
        bgfx::destroy(s.program);
        s.program = BGFX_INVALID_HANDLE;
    }
    s.ready = false;
}

bool imgui_backend_ready() {
    return state().ready;
}

void imgui_backend_submit(uint16_t view, uint32_t target_width,
                          uint32_t target_height) {
    State& s = state();
    if (!s.ready || ImGui::GetCurrentContext() == nullptr || target_width == 0
        || target_height == 0) {
        return;
    }
    const ImDrawData* dd = ImGui::GetDrawData();
    if (dd == nullptr || dd->CmdListsCount <= 0) {
        return;
    }
    // The lists are in INTERFACE units (logical points, so the same panel is
    // the same physical size on a Retina display and on a plain one). The
    // target may be any number of pixels; the ortho below is the whole of the
    // difference.
    const float ui_w = dd->DisplaySize.x;
    const float ui_h = dd->DisplaySize.y;
    if (ui_w <= 0.0f || ui_h <= 0.0f) {
        return;
    }
    const float sx = static_cast<float>(target_width) / ui_w;
    const float sy = static_cast<float>(target_height) / ui_h;

    // Ortho straight from the two extents. Written out rather than taken from
    // bx::mtxOrtho because z is CONSTANT ZERO here: zero is inside both clip
    // depth conventions, so this one matrix is right on Metal and on GL alike
    // and needs no capability query.
    float ortho[16] = {};
    ortho[0] = 2.0f / ui_w;
    ortho[5] = -2.0f / ui_h;
    ortho[12] = -1.0f;
    ortho[13] = 1.0f;
    ortho[15] = 1.0f;

    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(view, 0, 0, static_cast<uint16_t>(target_width),
                      static_cast<uint16_t>(target_height));
    bgfx::setViewTransform(view, nullptr, ortho);

    const ImVec2 origin = dd->DisplayPos;
    for (int list_i = 0; list_i < dd->CmdListsCount; ++list_i) {
        const ImDrawList* list = dd->CmdLists[list_i];
        const auto num_v = static_cast<uint32_t>(list->VtxBuffer.Size);
        const auto num_i = static_cast<uint32_t>(list->IdxBuffer.Size);
        if (num_v == 0 || num_i == 0) {
            continue;
        }
        // A LIST THAT DOES NOT FIT IS DROPPED WHOLE AND SAID SO. Splitting it
        // would draw half a panel, which reads as a rendering bug rather than
        // as the resource limit it is.
        if (bgfx::getAvailTransientVertexBuffer(num_v, s.layout) < num_v
            || bgfx::getAvailTransientIndexBuffer(num_i) < num_i) {
            static uint32_t told = 0;
            if (told < 3) {
                ++told;
                std::fprintf(stderr, "[imgui] список из %u вершин / %u индексов не "
                                     "поместился в переходный буфер — панель "
                                     "пропущена\n",
                             num_v, num_i);
            }
            continue;
        }
        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientVertexBuffer(&tvb, num_v, s.layout);
        bgfx::allocTransientIndexBuffer(&tib, num_i);
        std::memcpy(tvb.data, list->VtxBuffer.Data, num_v * sizeof(ImDrawVert));
        std::memcpy(tib.data, list->IdxBuffer.Data, num_i * sizeof(ImDrawIdx));

        for (int cmd_i = 0; cmd_i < list->CmdBuffer.Size; ++cmd_i) {
            const ImDrawCmd& cmd = list->CmdBuffer[cmd_i];
            if (cmd.UserCallback != nullptr) {
                cmd.UserCallback(list, &cmd);
                continue;
            }
            if (cmd.ElemCount == 0) {
                continue;
            }
            const float cx = (cmd.ClipRect.x - origin.x) * sx;
            const float cy = (cmd.ClipRect.y - origin.y) * sy;
            const float cz = (cmd.ClipRect.z - origin.x) * sx;
            const float cw = (cmd.ClipRect.w - origin.y) * sy;
            if (cz <= 0.0f || cw <= 0.0f || cx >= static_cast<float>(target_width)
                || cy >= static_cast<float>(target_height)) {
                continue;
            }
            const auto clamp_u16 = [](float v, uint32_t hi) {
                if (v < 0.0f) {
                    return static_cast<uint16_t>(0);
                }
                const auto u = static_cast<uint32_t>(v);
                return static_cast<uint16_t>(u > hi ? hi : u);
            };
            const uint16_t rx = clamp_u16(cx, target_width);
            const uint16_t ry = clamp_u16(cy, target_height);
            const uint16_t rz = clamp_u16(cz, target_width);
            const uint16_t rw = clamp_u16(cw, target_height);
            bgfx::setScissor(rx, ry, static_cast<uint16_t>(rz - rx),
                             static_cast<uint16_t>(rw - ry));

            const uint64_t packed = from_imgui_id(cmd.GetTexID());
            const bgfx::TextureHandle tex =
                (packed & TEX_VALID_BIT) != 0 ? texture_of(packed) : s.font;
            bgfx::setTexture(0, s.sampler, bgfx::isValid(tex) ? tex : s.font);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                           | BGFX_STATE_MSAA
                           | BGFX_STATE_BLEND_FUNC_SEPARATE(
                                 BGFX_STATE_BLEND_SRC_ALPHA,
                                 BGFX_STATE_BLEND_INV_SRC_ALPHA,
                                 BGFX_STATE_BLEND_ONE,
                                 BGFX_STATE_BLEND_INV_SRC_ALPHA));
            bgfx::setVertexBuffer(0, &tvb, 0, num_v);
            bgfx::setIndexBuffer(&tib, cmd.IdxOffset, cmd.ElemCount);
            bgfx::submit(view, s.program);
        }
    }
}

uint64_t imgui_backend_create_texture(uint32_t width, uint32_t height,
                                      const uint8_t* rgba) {
    if (!state().ready || width == 0 || height == 0 || rgba == nullptr) {
        return 0;
    }
    const bgfx::TextureHandle tex = bgfx::createTexture2D(
        static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
        bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_NONE,
        bgfx::copy(rgba, width * height * 4));
    if (!bgfx::isValid(tex)) {
        return 0;
    }
    return static_cast<uint64_t>(tex.idx) | TEX_VALID_BIT | TEX_OWNED_BIT;
}

void imgui_backend_update_texture(uint64_t texture, uint32_t width, uint32_t height,
                                  const uint8_t* rgba) {
    if (!state().ready || texture == 0 || rgba == nullptr) {
        return;
    }
    const bgfx::TextureHandle tex = texture_of(texture);
    if (!bgfx::isValid(tex)) {
        return;
    }
    bgfx::updateTexture2D(tex, 0, 0, 0, 0, static_cast<uint16_t>(width),
                          static_cast<uint16_t>(height),
                          bgfx::copy(rgba, width * height * 4));
}

void imgui_backend_destroy_texture(uint64_t texture) {
    // ONLY WHAT THIS FILE MADE. A renderer-owned texture arrives through
    // wrap_native without the bit, and freeing it here would leave the render
    // system holding a handle to nothing — a crash three subsystems away from
    // the mistake.
    if (texture == 0 || (texture & TEX_OWNED_BIT) == 0) {
        return;
    }
    const bgfx::TextureHandle tex = texture_of(texture);
    if (bgfx::isValid(tex)) {
        bgfx::destroy(tex);
    }
}

uint64_t imgui_backend_wrap_native(uint32_t native_handle) {
    if (native_handle > 0xFFFFu || native_handle == bgfx::kInvalidHandle) {
        return 0;
    }
    return static_cast<uint64_t>(native_handle) | TEX_VALID_BIT;
}

} // namespace dfn::platform
