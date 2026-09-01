/*
Module: engine/app
File: engine/app/sources/ModelViewer.cpp

Responsibility:
- The viewing stand's model: the scan of the three sources, the orbiting eye's
  arithmetic, and the framing. See ModelViewer.h.

Dependencies:
- Uses: glm, std only. No render, no window (Rule 3).
- Used by: engine/app (AppViewer.cpp), tests/app/ModelViewerTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Nothing here may read the clock, the filesystem's ORDER, or a door: the scan
  sorts what it found, so two runs list the same models in the same places.
*/

#include "engine/app/sources/ModelViewer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace dfn::app {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool ends_with(std::string_view s, std::string_view tail) {
    return s.size() >= tail.size()
           && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

[[nodiscard]] std::string lowered(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

/// WHAT A DOWNLOADED FILE SAYS ABOUT ITSELF. The wave that fetched the figures
/// leaves a SOURCE.txt beside each one; its first non-comment, non-empty line
/// is the human answer to "where is this from". A folder without one is not a
/// defect — the path is then the provenance, which is the truth available.
[[nodiscard]] std::string origin_from_dir(const fs::path& dir, const fs::path& file) {
    std::error_code ec;
    const fs::path note = dir / "SOURCE.txt";
    if (fs::is_regular_file(note, ec)) {
        std::ifstream in(note);
        std::string line;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line.empty() || line[0] == '#') {
                continue;
            }
            return line;
        }
    }
    return file.string();
}

/// THE FOLDER A DOWNLOADED FILE IS LISTED UNDER: its path relative to the
/// downloads root, without the file name. "gallery/kaykit-knight". A file
/// sitting directly in the root gets the root's own name, never an empty
/// category — an unlabelled row is a row nobody can look up afterwards.
[[nodiscard]] std::string relative_category(const fs::path& root, const fs::path& file) {
    std::error_code ec;
    const fs::path rel = fs::relative(file.parent_path(), root, ec);
    if (ec || rel.empty() || rel == ".") {
        return root.filename().string();
    }
    return rel.generic_string();
}

[[nodiscard]] bool is_far_form(const std::string& stem) {
    return stem.size() > 4 && stem.compare(stem.size() - 4, 4, "-far") == 0;
}

} // namespace

std::vector<ViewerItem> scan_viewer_items(const ViewerRoots& roots) {
    std::vector<ViewerItem> items;
    std::error_code ec;

    // (a) THE SHELVES. Every .dfo under assets/objects, whatever folder depth
    // a zone chose for its own forge: the list must not need a code change the
    // day someone bakes a new family, which is the same reason build_palette()
    // reads the shelf instead of a hand-written list.
    if (fs::is_directory(roots.shelves, ec)) {
        for (auto it = fs::recursive_directory_iterator(
                 roots.shelves, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                break;
            }
            const fs::path& p = it->path();
            if (p.extension() != ".dfo") {
                continue;
            }
            const std::string stem = p.stem().string();
            if (is_far_form(stem)) {
                continue;
            }
            ViewerItem item;
            item.name = stem;
            item.category = relative_category(roots.shelves, p);
            item.origin = p.generic_string();
            item.path = p.generic_string();
            item.source = ViewerSource::Shelf;
            items.push_back(std::move(item));
        }
    }

    // (b) THE CHARACTER MODELS AS THEY ARRIVED. glTF, not the .dfo our importer
    // bakes from them: the owner asked to see "что накачал", and the baked file
    // is already covered by (a). Listing both is the point — the pair is how a
    // reshape becomes visible.
    if (fs::is_directory(roots.characters, ec)) {
        for (const auto& e : fs::directory_iterator(roots.characters, ec)) {
            if (ec) {
                break;
            }
            const std::string ext = lowered(e.path().extension().string());
            if (ext != ".glb" && ext != ".gltf") {
                continue;
            }
            ViewerItem item;
            item.name = e.path().stem().string();
            item.category = fs::path(roots.characters).filename().string();
            item.origin = origin_from_dir(e.path().parent_path(), e.path());
            item.path = e.path().generic_string();
            item.source = ViewerSource::Character;
            items.push_back(std::move(item));
        }
    }

    // (c) EVERYTHING DOWNLOADED. The folder is READ, never rearranged: another
    // wave fills it while this one runs, and a viewer that moved files would
    // race it.
    if (fs::is_directory(roots.downloads, ec)) {
        for (auto it = fs::recursive_directory_iterator(
                 roots.downloads, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                break;
            }
            const fs::path& p = it->path();
            const std::string ext = lowered(p.extension().string());
            if (ext != ".stl" && ext != ".glb" && ext != ".gltf") {
                continue;
            }
            // THE CACHE IS OUR OWN OUTPUT AND IS NOT A SOURCE. Without this the
            // second run of the stand lists every converted model twice — once
            // as the file that was fetched and once as the file we made of it.
            const std::string rel = relative_category(roots.downloads, p);
            if (rel.rfind(".cache", 0) == 0) {
                continue;
            }
            ViewerItem item;
            item.name = p.stem().string();
            item.category = rel;
            item.origin = origin_from_dir(p.parent_path(), p);
            item.path = p.generic_string();
            item.source = ViewerSource::Downloaded;
            items.push_back(std::move(item));
        }
    }

    std::sort(items.begin(), items.end(), [](const ViewerItem& a, const ViewerItem& b) {
        if (a.source != b.source) {
            return static_cast<int>(a.source) < static_cast<int>(b.source);
        }
        if (a.category != b.category) {
            return a.category < b.category;
        }
        if (a.name != b.name) {
            return a.name < b.name;
        }
        return a.path < b.path;
    });
    return items;
}

