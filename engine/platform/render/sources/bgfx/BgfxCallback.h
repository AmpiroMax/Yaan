/*
Module: engine/platform/render
File: engine/platform/render/sources/bgfx/BgfxCallback.h

Responsibility:
- bgfx::CallbackI implementation: fatal/trace plumbing and the screenshot
  writer (PNG via bimg) that backs IRenderer::save_screenshot (Rule 27).

Key items:
- BgfxCallback: screenShot -> PNG on disk; last_screenshot_ok() for the
  renderer to report success.

Dependencies:
- Uses: bgfx callback interface, bimg encode, bx (this is platform backend
  code — third-party includes are legal here, Rule 1).
- Used by: BgfxRenderer (owns one, passes it to bgfx::Init).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This header stays inside sources/bgfx/ — never include it outside the backend.
*/

#pragma once

#include <bgfx/bgfx.h>

#include <atomic>
#include <cstdint>

namespace dfn::platform {

class BgfxCallback final : public bgfx::CallbackI {
public:
    void fatal(const char* file_path, uint16_t line, bgfx::Fatal::Enum code,
               const char* str) override;
    void traceVargs(const char* file_path, uint16_t line, const char* format,
                    va_list arg_list) override;
    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}

    /// Writes the captured frame as PNG. `format` is the backbuffer format
    /// reported by bgfx (BGRA8/RGBA8 expected); `yflip` per API.
    void screenShot(const char* file_path, uint32_t width, uint32_t height,
                    uint32_t pitch, bgfx::TextureFormat::Enum format,
                    const void* data, uint32_t size, bool yflip) override;

    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum,
                      bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}

    /// True if the most recent screenShot call wrote its file successfully.
    [[nodiscard]] bool last_screenshot_ok() const { return last_screenshot_ok_; }

private:
    std::atomic<bool> last_screenshot_ok_{false};
};

} // namespace dfn::platform
