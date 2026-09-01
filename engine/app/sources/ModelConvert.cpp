/*
Module: engine/app
File: engine/app/sources/ModelConvert.cpp

Responsibility:
- STL reading and the download cache. See ModelConvert.h.

Dependencies:
- Uses: engine/render (MeshData, ObjectRegistry), engine/core (fnv1a64), std.
- Used by: engine/app (AppViewer.cpp), tests/app/ModelConvertTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 in spirit for the STL read: explicit little-endian byte assembly, no
  struct memcpy. An STL header is 84 bytes and the record is 50, which is NOT
  a multiple of 4 — a memcpy of a packed struct is exactly how this format is
  read wrong on a machine with different alignment rules.
*/

#include "engine/app/sources/ModelConvert.h"

#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <glm/geometric.hpp>

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

namespace dfn::app {

namespace fs = std::filesystem;

namespace {

/// THE COLOUR EVERY CONVERTED MODEL IS DRAWN IN. One tone, deliberately: an
/// STL carries no colour at all, and inventing a per-model tint would make two
/// runs of the stand differ in the only channel a frame is read by.
///
/// GREY CLAY AND NOT BONE-WHITE, and the first acceptance frame is why. At 0.78
/// the figure came out a flat white silhouette with no shading in it at all:
/// the stand's own lamps add to the sun, and an albedo that high clips before
/// the lambert term has anywhere left to go. What the stand shows is FORM, and
/// form is read from the shading — so the albedo has to leave room for it.
constexpr glm::vec3 CONVERTED_ALBEDO{0.60f, 0.58f, 0.55f};

/// WHAT THIS CONVERTER PRODUCES, AS A NUMBER IN THE CACHE KEY. Bump it whenever
/// the BAKE changes (the upright turn, the albedo, the streams) — the source
/// file has not changed, so nothing else would invalidate a cache written by an
/// older build, and the stand would go on showing yesterday's bake for ever
/// with nothing to say so.
constexpr int CONVERT_VERSION = 2;

[[nodiscard]] float read_le_float(const unsigned char* p) {
    // Explicit assembly rather than a reinterpret_cast: the record is 50 bytes
    // and its floats are not 4-aligned inside the file.
    const std::uint32_t bits = static_cast<std::uint32_t>(p[0])
                             | (static_cast<std::uint32_t>(p[1]) << 8)
                             | (static_cast<std::uint32_t>(p[2]) << 16)
                             | (static_cast<std::uint32_t>(p[3]) << 24);
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

[[nodiscard]] std::uint32_t read_le_u32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

/// FLAT TRIANGLE INTO THE STREAM. Winding as the file gives it; the normal is
/// the cross product, normalised, and a degenerate facet gets +Y rather than a
/// NaN — one flat sliver must not blacken the whole model.
void push_triangle(render::MeshData& m, const glm::vec3& a, const glm::vec3& b,
                   const glm::vec3& c, std::uint32_t colour) {
    glm::vec3 n = glm::cross(b - a, c - a);
    const float len = glm::length(n);
    n = len > 1e-12f ? n / len : glm::vec3{0.0f, 1.0f, 0.0f};
    const auto base = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back({a, n, {0.0f, 0.0f}, colour});
    m.vertices.push_back({b, n, {0.0f, 0.0f}, colour});
    m.vertices.push_back({c, n, {0.0f, 0.0f}, colour});
    m.indices.push_back(base);
    m.indices.push_back(base + 1);
    m.indices.push_back(base + 2);
}

[[nodiscard]] std::optional<render::MeshData> read_stl_ascii(const std::string& text,
                                                             std::uint32_t colour) {
    render::MeshData mesh;
    std::array<glm::vec3, 3> tri{};
    int have = 0;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t eol = text.find('\n', at);
        std::string_view line(text.data() + at,
                              (eol == std::string::npos ? text.size() : eol) - at);
        at = (eol == std::string::npos) ? text.size() : eol + 1;
        // Leading spaces are the format's own indentation.
        std::size_t b = 0;
        while (b < line.size() && (line[b] == ' ' || line[b] == '\t')) {
            ++b;
        }
        line.remove_prefix(b);
        if (line.rfind("vertex", 0) != 0) {
            continue;
        }
        const std::string copy(line.substr(6));
        glm::vec3 v{0.0f};
        if (std::sscanf(copy.c_str(), "%f %f %f", &v.x, &v.y, &v.z) != 3) {
            return std::nullopt; // a vertex line that is not a vertex: refuse
        }
        tri[static_cast<std::size_t>(have)] = v;
        if (++have == 3) {
            push_triangle(mesh, tri[0], tri[1], tri[2], colour);
            have = 0;
        }
    }
    if (mesh.indices.empty()) {
        return std::nullopt;
    }
    return mesh;
}

} // namespace

std::optional<render::MeshData> read_stl(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
    if (bytes.size() < 84) {
        return std::nullopt;
    }
    const std::uint32_t colour = render::pack(CONVERTED_ALBEDO);

    // WHICH FLAVOUR. The reliable test is arithmetic, not the "solid" keyword:
    // plenty of binary STLs begin with the word "solid" in their 80-byte
    // header, and a reader that trusted the keyword would parse a binary file
    // as text and quietly produce nothing.
    const std::uint32_t count = read_le_u32(bytes.data() + 80);
    const std::size_t expected = 84u + static_cast<std::size_t>(count) * 50u;
    if (count > 0 && bytes.size() >= expected) {
        render::MeshData mesh;
        mesh.vertices.reserve(static_cast<std::size_t>(count) * 3);
        mesh.indices.reserve(static_cast<std::size_t>(count) * 3);
        const unsigned char* p = bytes.data() + 84;
        for (std::uint32_t i = 0; i < count; ++i, p += 50) {
            const glm::vec3 a{read_le_float(p + 12), read_le_float(p + 16),
                              read_le_float(p + 20)};
            const glm::vec3 b{read_le_float(p + 24), read_le_float(p + 28),
                              read_le_float(p + 32)};
            const glm::vec3 c{read_le_float(p + 36), read_le_float(p + 40),
                              read_le_float(p + 44)};
            push_triangle(mesh, a, b, c, colour);
        }
        return mesh;
    }
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return read_stl_ascii(text, colour);
}

void stl_z_up_to_y_up(render::MeshData& mesh) {
    for (platform::Vertex& v : mesh.vertices) {
        const glm::vec3 p = v.position;
        const glm::vec3 n = v.normal;
        v.position = {p.x, p.z, -p.y};
        v.normal = {n.x, n.z, -n.y};
    }
    // WINDING IS UNTOUCHED, AND THAT IS CORRECT rather than an oversight: the
    // turn is a rotation (determinant +1), so it preserves orientation. A
    // mirror would not, and reversing the winding here "to be safe" would flip
    // every face of every downloaded model inside out.
}

fs::path model_cache_dir(const fs::path& downloads_root) {
    return downloads_root / ".cache";
}

std::string cache_name_for(const fs::path& path) {
    std::error_code ec;
    const fs::path abs = fs::weakly_canonical(path, ec);
    const fs::path& key_path = ec ? path : abs;
    std::uintmax_t size = 0;
    std::int64_t stamp = 0;
    if (const auto s = fs::file_size(path, ec); !ec) {
        size = s;
    }
    if (const auto t = fs::last_write_time(path, ec); !ec) {
        stamp = static_cast<std::int64_t>(t.time_since_epoch().count());
    }
    char tail[64] = {};
    std::snprintf(tail, sizeof(tail), "|%llu|%lld|v%d",
                  static_cast<unsigned long long>(size),
                  static_cast<long long>(stamp), CONVERT_VERSION);
    const std::string key = key_path.generic_string() + tail;
    char name[128] = {};
    std::snprintf(name, sizeof(name), "%s-%016llx.dfo", path.stem().string().c_str(),
                  static_cast<unsigned long long>(serialization::fnv1a64(key)));
    return name;
}

fs::path find_gltf_tool(const fs::path& exe_dir) {
    std::error_code ec;
    if (const char* env = std::getenv("DFN_GLTF_TOOL"); env != nullptr && *env != '\0') {
        const fs::path p(env);
        if (fs::is_regular_file(p, ec)) {
            return p;
        }
    }
    // Beside the running binary first: that is where the build puts it, and it
    // is the only candidate guaranteed to be the SAME build as the game.
    std::vector<fs::path> candidates;
    if (!exe_dir.empty()) {
        candidates.push_back(exe_dir / "dfn_import_gltf");
    }
    for (const char* dir : {"build_lead", "build_editor", "build"}) {
        candidates.emplace_back(fs::path(dir) / "dfn_import_gltf");
    }
    for (const fs::path& c : candidates) {
        if (fs::is_regular_file(c, ec)) {
            return c;
        }
    }
    return {};
}

ConvertResult convert_model(const fs::path& path, const fs::path& downloads_root,
                            const fs::path& gltf_tool) {
    ConvertResult out;
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        out.reason = "нет файла";
        return out;
    }
    const std::string ext = [&] {
        std::string e = path.extension().string();
        for (char& c : e) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return e;
    }();

