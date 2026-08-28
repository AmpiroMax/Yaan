/*
Created: 21:08:2026 - 00:40:00
Last updated: 28:08:2026 - 14:05:00
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
- 28:08:2026 - 14:05:00: ФАСКА НА РЁБРА ПРИЗМЫ (mb.bevel_m; дефект 3 ТЗ
  материалов, критерий К4). Все девять модулей-алгоритмов кладут тела ЧЕРЕЗ
  push_prism, поэтому фаска заведена здесь одна на всех: дом, убранство и
  запечённый предмет реестра получают её тем же кодом, а не тремя копиями.
  Ширина ноль — прежняя ветка целиком, бит-в-бит (правило 47). КОЛЛАЙДЕР НЕ
  ТРОГАЕТСЯ вовсе: выпуклые куски по-прежнему строятся на ИСХОДНОМ контуре и
  исходной оси, поэтому физика города бит-в-бит при любой ширине — фаска в
  сантиметр не имеет права двигать опору под ногами.
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {

/// ФАСКА: НАИМЕНЬШАЯ ШИРИНА, КОТОРУЮ ЕЩЁ ИМЕЕТ СМЫСЛ СТРОИТЬ, метры.
/// Полмиллиметра — ниже разрешения всякого кадра, с которого предмет вообще
/// видно, и ниже допуска, с которым сходятся контуры .dfh. Всё, что ужалось
/// до этого, строится ПРЕЖНЕЙ веткой: лучше острое ребро, чем полоска граней
/// шириной в ошибку округления.
inline constexpr float BEVEL_MIN_M = 0.0005f;

/// ДВА КОЛЬЦА ФАСКИ ВОКРУГ ОДНОГО КОНТУРА.
///
/// `outer` — исходный контур со СРЕЗАННЫМИ выпуклыми углами: каждая такая
/// вершина заменена парой, отступившей на w вдоль своих же рёбер. Это ребро
/// призмы, идущее ВДОЛЬ оси: между парой встаёт узкая грань под 45°.
/// `inner` — то же кольцо, вжатое внутрь на w. На нём стоят крышка и днище, а
/// полоса между ним и `outer` и есть фаска ребра ПОПЕРЁК оси.
///
/// ПОЧЕМУ ТРИАНГУЛЯЦИЯ СЧИТАЕТСЯ ЗДЕСЬ, А НЕ БЕРЁТСЯ ЧУЖАЯ. Вызывающий даёт
/// разбиение ИСХОДНОГО контура (отсечение ушей держит невыпуклую комнату), и
/// оно остаётся верным для «ядра» — кольца из первых вершин каждой пары:
/// выпуклая вершина сдвигается ВДОЛЬ своего входящего ребра, то есть внутрь
/// собственного контура, а невыпуклая не сдвигается вовсе. Остаток — по
/// треугольнику на срез, и он всегда ухо: точка `b` лежит вне хорды `a_i ->
/// a_(i+1)` ровно потому, что угол выпуклый.
struct BevelRings {
    std::vector<glm::vec3> outer;
    std::vector<glm::vec3> inner;
    std::vector<std::uint32_t> tris; ///< индексы годятся обоим кольцам
};

/// Нормаль контура по Ньюэллу — та, для которой контур обойдён ПРОТИВ часовой.
/// Знак здесь решает всё: по нему отличается выпуклый угол от невыпуклого и
/// «внутрь» от «наружу», и ошибка в нём вывернула бы фаску мехом внутрь.
[[nodiscard]] glm::vec3 loop_normal(std::span<const glm::vec3> loop, glm::vec3 fallback) {
    glm::vec3 n{0.0f};
    const std::size_t count = loop.size();
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3& a = loop[i];
        const glm::vec3& b = loop[(i + 1) % count];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    const float len = glm::length(n);
    return len < HOUSE_GEOM_EPS ? fallback : n / len;
}

[[nodiscard]] BevelRings make_bevel_rings(std::span<const glm::vec3> loop,
                                          std::span<const std::uint32_t> tris,
                                          glm::vec3 poly_n, float w) {
    BevelRings r;
    const std::size_t count = loop.size();
    std::vector<std::uint32_t> at_a(count, 0);
    std::vector<std::uint32_t> at_b(count, 0);
    r.outer.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3& prev = loop[(i + count - 1) % count];
        const glm::vec3& cur = loop[i];
        const glm::vec3& next = loop[(i + 1) % count];
        glm::vec3 ein = cur - prev;
        glm::vec3 eout = next - cur;
        const float lin = glm::length(ein);
        const float lout = glm::length(eout);
        bool cut = lin > HOUSE_GEOM_EPS && lout > HOUSE_GEOM_EPS;
        float set = 0.0f;
        if (cut) {
            ein /= lin;
            eout /= lout;
            // Выпуклый угол — и только он. Невыпуклый (внутренний угол
            // Г-образной комнаты) срезать нечем: фаска там ЗАБИРАЛА бы
            // материал снаружи тела, а не с ребра.
            const float turn = glm::dot(glm::cross(ein, eout), poly_n);
            set = std::min(w, 0.45f * std::min(lin, lout));
            // УГОЛ, КОТОРЫЙ УЖЕ НЕ ОСТРЫЙ, НЕ СРЕЗАЕТСЯ. Восьмигранное бревно,
            // бочка, кружка, круглая тарелка: у них излом 45°, то есть ровно
            // то, что фаска и делает, — и по мере К4 такое ребро проходит.
            // ТРИ ПРИЧИНЫ, И ТРЕТЬЯ РЕШАЮЩАЯ: не тратить треугольники там,
            // где грань уже есть; не делать из восьмигранника
            // шестнадцатигранник; и НЕ ДВИГАТЬ ГАБАРИТ. У многогранника
            // крайнюю точку силуэта держит ОДНА вершина, и срез её сдвигал
            // габарит внутрь на ширину фаски — замер волны номенклатуры:
            // furn-cup 0.11 -> 0.10, furn-basket 0.50 -> 0.49. Габарит стоит
            // колонкой в перечне объектов, по ней раскладка садит предметы на
            // столешницы, и молча уехавший сантиметр — это уехавшая посуда.
            const float shallow_cos =
                std::cos(HOUSE_BEVEL_TURN_DEG * HOUSE_PI_F / 180.0f);
            cut = turn > 1e-4f && glm::dot(ein, eout) < shallow_cos && set >= BEVEL_MIN_M;
        }
        at_a[i] = static_cast<std::uint32_t>(r.outer.size());
        if (cut) {
            r.outer.push_back(cur - ein * set);
            at_b[i] = static_cast<std::uint32_t>(r.outer.size());
            r.outer.push_back(cur + eout * set);
        } else {
            r.outer.push_back(cur);
            at_b[i] = at_a[i];
        }
    }
    r.tris.reserve(tris.size() + count * 3);
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        r.tris.push_back(at_a[tris[t]]);
        r.tris.push_back(at_a[tris[t + 1]]);
        r.tris.push_back(at_a[tris[t + 2]]);
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (at_b[i] != at_a[i]) {
            r.tris.push_back(at_a[i]);
            r.tris.push_back(at_b[i]);
            r.tris.push_back(at_a[(i + 1) % count]);
        }
    }
    // Вжатие: вершина едет по биссектрисе двух внутренних нормалей на столько,
    // чтобы ОБЕ соседние грани отступили ровно на w. Множитель ограничен
    // тремя: на игольчатом угле точное вжатие улетает в бесконечность, и
    // лучше недорезанный угол, чем контур, вывернутый наизнанку.
    const std::size_t m = r.outer.size();
    r.inner.resize(m);
    for (std::size_t j = 0; j < m; ++j) {
        const glm::vec3& prev = r.outer[(j + m - 1) % m];
        const glm::vec3& cur = r.outer[j];
        const glm::vec3& next = r.outer[(j + 1) % m];
        glm::vec3 din = cur - prev;
        glm::vec3 dout = next - cur;
        const float lin = glm::length(din);
        const float lout = glm::length(dout);
        din = lin > HOUSE_GEOM_EPS ? din / lin : dout;
        dout = lout > HOUSE_GEOM_EPS ? dout / lout : din;
        const glm::vec3 nin = glm::cross(poly_n, din);
        const glm::vec3 nout = glm::cross(poly_n, dout);
        glm::vec3 bis = nin + nout;
        const float blen = glm::length(bis);
        if (blen < 1e-4f) {
            r.inner[j] = cur + nin * w;
            continue;
        }
        bis /= blen;
        r.inner[j] = cur + bis * (w / std::max(glm::dot(bis, nin), 0.34f));
    }
    return r;
}

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
    // ШИРИНА ФАСКИ ЭТОГО ТЕЛА. Заказанная ширина — верхняя граница, не
    // приказ: у плашки подтёка в два сантиметра толщиной сантиметровая фаска
    // съела бы тело целиком. Ограничители названы по причине, а не по вкусу —
    // четверть высоты держит боковую грань не уже самих фасок, четверть
    // кратчайшего ребра держит вжатое кольцо от выворачивания наизнанку.
    float bevel_w = mb.bevel_m;
    if (bevel_w >= BEVEL_MIN_M) {
        // ТОЛЩИНА ТЕЛА — наименьшая сторона его габаритного ящика. Ящик
        // никогда не занижает толщину (у косо стоящей плиты он её завышает),
        // поэтому порог ошибается только в сторону «поставить фаску», а не в
        // сторону «молча снять её с бруска».
        glm::vec3 lo{loop[0]};
        glm::vec3 hi{loop[0]};
        float min_edge = axis_len;
        for (std::size_t i = 0; i < count; ++i) {
            lo = glm::min(glm::min(lo, loop[i]), loop[i] + axis);
            hi = glm::max(glm::max(hi, loop[i]), loop[i] + axis);
            const float len = glm::length(loop[(i + 1) % count] - loop[i]);
            if (len > 1e-4f) {
                min_edge = std::min(min_edge, len);
            }
        }
        const glm::vec3 box = hi - lo;
        if (std::min({box.x, box.y, box.z}) < HOUSE_BEVEL_MIN_THICK_M) {
            bevel_w = 0.0f; // полотно, а не брусок: у него кромка, не фаска
        } else {
            // ШИРИНА ПРИХОДИТ ОТ КУСКА: заказанная — верхняя граница. Четверть
            // высоты держит боковую грань не уже самих фасок, четверть
            // кратчайшего ребра держит вжатое кольцо от выворачивания.
            bevel_w = std::min({bevel_w, axis_len * 0.25f, min_edge * 0.25f});
            if (bevel_w < BEVEL_MIN_M) {
                bevel_w = 0.0f;
            }
        }
    } else {
        bevel_w = 0.0f;
    }

    if (bevel_w > 0.0f) {
        const BevelRings rings =
            make_bevel_rings(loop, tris, loop_normal(loop, n), bevel_w);
        const glm::vec3 lift = n * bevel_w;
        // Крышка и днище встают на ВЖАТОМ кольце; полоса до внешнего — фаска
        // поперечного ребра. Рамки uv остались прежними (та же середина
        // грани, тот же поворот): фаска обязана продолжать узор грани, а не
        // начинать свой.
        for (std::size_t t = 0; t + 2 < rings.tris.size(); t += 3) {
            mb.push_triangle(rings.inner[rings.tris[t]] + axis,
                             rings.inner[rings.tris[t + 1]] + axis,
                             rings.inner[rings.tris[t + 2]] + axis, top_uv);
            mb.push_triangle(rings.inner[rings.tris[t + 2]], rings.inner[rings.tris[t + 1]],
                             rings.inner[rings.tris[t]], bottom_uv);
        }
        // Три пояса: фаска у днища, сам рант, фаска у крышки. Продольные рёбра
        // призмы срезаны В САМОМ кольце `outer`, поэтому узкие грани углов
        // приходят поясами наравне с широкими — отдельного кода у них нет.
        const auto band = [&](const std::vector<glm::vec3>& lo_ring, glm::vec3 lo_off,
                              const std::vector<glm::vec3>& hi_ring, glm::vec3 hi_off) {
            const std::size_t m = lo_ring.size();
            for (std::size_t j = 0; j < m; ++j) {
                const std::size_t k = (j + 1) % m;
                const glm::vec3 p0 = lo_ring[j] + lo_off;
                const glm::vec3 p1 = lo_ring[k] + lo_off;
                const glm::vec3 p2 = hi_ring[k] + hi_off;
                const glm::vec3 p3 = hi_ring[j] + hi_off;
                const glm::vec3 raw = glm::cross(p1 - p0, p3 - p0);
                if (glm::length(raw) < HOUSE_GEOM_EPS) {
                    continue;
                }
                glm::vec3 dir = p1 - p0;
                if (glm::length(dir) < HOUSE_GEOM_EPS) {
                    dir = p2 - p3;
                }
                const UvFrame face_uv = make_uv_frame((p0 + p1 + p2 + p3) * 0.25f,
                                                      glm::normalize(raw), dir, tex_deg);
                mb.push_quad(p0, p1, p2, p3, face_uv);
            }
        };
        band(rings.inner, glm::vec3{0.0f}, rings.outer, lift);
        band(rings.outer, lift, rings.outer, axis - lift);
        band(rings.outer, axis - lift, rings.inner, axis);
    } else {
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
            mb.push_triangle(loop[tris[t]] + axis, loop[tris[t + 1]] + axis,
                             loop[tris[t + 2]] + axis, top_uv);
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