float viewer_display_scale(ViewerSource source, const glm::vec3& lo,
                           const glm::vec3& hi) {
    if (source != ViewerSource::Downloaded) {
        return 1.0f; // our own pipeline speaks metres -- see the header
    }
    const glm::vec3 span = hi - lo;
    const float biggest = std::max(span.x, std::max(span.y, span.z));
    if (!(biggest > 0.0f)) {
        return 1.0f; // an empty bound: scaling nothing by anything is nothing
    }
    return VIEWER_SCALE_TARGET_M / biggest;
}

float viewer_fit_distance(const glm::vec3& lo, const glm::vec3& hi, float fov_y,
                          float aspect) {
    const glm::vec3 span = hi - lo;
    // The cylinder about +Y that contains the bound: a radius across, a half
    // height up. Invariant under every turn this mode can make (see header).
    const float r = 0.5f * std::sqrt(span.x * span.x + span.z * span.z);
    const float half_height = 0.5f * span.y;
    if (!(fov_y > 0.0f) || (!(r > 0.0f) && !(half_height > 0.0f))) {
        return 1.0f;
    }
    const float half_v = 0.5f * fov_y;
    const float half_h = std::atan(std::tan(half_v) * std::max(aspect, 0.01f));

    // VERTICAL. The highest point of the cylinder can also be its NEAREST point,
    // so the depth available to it is (d - r), not d. Solving
    // half_height <= (d - r) * tan(half_v) for d is what the `+ r` is.
    const float d_v = half_height / std::max(std::tan(half_v), 1e-3f) + r;
    // HORIZONTAL. A circle of radius r fits the cone when the eye stands
    // r / sin(half) away — the tangent condition, and the one place a sphere's
    // arithmetic is the right arithmetic.
    const float d_h = r / std::max(std::sin(half_h), 1e-3f);
    return std::max(d_v, d_h) * VIEWER_FIT_MARGIN;
}

ViewerView viewer_reset(ViewerSource source, const glm::vec3& lo, const glm::vec3& hi,
                        float fov_y, float aspect) {
    const float scale = viewer_display_scale(source, lo, hi);
    ViewerView v;
    v.orbit_yaw = VIEWER_START_YAW;
    v.orbit_pitch = VIEWER_START_PITCH;
    v.fit_dist_m = viewer_fit_distance(lo * scale, hi * scale, fov_y, aspect);
    v.dist_m = v.fit_dist_m;
    v.target_y = 0.5f * (lo.y + hi.y) * scale;
    v.model_yaw = 0.0f;
    return v;
}

