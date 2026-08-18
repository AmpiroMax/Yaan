/*
Created: 18:08:2026 - 17:29:01
Last updated: 18:08:2026 - 17:29:01
Module: tests
File: tests/core/HouseMeshTests.cpp

Responsibility:
- ГЕОМЕТРИЯ ПОСТРОЙКИ ИЗ ГРАФА. Держит то, ради чего геометрия отделена от
  модели: тела считаются БЕЗ рендерера (правило 3), крепление идёт к осям и
  точкам, а кривой контур отличается от наклонного ЧИСЛОМ, а не на глаз.

Key items:
- Правило важнее всех: смена радиуса столба не двигает стену НИ НА МИКРОН.
- Отсечение ушей на Г-образной комнате против веера — с контрольной рукой.
- Мера неплоскости: таблица принятых и отвергнутых образцов вокруг порога.
- Поворот текстуры вокруг нормали против поворота вокруг мировой вертикали —
  обе руки в одном бинарнике (правило 47).

Dependencies:
- Uses: doctest, dfn_world.
- Used by: ctest (test_house_mesh).

AI Agents Notice (must follow):
- Правило 30: у каждого утверждения тут есть рука, которая обязана краснеть.
  Где утверждение про механизм — рядом стоит контрфакт, считающий ЧИСЛО, а не
  говорящий «стало лучше».
*/
/*
UPD:
- 18:08:2026 - 17:29:01: Создан вместе с HouseMesh.
*/

#include <doctest/doctest.h>

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include <map>
#include <utility>
#include <vector>

using dfn::world::Anchoring;
using dfn::world::ConvexPart;
using dfn::world::ElementId;
using dfn::world::ElementKind;
using dfn::world::ElementParams;
using dfn::world::FittedPlane;
using dfn::world::HouseGraph;
using dfn::world::HouseMesh;
using dfn::world::LineForm;
using dfn::world::MeshIssue;
using dfn::world::MeshPart;
using dfn::world::VertexId;

namespace {

constexpr float FLATNESS_MAX = dfn::world::HOUSE_CONTOUR_FLATNESS_MAX;

/// Объём замкнутого тела по теореме о дивергенции. Прибор двойного назначения:
/// величина проверяет размеры, а ЗНАК проверяет, что нормали смотрят наружу.
/// Вывернутая наизнанку оболочка даёт отрицательный объём и краснеет тут же, а
/// не через неделю на кадре.
float part_volume(const HouseMesh& m, const MeshPart& p) {
    double acc = 0.0;
    for (std::uint32_t i = 0; i < p.index_count; i += 3) {
        const glm::vec3 a = m.vertices[m.indices[p.index_begin + i]].pos;
        const glm::vec3 b = m.vertices[m.indices[p.index_begin + i + 1]].pos;
        const glm::vec3 c = m.vertices[m.indices[p.index_begin + i + 2]].pos;
        acc += static_cast<double>(glm::dot(a, glm::cross(b, c))) / 6.0;
    }
    return static_cast<float>(acc);
}

std::int64_t quant(float v) { return static_cast<std::int64_t>(std::llround(v * 10000.0f)); }

using PointKey = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
PointKey key_of(glm::vec3 p) { return {quant(p.x), quant(p.y), quant(p.z)}; }

/// ЗАМКНУТО ЛИ ТЕЛО (правило 52). Каждое направленное ребро обязано встретиться
/// ровно один раз, и у каждого обязан быть обратный близнец. Это ровно
/// определение замкнутой ориентируемой поверхности, и оно ловит и дыру, и
/// склеенную не той стороной грань.
bool part_watertight(const HouseMesh& m, const MeshPart& p) {
    std::map<std::pair<PointKey, PointKey>, int> edges;
    for (std::uint32_t i = 0; i < p.index_count; i += 3) {
        const PointKey k[3] = {key_of(m.vertices[m.indices[p.index_begin + i]].pos),
                               key_of(m.vertices[m.indices[p.index_begin + i + 1]].pos),
                               key_of(m.vertices[m.indices[p.index_begin + i + 2]].pos)};
        for (int e = 0; e < 3; ++e) {
            edges[{k[e], k[(e + 1) % 3]}] += 1;
        }
    }
    for (const auto& [edge, count] : edges) {
        if (count != 1) {
            return false;
        }
        const auto twin = edges.find({edge.second, edge.first});
        if (twin == edges.end() || twin->second != 1) {
            return false;
        }
    }
    return true;
}

std::vector<glm::vec3> part_positions(const HouseMesh& m, const MeshPart& p) {
    std::vector<glm::vec3> out;
    for (std::uint32_t i = 0; i < p.index_count; ++i) {
        out.push_back(m.vertices[m.indices[p.index_begin + i]].pos);
    }
    return out;
}

float flatness_of(std::vector<glm::vec3> pts) { return dfn::world::fit_contour_plane(pts).flatness; }

/// Веер из вершины start — ТА САМАЯ наивная триангуляция, ради отличия от
/// которой отсечение ушей и написано.
std::vector<std::uint32_t> fan_triangulate(std::size_t count, std::size_t start) {
    std::vector<std::uint32_t> tris;
    for (std::size_t k = 1; k + 1 < count; ++k) {
        tris.push_back(static_cast<std::uint32_t>(start));
        tris.push_back(static_cast<std::uint32_t>((start + k) % count));
        tris.push_back(static_cast<std::uint32_t>((start + k + 1) % count));
    }
    return tris;
}

bool point_in_polygon(glm::vec2 p, const std::vector<glm::vec2>& poly) {
    bool inside = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const bool straddles = (poly[i].y > p.y) != (poly[j].y > p.y);
        if (!straddles) {
            continue;
        }
        const float x = poly[i].x + (p.y - poly[i].y) * (poly[j].x - poly[i].x) /
                                        (poly[j].y - poly[i].y);
        if (p.x < x) {
            inside = !inside;
        }
    }
    return inside;
}

