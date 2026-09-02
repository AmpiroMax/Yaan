/*
Module: engine/app
File: engine/app/sources/CharacterTextures.cpp

Responsibility:
- body_albedo_asset(): resolve the TEX reference to a file, verify the
  SHA-256, decode the PNG, upload with a mip chain, cache by sha.

Dependencies:
- Uses: CharacterTextures.h, PngImage, AppDoors, core Sha256.
- Used by: dfn_app, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every zero returned is preceded by a line on stderr saying why.
*/

#include "engine/app/sources/CharacterTextures.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/PngImage.h"
#include "engine/core/serialization/sources/Sha256.h"

#include <cstdio>
#include <string>
#include <unordered_map>

namespace dfn::app {
namespace {

/// sha256 hex -> render texture asset id. Process-wide (see the header).
std::unordered_map<std::string, uint32_t>& cache() {
    static std::unordered_map<std::string, uint32_t> c;
    return c;
}

/// WHERE THE REFERENCED FILE IS. Absolute as is; otherwise the current
/// directory (the game runs from the repository root), then each ancestor
/// of the .dfo's own directory -- a tool run from elsewhere still finds a
/// sheet that lies in the tree beside the body.
std::filesystem::path resolve_sheet(const std::string& stored,
                                    const std::filesystem::path& dfo_path) {
    std::error_code ec;
    const std::filesystem::path p(stored);
    if (p.is_absolute()) {
        return p;
    }
    if (std::filesystem::is_regular_file(p, ec)) {
        return p;
    }
    std::filesystem::path dir = std::filesystem::absolute(dfo_path, ec).parent_path();
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        const std::filesystem::path candidate = dir / p;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
        const std::filesystem::path up = dir.parent_path();
        if (up == dir) {
            break;
        }
        dir = up;
    }
    return p; // not found: reported by the caller with this very name
}

} // namespace

bool body_palette_door() {
    static const bool on = [] {
        const char* v = door_value("DFN_BODY_PALETTE");
        const bool palette = v != nullptr && v[0] == '1';
        if (palette) {
            std::fprintf(stderr, "[character] DFN_BODY_PALETTE=1: листы кожи не "
                                 "поднимаются, тело красится палитрой частей "
                                 "(рука «до»)\n");
        }
        return palette;
    }();
    return on;
}

uint32_t sheet_asset(render::RenderSystem& render_system, platform::IRenderer& renderer,
                     const render::TextureRef& ref, const std::string& owner,
                     const std::filesystem::path& dfo_path) {
    if (body_palette_door()) {
        return 0;
    }
    if (const auto hit = cache().find(ref.sha256); hit != cache().end()) {
        return hit->second;
    }
    const std::filesystem::path file = resolve_sheet(ref.path, dfo_path);
    const auto sha = serialization::sha256_file(file);
    if (!sha.has_value()) {
        std::fprintf(stderr,
                     "[character] \"%s\": лист %s \"%s\" не найден — рисуется без "
                     "него (палитрой вершин)\n",
                     owner.c_str(), ref.role.c_str(), ref.path.c_str());
        cache().emplace(ref.sha256, 0u); // complain once, not once per body
        return 0;
    }
    if (*sha != ref.sha256) {
        std::fprintf(stderr,
                     "[character] \"%s\": лист %s \"%s\" — НЕ ТОТ ФАЙЛ: выпечка "
                     "видела sha256 %s, на диске %s. Перепеки (dfn_import_gltf) "
                     "или верни PNG. Рисуется без листа.\n",
                     owner.c_str(), ref.role.c_str(), ref.path.c_str(), ref.sha256.c_str(),
                     sha->c_str());
        cache().emplace(ref.sha256, 0u);
        return 0;
    }
    const Image img = load_png(file.string());
    if (img.empty()) {
        std::fprintf(stderr,
                     "[character] \"%s\": лист %s \"%s\" не читается как PNG — "
                     "рисуется без листа\n",
                     owner.c_str(), ref.role.c_str(), ref.path.c_str());
        cache().emplace(ref.sha256, 0u);
        return 0;
    }
    // DFN_BODY_MIPS=0 — контрольная рука: тот же лист одной ступенью.
    static const bool mips = [] {
        const char* v = door_value("DFN_BODY_MIPS");
        const bool off = v != nullptr && v[0] == '0';
        if (off) {
            std::fprintf(stderr, "[character] DFN_BODY_MIPS=0: листы без цепочки мипов "
                                 "(рука «до»)\n");
        }
        return !off;
    }();
    const uint32_t asset = render_system.register_texture_asset(
        renderer, static_cast<uint32_t>(img.width), static_cast<uint32_t>(img.height),
        img.rgba, /*mip_chain=*/mips);
    cache().emplace(ref.sha256, asset);
    if (asset != 0) {
        std::fprintf(stderr,
                     "[character] \"%s\": лист %s %s (%dx%d, sha256 %.12s…) "
                     "поднят с цепочкой мипов, ассет %u\n",
                     owner.c_str(), ref.role.c_str(), ref.path.c_str(), img.width,
                     img.height, ref.sha256.c_str(), asset);
    }
    return asset;
}

uint32_t body_albedo_asset(render::RenderSystem& render_system,
                           platform::IRenderer& renderer,
                           const render::RegistryObject& object,
                           const std::filesystem::path& dfo_path) {
    const render::TextureRef* ref = object.texture("albedo");
    if (ref == nullptr) {
        return 0; // a body without a sheet is the ordinary case, not a fault
    }
    return sheet_asset(render_system, renderer, *ref, object.name, dfo_path);
}

std::size_t body_textures_loaded() {
    std::size_t n = 0;
    for (const auto& [sha, asset] : cache()) {
        if (asset != 0) {
            ++n;
        }
    }
    return n;
}

} // namespace dfn::app
