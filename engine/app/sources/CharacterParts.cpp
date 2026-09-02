/*
Module: engine/app
File: engine/app/sources/CharacterParts.cpp

Responsibility:
- Implements CharacterParts: skeleton check, bone-ratio scale, GPU upload of
  each selected part and its sheets, the draws and the hidden-vertex union.

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
#include <cmath>
#include <cstdio>
#include <cstring>

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
                            uint32_t max_parts, const char* selection) {
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
        std::fprintf(stderr,
                     "[parts] \"%s\": часть «%s» — меш %u, %zu треугольников, %s%s%s, "
                     "листы albedo %u normal %u, закрывает %zu вершин тела, масштаб "
                     "%.4f\n",
                     name.c_str(), a.name.c_str(), a.mesh_asset, a.triangles,
                     a.cutout ? "вырез" : "сплошная",
                     a.two_sided ? ", двусторонний свет" : "",
                     a.normal_asset != 0 ? ", рельеф" : "", a.texture_asset,
                     a.normal_asset, a.hide_body_vertices.size(), static_cast<double>(k));
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
    last_scale_ = 1.0f;
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

} // namespace dfn::app