int triangles_outside(const std::vector<glm::vec2>& poly, const std::vector<std::uint32_t>& tris) {
    int bad = 0;
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        const glm::vec2 c = (poly[tris[t]] + poly[tris[t + 1]] + poly[tris[t + 2]]) / 3.0f;
        if (!point_in_polygon(c, poly)) {
            ++bad;
        }
    }
    return bad;
}

float abs_area_sum(const std::vector<glm::vec2>& poly, const std::vector<std::uint32_t>& tris) {
    float acc = 0.0f;
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        const glm::vec2 a = poly[tris[t]];
        const glm::vec2 b = poly[tris[t + 1]];
        const glm::vec2 c = poly[tris[t + 2]];
        acc += std::fabs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
    }
    return acc;
}

/// Столб и стена, упирающаяся в его ОСЬ. Стиль столба — снаружи, потому что вся
/// затея случая в том, чтобы поменять ОДИН радиус и посмотреть, что не поехало.
HouseGraph post_and_wall(const char* post_style) {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId top = g.add_vertex(Anchoring::Free, {0.0f, 3.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    ElementId post = 0;
    ElementId wall = 0;
    g.add_element(ElementKind::Line, {a, top}, post_style, post);
    g.add_element(ElementKind::Surface, {a, b}, "plank;height=2.6;thickness=0.2", wall);
    return g;
}

/// Г-образная комната — пример пользователя. Площадь 19.5 м2, вогнутая вершина
/// одна. Стороны НАРОЧНО неравные: у симметричной буквы Г центры тяжести
/// веерных треугольников садятся ровно на границу, и «внутри или снаружи»
/// становится вопросом округления, то есть прибор перестаёт мерить предмет.
const std::vector<glm::vec2>& l_room() {
    static const std::vector<glm::vec2> poly = {{0.0f, 0.0f}, {6.0f, 0.0f}, {6.0f, 2.0f},
                                                {2.5f, 2.0f}, {2.5f, 5.0f}, {0.0f, 5.0f}};
    return poly;
}

} // namespace

TEST_CASE("прямая: тело замкнуто, а radius меряется до ГРАНИ, не до угла") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 3.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak;form=round;radius=0.12", post).ok);

    const HouseMesh m = dfn::world::build_house_mesh(g);
    REQUIRE(m.findings.empty());
    const MeshPart* p = m.part_of(post);
    REQUIRE(p != nullptr);

    // Восьмигранник: 8 боковых четырёхугольников (16 треугольников) и два
    // торца-веера по 6. Торцы обязательны — правило 52: у предмета мира нет
    // плоских частей и нет открытых концов.
    CHECK(p->index_count / 3 == 28);
    CHECK(part_watertight(m, *p));

    // Объём правильного восьмигранника с АПОФЕМОЙ 0.12: n*a^2*tan(pi/n) * длину.
    const float apothem = 0.12f;
    const float face_area = 8.0f * apothem * apothem * std::tan(3.14159265f / 8.0f);
    CHECK(part_volume(m, *p) == doctest::Approx(face_area * 3.0f).epsilon(0.002));

    // Все вершины кольца сидят на ОПИСАННОМ радиусе 0.12/cos(22.5) = 0.1299:
    // расстояние до угла больше расстояния до грани ровно в 1/cos(pi/n).
    const float corner = 0.12f / std::cos(3.14159265f / 8.0f);
    float max_r = 0.0f;
    float min_r = 1e9f;
    for (const glm::vec3& q : part_positions(m, *p)) {
        const float r = std::sqrt(q.x * q.x + q.z * q.z);
        max_r = std::max(max_r, r);
        min_r = std::min(min_r, r);
    }
    CHECK(max_r == doctest::Approx(corner).epsilon(0.001));
    CHECK(min_r == doctest::Approx(corner).epsilon(0.001));
}

TEST_CASE("квадратный брус: radius=0.12 значит 24 см толщиной, а не 17") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 2.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak;form=square;radius=0.12", post).ok);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    const MeshPart* p = m.part_of(post);
    REQUIRE(p != nullptr);
    CHECK(p->index_count / 3 == 12); // 4 боковых четырёхугольника + 2 торца по 2

    // 0.24 x 0.24 x 2.0 = 0.1152. КОНТРФАКТ ПРЯМО ЗДЕСЬ: если бы radius значил
    // полудиагональ, сечение было бы 0.17 x 0.17 и объём 0.0576 — ровно вдвое
    // меньше. Два прочтения одного слова расходятся в ДВА РАЗА, поэтому слово
    // определено в заголовке, а не оставлено на догадку.
    CHECK(part_volume(m, *p) == doctest::Approx(0.24f * 0.24f * 2.0f).epsilon(0.001));
    CHECK(part_volume(m, *p) != doctest::Approx(0.17f * 0.17f * 2.0f).epsilon(0.02));
    CHECK(part_watertight(m, *p));
}

