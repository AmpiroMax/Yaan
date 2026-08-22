/*
Created: 21:08:2026 - 00:40:00
Last updated: 23:08:2026 - 05:20:00
Module: engine/world
File: engine/world/sources/HouseBodies.cpp

Responsibility:
- ТЕЛА ВОКРУГ ОСИ: кольцо профиля, призма с крышками и выпуклым куском в
  коллайдер, прямая (брус/бревно/доска, лестница-наследие).

Key items:
- profile_ring / push_prism / build_line.

Dependencies:
- Uses: HouseMeshDetail.h, HouseStairs (лестница-прямая)
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/
/*
UPD:
- 21:08:2026 - 00:40:00: Вырезан из HouseMesh.cpp (1942 строки, девять алгоритмов в одном файле).
- 21:08:2026 - 01:50:00: push_prism уважает MeshBuilder.collider.
- 23:08:2026 - 05:20:00: рамки uv крышки/днища сдвигаются на uv_shift — перенос начала рамки
  по её осям сдвигает фазу выреза плитки.
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include <cmath>

namespace dfn::world {

namespace {


} // namespace

/// Кольцо профиля вокруг оси. Точки идут против часовой стрелки в базисе (u,v).
std::vector<glm::vec3> profile_ring(glm::vec3 center, glm::vec3 axis_u, glm::vec3 axis_v,
                                    float face_radius, int sides) {
    std::vector<glm::vec3> ring;
    ring.reserve(static_cast<std::size_t>(sides));
    // radius — расстояние до ГРАНИ, значит до УГЛА дальше в 1/cos(pi/N) раз.
    // Одна формула на все формы: у круга это ровно вписанный радиус, у
    // квадрата — половина толщины бруса. Так «radius=0.12» значит «24 см
    // толщиной» независимо от формы, а не «то ли 24, то ли 17».
    const float corner = face_radius / std::cos(HOUSE_PI_F / static_cast<float>(sides));
    for (int k = 0; k < sides; ++k) {
        const float a = 2.0f * HOUSE_PI_F * (static_cast<float>(k) + 0.5f) / static_cast<float>(sides);
        ring.push_back(center + (axis_u * std::cos(a) + axis_v * std::sin(a)) * corner);
    }
    return ring;
}

/// Призма: нижнее кольцо loop (обходимое против часовой стрелки, если смотреть
/// со стороны +axis), верхнее — loop + axis. Треугольники кольца уже посчитаны
/// вызывающим и переиспользуются для крышки, днища и коллайдера.
void push_prism(MeshBuilder& mb, std::span<const glm::vec3> loop,
                std::span<const std::uint32_t> tris, glm::vec3 axis, float tex_deg,
                HouseMesh& mesh, ElementId owner, glm::vec2 uv_shift) {
    const std::size_t count = loop.size();
    if (count < 3 || tris.size() < 3) {
        return;
    }
    const float axis_len = glm::length(axis);
    if (axis_len < HOUSE_GEOM_EPS) {
        return;
    }
    const glm::vec3 n = axis / axis_len;
    glm::vec3 edge_hint = loop[1] - loop[0];
    if (glm::length(edge_hint) < HOUSE_GEOM_EPS) {
        edge_hint = stable_reference_axis(n);
    }

    // ПОВОРОТ ТЕКСТУРЫ — ВОКРУГ СРЕДНЕЙ ТОЧКИ ГРАНИ (заказ 19.08: «текстуру
    // крутить надо не вокруг какой-то точки из якорей, а вокруг средней»).
    // Раньше рамка стояла в loop[0] — первом якоре: узор при повороте уезжал
    // вбок, потому что вращался вокруг угла, а не вокруг себя.
    glm::vec3 centre{0.0f};
    for (std::size_t i = 0; i < count; ++i) {
        centre += loop[i];
    }
    centre = centre / static_cast<float>(count) + axis * 0.5f;
    // Крышка (лицо, +n) и днище (-n). Днище — те же треугольники наоборот.
    // Сдвиг фазы: перенос НАЧАЛА рамки на -shift по её же осям сдвигает uv
    // каждой точки на +shift — плитка та же, вырез из неё другой.
    const auto shifted = [&uv_shift](UvFrame f) {
        f.origin -= f.u * uv_shift.x + f.v * uv_shift.y;
        return f;
    };
    const UvFrame top_uv = shifted(make_uv_frame(centre, n, edge_hint, tex_deg));
    const UvFrame bottom_uv = shifted(make_uv_frame(centre, -n, edge_hint, tex_deg));
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        mb.push_triangle(loop[tris[t]] + axis, loop[tris[t + 1]] + axis, loop[tris[t + 2]] + axis,
                         top_uv);
        mb.push_triangle(loop[tris[t + 2]], loop[tris[t + 1]], loop[tris[t]], bottom_uv);
    }
    // Рант. Каждое ребро контура даёт одну грань, наружу.
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3& a = loop[i];
        const glm::vec3& b = loop[(i + 1) % count];
        const glm::vec3 dir = b - a;
        if (glm::length(dir) < HOUSE_GEOM_EPS) {
            continue;
        }
        const glm::vec3 face_n = glm::normalize(glm::cross(dir, n));
        // Середина ГРАНИ, той же причиной, что у крышки.
        const UvFrame side_uv =
            make_uv_frame((a + b) * 0.5f + axis * 0.5f, face_n, dir, tex_deg);
        mb.push_quad(a, b, b + axis, a + axis, side_uv);
    }
    // Коллайдер: одна выпуклая призма на треугольник. Разбор идёт по ТЕМ ЖЕ
    // треугольникам, что и меш, поэтому Г-образная комната получает физику по
    // своей форме, а не по своей выпуклой оболочке. Отдельный алгоритм был бы
    // второй копией правды (правило 39) и разъехался бы с мешем в тот день,
    // когда кто-нибудь поправит триангуляцию.
    if (!mb.collider) {
        return; // косметический слой: картинка без физического тела
    }
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        ConvexPart part;
        part.element = owner;
        part.points = {loop[tris[t]],        loop[tris[t + 1]],        loop[tris[t + 2]],
                       loop[tris[t]] + axis, loop[tris[t + 1]] + axis, loop[tris[t + 2]] + axis};
        mesh.convex.push_back(std::move(part));
    }
}

void build_line(const HouseGraph& g, const Element& e, const ElementParams& p, MeshBuilder& mb,
                HouseMesh& mesh) {
    const glm::vec3 a = g.resolved_local(e.refs.front());
    glm::vec3 b{0.0f};
    if (p.stairs > 0.5f && e.refs.size() >= 2) {
        build_stairs(e, p, a, g.resolved_local(e.refs[1]), std::max(p.radius, 0.15f), mb,
                     mesh);
        return;
    }
    if (e.refs.size() >= 2) {
        if (e.refs.size() > 2) {
            mesh.findings.push_back({e.id, MeshIssue::LineExtraRefs, 0.0f,
                                     "у прямой больше двух вершин: взяты первые две"});
        }
        b = g.resolved_local(e.refs[1]);
    } else {
        if (p.length <= HOUSE_GEOM_EPS) {
            mesh.findings.push_back({e.id, MeshIssue::LineNeedsLength, 0.0f,
                                     "прямая на одной вершине без length"});
            return;
        }
        // Направление: angle_x — отклонение от вертикали, angle_y — куда оно
        // направлено. Стойка по умолчанию СТОИТ (нулевые углы дают +Y), потому
        // что это самый частый случай, а не потому что +Y красивее.
        const float ax = p.angle_x * HOUSE_DEG2RAD;
        const float ay = p.angle_y * HOUSE_DEG2RAD;
        const glm::vec3 dir{std::sin(ay) * std::sin(ax), std::cos(ax), std::cos(ay) * std::sin(ax)};
        b = a + dir * p.length;
    }
    const glm::vec3 axis = b - a;
    const float len = glm::length(axis);
    if (len < HOUSE_GEOM_EPS) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate, len, "у прямой нулевая длина"});
        return;
    }
    const glm::vec3 w = axis / len;
    const glm::vec3 ref = stable_reference_axis(w);
    const glm::vec3 u0 = glm::normalize(glm::cross(ref, w));
    const glm::vec3 v0 = glm::cross(w, u0);
    // angle_z ВРАЩАЕТ ПРОФИЛЬ вокруг собственной оси. Для круглого бревна это
    // не меняет ничего, для квадратного бруса — меняет всё, и потому углов три,
    // а не два: направление стоит двух чисел, крен профиля — третьего.
    const float rz = p.angle_z * HOUSE_DEG2RAD;
    const glm::vec3 u = u0 * std::cos(rz) + v0 * std::sin(rz);
    const glm::vec3 v = -u0 * std::sin(rz) + v0 * std::cos(rz);

    const int sides = profile_sides(p);
    const float radius = std::max(p.radius, HOUSE_GEOM_EPS);
    // ДОСКА — прямоугольник 4:1 (radius — половина ШИРИНЫ): плоская обшивочная
    // палка, которую квадратным брусом не изобразить, не соврав в толщине.
    const std::vector<glm::vec3> ring =
        p.form == LineForm::Plank
            ? std::vector<glm::vec3>{a - u * radius - v * (radius * 0.25f),
                                     a + u * radius - v * (radius * 0.25f),
                                     a + u * radius + v * (radius * 0.25f),
                                     a - u * radius + v * (radius * 0.25f)}
            : profile_ring(a, u, v, radius, sides);
    std::vector<std::uint32_t> tris;
    tris.reserve(static_cast<std::size_t>(sides - 2) * 3);
    for (int k = 1; k + 1 < sides; ++k) {
        tris.push_back(0);
        tris.push_back(static_cast<std::uint32_t>(k));
        tris.push_back(static_cast<std::uint32_t>(k + 1));
    }
    // Кольцо обходится против часовой стрелки в базисе (u,v), а u x v == w,
    // значит веер смотрит по оси — ровно то, чего ждёт push_prism.
    push_prism(mb, ring, tris, axis, p.tex_deg, mesh, e.id);
}


} // namespace dfn::world
