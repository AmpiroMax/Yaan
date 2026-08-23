/*
Created: 21:08:2026 - 00:40:00
Last updated: 23:08:2026 - 18:30:00
Module: engine/world
File: engine/world/sources/HousePlate.cpp

Responsibility:
- ПЛАСТИНА ЗАМКНУТОГО КОНТУРА: триангуляция в МНК-плоскости, симметричная
  толщина, разворот лица; маршрутизация покрытий (паркет, кровля, марш),
  потолочные балки.

Key items:
- build_contour_surface.

Dependencies:
- Uses: HouseMeshDetail.h, HouseParquet, HouseRoof, HouseStairs
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/
/*
UPD:
- 21:08:2026 - 00:40:00: Вырезан из HouseMesh.cpp (1942 строки, девять алгоритмов в одном файле).
- 23:08:2026 - 18:30:00: решётка потолочных балок прибита к НАЧАЛУ ПОСТРОЙКИ постоянным
  шагом HOUSE_BEAM_PITCH_M, а не делит пролёт СВОЕГО куска (заказ владельца:
  «балки, что вторые этажи держат, кривые стоят»). Замер: два куска одного
  перекрытия таверны давали шаги 1.600 и 1.500 с отступами 0.800 и 0.750, и на
  шве балки расходились на 0.05 и 0.15 м — балка, переломленная посреди комнаты.
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {


} // namespace

void build_contour_surface(const Element& e, const ElementParams& p,
                           std::span<const glm::vec3> pts, MeshBuilder& mb, HouseMesh& mesh) {
    const FittedPlane plane = fit_contour_plane(pts);
    // САМОПЕРЕСЕЧЕНИЕ ПРОВЕРЯЕТСЯ ПЕРВЫМ, до отказа по вырождению, и порядок
    // здесь — часть ответа: у бантика заметённая площадь ровно ноль, поэтому
    // обратный порядок назвал бы его «контуром нулевой площади» и отправил
    // дизайнера искать совпавшие вершины, которых нет.
    glm::vec3 u{0.0f};
    glm::vec3 v{0.0f};
    const std::vector<glm::vec2> flat = project_contour(pts, plane, u, v);
    if (contour_self_intersects(flat)) {
        mesh.findings.push_back(
            {e.id, MeshIssue::ContourSelfIntersects, 0.0f, "контур самопересекается"});
    }
    if (plane.degenerate) {
        mesh.findings.push_back(
            {e.id, MeshIssue::Degenerate, plane.area, "у контура нулевая площадь"});
        return;
    }
    if (plane.flatness > HOUSE_CONTOUR_FLATNESS_MAX) {
        // ГОВОРИТСЯ, НО НЕ ЗАПРЕЩАЕТСЯ. Дизайнер посреди правки обязан видеть
        // то, что сделал; отказ строить превратил бы промах по якорю в
        // исчезнувший пол, а исчезнувший пол объясняет причину хуже, чем
        // сложенный пополам.
        mesh.findings.push_back({e.id, MeshIssue::ContourNonPlanar, plane.flatness,
                                 "контур слишком кривой для наклонной поверхности"});
    }
    // ЛЕСТНИЦА ЗАМЕЩАЕТ ПЛАСТИНУ: контур с fill=6 — это марш, а не плита с
    // маршем поверх; наклонная плита под ступенями лишь спорила бы с их
    // коллайдером.
    if (fill_kind(p) == WallFill::Stairs) {
        build_stairs_contour(e, p, pts, mb, mesh);
        return;
    }
    const std::vector<std::uint32_t> tris = triangulate_contour(flat);
    if (tris.empty()) {
        mesh.findings.push_back({e.id, MeshIssue::TriangulationFailed, 0.0f,
                                 "уши кончились раньше треугольников"});
        return;
    }
    // ЛИЦО — по обходу, переключатель разворачивает (§2.3).
    const glm::vec3 n = e.facing_flipped ? -plane.normal : plane.normal;
    const float half = std::max(p.thickness, HOUSE_GEOM_EPS) * 0.5f;

    // ТОЛЩИНА СИММЕТРИЧНА: срединная плоскость проходит ровно по якорям, как
    // тело прямой сидит вокруг оси, а не сбоку от неё. Одно правило на два
    // вида, и разворот лица не двигает ни одной точки.
    std::vector<glm::vec3> loop;
    loop.reserve(pts.size());
    std::vector<std::uint32_t> use = tris;
    if (e.facing_flipped) {
        // Кольцо обязано обходиться против часовой стрелки со стороны +n. При
        // развороте лица переворачивается и порядок, и нумерация в тройках.
        for (std::size_t i = pts.size(); i-- > 0;) {
            loop.push_back(pts[i] - n * half);
        }
        const std::uint32_t last = static_cast<std::uint32_t>(pts.size() - 1);
        for (std::size_t t = 0; t + 2 < use.size(); t += 3) {
            const std::uint32_t x = last - use[t];
            const std::uint32_t y = last - use[t + 1];
            const std::uint32_t z = last - use[t + 2];
            use[t] = z;
            use[t + 1] = y;
            use[t + 2] = x;
        }
    } else {
        for (const glm::vec3& q : pts) {
            loop.push_back(q - n * half);
        }
    }
    push_prism(mb, loop, use, n * (half * 2.0f), p.tex_deg, mesh, e.id);
    if (fill_kind(p) == WallFill::Parquet) {
        build_parquet(e, p, plane, flat, u, v, mb, mesh);
    }
    if (fill_kind(p) == WallFill::Shingle || fill_kind(p) == WallFill::Tile) {
        build_roof_courses(e, p, plane, flat, u, v, mb, mesh);
    }
    // ПОТОЛОЧНЫЕ БАЛКИ (beams=1): брусья под ИЗНАНКОЙ пластины вдоль
    // короткой оси bbox, шаг HOUSE_BEAM_PITCH_M, режутся контуром — как
    // паркет, только редкие, толстые и снизу (INTERIOR_CATALOG.md: балка
    // 0.2x0.25, шаг 1.2-1.6, встреча со стеной — врезкой, что нахлёст и даёт).
    //
    // РЕШЁТКА БАЛОК ПРИБИТА К ПОСТРОЙКЕ, А НЕ К КУСКУ ПЕРЕКРЫТИЯ (заказ
    // владельца 23.08: «внутри домов балки, что вторые этажи держат, кривые
    // стоят»). ДИАГНОЗ ЗАМЕРЕН. Перекрытие непрямоугольной формы собирается
    // ИЗ НЕСКОЛЬКИХ контуров (Г-образный этаж таверны — 8x8 плюс 2x3: невыпуклый
    // контур пришлось бы триангулировать веером). Прежняя раскладка делила
    // СВОЙ кусок на равные доли: у половины 8 м вышло 5 балок с шагом 1.600 и
    // отступом 0.800, у половины 2x3 — 2 балки с шагом 1.500 и отступом 0.750.
    // Балки ОДНОГО потолка встречались на шве x = 8 со сдвигом 0.05 и 0.15 м:
    // с земли это читается балкой, переломленной посреди комнаты, — ровно то,
    // что владелец назвал «кривые стоят».
    //
    // Теперь ось балки — узел решётки с ПОСТОЯННЫМ шагом, отсчитанной от
    // НАЧАЛА ПОСТРОЙКИ: c_мир = (k + 0.5) * шаг. Два куска одного этажа с одной
    // осью балок кладут брусья на ОДНИ И ТЕ ЖЕ линии по построению, и шов
    // исчезает не подгонкой, а тем, что подгонять стало нечего. Плата названа:
    // у куска, чья ширина не кратна шагу, отступы у стен выходят разные
    // (0.7 и 1.3 у зала 16 м) — так и лежит настоящий накат, остаток уходит в
    // последний пролёт.
    if (p.beams > 0.5f) {
        glm::vec2 blo = flat.front();
        glm::vec2 bhi = blo;
        for (const glm::vec2& f : flat) {
            blo = glm::min(blo, f);
            bhi = glm::max(bhi, f);
        }
        const bool along_u = (bhi.x - blo.x) < (bhi.y - blo.y);
        const float lo_c = along_u ? blo.y : blo.x;
        const float hi_c = along_u ? bhi.y : bhi.x;
        // Куда смотрит ПОПЕРЕЧНАЯ ось в координатах постройки: по ней и
        // считается номер узла решётки. Начало контура (центр тяжести куска)
        // из счёта вычитается — иначе решётка была бы своя у каждого куска,
        // то есть ровно та беда, которую правило и лечит.
        const glm::vec3 across3 = along_u ? v : u;
        const float shift = glm::dot(plane.origin, across3);
        const float pitch = HOUSE_BEAM_PITCH_M;
        int k0 = static_cast<int>(std::ceil((lo_c + shift) / pitch - 0.5f));
        int k1 = static_cast<int>(std::floor((hi_c + shift) / pitch - 0.5f));
        // Кусок уже шага (альков, ниша) не остаётся БЕЗ балки: узлов внутри
        // нет, но перекрытие есть, и голая пластина рядом с накатом читается
        // дырой в потолке.
        bool single = false;
        if (k1 < k0) {
            single = true;
        }
        mb.set_material(0, 2); // тёмный тёсаный брус
        for (int bi = k0; bi <= (single ? k0 : k1); ++bi) {
            const float c = single ? (lo_c + hi_c) * 0.5f
                                   : (static_cast<float>(bi) + 0.5f) * pitch - shift;
            const glm::vec2 rlo = along_u ? glm::vec2{blo.x, c - 0.1f}
                                          : glm::vec2{c - 0.1f, blo.y};
            const glm::vec2 rhi = along_u ? glm::vec2{bhi.x, c + 0.1f}
                                          : glm::vec2{c + 0.1f, bhi.y};
            const std::vector<glm::vec2> strip = clip_contour_to_rect(flat, rlo, rhi);
            if (strip.size() < 3) {
                continue;
            }
            const std::vector<std::uint32_t> btris = triangulate_contour(strip);
            if (btris.empty()) {
                continue;
            }
            std::vector<glm::vec3> loop2;
            loop2.reserve(strip.size());
            // Под изнанкой: от нижней грани пластины вниз на 0.25.
            const glm::vec3 drop = n * (half + 0.25f);
            for (const glm::vec2& q : strip) {
                loop2.push_back(plane.origin + u * q.x + v * q.y - drop);
            }
            push_prism(mb, loop2, btris, n * 0.25f, p.tex_deg, mesh, e.id);
        }
        mb.set_material(-1, -1);
    }
}

// ---------------------------------------------------------------------------
// Обшивка по раскладке (HouseStyle): доски, раскосы, рамы проёмов
// ---------------------------------------------------------------------------


} // namespace dfn::world