TEST_CASE("ГЛАВНОЕ ПРАВИЛО: смена радиуса столба не двигает стену ни на микрон") {
    // Решение пользователя 18.08 (§7.0 замысла): всё крепится к ОСЯМ и ТОЧКАМ,
    // никогда к поверхностям; нахлёст тел — ожидаемое состояние. Прямое
    // следствие, ради которого правило и принято: «радиус менять должно быть
    // можно», и это не должно стоить пересчёта стен.
    const HouseGraph thin = post_and_wall("oak;form=round;radius=0.15");
    const HouseGraph fat = post_and_wall("oak;form=round;radius=0.45");
    const HouseMesh a = dfn::world::build_house_mesh(thin);
    const HouseMesh b = dfn::world::build_house_mesh(fat);

    const MeshPart* wall_a = a.part_of(2);
    const MeshPart* wall_b = b.part_of(2);
    REQUIRE(wall_a != nullptr);
    REQUIRE(wall_b != nullptr);
    const std::vector<glm::vec3> pa = part_positions(a, *wall_a);
    const std::vector<glm::vec3> pb = part_positions(b, *wall_b);
    REQUIRE(pa.size() == pb.size());
    float worst = 0.0f;
    for (std::size_t i = 0; i < pa.size(); ++i) {
        worst = std::max(worst, glm::length(pa[i] - pb[i]));
    }
    // НИ НА МИКРОН — это буквально: сравнение точное, а не «в пределах допуска».
    CHECK(worst == 0.0f);

    // КОНТРОЛЬ (правило 30а): рука обязана уметь показать РАЗНИЦУ, иначе она
    // доказывает только то, что сравнивает две копии одного массива. Столб
    // изменился, и сильно.
    const MeshPart* post_a = a.part_of(1);
    const MeshPart* post_b = b.part_of(1);
    REQUIRE(post_a != nullptr);
    REQUIRE(post_b != nullptr);
    CHECK(part_volume(b, *post_b) > part_volume(a, *post_a) * 8.0f);

    // И НАХЛЁСТ — НОРМА, а не дефект. Торец стены стоит РОВНО НА ОСИ столба:
    // ближайшая к оси точка стены лежит в плоскости x == 0, то есть в той самой
    // плоскости, которой ось принадлежит, а её удаление от оси — это ровно
    // половина толщины стены, 0.10 м. И 0.10 < 0.45, значит весь торец сидит
    // ВНУТРИ тела столба. Ни один радиус нигде не вычитался, чтобы этого
    // избежать, — по решению пользователя избегать нечего.
    float nearest_x = 1e9f;
    float min_axis_dist = 1e9f;
    for (const glm::vec3& q : pb) {
        nearest_x = std::min(nearest_x, std::fabs(q.x));
        min_axis_dist = std::min(min_axis_dist, std::sqrt(q.x * q.x + q.z * q.z));
    }
    CHECK(nearest_x == doctest::Approx(0.0f).epsilon(0.0001));
    CHECK(min_axis_dist == doctest::Approx(0.10f).epsilon(0.001));
    CHECK(min_axis_dist < 0.45f);
}

TEST_CASE("Г-образная комната: отсечение ушей кроет выемку, веер закрашивает её") {
    const std::vector<glm::vec2>& poly = l_room();
    const std::vector<std::uint32_t> ears = dfn::world::triangulate_contour(poly);
    REQUIRE(ears.size() == 12); // шесть вершин дают четыре треугольника

    // ПРЕДМЕТ: ни один треугольник не лежит вне комнаты, и покрытая площадь
    // равна площади комнаты.
    CHECK(triangles_outside(poly, ears) == 0);
    CHECK(abs_area_sum(poly, ears) == doctest::Approx(19.5f).epsilon(0.0001));

    // КОНТРОЛЬНАЯ РУКА В ТОМ ЖЕ БИНАРНИКЕ (правило 47): веер — это то, что
    // получилось бы без проверки «ухо ПУСТОЕ». Он кроет 30.0 м2 вместо 19.5,
    // то есть 10.5 м2 пола нарисовано в вырезе, где пола нет.
    const std::vector<std::uint32_t> fan = fan_triangulate(poly.size(), 2);
    CHECK(triangles_outside(poly, fan) == 2);
    CHECK(abs_area_sum(poly, fan) == doctest::Approx(30.0f).epsilon(0.0001));

    // И У КОНТРФАКТА ЕСТЬ ЧЕСТНАЯ ОГОВОРКА: веер врёт не из ЛЮБОЙ вершины.
    // Из вершин 0 и 3 он случайно даёт верный ответ, и это ровно то, чем
    // опасна проверка «работает на моём примере»: четыре старта из шести
    // краснеют, два зеленеют, а алгоритм один и тот же неверный.
    int wrong_starts = 0;
    for (std::size_t start = 0; start < poly.size(); ++start) {
        if (triangles_outside(poly, fan_triangulate(poly.size(), start)) > 0) {
            ++wrong_starts;
        }
    }
    CHECK(wrong_starts == 4);
}

