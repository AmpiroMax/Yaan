/*
Module: engine/render
File: engine/render/sources/MorphBlend.cpp

Responsibility:
- Реализация CPU-бленда морфов. См. MorphBlend.h — там весь довод.

Dependencies:
- Uses: MorphBlend.h, glm.
- Used by: dfn_render.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НИ ОДНОГО РАННЕГО ВЫХОДА ПО «весов нет — и ладно»: пустые веса означают
  «рест-поза», и рест-поза обязана выйти отсюда КОПИЕЙ входа, а не пустотой.
*/

#include "engine/render/sources/MorphBlend.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace dfn::render {

int morph_index(std::span<const MorphTarget> targets, std::string_view name) {
    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (targets[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void blend_morphs(std::span<const platform::SkinnedVertex> rest,
                  std::span<const MorphTarget> targets,
                  std::span<const float> weights,
                  std::span<const std::uint32_t> indices,
                  std::vector<platform::SkinnedVertex>& out) {
    out.assign(rest.begin(), rest.end());
    if (rest.empty()) {
        return;
    }
    // СКОЛЬКО ЦЕЛЕЙ МЫ ВПРАВЕ СЛОЖИТЬ — минимум из двух длин, а не длина
    // весов. Веса приходят из пресета, а пресет старше файла на одну цель —
    // обычное дело; сложить лишний вес не с чем, и молчаливое чтение за край
    // списка целей было бы куда хуже, чем непрокрученный ползунок.
    const std::size_t n = std::min(targets.size(), weights.size());
    bool moved_any = false;
    for (std::size_t t = 0; t < n; ++t) {
        const float w = weights[t];
        if (w == 0.0f) {
            continue;
        }
        for (const MorphDelta& d : targets[t].deltas) {
            if (d.index >= out.size()) {
                continue; // читатель .dfo это уже отверг; здесь — на случай сборки в памяти
            }
            out[d.index].position += d.offset * w;
            moved_any = true;
        }
    }
    if (!moved_any) {
        return;
    }
    shift_normals_by_difference(rest, indices, out);
}

void shift_normals_by_difference(std::span<const platform::SkinnedVertex> before,
                                 std::span<const std::uint32_t> indices,
                                 std::vector<platform::SkinnedVertex>& out) {
    if (indices.size() < 3 || out.size() != before.size()) {
        return;
    }
    // --- НОРМАЛИ: НЕ ЗАМЕНА, А РАЗНИЦА. Площадно-взвешенная сумма нормалей
    // треугольников (длина векторного произведения — удвоенная площадь, и
    // взвешивание по ней не даёт вееру осколков перекричать ту большую грань,
    // на которой вершина на самом деле лежит — тот же довод и та же формула,
    // что в tools/import_gltf.cpp после --reshape).
    //
    // НО СЧИТАЕТСЯ ОНА ДВАЖДЫ, И ЭТО НЕ РАСТОЧИТЕЛЬСТВО. Вершины .dfo живут в
    // ПРОСТРАНСТВЕ ПРИВЯЗКИ, а нормали в файле посчитаны по РЕСТ-ПОЗЕ и
    // занесены обратно транспонированной матрицей вершины: у тела, подогнанного
    // под канон, это разные вещи (замер: max|W·invBind − I| = 0.386 у
    // HumanBase). Посчитай мы нормаль прямо здесь и ПОЛОЖИ её как есть — тело
    // сменило бы освещение в тот момент, когда все ползунки стоят на нуле, то
    // есть до всякого морфа. Поэтому кладётся РАЗНИЦА двух нормалей одной и
    // той же формулы: n_out = normalize(n_файла + (n1 − n0)). При нулевых
    // весах n1 == n0, и на выходе — БАЙТОВО нормаль файла.
    const auto accumulate = [&indices](std::span<const platform::SkinnedVertex> v,
                                       std::vector<glm::vec3>& n) {
        n.assign(v.size(), glm::vec3{0.0f});
        for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            const std::uint32_t a = indices[i];
            const std::uint32_t b = indices[i + 1];
            const std::uint32_t c = indices[i + 2];
            if (a >= v.size() || b >= v.size() || c >= v.size()) {
                continue;
            }
            const glm::vec3 fn = glm::cross(v[b].position - v[a].position,
                                            v[c].position - v[a].position);
            n[a] += fn;
            n[b] += fn;
            n[c] += fn;
        }
    };
    std::vector<glm::vec3> n0;
    std::vector<glm::vec3> n1;
    accumulate(before, n0);
    accumulate(out, n1);
    for (std::size_t i = 0; i < out.size(); ++i) {
        // ВЫРОЖДЕННАЯ ВЕРШИНА ОСТАЁТСЯ СО СВОЕЙ НОРМАЛЬЮ. Нуль в нормали — это
        // чёрное пятно на теле, и выглядит оно как ошибка света, а не как
        // ошибка геометрии, из-за которой возникло.
        if (glm::length(n0[i]) < 1e-12f || glm::length(n1[i]) < 1e-12f) {
            continue;
        }
        const glm::vec3 moved =
            out[i].normal + (glm::normalize(n1[i]) - glm::normalize(n0[i]));
        if (glm::length(moved) > 1e-6f) {
            out[i].normal = glm::normalize(moved);
        }
    }
}

MorphSpread morph_spread(std::span<const platform::SkinnedVertex> rest,
                         const MorphTarget& target, float weight,
                         float threshold_m) {
    MorphSpread s;
    bool first = true;
    for (const MorphDelta& d : target.deltas) {
        if (d.index >= rest.size()) {
            continue;
        }
        const float len = glm::length(d.offset * weight);
        if (len <= threshold_m) {
            continue;
        }
        ++s.moved;
        s.worst_m = std::max(s.worst_m, len);
        const float y = rest[d.index].position.y;
        if (first) {
            s.lowest_y = y;
            s.highest_y = y;
            first = false;
        } else {
            s.lowest_y = std::min(s.lowest_y, y);
            s.highest_y = std::max(s.highest_y, y);
        }
    }
    return s;
}

} // namespace dfn::render
