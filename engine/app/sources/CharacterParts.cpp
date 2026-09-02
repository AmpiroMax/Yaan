/*
Module: engine/app
File: engine/app/sources/CharacterParts.cpp

Responsibility:
- Implements CharacterParts: skeleton check, bone-ratio scale, GPU upload of
  each selected part and its sheets, the draws and the hidden-vertex union;
  follow maps against the neutral body, rigid frames from face masks, the
  per-slider follow and its instruments; the face.masks reader.

Dependencies:
- Uses: CharacterParts.h, CharacterTextures (sheet_asset), AppDoors.
- Used by: dfn_app, tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every refusal names the part and the reason on stderr.
*/

#include "engine/app/sources/CharacterParts.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/CharacterTextures.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace dfn::app {
namespace {

/// МНОЖИТЕЛЬ «ЧАСТИ → ТЕЛО» ПО КОСТЯМ: медиана отношения длин смещений
/// некорневых суставов. Корень несёт заземление (сдвиг подошв на y = 0) и в
/// отношение не входит. False — не тот риг (имена, порядок, разброс).
[[nodiscard]] bool scale_between(const skel::Skeleton& body, const skel::Skeleton& parts,
                                 const std::string& label, float& out) {
    if (body.joints.size() != parts.joints.size()) {
        std::fprintf(stderr,
                     "[parts] \"%s\": %zu суставов против %zu у тела — не тот риг, "
                     "части не прикреплены\n",
                     label.c_str(), parts.joints.size(), body.joints.size());
        return false;
    }
    for (std::size_t i = 0; i < body.joints.size(); ++i) {
        if (body.joints[i].name != parts.joints[i].name
            || body.joints[i].parent != parts.joints[i].parent) {
            std::fprintf(stderr,
                         "[parts] \"%s\": сустав %zu — «%s» у частей, «%s» у тела — не "
                         "тот риг, части не прикреплены\n",
                         label.c_str(), i, parts.joints[i].name.c_str(),
                         body.joints[i].name.c_str());
            return false;
        }
    }
    std::vector<float> ratios;
    for (std::size_t i = 0; i < body.joints.size(); ++i) {
        if (body.joints[i].parent < 0) {
            continue;
        }
        const float lp = glm::length(parts.joints[i].bind_translation);
        const float lb = glm::length(body.joints[i].bind_translation);
        if (lp > 1e-3f && lb > 1e-3f) {
            ratios.push_back(lb / lp);
        }
    }
    if (ratios.empty()) {
        std::fprintf(stderr, "[parts] \"%s\": нет ни одного смещения кости, по которому "
                             "взять масштаб — части не прикреплены\n",
                     label.c_str());
        return false;
    }
    std::sort(ratios.begin(), ratios.end());
    const float k = ratios[ratios.size() / 2];
    const float spread = (ratios.back() - ratios.front()) / k;
    if (spread > 0.01f) {
        std::fprintf(stderr,
                     "[parts] \"%s\": кости тела относятся к костям частей как %.4f..%.4f "
                     "(разброс %.2f %%) — не равномерный масштаб, не то тело; части не "
                     "прикреплены\n",
                     label.c_str(), static_cast<double>(ratios.front()),
                     static_cast<double>(ratios.back()),
                     static_cast<double>(spread * 100.0f));
        return false;
    }
    out = k;
    return true;
}

} // namespace

PartsArm parts_arm_door() {
    static const PartsArm arm = [] {
        PartsArm a;
        const char* v = door_value("DFN_PARTS_ARM");
        if (v == nullptr || v[0] == '\0') {
            return a;
        }
        const std::string text = v;
        if (text.find("nocutout") != std::string::npos) {
            a.cutout = false;
        }
        if (text.find("flat") != std::string::npos) {
            a.two_sided = false;
        }
        if (text.find("nonormal") != std::string::npos) {
            a.normal = false;
        }
        std::fprintf(stderr,
                     "[parts] DFN_PARTS_ARM=%s: вырез %s, двусторонний свет %s, рельеф "
                     "%s (контрольная рука)\n",
                     v, a.cutout ? "да" : "НЕТ", a.two_sided ? "да" : "НЕТ",
                     a.normal ? "да" : "НЕТ");
        return a;
    }();
    return arm;
}