TEST_CASE("Г-образный пол: коллайдер разбирается по ТЕМ ЖЕ треугольникам") {
    HouseGraph g;
    std::vector<VertexId> refs;
    for (const glm::vec2& q : l_room()) {
        refs.push_back(g.add_vertex(Anchoring::OnGround, {q.x, 0.0f, q.y}));
    }
    ElementId floor = 0;
    REQUIRE(g.add_element(ElementKind::Surface, refs, "oak;thickness=0.25", floor).ok);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    REQUIRE(m.findings.empty());

    // Четыре уха — четыре выпуклые призмы по шесть точек. Не выпуклая оболочка:
    // она накрыла бы вырез и человек упирался бы в воздух.
    CHECK(m.convex.size() == 4);
    float collider_volume = 0.0f;
    for (const ConvexPart& c : m.convex) {
        REQUIRE(c.points.size() == 6);
        CHECK(c.element == floor);
        const glm::vec3 e1 = c.points[1] - c.points[0];
        const glm::vec3 e2 = c.points[2] - c.points[0];
        const glm::vec3 h = c.points[3] - c.points[0];
        collider_volume += std::fabs(glm::dot(glm::cross(e1, e2), h)) * 0.5f;
    }
    CHECK(collider_volume == doctest::Approx(19.5f * 0.25f).epsilon(0.001));

    const MeshPart* p = m.part_of(floor);
    REQUIRE(p != nullptr);
    CHECK(part_watertight(m, *p));
    CHECK(part_volume(m, *p) == doctest::Approx(19.5f * 0.25f).epsilon(0.001));
}

TEST_CASE("нормаль берётся из ПОРЯДКА ОБХОДА, а facing её разворачивает") {
    // Пол по часовой стрелке, если смотреть сверху, обязан смотреть ВНИЗ, а
    // обратный обход — ВВЕРХ. Это §2.3: порядок даёт верный ответ почти всегда,
    // переключатель спасает тот случай, когда обошли не в ту сторону.
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId c = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
    const VertexId d = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 3.0f});
    ElementId one = 0;
    ElementId two = 0;
    REQUIRE(g.add_element(ElementKind::Surface, {a, b, c, d}, "oak", one).ok);
    REQUIRE(g.add_element(ElementKind::Surface, {d, c, b, a}, "oak", two).ok);

    glm::vec3 n1{0.0f};
    glm::vec3 n2{0.0f};
    REQUIRE(dfn::world::surface_normal(g, one, n1));
    REQUIRE(dfn::world::surface_normal(g, two, n2));
    CHECK(n1.y == doctest::Approx(-1.0f).epsilon(0.001));
    CHECK(n2.y == doctest::Approx(1.0f).epsilon(0.001));

    REQUIRE(g.set_facing(one, true).ok);
    glm::vec3 n1f{0.0f};
    REQUIRE(dfn::world::surface_normal(g, one, n1f));
    CHECK(n1f.y == doctest::Approx(1.0f).epsilon(0.001));

    // И РАЗВОРОТ ЛИЦА НЕ ДВИГАЕТ ГЕОМЕТРИЮ: толщина растёт симметрично от
    // срединной плоскости, как тело прямой сидит вокруг оси. Иначе «повернуть
    // текстуру» тихо превращалось бы в «подвинуть пол на 10 см».
    HouseGraph plain;
    const VertexId pa = plain.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId pb = plain.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId pc = plain.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
    const VertexId pd = plain.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 3.0f});
    ElementId flat = 0;
    REQUIRE(plain.add_element(ElementKind::Surface, {pa, pb, pc, pd}, "oak", flat).ok);
    const HouseMesh before = dfn::world::build_house_mesh(plain);
    REQUIRE(plain.set_facing(flat, true).ok);
    const HouseMesh after = dfn::world::build_house_mesh(plain);
    const MeshPart* pb1 = before.part_of(flat);
    const MeshPart* pb2 = after.part_of(flat);
    REQUIRE(pb1 != nullptr);
    REQUIRE(pb2 != nullptr);
    CHECK(part_volume(before, *pb1) == doctest::Approx(part_volume(after, *pb2)).epsilon(0.0001));
    std::vector<glm::vec3> sa = part_positions(before, *pb1);
    std::vector<glm::vec3> sb = part_positions(after, *pb2);
    const auto by_xyz = [](glm::vec3 l, glm::vec3 r) {
        return std::tie(l.x, l.y, l.z) < std::tie(r.x, r.y, r.z);
    };
    std::sort(sa.begin(), sa.end(), by_xyz);
    std::sort(sb.begin(), sb.end(), by_xyz);
    REQUIRE(sa.size() == sb.size());
    float worst = 0.0f;
    for (std::size_t i = 0; i < sa.size(); ++i) {
        worst = std::max(worst, glm::length(sa[i] - sb[i]));
    }
    CHECK(worst == doctest::Approx(0.0f).epsilon(0.0001));
}