void viewer_orbit(ViewerView& view, float dx_px, float dy_px, float sensitivity) {
    view.orbit_yaw += dx_px * sensitivity;
    // JUST SHORT OF THE POLES. At exactly +-pi/2 the azimuth stops meaning
    // anything and the frame rolls around the model as the mouse moves
    // sideways, which reads as the model spinning.
    constexpr float POLE = 1.5533f; // 89 degrees
    view.orbit_pitch = std::clamp(view.orbit_pitch - dy_px * sensitivity, -POLE, POLE);
}

void viewer_zoom(ViewerView& view, float notches) {
    if (notches == 0.0f) {
        return;
    }
    view.dist_m *= std::pow(1.0f - VIEWER_ZOOM_STEP, notches);
    view.dist_m = std::clamp(view.dist_m, view.fit_dist_m * VIEWER_ZOOM_MIN_FACTOR,
                             view.fit_dist_m * VIEWER_ZOOM_MAX_FACTOR);
}

glm::vec3 viewer_eye(const glm::vec3& target, const ViewerView& view) {
    // The frame's convention (FirstPersonCamera): yaw 0 looks along -Z, +yaw
    // right, +pitch up. The eye stands OPPOSITE the look direction, so that
    // the pose {eye, yaw, pitch} handed to the camera looks at `target`.
    const float cp = std::cos(view.orbit_pitch);
    const glm::vec3 look{std::sin(view.orbit_yaw) * cp, std::sin(view.orbit_pitch),
                         -std::cos(view.orbit_yaw) * cp};
    return target - look * view.dist_m;
}

glm::vec3 viewer_target(const glm::vec3& pad_origin, const glm::vec3& lo,
                        const glm::vec3& hi, float scale) {
    // The model STANDS on the pad: its own lowest point is put on the ground,
    // and the eye looks at the middle of what is then above it. Looking at the
    // origin instead would frame the ankles of anything modelled from its feet
    // and the waist of anything modelled from its centre — two conventions
    // this tree genuinely contains.
    const float mid = 0.5f * (hi.y - lo.y) * scale;
    return {pad_origin.x, pad_origin.y + mid, pad_origin.z};
}

int viewer_step_index(const std::vector<ViewerItem>& items, int index, int step,
                      bool by_category) {
    const int n = static_cast<int>(items.size());
    if (n == 0) {
        return 0;
    }
    const int dir = step >= 0 ? 1 : -1;
    int at = ((index % n) + n) % n;
    if (!by_category) {
        return ((at + dir) % n + n) % n;
    }
    // BY CATEGORY: walk until the category name changes, then — going
    // backwards — keep walking to the FIRST line of the category we landed in.
    // Without that second half, "previous category" would land on the LAST
    // part of the previous shelf, which reads as one step back, not a jump.
    const std::string from = items[static_cast<std::size_t>(at)].category;
    int guard = 0;
    while (guard++ < n) {
        at = ((at + dir) % n + n) % n;
        if (items[static_cast<std::size_t>(at)].category != from) {
            break;
        }
    }
    if (dir < 0) {
        const std::string landed = items[static_cast<std::size_t>(at)].category;
        guard = 0;
        while (guard++ < n) {
            const int prev = ((at - 1) % n + n) % n;
            if (prev == at || items[static_cast<std::size_t>(prev)].category != landed) {
                break;
            }
            at = prev;
        }
    }
    return at;
}

int viewer_find_item(const std::vector<ViewerItem>& items, const std::string& name) {
    if (name.empty()) {
        return -1;
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].name == name) {
            return static_cast<int>(i);
        }
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (ends_with(items[i].path, name)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

ViewerTally viewer_tally(const std::vector<ViewerItem>& items) {
    ViewerTally t;
    for (const ViewerItem& i : items) {
        switch (i.source) {
        case ViewerSource::Shelf: ++t.shelf; break;
        case ViewerSource::Character: ++t.character; break;
        case ViewerSource::Downloaded: ++t.downloaded; break;
        case ViewerSource::Count: break;
        }
    }
    return t;
}

ViewerSize viewer_size(ViewerSource source, const glm::vec3& lo, const glm::vec3& hi) {
    const glm::vec3 span = hi - lo;
    ViewerSize s;
    s.width_m = std::max(span.x, 0.0f);
    s.height_m = std::max(span.y, 0.0f);
    s.depth_m = std::max(span.z, 0.0f);
    s.scale = viewer_display_scale(source, lo, hi);
    return s;
}

} // namespace dfn::app