bool parts_follow_door() {
    static const bool on = [] {
        const char* v = door_value("DFN_PARTS_FOLLOW");
        const bool off = v != nullptr && v[0] == '0';
        if (off) {
            std::fprintf(stderr, "[parts] DFN_PARTS_FOLLOW=0: части морфам НЕ следуют "
                                 "(контрольная рука)\n");
        }
        return !off;
    }();
    return on;
}

const std::vector<std::uint32_t>* FaceMasks::find(std::string_view name) const {
    for (const auto& [n, verts] : masks) {
        if (n == name) {
            return &verts;
        }
    }
    return nullptr;
}

bool read_face_masks(const std::filesystem::path& path, FaceMasks& out) {
    out = FaceMasks{};
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "[parts] масок лица \"%s\" нет — глаза и зубы поедут "
                             "переносом, не жёстко\n", path.string().c_str());
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream row(line);
        std::string head;
        row >> head;
        if (head == "verts") {
            row >> out.verts;
            continue;
        }
        if (head.empty() || head.back() != ':') {
            continue;
        }
        head.pop_back();
        std::vector<std::uint32_t> verts;
        std::uint32_t v = 0;
        while (row >> v) {
            verts.push_back(v);
        }
        std::sort(verts.begin(), verts.end());
        verts.erase(std::unique(verts.begin(), verts.end()), verts.end());
        out.masks.emplace_back(std::move(head), std::move(verts));
    }
    if (out.verts == 0 || out.masks.empty()) {
        std::fprintf(stderr, "[parts] \"%s\": не маски лица (нет строки verts или "
                             "областей) — отвергнуты\n", path.string().c_str());
        out = FaceMasks{};
        return false;
    }
    return true;
}

bool part_selected(const char* selection, std::string_view name) {
    if (selection == nullptr || selection[0] == '\0') {
        return true;
    }
    if (std::strcmp(selection, "none") == 0 || std::strcmp(selection, "0") == 0) {
        return false;
    }
    std::string_view rest = selection;
    while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        const std::string_view item = rest.substr(0, comma);
        if (item == name) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(comma + 1);
    }
    return false;
}