TEST_CASE("поворот текстуры ВОКРУГ НОРМАЛИ не тянет узор на наклонном полу") {
    // Наклон 30 градусов: скат крыши, он же пандус.
    const float rise = 3.0f * std::tan(30.0f * 3.14159265f / 180.0f);
    const auto slope = [rise](float tex_deg) {
        HouseGraph g;
        const VertexId a = g.add_vertex(Anchoring::Free, {0.0f, 0.0f, 0.0f});
        const VertexId b = g.add_vertex(Anchoring::Free, {4.0f, 0.0f, 0.0f});
        const VertexId c = g.add_vertex(Anchoring::Free, {4.0f, rise, 3.0f});
        const VertexId d = g.add_vertex(Anchoring::Free, {0.0f, rise, 3.0f});
        ElementId s = 0;
        const std::string style = "oak;thickness=0.1;tex_deg=" + std::to_string(tex_deg);
        g.add_element(ElementKind::Surface, {a, b, c, d}, style, s);
        return g;
    };

    // ПРЕДМЕТ: развёртка ИЗОМЕТРИЧНА — расстояние в текстуре равно расстоянию
    // по поверхности, при любом угле поворота. Ось u остаётся В ПЛОСКОСТИ
    // грани, потому что вращается вокруг её нормали.
    // ОБЕ РУКИ МЕРЯЮТ ОДНИ И ТЕ ЖЕ ТРЕУГОЛЬНИКИ — лицевые, те, чья нормаль
    // совпадает с нормалью ската. Иначе сравнение поехало бы на ранте, где
    // грани почти вертикальны и любая проекция врёт сильнее.
    const glm::vec3 face_dir = glm::normalize(glm::vec3{0.0f, 3.0f, -rise});
    const auto is_face = [&face_dir](glm::vec3 n) {
        return std::fabs(glm::dot(n, face_dir)) > 0.99f;
    };

    float worst_around_normal = 0.0f;
    for (const float deg : {0.0f, 30.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
        const HouseGraph g = slope(deg);
        const HouseMesh m = dfn::world::build_house_mesh(g);
        const MeshPart* p = m.part_of(1);
        REQUIRE(p != nullptr);
        for (std::uint32_t i = 0; i < p->index_count; i += 3) {
            if (!is_face(m.vertices[m.indices[p->index_begin + i]].normal)) {
                continue;
            }
            // ВСЕ ТРИ РЕБРА, а не первое: у половины треугольников первое ребро
            // идёт поперёк уклона, там любая проекция точна, и прибор, читающий
            // только его, честно отвечал бы НОЛЬ на обе руки сразу.
            for (int e = 0; e < 3; ++e) {
                const auto& v0 = m.vertices[m.indices[p->index_begin + i + e]];
                const auto& v1 = m.vertices[m.indices[p->index_begin + i + (e + 1) % 3]];
                const float world = glm::length(v1.pos - v0.pos);
                const float tex = glm::length(v1.uv - v0.uv);
                if (world > 0.01f) {
                    worst_around_normal =
                        std::max(worst_around_normal, std::fabs(tex - world) / world);
                }
            }
        }
    }
    CHECK(worst_around_normal < 0.001f);

    // КОНТРОЛЬНАЯ РУКА В ТОМ ЖЕ БИНАРНИКЕ (правило 47): «поверни вокруг мировой
    // вертикали и спроецируй на XZ» — самый напрашивающийся способ, и на
    // наклонной грани он тянет узор в 1/cos(наклон) раз. Считается ЗДЕСЬ, а не
    // словами, на тех же самых треугольниках.
    const HouseGraph g = slope(0.0f);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    const MeshPart* p = m.part_of(1);
    REQUIRE(p != nullptr);
    float worst_planar = 0.0f;
    for (std::uint32_t i = 0; i < p->index_count; i += 3) {
        if (!is_face(m.vertices[m.indices[p->index_begin + i]].normal)) {
            continue;
        }
        for (int e = 0; e < 3; ++e) {
            const glm::vec3 a = m.vertices[m.indices[p->index_begin + i + e]].pos;
            const glm::vec3 b = m.vertices[m.indices[p->index_begin + i + (e + 1) % 3]].pos;
            const float world = glm::length(b - a);
            const glm::vec2 ua{a.x, a.z};
            const glm::vec2 ub{b.x, b.z};
            const float tex = glm::length(ub - ua);
            if (world > 0.01f) {
                worst_planar = std::max(worst_planar, std::fabs(tex - world) / world);
            }
        }
    }
    // 1/cos(30) - 1 = 0.1547. Ошибка идёт В СЖАТИЕ (проекция короче ската),
    // то есть узор на скате крупнее, чем на полу рядом, ровно на 13.4%.
    CHECK(worst_planar == doctest::Approx(1.0f - std::cos(30.0f * 3.14159265f / 180.0f))
                              .epsilon(0.01));
    CHECK(worst_planar > 100.0f * worst_around_normal);
}

TEST_CASE("мера неплоскости различает наклонный пол и промах по якорю") {
    // ПРИНЯТЫЕ: все якоря те, что дизайнер хотел; кривизна пришла от земли, на
    // которой дом стоит. Пользователь разрешил её прямо: «пусть выкопанный
    // овраг под землёй будет нормой, провисшие дома тоже норм».
    const float ramp = flatness_of({{0, 0, 0}, {4, 0, 0}, {4, 1.7320508f, 3}, {0, 1.7320508f, 3}});
    const float ridge = flatness_of({{0, 3, 0}, {6, 3.05f, 0}, {6, 3, 4}, {0, 3, 4}});
    const float floor_86 = flatness_of({{0, 0.08f, 0}, {6, -0.08f, 0}, {6, 0.08f, 5}, {0, -0.08f, 5}});
    const float terrace = flatness_of({{0, 0.25f, 0}, {12, -0.25f, 0}, {12, 0.25f, 8}, {0, -0.25f, 8}});
    // ОТВЕРГНУТЫЕ: один якорь взят не тот. Подоконный вместо углового, потом
    // потолочный вместо углового, потом контур, сложенный пополам.
    const float sill_65 = flatness_of({{0, 0, 0}, {6, 0, 0}, {6, 0, 5}, {0, 0.9f, 5}});
    const float sill_33 = flatness_of({{0, 0, 0}, {3, 0, 0}, {3, 0, 3}, {0, 0.5f, 3}});
    const float ceil_86 = flatness_of({{0, 0, 0}, {8, 0, 0}, {8, 0, 6}, {0, 2.6f, 6}});
    const float ceil_43 = flatness_of({{0, 0, 0}, {4, 0, 0}, {4, 0, 3}, {0, 2.6f, 3}});
    const float folded = flatness_of({{0, 0, 0}, {4, 0, 0}, {4, 2.5f, 3}, {0, -2.5f, 3}});

    // ТРЕУГОЛЬНИК НЕПЛОСКИМ НЕ БЫВАЕТ: три точки задают плоскость точно, и мера
    // обязана давать РОВНО ноль, а не «маленькое число». Иначе порог начал бы
    // ловить арифметику вместо замысла.
    CHECK(flatness_of({{0, 0, 0}, {4, 0, 0}, {2, 2.5f, 3}}) == doctest::Approx(0.0f).epsilon(1e-6));

    CHECK(ramp == doctest::Approx(0.0000f).epsilon(0.01));
    CHECK(ridge == doctest::Approx(0.0017f).epsilon(0.05));
    CHECK(floor_86 == doctest::Approx(0.0102f).epsilon(0.02));
    CHECK(terrace == doctest::Approx(0.0173f).epsilon(0.02));
    CHECK(sill_65 == doctest::Approx(0.0288f).epsilon(0.02));
    CHECK(sill_33 == doctest::Approx(0.0295f).epsilon(0.02));
    CHECK(ceil_86 == doctest::Approx(0.0648f).epsilon(0.02));
    CHECK(ceil_43 == doctest::Approx(0.1228f).epsilon(0.02));
    CHECK(folded == doctest::Approx(0.2009f).epsilon(0.02));

    // ПОРОГ СТОИТ МЕЖДУ ИЗМЕРЕННЫМИ ОБРАЗЦАМИ (правило 45), и обе стороны
    // воспроизводимы вот этими самыми строками, а не помнятся.
    for (const float accepted : {ramp, ridge, floor_86, terrace}) {
        CHECK(accepted < FLATNESS_MAX);
    }
    for (const float rejected : {sill_65, sill_33, ceil_86, ceil_43, folded}) {
        CHECK(rejected > FLATNESS_MAX);
    }
    // И ЗАПАС НАЗВАН ЧИСЛОМ, а не словом «с запасом»: 1.29x в обе стороны.
    CHECK(FLATNESS_MAX / terrace == doctest::Approx(1.29f).epsilon(0.03));
    CHECK(sill_65 / FLATNESS_MAX == doctest::Approx(1.29f).epsilon(0.03));
}

TEST_CASE("кривой контур ГОВОРИТСЯ находкой, но пол не исчезает") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId c = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
    const VertexId d = g.add_vertex(Anchoring::Free, {0.0f, 2.6f, 3.0f});
    ElementId bad = 0;
    REQUIRE(g.add_element(ElementKind::Surface, {a, b, c, d}, "oak", bad).ok);
    const HouseMesh m = dfn::world::build_house_mesh(g);

    REQUIRE(m.findings.size() == 1);
    CHECK(m.findings.front().issue == MeshIssue::ContourNonPlanar);
    CHECK(m.findings.front().element == bad);
    CHECK(m.findings.front().value == doctest::Approx(0.1228f).epsilon(0.02));
    // ГЕОМЕТРИЯ ВСЁ РАВНО ЕСТЬ. Отказ строить превратил бы промах по якорю в
    // исчезнувший пол, а исчезнувший пол объясняет причину хуже, чем сложенный.
    CHECK(m.part_of(bad) != nullptr);
    CHECK(m.triangle_count() > 0);
}