    // A BAKED OBJECT IS ALREADY THE ANSWER. No copy into the cache: two files
    // holding one object under one name is how the two start drifting.
    if (ext == ".dfo") {
        out.dfo = path;
        out.from_cache = true;
        return out;
    }

    const fs::path cache = model_cache_dir(downloads_root);
    const fs::path target = cache / cache_name_for(path);
    if (fs::is_regular_file(target, ec)) {
        out.dfo = target;
        out.from_cache = true;
        return out;
    }
    fs::create_directories(cache, ec);

    if (ext == ".stl") {
        auto mesh = read_stl(path);
        if (!mesh) {
            out.reason = "STL не прочитан";
            return out;
        }
        // UPRIGHT AT THE BAKE, ONCE. Not in the frame and not in the reader:
        // baked, the cached .dfo is a model like any other on a shelf, and
        // nothing downstream has to remember where it came from.
        stl_z_up_to_y_up(*mesh);
        render::RegistryObject obj;
        obj.name = path.stem().string();
        obj.kind = "download";
        obj.source = "stl:" + path.generic_string();
        obj.wood = std::move(*mesh);
        out.triangles = obj.wood.triangle_count();
        if (!render::write_object(obj, target)) {
            out.reason = "кэш не записан";
            return out;
        }
        out.dfo = target;
        return out;
    }

    if (ext == ".glb" || ext == ".gltf") {
        if (gltf_tool.empty()) {
            out.reason = "нет dfn_import_gltf: соберите его и повторите";
            return out;
        }
        // THE OFFLINE IMPORTER, RUN AS A CACHING STEP. Quoted paths: this tree
        // has folders with spaces in them, and an unquoted command would open
        // "Daggerfall" and fail on "N".
        std::string cmd = "\"" + gltf_tool.string() + "\" \"" + path.string()
                        + "\" --out \"" + target.string() + "\" --name \""
                        + path.stem().string() + "\"";
        cmd += " >/dev/null 2>&1";
        const int rc = std::system(cmd.c_str());
        if (rc != 0 || !fs::is_regular_file(target, ec)) {
            out.reason = "dfn_import_gltf отказал";
            return out;
        }
        out.dfo = target;
        return out;
    }

    out.reason = "формат не поддержан";
    return out;
}

} // namespace dfn::app
