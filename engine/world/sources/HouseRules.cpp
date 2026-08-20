/*
Created: 21:08:2026 - 00:40:00
Last updated: 21:08:2026 - 00:40:00
Module: engine/world
File: engine/world/sources/HouseRules.cpp

Responsibility:
- ПРАВИЛА-СУДЬИ ГРАФА: опора крыш (вершина кровли без стены под ней —
  находка; unsupported=1 выводит руину из-под правила).

Key items:
- check_roof_support.

Dependencies:
- Uses: HouseMeshDetail.h (element_params_of через HouseMesh.h)
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/
/*
UPD:
- 21:08:2026 - 00:40:00: Вырезан из HouseMesh.cpp (1942 строки, девять алгоритмов в одном файле).
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include <string>

namespace dfn::world {

namespace {


} // namespace

std::vector<MeshFinding> check_roof_support(const HouseGraph& g) {
    std::vector<MeshFinding> out;
    // Вершины всех НЕ-кровельных элементов — потенциальные опоры.
    std::vector<glm::vec3> supports;
    std::vector<const Element*> roofs;
    for (const Element& e : g.elements()) {
        const ElementParams p = element_params_of(e, nullptr);
        const WallFill fill = fill_kind(p);
        const bool is_roof =
            p.roof > 0.5f || fill == WallFill::Shingle || fill == WallFill::Tile;
        if (is_roof) {
            if (p.unsupported < 0.5f) {
                roofs.push_back(&e);
            }
            continue;
        }
        for (const VertexId r : e.refs) {
            const glm::vec3 base = g.resolved_local(r);
            supports.push_back(base);
            // Стена держит крышу ВЕРХНЕЙ кромкой: цепочка с высотой поднимает
            // свои опорные точки на height (низовой якорь стоит на земле).
            if (e.kind == ElementKind::Surface && !e.closed && p.height > 0.0f) {
                supports.push_back(base + glm::vec3{0.0f, p.height, 0.0f});
            }
            // Столб держит конёк верхним концом — Line из двух вершин обе
            // точки уже дал; поднимать нечего.
        }
    }
    for (const Element* e : roofs) {
        for (const VertexId r : e->refs) {
            const glm::vec3 rp = g.resolved_local(r);
            bool held = false;
            for (const glm::vec3& sp : supports) {
                const float dx = sp.x - rp.x;
                const float dz = sp.z - rp.z;
                const float dy = rp.y - sp.y; // опора НИЖЕ вершины крыши
                if (dx * dx + dz * dz <= 1.0f && dy >= -0.6f && dy <= 1.8f) {
                    held = true;
                    break;
                }
            }
            if (!held) {
                out.push_back({e->id, MeshIssue::RoofUnsupported,
                               rp.y,
                               "вершина крыши висит без стены под ней (v"
                                   + std::to_string(r)
                                   + "); руина скажет unsupported=1"});
            }
        }
    }
    return out;
}


} // namespace dfn::world
