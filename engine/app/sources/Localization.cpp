/*
Module: engine/app
File: engine/app/sources/Localization.cpp

Responsibility:
- Implementation of the key-hash to string table. See the header for why a
  miss is loud rather than empty.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/

#include "engine/app/sources/Localization.h"

#include "engine/core/serialization/sources/ContentHash.h"

#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace dfn::app {

namespace {

std::unordered_map<uint64_t, std::string>& table() {
    static std::unordered_map<uint64_t, std::string> t;
    return t;
}

// Placeholders are stored rather than formatted per call, so localized() can
// return a string_view that outlives the call without allocating every frame.
std::unordered_map<uint64_t, std::string>& misses() {
    static std::unordered_map<uint64_t, std::string> m;
    return m;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

} // namespace

bool load_localization(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        // Loud, not silent: a missing table is indistinguishable from a table
        // of empty strings unless somebody says so here.
        std::fprintf(stderr,
                     "[localization] cannot open %s — every string will draw as a placeholder\n",
                     path.c_str());
        return false;
    }
    table().clear();
    misses().clear();
    std::string line;
    while (std::getline(in, line)) {
        const std::string_view sv = trim(line);
        if (sv.empty() || sv.front() == '#') {
            continue;
        }
        const auto eq = sv.find('=');
        if (eq == std::string_view::npos) {
            std::fprintf(stderr, "[localization] %s: line without '=' ignored: %.*s\n",
                         path.c_str(), static_cast<int>(sv.size()), sv.data());
            continue;
        }
        const std::string_view key = trim(sv.substr(0, eq));
        const std::string_view value = trim(sv.substr(eq + 1));
        if (key.empty()) {
            continue;
        }
        table().emplace(serialization::fnv1a64(key), std::string(value));
    }
    return true;
}

std::string_view localized(uint64_t key_hash) {
    const auto it = table().find(key_hash);
    if (it != table().end()) {
        return it->second;
    }
    // A miss must be impossible to read as content. A bare key looks enough
    // like text that a screenshot of it passes for an unpolished translation;
    // this cannot. The hex keeps it debuggable without a second tool.
    auto& m = misses();
    const auto found = m.find(key_hash);
    if (found != m.end()) {
        return found->second; // already reported: once per key, not per frame
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "?<0x%016llx>?",
                  static_cast<unsigned long long>(key_hash));
    std::fprintf(stderr, "[localization] no string for key hash 0x%016llx\n",
                 static_cast<unsigned long long>(key_hash));
    return m.emplace(key_hash, std::string(buf)).first->second;
}

} // namespace dfn::app