bool CharacterParts::attach(render::RenderSystem& render_system,
                            platform::IRenderer& renderer,
                            const render::RegistryObject& object,
                            const std::filesystem::path& label,
                            const skel::Skeleton& body_skeleton, uint32_t first_mesh_id,
                            uint32_t max_parts, const char* selection,
                            std::span<const platform::SkinnedVertex> neutral_body,
                            std::span<const uint32_t> neutral_indices,
                            const FaceMasks* masks) {
    const std::string name = label.string();
    if (object.parts.empty()) {
        std::fprintf(stderr, "[parts] \"%s\": нет секции PART — это не набор частей\n",
                     name.c_str());
        return false;
    }
    float k = 1.0f;
    if (!scale_between(body_skeleton, object.skeleton, name, k)) {
        return false;
    }
    last_scale_ = k;
    const PartsArm arm = parts_arm_door();
    // НЕЙТРАЛЬ — ОДНА НА НАБОР, в масштабе частей. Второй attach (одежда после
    // волос) обязан принести то же тело: другое число вершин — другое тело, и
    // его части следовать не могут.
    bool follow_here = parts_follow_door() && !neutral_body.empty();
    if (follow_here) {
        if (!neutral_.empty() && neutral_.size() != neutral_body.size()) {
            std::fprintf(stderr,
                         "[parts] \"%s\": нейтраль на %zu вершин против %zu у уже "
                         "прикреплённых частей — не то тело, этот набор морфам не "
                         "следует\n",
                         name.c_str(), neutral_body.size(), neutral_.size());
            follow_here = false;
        } else if (neutral_.empty()) {
            neutral_.assign(neutral_body.begin(), neutral_body.end());
            neutral_indices_.assign(neutral_indices.begin(), neutral_indices.end());
            if (std::fabs(k - 1.0f) > 1e-6f) {
                for (platform::SkinnedVertex& v : neutral_) {
                    v.position *= k;
                }
            }
        }
    } else if (parts_follow_door() && neutral_.empty()) {
        std::fprintf(stderr, "[parts] \"%s\": нейтрального тела не дано — части морфам "
                             "не следуют\n", name.c_str());
    }
    // Имена из дозы, которых в файле нет, — вслух: «часть, которую попросили
    // и не получили» иначе неотличима от «части не бывает».
    if (selection != nullptr && selection[0] != '\0' && std::strcmp(selection, "none") != 0
        && std::strcmp(selection, "0") != 0) {
        std::string_view rest = selection;
        while (!rest.empty()) {
            const std::size_t comma = rest.find(',');
            const std::string_view item = rest.substr(0, comma);
            bool known = false;
            for (const render::SkinPart& p : object.parts) {
                known = known || p.name == item;
            }
            if (!known) {
                std::fprintf(stderr,
                             "[parts] \"%s\": в дозе названа часть «%.*s», которой в файле "
                             "нет (есть:",
                             name.c_str(), static_cast<int>(item.size()), item.data());
                for (const render::SkinPart& p : object.parts) {
                    std::fprintf(stderr, " %s", p.name.c_str());
                }
                std::fprintf(stderr, ")\n");
            }
            if (comma == std::string_view::npos) {
                break;
            }
            rest.remove_prefix(comma + 1);
        }
    }
    std::size_t attached = 0;
    std::vector<platform::SkinnedVertex> scaled;
    for (const render::SkinPart& part : object.parts) {
        if (!part_selected(selection, part.name)) {
            continue;
        }
        if (parts_.size() >= max_parts) {
            std::fprintf(stderr,
                         "[parts] \"%s\": часть «%s» не прикреплена — полоса номеров "
                         "мешей хозяина исчерпана (%u частей)\n",
                         name.c_str(), part.name.c_str(), max_parts);
            continue;
        }
        const uint32_t mesh_id = first_mesh_id + static_cast<uint32_t>(parts_.size());
        std::span<const platform::SkinnedVertex> verts = part.mesh.vertices;
        if (std::fabs(k - 1.0f) > 1e-6f) {
            scaled = part.mesh.vertices;
            for (platform::SkinnedVertex& v : scaled) {
                v.position *= k;
            }
            verts = scaled;
        }
        if (!render_system.register_skinned_mesh(renderer, mesh_id, verts,
                                                 part.mesh.indices)) {
            std::fprintf(stderr,
                         "[parts] \"%s\": часть «%s» отвергнута реестром мешей под номером "
                         "%u — не прикреплена\n",
                         name.c_str(), part.name.c_str(), mesh_id);
            continue;
        }
        AttachedPart a;
        a.name = part.name;
        a.mesh_asset = mesh_id;
        a.triangles = part.mesh.indices.size() / 3;
        if (const render::TextureRef* albedo = part.texture("albedo"); albedo != nullptr) {
            a.texture_asset =
                sheet_asset(render_system, renderer, *albedo, name + ":" + part.name, label);
        }
        if (const render::TextureRef* normal = part.texture("normal");
            normal != nullptr && arm.normal) {
            a.normal_asset =
                sheet_asset(render_system, renderer, *normal, name + ":" + part.name, label);
        }
        a.cutout = part.alpha_mask && arm.cutout;
        a.alpha_cutoff = part.alpha_cutoff;
        a.two_sided = part.double_sided && arm.two_sided;
        a.hide_body_vertices = part.hide_body_vertices;
        a.rest.assign(verts.begin(), verts.end());
        a.indices = part.mesh.indices;
        if (follow_here) {
            bind_follow(a, name, masks);
        }
        std::fprintf(stderr,
                     "[parts] \"%s\": часть «%s» — меш %u, %zu треугольников, %s%s%s, "
                     "листы albedo %u normal %u, закрывает %zu вершин тела, масштаб "
                     "%.4f, морфам %s\n",
                     name.c_str(), a.name.c_str(), a.mesh_asset, a.triangles,
                     a.cutout ? "вырез" : "сплошная",
                     a.two_sided ? ", двусторонний свет" : "",
                     a.normal_asset != 0 ? ", рельеф" : "", a.texture_asset,
                     a.normal_asset, a.hide_body_vertices.size(), static_cast<double>(k),
                     a.follow == PartFollow::Transfer ? "следует переносом"
                     : a.follow == PartFollow::Rigid  ? "следует жёстко (маски)"
                                                      : "НЕ следует");
        parts_.push_back(std::move(a));
        ++attached;
    }
    if (attached == 0) {
        std::fprintf(stderr, "[parts] \"%s\": ни одна часть не прикреплена (доза «%s»)\n",
                     name.c_str(), selection != nullptr ? selection : "");
        return false;
    }
    return true;
}