TEST_CASE("контур-бантик ловится самопересечением") {
    const std::vector<glm::vec2> bowtie = {{0.0f, 0.0f}, {4.0f, 0.0f}, {0.0f, 3.0f}, {4.0f, 3.0f}};
    CHECK(dfn::world::contour_self_intersects(bowtie));
    // КОНТРОЛЬ: тот же четырёхугольник в правильном порядке чист. Без него
    // утверждение прошло бы и на проверке, которая всегда отвечает «да».
    const std::vector<glm::vec2> square = {{0.0f, 0.0f}, {4.0f, 0.0f}, {4.0f, 3.0f}, {0.0f, 3.0f}};
    CHECK_FALSE(dfn::world::contour_self_intersects(square));
    CHECK_FALSE(dfn::world::contour_self_intersects(l_room()));

    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId c = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 3.0f});
    const VertexId d = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
    ElementId tie = 0;
    REQUIRE(g.add_element(ElementKind::Surface, {a, b, c, d}, "oak", tie).ok);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    const auto found = std::find_if(m.findings.begin(), m.findings.end(), [](const auto& f) {
        return f.issue == MeshIssue::ContourSelfIntersects;
    });
    CHECK(found != m.findings.end());
}

TEST_CASE("стена-цепочка: излом даёт ДВА тела, и они входят друг в друга") {
    // §7.1: три вершины A-B-C дают два отрезка стены, встречающихся в углу.
    // Ответ пользователя — столб в углу, стены упираются в его ось, а НЕ ус.
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    const VertexId c = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 3.0f});
    ElementId wall = 0;
    REQUIRE(g.add_element(ElementKind::Surface, {a, b, c}, "frame;height=2.6;thickness=0.2", wall)
                .ok);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    REQUIRE(m.findings.empty());
    const MeshPart* p = m.part_of(wall);
    REQUIRE(p != nullptr);

    // Две коробки: 12 треугольников каждая. Никакого уса, никакого срезания.
    CHECK(p->index_count / 3 == 24);
    CHECK(m.convex.size() == 4); // по два уха на коробку

    // НАХЛЁСТ СЧИТАЕТСЯ ЧИСЛОМ, а не признаётся на словах. Сумма объёмов двух
    // тел 4*2.6*0.2 + 3*2.6*0.2 = 3.640; настоящий объём объединения меньше на
    // общий уголок 0.2*0.2*2.6 = 0.104. Прибор видит 3.640 именно потому, что
    // тела пересекаются, и это ожидаемое состояние (§7.0).
    CHECK(part_volume(m, *p) == doctest::Approx(3.640f).epsilon(0.001));

    // Лицо стены — по правилу правой руки от порядка вершин и вертикали.
    glm::vec3 n{0.0f};
    REQUIRE(dfn::world::surface_normal(g, wall, n));
    CHECK(n.z == doctest::Approx(1.0f).epsilon(0.001));
}

