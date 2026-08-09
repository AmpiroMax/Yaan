/*
Created: 09:08:2026 - 00:45:00
Last updated: 09:08:2026 - 10:14:00
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxCallback.cpp

Responsibility:
- BgfxCallback implementation: PNG screenshot writing via bimg, fatal/trace
  forwarding to stderr.

Key items:
- BgfxCallback::screenShot (bimg::imageWritePng), fatal, traceVargs.

Dependencies:
- Uses: bimg/encode, bx file IO.
- Used by: dfn_platform_render target (bgfx backend).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Stage 2 — initial implementation.
- 09:08:2026 - 10:14:00: LEAD integration fix (render agent lost to reboot):
                         screenShot signature updated to the pinned bgfx API
                         (added TextureFormat::Enum format parameter).
*/

#include "engine/platform/render/sources/bgfx/BgfxCallback.h"

#include <bimg/encode.h>
#include <bx/file.h>

#include <cstdio>
#include <cstdlib>

namespace dfn::platform {

void BgfxCallback::fatal(const char* file_path, uint16_t line, bgfx::Fatal::Enum code,
                         const char* str) {
    std::fprintf(stderr, "[bgfx FATAL %d] %s:%u: %s\n", static_cast<int>(code),
                 file_path != nullptr ? file_path : "?", line, str);
    if (code != bgfx::Fatal::DebugCheck) {
        std::abort();
    }
}

void BgfxCallback::traceVargs(const char* file_path, uint16_t line, const char* format,
                              va_list arg_list) {
#if !defined(NDEBUG)
    std::fprintf(stderr, "[bgfx] %s:%u: ", file_path != nullptr ? file_path : "?", line);
    std::vfprintf(stderr, format, arg_list);
#else
    (void)file_path;
    (void)line;
    (void)format;
    (void)arg_list;
#endif
}

void BgfxCallback::screenShot(const char* file_path, uint32_t width, uint32_t height,
                              uint32_t pitch, bgfx::TextureFormat::Enum format,
                              const void* data, uint32_t size, bool yflip) {
    (void)size;
    last_screenshot_ok_ = false;

    bx::FileWriter writer;
    bx::Error err;
    if (!bx::open(&writer, file_path, false, &err)) {
        std::fprintf(stderr, "[render] screenshot open failed: %s\n", file_path);
        return;
    }
    // bgfx reports the backbuffer format explicitly since the pinned release;
    // rows arrive with the given pitch.
    bimg::imageWritePng(&writer, width, height, pitch, data,
                        static_cast<bimg::TextureFormat::Enum>(format), yflip, &err);
    bx::close(&writer);
    last_screenshot_ok_ = err.isOk();
    if (!last_screenshot_ok_) {
        std::fprintf(stderr, "[render] screenshot encode failed: %s\n", file_path);
    }
}

} // namespace dfn::platform