void CharacterParts::release(render::RenderSystem& render_system,
                             platform::IRenderer& renderer) {
    for (const AttachedPart& p : parts_) {
        (void)render_system.drop_skinned_mesh(renderer, p.mesh_asset);
    }
    parts_.clear();
    neutral_.clear();
    neutral_indices_.clear();
    last_scale_ = 1.0f;
    last_follow_ms_ = 0.0;
}

void CharacterParts::append_draws(render::RenderSystem::SkinnedDraw body,
                                  std::vector<render::RenderSystem::SkinnedDraw>& out) const {
    for (const AttachedPart& p : parts_) {
        render::RenderSystem::SkinnedDraw d = body;
        d.mesh_asset = p.mesh_asset;
        d.texture_asset = p.texture_asset;
        d.normal_asset = p.normal_asset;
        d.cutout = p.cutout;
        d.alpha_cutoff = p.alpha_cutoff;
        d.two_sided = p.two_sided;
        out.push_back(d);
    }
}

std::vector<uint32_t> CharacterParts::hidden_body_vertices() const {
    std::vector<uint32_t> all;
    for (const AttachedPart& p : parts_) {
        all.insert(all.end(), p.hide_body_vertices.begin(), p.hide_body_vertices.end());
    }
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    return all;
}

// ------------------------------------------------ СЛЕДОВАНИЕ ЗА МОРФАМИ ---

namespace {

/// ЖЁСТКИЕ ЧАСТИ ПО ИМЕНИ: глаза — две рамки (eye-l / eye-r) с масштабом,
/// зубы и язык — одна рамка рта без масштаба. Всё остальное — перенос.
struct RigidRecipe {
    std::vector<const char*> masks;
    bool scale = false;
    bool one_group = false; ///< объединить маски в одну рамку
};
[[nodiscard]] bool rigid_recipe(std::string_view part, RigidRecipe& out) {
    if (part == "eyes") {
        out = RigidRecipe{{"eye-l", "eye-r"}, true, false};
        return true;
    }
    if (part == "teeth" || part == "tongue") {
        out = RigidRecipe{{"mouth-angles", "lip-upper", "lip-lower"}, false, true};
        return true;
    }
    return false;
}

} // namespace

