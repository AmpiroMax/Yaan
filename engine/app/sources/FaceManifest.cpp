/*
Module: engine/app
File: engine/app/sources/FaceManifest.cpp

Responsibility:
- Разбор манифеста лицевых ручек и файла калибровки (см. FaceManifest.h).

Dependencies:
- Uses: stdlib. Used by: CharGen.cpp, AppCharGen.cpp, tests/app/CharGenTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- face_handle_name() и handle_name() экспортёра — ОДНО правило в двух языках;
  менять только вместе.
*/

#include "engine/app/sources/FaceManifest.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace dfn::app {

namespace {

constexpr std::string_view SIDES = "{l,r}-";

[[nodiscard]] std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

[[nodiscard]] std::string_view strip_sides(std::string_view spec) {
    if (spec.substr(0, SIDES.size()) == SIDES) {
        spec.remove_prefix(SIDES.size());
    }
    return spec;
}

[[nodiscard]] bool parse_float(std::string_view s, float& out) {
    const std::string tmp(s);
    char* end = nullptr;
    out = std::strtof(tmp.c_str(), &end);
    return end != tmp.c_str() && *end == '\0';
}

} // namespace

std::string face_handle_name(std::string_view spec) {
    const std::string_view stem = strip_sides(trim(spec));
    const std::size_t slash = stem.find('/');
    if (slash == std::string_view::npos) {
        return std::string(stem);
    }
    const std::string_view left = stem.substr(0, slash);
    const std::size_t dash = left.rfind('-');
    return std::string(dash == std::string_view::npos ? left : left.substr(0, dash));
}

std::string face_group_id(std::string_view spec) {
    const std::string_view stem = strip_sides(trim(spec));
    const std::size_t dash = stem.find('-');
    return std::string(dash == std::string_view::npos ? stem : stem.substr(0, dash));
}

std::size_t FacePlan::handle_count() const {
    std::size_t n = 0;
    for (const FaceGroup& g : groups) {
        n += g.handles.size();
    }
    return n;
}

const FaceHandle* FacePlan::find(std::string_view name) const {
    for (const FaceGroup& g : groups) {
        for (const FaceHandle& h : g.handles) {
            if (h.name == name) {
                return &h;
            }
        }
    }
    return nullptr;
}

FaceHandle* FacePlan::find(std::string_view name) {
    return const_cast<FaceHandle*>(static_cast<const FacePlan&>(*this).find(name));
}

bool parse_face_manifest(std::string_view text, FacePlan& out, std::string& why) {
    out = FacePlan{};
    std::size_t line_no = 0;
    std::string last_group; // русское слово группы, как в файле
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string_view raw =
            text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
        ++line_no;
        const std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        // Шесть колонок через «|».
        std::vector<std::string_view> cols;
        std::size_t start = 0;
        while (true) {
            const std::size_t bar = line.find('|', start);
            cols.push_back(trim(line.substr(start, bar == std::string_view::npos
                                                       ? std::string_view::npos
                                                       : bar - start)));
            if (bar == std::string_view::npos) {
                break;
            }
            start = bar + 1;
        }
        if (cols.size() != 6) {
            why = "строка " + std::to_string(line_no) + ": не шесть колонок";
            return false;
        }
        FaceHandle h;
        h.spec = std::string(cols[2]);
        h.name = face_handle_name(cols[2]);
        if (h.name.empty()) {
            why = "строка " + std::to_string(line_no) + ": пустая цель";
            return false;
        }
        std::istringstream band{std::string(cols[3])};
        std::string lo_s;
        std::string hi_s;
        band >> lo_s >> hi_s;
        if (!parse_float(lo_s, h.lo) || !parse_float(hi_s, h.hi) || !(h.lo < h.hi)) {
            why = "строка " + std::to_string(line_no) + ": полоса «" + std::string(cols[3])
                  + "» — не два числа lo < hi";
            return false;
        }
        if (out.find(h.name) != nullptr) {
            why = "строка " + std::to_string(line_no) + ": ручка «" + h.name
                  + "» названа дважды";
            return false;
        }
        // ГРУППА — ПО ПЕРВОЙ КОЛОНКЕ, ПОДРЯД: смена слова открывает раздел. Id
        // раздела — первый стем его первой ручки.
        if (out.groups.empty() || std::string(cols[0]) != last_group) {
            last_group = std::string(cols[0]);
            FaceGroup g;
            g.id = face_group_id(cols[2]);
            for (const FaceGroup& seen : out.groups) {
                if (seen.id == g.id) {
                    why = "строка " + std::to_string(line_no) + ": группа «" + g.id
                          + "» открыта второй раз";
                    return false;
                }
            }
            out.groups.push_back(std::move(g));
        }
        out.groups.back().handles.push_back(std::move(h));
    }
    if (out.groups.empty()) {
        why = "ни одной строки ручки";
        return false;
    }
    return true;
}

bool read_face_manifest(const std::filesystem::path& path, FacePlan& out, std::string& why) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        why = "файл не открылся: " + path.string();
        return false;
    }
    std::ostringstream buf;
    buf << f.rdbuf();
    return parse_face_manifest(buf.str(), out, why);
}

bool read_face_bands(const std::filesystem::path& path, std::vector<FaceBand>& out,
                     std::string& why) {
    out.clear();
    why.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    std::string raw;
    std::size_t line_no = 0;
    while (std::getline(f, raw)) {
        ++line_no;
        const std::size_t hash = raw.find('#');
        const std::string_view line = trim(std::string_view(raw).substr(0, hash));
        if (line.empty()) {
            continue;
        }
        std::istringstream s{std::string(line)};
        FaceBand b;
        std::string lo_s;
        std::string hi_s;
        std::string flag;
        s >> b.name >> lo_s >> hi_s >> flag;
        if (b.name.empty() || !parse_float(lo_s, b.lo) || !parse_float(hi_s, b.hi)
            || !(b.lo < b.hi) || (flag != "measured" && flag != "blind")) {
            why = path.string() + ": строка " + std::to_string(line_no)
                  + " — не «имя lo hi measured|blind»";
            out.clear();
            return false;
        }
        b.measured = flag == "measured";
        out.push_back(std::move(b));
    }
    return true;
}

std::size_t face_plan_apply_bands(FacePlan& plan, const std::vector<FaceBand>& bands) {
    std::size_t touched = 0;
    for (const FaceBand& b : bands) {
        if (FaceHandle* h = plan.find(b.name); h != nullptr) {
            h->measured = b.measured;
            ++touched;
        }
    }
    return touched;
}

} // namespace dfn::app