TEST_CASE("цепочка без высоты не выдумывает стену, а ГОВОРИТ") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::OnGround, {4.0f, 0.0f, 0.0f});
    ElementId wall = 0;
    REQUIRE(g.add_element(ElementKind::Surface, {a, b}, "frame", wall).ok);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    REQUIRE(m.findings.size() == 1);
    CHECK(m.findings.front().issue == MeshIssue::ChainNeedsHeight);
    CHECK(m.triangle_count() == 0);
}

TEST_CASE("прямая на ОДНОЙ вершине: углы задают направление, длина — длину") {
    const auto tip = [](const char* style) {
        HouseGraph g;
        const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
        ElementId e = 0;
        g.add_element(ElementKind::Line, {a}, style, e);
        const HouseMesh m = dfn::world::build_house_mesh(g);
        glm::vec3 far{0.0f};
        float best = -1.0f;
        const MeshPart* p = m.part_of(e);
        if (p == nullptr) {
            return far;
        }
        for (const glm::vec3& q : part_positions(m, *p)) {
            if (glm::length(q) > best) {
                best = glm::length(q);
                far = q;
            }
        }
        return far;
    };
    // Нулевые углы: стойка СТОИТ. Самый частый случай стоит нуля работы.
    CHECK(tip("oak;length=3").y == doctest::Approx(3.0f).epsilon(0.05));
    // angle_x кладёт её, angle_y решает, куда именно.
    const glm::vec3 lying = tip("oak;length=3;angle_x=90;angle_y=90");
    CHECK(lying.x == doctest::Approx(3.0f).epsilon(0.05));
    CHECK(lying.y == doctest::Approx(0.0f).epsilon(0.1));
    const glm::vec3 lying_z = tip("oak;length=3;angle_x=90;angle_y=0");
    CHECK(lying_z.z == doctest::Approx(3.0f).epsilon(0.05));

    // Без длины прямая не выдумывается: одна вершина и ничего больше — это
    // намерение, а не элемент.
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    ElementId e = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a}, "oak", e).ok);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    REQUIRE(m.findings.size() == 1);
    CHECK(m.findings.front().issue == MeshIssue::LineNeedsLength);
}

TEST_CASE("angle_z вращает ПРОФИЛЬ, и для квадратного бруса это не косметика") {
    const auto extent = [](const char* style) {
        HouseGraph g;
        const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
        const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 2.0f, 0.0f});
        ElementId e = 0;
        g.add_element(ElementKind::Line, {a, b}, style, e);
        const HouseMesh m = dfn::world::build_house_mesh(g);
        const MeshPart* p = m.part_of(e);
        float widest = 0.0f;
        for (const glm::vec3& q : part_positions(m, *p)) {
            widest = std::max(widest, std::sqrt(q.x * q.x + q.z * q.z));
        }
        return widest;
    };
    // Квадрат 0.24x0.24: полудиагональ 0.1697 при любом крене, потому что
    // меряется расстояние до УГЛА. Меряем то, что крен действительно двигает:
    // положение угла по осям.
    const float straight = extent("oak;form=square;radius=0.12");
    const float rolled = extent("oak;form=square;radius=0.12;angle_z=45");
    CHECK(straight == doctest::Approx(rolled).epsilon(0.001)); // радиус тот же

    const auto corner_x = [](const char* style) {
        HouseGraph g;
        const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
        const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 2.0f, 0.0f});
        ElementId e = 0;
        g.add_element(ElementKind::Line, {a, b}, style, e);
        const HouseMesh m = dfn::world::build_house_mesh(g);
        float widest = 0.0f;
        for (const glm::vec3& q : part_positions(m, *m.part_of(e))) {
            widest = std::max(widest, std::fabs(q.x));
        }
        return widest;
    };
    // Брус гранью к оси X: половина толщины 0.12. Повернули на 45 — к оси
    // смотрит УГОЛ, и габарит по X стал 0.1697. Разница 1.414 раза, и это ровно
    // то, ради чего третий угол существует.
    CHECK(corner_x("oak;form=square;radius=0.12") == doctest::Approx(0.12f).epsilon(0.01));
    CHECK(corner_x("oak;form=square;radius=0.12;angle_z=45") ==
          doctest::Approx(0.12f * std::sqrt(2.0f)).epsilon(0.01));
}