void CharacterParts::bind_follow(AttachedPart& part, const std::string& label,
                                 const FaceMasks* masks) {
    part.follow = PartFollow::None;
    part.map.binds.clear();
    part.rigid.clear();
    if (neutral_.empty() || part.rest.empty()) {
        return;
    }
    render::build_follow_map(neutral_, neutral_indices_, part.rest, part.map);
    if (part.map.binds.size() != part.rest.size()) {
        std::fprintf(stderr, "[parts] \"%s\": часть «%s» — карта соседей не построилась "
                             "(%zu из %zu) — не следует\n",
                     label.c_str(), part.name.c_str(), part.map.binds.size(),
                     part.rest.size());
        return;
    }
    part.follow = PartFollow::Transfer;
    RigidRecipe recipe;
    if (!rigid_recipe(part.name, recipe)) {
        return;
    }
    if (masks == nullptr || masks->empty()) {
        std::fprintf(stderr, "[parts] \"%s\": часть «%s» без масок лица — следует "
                             "переносом, не жёстко\n", label.c_str(), part.name.c_str());
        return;
    }
    if (masks->verts != neutral_.size()) {
        std::fprintf(stderr, "[parts] \"%s\": маски лица на %u вершин против %zu у тела — "
                             "часть «%s» следует переносом, не жёстко\n",
                     label.c_str(), masks->verts, neutral_.size(), part.name.c_str());
        return;
    }
    std::vector<RigidGroup> groups;
    for (const char* mask_name : recipe.masks) {
        const std::vector<std::uint32_t>* mask = masks->find(mask_name);
        if (mask == nullptr || mask->empty()) {
            std::fprintf(stderr, "[parts] \"%s\": маски «%s» нет — часть «%s» без неё\n",
                         label.c_str(), mask_name, part.name.c_str());
            continue;
        }
        if (recipe.one_group && !groups.empty()) {
            RigidGroup& g = groups.front();
            g.mask.insert(g.mask.end(), mask->begin(), mask->end());
            g.mask_name += std::string("+") + mask_name;
            continue;
        }
        RigidGroup g;
        g.mask_name = mask_name;
        g.mask = *mask;
        g.scale = recipe.scale;
        groups.push_back(std::move(g));
    }
    if (groups.empty()) {
        std::fprintf(stderr, "[parts] \"%s\": ни одной маски для «%s» — следует "
                             "переносом\n", label.c_str(), part.name.c_str());
        return;
    }
    for (RigidGroup& g : groups) {
        std::sort(g.mask.begin(), g.mask.end());
        g.mask.erase(std::unique(g.mask.begin(), g.mask.end()), g.mask.end());
        g.rest = render::rigid_frame(neutral_, g.mask);
    }
    // Каждая вершина части — к ближайшей рамке (у глаз: левое яблоко к левой
    // маске). Одна рамка — все вершины ей.
    for (std::uint32_t i = 0; i < part.rest.size(); ++i) {
        std::size_t best = 0;
        float best_d = std::numeric_limits<float>::max();
        for (std::size_t g = 0; g < groups.size(); ++g) {
            const float d = glm::length(part.rest[i].position - groups[g].rest.centroid);
            if (d < best_d) {
                best_d = d;
                best = g;
            }
        }
        groups[best].vertices.push_back(i);
    }
    part.rigid = std::move(groups);
    part.follow = PartFollow::Rigid;
    for (const RigidGroup& g : part.rigid) {
        std::fprintf(stderr,
                     "[parts] \"%s\": «%s» жёстко за маской %s (%zu вершин тела, радиус "
                     "%.1f мм) — %zu вершин части%s\n",
                     label.c_str(), part.name.c_str(), g.mask_name.c_str(), g.mask.size(),
                     static_cast<double>(g.rest.radius * 1000.0f), g.vertices.size(),
                     g.scale ? ", с масштабом" : "");
    }
}

