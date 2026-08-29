/*
Module: engine/app
File: engine/app/sources/ChatLog.cpp

Responsibility:
- Serialisation of the JSONL chat beside a map and the telemetry ring
  (see ChatLog.h and docs/MAP_LAYOUT.md).

Dependencies:
- Uses: std, <filesystem>.
- Used by: App only.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned zone (Rule 25), app cut.
*/

#include "engine/app/sources/ChatLog.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace dfn::app {

namespace {

// JSON string body: escape what JSON requires and pass UTF-8 bytes through
// untouched, so Cyrillic (multi-byte UTF-8) survives verbatim. Only the seven
// mandatory escapes plus a \u00xx fallback for other control bytes.
std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const unsigned char c : in) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char u[8];
                std::snprintf(u, sizeof(u), "\\u%04x", c);
                out += u;
            } else {
                out += static_cast<char>(c); // includes all UTF-8 continuation bytes
            }
        }
    }
    return out;
}

// Appends `"key":"value"` when value is non-empty. The comma before it is the
// caller's job (every field here follows the mandatory `t`/`who`).
void put_str(std::string& line, const char* key, const std::string& value) {
    if (!value.empty()) {
        line += ",\"";
        line += key;
        line += "\":\"";
        line += json_escape(value);
        line += '"';
    }
}

} // namespace

bool append_chat_entry(const std::string& chat_path, double t,
                       const ChatEntry& entry,
                       const std::optional<std::string>& date) {
    std::string line = "{\"t\":";
    char tbuf[48];
    std::snprintf(tbuf, sizeof(tbuf), "%.6f", t);
    line += tbuf;
    line += ",\"who\":\"";
    line += json_escape(entry.who.empty() ? "human" : entry.who);
    line += '"';
    put_str(line, "text", entry.text);
    put_str(line, "capture", entry.capture);
    put_str(line, "trajectory", entry.trajectory);
    // Best-effort wall date -- an extra, never the order key (MAP_LAYOUT.md).
    if (date) {
        put_str(line, "date", *date);
    }
    line += "}\n";

    std::ofstream f(chat_path, std::ios::binary | std::ios::app);
    if (!f) {
        std::fprintf(stderr, "[chat] cannot append to %s\n", chat_path.c_str());
        return false;
    }
    f.write(line.data(), static_cast<std::streamsize>(line.size()));
    if (!f) {
        std::fprintf(stderr, "[chat] short write to %s\n", chat_path.c_str());
        return false;
    }
    std::fprintf(stderr, "[chat] appended to %s: %s", chat_path.c_str(), line.c_str());
    return true;
}

std::string chat_path_for_map(const std::string& map_file) {
    const std::string suffix = ".map";
    if (map_file.size() >= suffix.size()
        && map_file.compare(map_file.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return map_file.substr(0, map_file.size() - suffix.size()) + ".chat.jsonl";
    }
    return map_file + ".chat.jsonl";
}

namespace {

// The transitional stand id -> stand name bridge (MAP_LAYOUT.md): matched
// against a manifest's `source = stand:<name>`. Two stands today; new ones are
// one line here until the editor's browser hands the app the open .map path and
// this scan is retired.
const char* stand_name(uint32_t stand) {
    switch (stand) {
    case 0: return "Testbed";
    case 1: return "Forest";
    default: return nullptr;
    }
}

// Reads the `source = stand:<name>` value from a .map manifest, or "" if none.
std::string read_source_stand(const std::filesystem::path& map) {
    std::ifstream in(map);
    if (!in) {
        return {};
    }
    std::string ln;
    while (std::getline(in, ln)) {
        const auto eq = ln.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        auto trim = [](std::string s) {
            const auto a = s.find_first_not_of(" \t\r\n");
            const auto b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string{} : s.substr(a, b - a + 1);
        };
        if (trim(ln.substr(0, eq)) == "source") {
            const std::string v = trim(ln.substr(eq + 1));
            const std::string pfx = "stand:";
            if (v.size() >= pfx.size() && v.compare(0, pfx.size(), pfx) == 0) {
                return v.substr(pfx.size());
            }
            return {};
        }
    }
    return {};
}

} // namespace

std::string map_file_for_stand(const std::string& maps_root, uint32_t stand) {
    const char* want = stand_name(stand);
    if (want == nullptr) {
        return {};
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(maps_root, ec)) {
        return {};
    }
    for (const auto& e :
         std::filesystem::recursive_directory_iterator(maps_root, ec)) {
        if (ec) {
            break;
        }
        if (!e.is_regular_file() || e.path().extension() != ".map") {
            continue;
        }
        if (read_source_stand(e.path()) == want) {
            return e.path().string();
        }
    }
    return {};
}

// ---------------------------------------------------------------------------

TelemetryRing::TelemetryRing(size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) {
        capacity_ = 1; // a zero-length ring would divide by zero on push
    }
    buf_.resize(capacity_);
}

void TelemetryRing::push(const TelemetrySample& s) {
    buf_[next_] = s;
    next_ = (next_ + 1) % capacity_;
    if (count_ < capacity_) {
        ++count_;
    }
}

bool TelemetryRing::flush(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        std::fprintf(stderr, "[telemetry] cannot open %s\n", path.c_str());
        return false;
    }
    std::fprintf(f,
                 "# Daggerfall N telemetry ring -- the last %zu samples, oldest first.\n"
                 "# Sampled on the counted clock; render columns (tris, aim) are 0/empty\n"
                 "# until a render hook fills them.\n"
                 "# game_s pos_x pos_y pos_z yaw pitch fps frame_ms chunks lod tris aim capture\n",
                 count_);
    // Oldest-first: when the ring has wrapped, the oldest live sample sits at
    // `next_`; before it wraps, the oldest is index 0.
    const size_t start = (count_ == capacity_) ? next_ : 0;
    for (size_t i = 0; i < count_; ++i) {
        const TelemetrySample& s = buf_[(start + i) % capacity_];
        std::fprintf(f,
                     "%.6f %.3f %.3f %.3f %.5f %.5f %.2f %.3f %u %u %u %s %s\n",
                     s.game_seconds, static_cast<double>(s.position.x),
                     static_cast<double>(s.position.y), static_cast<double>(s.position.z),
                     static_cast<double>(s.yaw), static_cast<double>(s.pitch),
                     static_cast<double>(s.fps), static_cast<double>(s.frame_ms),
                     s.chunks_resident, s.lod_nodes, s.triangles,
                     s.aim_target.empty() ? "-" : s.aim_target.c_str(),
                     s.capture.empty() ? "-" : s.capture.c_str());
    }
    std::fclose(f);
    std::fprintf(stderr, "[telemetry] flushed %zu samples to %s\n", count_, path.c_str());
    return true;
}

} // namespace dfn::app
