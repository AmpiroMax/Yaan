/*
Created: 14:08:2026 - 16:50:36
Last updated: 15:08:2026 - 01:04:30
Module: engine/app
File: engine/app/sources/MapCatalog.cpp

Responsibility:
- MapCatalog: parse .map manifests and scan the category folders. See header.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 14:08:2026 - 16:50:36: Created.
- 15:08:2026 - 01:04:30: разбор ключа objects — карта выбирает свою полку реестра.
*/

#include "engine/app/sources/MapCatalog.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace dfn::app {

namespace {

// Trim ASCII spaces/tabs/CR from both ends. UTF-8 payload is left intact:
// trimming only the ASCII whitespace never lands inside a multi-byte codepoint.
std::string_view trim(std::string_view s) {
    const auto is_ws = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    size_t b = 0;
    size_t e = s.size();
    while (b < e && is_ws(s[b])) {
        ++b;
    }
    while (e > b && is_ws(s[e - 1])) {
        --e;
    }
    return s.substr(b, e - b);
}

} // namespace

const std::vector<std::string>& map_categories() {
    // The fixed list from docs/MAP_LAYOUT.md (в40). Order is the browser order.
    static const std::vector<std::string> kCategories = {
        "landscape", "trees", "bushes", "fallen-tree", "houses",
        "light",     "water", "mountain", "caves"};
    return kCategories;
}

MapManifest parse_map_manifest(std::string_view text) {
    MapManifest m;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) {
            nl = text.size();
        }
        const std::string_view line = trim(text.substr(pos, nl - pos));
        pos = nl + 1;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));
        if (key == "name") {
            m.name = std::string(value);
        } else if (key == "zone") {
            m.zone = std::string(value);
        } else if (key == "source") {
            m.source = std::string(value);
        } else if (key == "description") {
            m.description = std::string(value);
        } else if (key == "objects") {
            m.objects = std::string(value);
        } else if (key == "size_chunks") {
            int v = 0;
            if (std::sscanf(std::string(value).c_str(), "%d", &v) == 1) {
                m.size_chunks = v;
            }
        }
        // Unknown keys (built_commit, future fields) ignored on purpose.
    }
    return m;
}

bool split_map_source(std::string_view source, std::string& scheme,
                      std::string& value) {
    const size_t colon = source.find(':');
    if (colon == std::string_view::npos) {
        return false;
    }
    scheme = std::string(trim(source.substr(0, colon)));
    value = std::string(trim(source.substr(colon + 1)));
    return true;
}

const MapManifest* MapCatalog::find(std::string_view category,
                                    std::string_view stem) const {
    for (const auto& cat : categories) {
        if (cat.slug != category) {
            continue;
        }
        for (const auto& m : cat.maps) {
            if (m.file_stem == stem) {
                return &m;
            }
        }
    }
    return nullptr;
}

MapCatalog scan_map_catalog(const std::string& root) {
    namespace fs = std::filesystem;
    MapCatalog catalog;
    for (const std::string& slug : map_categories()) {
        MapCategory cat;
        cat.slug = slug;
        const fs::path dir = fs::path(root) / slug;
        std::error_code ec;
        if (fs::is_directory(dir, ec)) {
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (ec) {
                    break;
                }
                const fs::path& p = entry.path();
                if (p.extension() != ".map") {
                    continue;
                }
                std::ifstream in(p, std::ios::binary);
                if (!in) {
                    std::fprintf(stderr, "[maps] cannot read %s\n", p.string().c_str());
                    continue;
                }
                const std::string text((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
                MapManifest m = parse_map_manifest(text);
                m.category = slug;
                m.file_stem = p.stem().string();
                // A manifest with no name is still listed, under its file stem,
                // rather than dropped: a nameless entry the author can SEE is
                // better than a map that silently does not appear.
                if (m.name.empty()) {
                    m.name = m.file_stem;
                }
                cat.maps.push_back(std::move(m));
            }
        }
        std::sort(cat.maps.begin(), cat.maps.end(),
                  [](const MapManifest& a, const MapManifest& b) {
                      return a.file_stem < b.file_stem;
                  });
        catalog.categories.push_back(std::move(cat));
    }
    return catalog;
}

} // namespace dfn::app