void CharacterParts::pose_part(const AttachedPart& part,
                               std::span<const platform::SkinnedVertex> body_now,
                               std::vector<platform::SkinnedVertex>& out) const {
    switch (part.follow) {
    case PartFollow::Transfer:
        render::apply_follow(neutral_, neutral_indices_, body_now, part.map, part.rest,
                             part.indices, out);
        return;
    case PartFollow::Rigid:
        out.assign(part.rest.begin(), part.rest.end());
        for (const RigidGroup& g : part.rigid) {
            const render::RigidFrame now = render::rigid_frame(body_now, g.mask);
            render::apply_rigid(g.rest, now, g.scale, g.vertices, out);
        }
        return;
    case PartFollow::None:
        out.assign(part.rest.begin(), part.rest.end());
        return;
    }
}

bool CharacterParts::following() const {
    for (const AttachedPart& p : parts_) {
        if (p.follow != PartFollow::None) {
            return true;
        }
    }
    return false;
}

bool CharacterParts::follow(render::RenderSystem& render_system,
                            platform::IRenderer& renderer,
                            std::span<const platform::SkinnedVertex> body_now) {
    if (neutral_.empty()) {
        return true;
    }
    if (body_now.size() != neutral_.size()) {
        std::fprintf(stderr, "[parts] follow: тело на %zu вершин против нейтрали %zu — "
                             "части стоят\n", body_now.size(), neutral_.size());
        return false;
    }
    const auto t0 = std::chrono::steady_clock::now();
    bool ok = true;
    for (AttachedPart& p : parts_) {
        if (p.follow == PartFollow::None) {
            continue;
        }
        pose_part(p, body_now, p.now);
        if (!render_system.replace_skinned_mesh(renderer, p.mesh_asset, p.now, p.indices)) {
            std::fprintf(stderr, "[parts] часть «%s»: перекладка меша %u за морфом не "
                                 "удалась — стоит в прежней форме\n",
                         p.name.c_str(), p.mesh_asset);
            ok = false;
        }
    }
    last_follow_ms_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
            .count();
    return ok;
}

std::vector<PartFollowReport>
CharacterParts::follow_report(std::span<const platform::SkinnedVertex> body_now) const {
    std::vector<PartFollowReport> out;
    if (neutral_.empty() || body_now.size() != neutral_.size()) {
        return out;
    }
    std::vector<platform::SkinnedVertex> posed;
    for (const AttachedPart& p : parts_) {
        PartFollowReport r;
        r.name = p.name;
        r.follow = p.follow;
        pose_part(p, body_now, posed);
        r.vertices = p.rest.size();
        if (!p.map.empty()) {
            render::follow_gap_change(neutral_, neutral_indices_, body_now, p.map, p.rest,
                                      posed, r.gap_grow_m, r.gap_shrink_m);
            r.vertex_gap_error_m =
                render::follow_vertex_gap_error(neutral_, body_now, p.map, p.rest, posed);
            r.under_skin_rest = render::follow_penetrations(neutral_, neutral_indices_, p.map,
                                                            p.rest, 0.001f);
            r.under_skin_now = render::follow_penetrations(body_now, neutral_indices_, p.map,
                                                           posed, 0.001f);
        }
        for (const RigidGroup& g : p.rigid) {
            // Центр группы вершин части против центроида маски: сдвиг этой
            // разницы от реста и есть «глаз выехал из глазницы».
            glm::vec3 c_rest{0.0f};
            glm::vec3 c_now{0.0f};
            for (const std::uint32_t i : g.vertices) {
                c_rest += p.rest[i].position;
                c_now += posed[i].position;
            }
            if (!g.vertices.empty()) {
                c_rest /= static_cast<float>(g.vertices.size());
                c_now /= static_cast<float>(g.vertices.size());
            }
            const render::RigidFrame now = render::rigid_frame(body_now, g.mask);
            const glm::vec3 off_rest = c_rest - g.rest.centroid;
            const glm::vec3 off_now = c_now - now.centroid;
            r.rigid_offset_m = std::max(r.rigid_offset_m, glm::length(off_now - off_rest));
        }
        out.push_back(std::move(r));
    }
    return out;
}

} // namespace dfn::app
