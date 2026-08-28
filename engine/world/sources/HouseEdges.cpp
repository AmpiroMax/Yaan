/*
Created: 28:08:2026 - 14:05:00
Last updated: 28:08:2026 - 14:05:00
Module: engine/world
File: engine/world/sources/HouseEdges.cpp

Responsibility:
- ПЕРЕПИСЬ РЁБЕР ГОТОВОГО МЕША — прибор критерия К4 ТЗ материалов («доля
  рёбер с фаской = 100 %»). Один алгоритм: свести вершины по месту, найти
  рёбра с ровно двумя гранями, отделить выпуклые от невыпуклых и измерить
  излом.

Key items:
- house_edge_census().

Dependencies:
- Uses: HouseMesh.h, glm.
- Used by: рукав test_house_bevel, приёмка dfn_bevel_check, отчёт bevel.html.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): здесь МЕРА, а не
  постройка. Соблазн дописать её в HouseMesh.cpp был; мера и построитель
  расходятся по времени жизни — построитель правят ради вида, меру правят
  ради приёмки, и общий файл склеил бы два повода к правке в один.
- ПРИБОР МЕРИТ МЕШ, А НЕ НАМЕРЕНИЕ. Он не знает ни про bevel_m, ни про то,
  звали ли фаску: он спрашивает у треугольников, ловит ли ребро свет
  отдельной гранью. Прибор, читающий свою же дозу, всегда зелёный
  (правило 47).
*/
/*
UPD:
- 28:08:2026 - 14:05:00: Создан вместе с фаской (волна материалов-1).
*/

#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

/// ШАГ СВЕДЕНИЯ ВЕРШИН, метры. Десятая миллиметра: мельче любой настоящей
/// щели набора (наименьшая — зазор досок обшивки 12 мм) и крупнее ошибки
/// накопления float на координатах постройки в пределах сотни метров.
constexpr float WELD_M = 1e-4f;

[[nodiscard]] std::uint64_t weld_key(glm::vec3 p) {
    const auto q = [](float v) {
        return static_cast<std::int64_t>(std::llround(static_cast<double>(v) / WELD_M));
    };
    // Три координаты в 64 бита по 21 биту: ±1 048 575 шагов по 0.1 мм — это
    // ±104 метра, больше всякой постройки набора. Коллизия ключа означала бы
    // сведённые в одну РАЗНЫЕ вершины, поэтому разрядность названа числом, а
    // не подобрана.
    const std::uint64_t x = static_cast<std::uint64_t>(q(p.x) + (1 << 20)) & 0x1FFFFF;
    const std::uint64_t y = static_cast<std::uint64_t>(q(p.y) + (1 << 20)) & 0x1FFFFF;
    const std::uint64_t z = static_cast<std::uint64_t>(q(p.z) + (1 << 20)) & 0x1FFFFF;
    return (x << 42) | (y << 21) | z;
}

struct EdgeUse {
    int faces = 0;
    glm::vec3 normal[2]{glm::vec3{0.0f}, glm::vec3{0.0f}};
    glm::vec3 apex[2]{glm::vec3{0.0f}, glm::vec3{0.0f}}; ///< третья вершина грани
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
};

} // namespace

HouseEdgeCensus house_edge_census(const HouseMesh& mesh, float max_turn_deg) {
    HouseEdgeCensus out;
    std::unordered_map<std::uint64_t, EdgeUse> edges;
    edges.reserve(mesh.indices.size());

    const auto edge_key = [](std::uint64_t p, std::uint64_t q) {
        // Ключ ребра — пара ключей вершин, ПОРЯДОК СНЯТ: одно и то же ребро
        // обходится соседними гранями в разные стороны.
        const std::uint64_t lo = std::min(p, q);
        const std::uint64_t hi = std::max(p, q);
        return lo * 1000003ull ^ (hi + 0x9E3779B97F4A7C15ull + (lo << 6) + (lo >> 2));
    };

    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const glm::vec3 p[3] = {mesh.vertices[mesh.indices[t]].pos,
                                mesh.vertices[mesh.indices[t + 1]].pos,
                                mesh.vertices[mesh.indices[t + 2]].pos};
        const glm::vec3 raw = glm::cross(p[1] - p[0], p[2] - p[0]);
        const float area2 = glm::length(raw);
        if (area2 < 1e-9f) {
            continue;
        }
        const glm::vec3 fn = raw / area2;
        const std::uint64_t k[3] = {weld_key(p[0]), weld_key(p[1]), weld_key(p[2])};
        for (int i = 0; i < 3; ++i) {
            const int j = (i + 1) % 3;
            const int o = (i + 2) % 3;
            if (k[i] == k[j]) {
                continue;
            }
            EdgeUse& use = edges[edge_key(k[i], k[j])];
            if (use.faces < 2) {
                use.normal[use.faces] = fn;
                use.apex[use.faces] = p[o];
                use.a = p[i];
                use.b = p[j];
            }
            ++use.faces;
        }
    }

    const float cos_limit = std::cos(max_turn_deg * 3.14159265358979323846f / 180.0f);
    for (const auto& [key, use] : edges) {
        (void)key;
        if (use.faces != 2) {
            continue; // шов двух вставленных друг в друга тел, не ребро
        }
        const float dot_n = glm::clamp(glm::dot(use.normal[0], use.normal[1]), -1.0f, 1.0f);
        if (dot_n > 0.99619f) {
            continue; // грани сходятся ровнее 5° — плоский стык, ребра нет
        }
        // ВЫПУКЛОСТЬ РЕШАЕТСЯ ЧУЖОЙ ВЕРШИНОЙ, а не знаком векторного
        // произведения: у ребра нет собственной стороны, а у пары граней есть.
        // Если вершина второй грани лежит ПОЗАДИ плоскости первой, тело в этом
        // месте выпукло.
        const float behind = glm::dot(use.normal[0], use.apex[1] - use.apex[0]);
        if (behind > -1e-6f) {
            continue; // невыпуклое ребро: внутренний угол
        }
        const float len = glm::length(use.b - use.a);
        const float deg = std::acos(dot_n) * 180.0f / 3.14159265358979323846f;
        ++out.convex;
        out.convex_len_m += len;
        out.worst_deg = std::max(out.worst_deg, deg);
        if (dot_n >= cos_limit) {
            ++out.bevelled;
        } else {
            ++out.sharp;
            out.sharp_len_m += len;
        }
    }
    return out;
}

} // namespace dfn::world