TEST_CASE("свойства переживают круговой прогон файла, а опечатка ГОВОРИТСЯ") {
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 3.0f, 0.0f});
    ElementId post = 0;
    // Разделитель — точка с запятой, а НЕ пробел, и это не вкус: HouseFile
    // пишет style= одним токеном, и пробел разорвал бы строку на ссылки.
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak;form=square;radius=0.15;tex_deg=45", post)
                .ok);
    const std::string text = dfn::world::write_house(g);
    HouseGraph back;
    REQUIRE(dfn::world::read_house(text, back).ok);
    REQUIRE(back.element(post) != nullptr);
    const ElementParams p = dfn::world::parse_element_params(back.element(post)->style);
    CHECK(p.name == "oak");
    CHECK(p.form == LineForm::Square);
    CHECK(p.radius == doctest::Approx(0.15f));
    CHECK(p.tex_deg == doctest::Approx(45.0f));
    CHECK(dfn::world::write_house(back) == text);

    // ОПЕЧАТКА НЕ ПРОГЛАТЫВАЕТСЯ. Молча проигнорированное свойство — это
    // свойство, которое дизайнер задал, а движок не применил, и разбираться с
    // этим он будет глазами, а не сообщением.
    std::vector<dfn::world::ParamIssue> issues;
    const ElementParams typo = dfn::world::parse_element_params("oak;radus=0.15;height=2.6", &issues);
    REQUIRE(issues.size() == 1);
    CHECK(issues.front().token == "radus=0.15");
    CHECK(typo.radius == doctest::Approx(dfn::world::HOUSE_LINE_RADIUS_DEFAULT));
    CHECK(typo.height == doctest::Approx(2.6f)); // остальное применилось
}

TEST_CASE("две сборки одного графа дают ПОБАЙТОВО один меш") {
    // Без этого «до и после» ничего не значит: «до» каждый раз разное. Правило
    // 13.1 на геометрии, и оно же условие того, чтобы типовой дом, поставленный
    // дважды, был одним домом.
    HouseGraph g;
    std::vector<VertexId> refs;
    for (const glm::vec2& q : l_room()) {
        refs.push_back(g.add_vertex(Anchoring::OnGround, {q.x, 0.0f, q.y}));
    }
    ElementId floor = 0;
    ElementId post = 0;
    ElementId wall = 0;
    REQUIRE(g.add_element(ElementKind::Surface, refs, "oak;thickness=0.25", floor).ok);
    const VertexId top = g.add_vertex(Anchoring::Free, {6.0f, 3.0f, 0.0f});
    REQUIRE(g.add_element(ElementKind::Line, {refs[1], top}, "oak;radius=0.15", post).ok);
    REQUIRE(g.add_element(ElementKind::Surface, {refs[0], refs[1]},
                          "frame;height=2.6;thickness=0.2", wall)
                .ok);

    const HouseMesh a = dfn::world::build_house_mesh(g);
    const HouseMesh b = dfn::world::build_house_mesh(g);
    REQUIRE(a.vertices.size() == b.vertices.size());
    REQUIRE(a.indices.size() == b.indices.size());
    CHECK(a.indices == b.indices);
    bool identical = true;
    for (std::size_t i = 0; i < a.vertices.size(); ++i) {
        identical = identical && a.vertices[i].pos == b.vertices[i].pos &&
                    a.vertices[i].normal == b.vertices[i].normal &&
                    a.vertices[i].uv == b.vertices[i].uv;
    }
    CHECK(identical);

    // Порядок элементов — по возрастанию имени, тот же, что в файле.
    REQUIRE(a.parts.size() == 3);
    CHECK(a.parts[0].element == floor);
    CHECK(a.parts[1].element == post);
    CHECK(a.parts[2].element == wall);
}

TEST_CASE("вершина НА ОСИ тянет за собой геометрию, которая на ней висит") {
    // Ради этого вся затея с графом: двинул якорь — поехало всё привязанное, и
    // ничего не пересчитывалось руками.
    HouseGraph g;
    const VertexId a = g.add_vertex(Anchoring::OnGround, {0.0f, 0.0f, 0.0f});
    const VertexId b = g.add_vertex(Anchoring::Free, {0.0f, 4.0f, 0.0f});
    ElementId post = 0;
    REQUIRE(g.add_element(ElementKind::Line, {a, b}, "oak;radius=0.15", post).ok);
    VertexId mid = 0;
    REQUIRE(g.add_vertex_on_edge(post, 0.5f, mid).ok);
    const VertexId far = g.add_vertex(Anchoring::Free, {4.0f, 2.0f, 0.0f});
    ElementId beam = 0;
    REQUIRE(g.add_element(ElementKind::Line, {mid, far}, "oak;radius=0.1", beam).ok);

    // Мерим САМУЮ ВЫСОКУЮ точку балки: её несёт тот конец, что сидит на оси.
    // Первая вершина буфера для этого не годится — веер торца начинается с
    // дальнего конца, и прибор, читающий её, отвечал бы про НЕПОДВИЖНЫЙ конец.
    const auto top_of_beam = [beam](const HouseMesh& m) {
        float best = -1e9f;
        const MeshPart* p = m.part_of(beam);
        for (const glm::vec3& q : part_positions(m, *p)) {
            best = std::max(best, q.y);
        }
        return best;
    };
    const HouseMesh before = dfn::world::build_house_mesh(g);
    const float y_before = top_of_beam(before);
    CHECK(y_before == doctest::Approx(2.0f).epsilon(0.1));

    // Подняли ВЕРХ СТОЛБА — балка поехала за серединой оси, хотя её никто не
    // трогал. Ни одной строки пересчёта: геометрия нигде не хранится.
    REQUIRE(g.move_vertex(b, {0.0f, 8.0f, 0.0f}).ok);
    const HouseMesh after = dfn::world::build_house_mesh(g);
    const float y_after = top_of_beam(after);
    CHECK(y_after == doctest::Approx(4.0f).epsilon(0.1));
    CHECK(y_after - y_before == doctest::Approx(2.0f).epsilon(0.1));
}
