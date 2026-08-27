/*
Created: 21:08:2026 - 00:40:00
Last updated: 27:08:2026 - 22:25:00
Module: engine/world
File: engine/world/sources/HouseStairs.cpp

Responsibility:
- ЛЕСТНИЧНЫЙ МАРШ: ступени считаются из высоты (ровный шаг), коробы в
  коллайдер, тетивы; марш на четырёх точках; открытые доски и каменные
  блоки с зазорами.

Key items:
- build_stairs / build_stairs_contour; подступёнок (rise) и линия пандуса (nosing).

Dependencies:
- Uses: HouseMeshDetail.h
- Used by: сборка build_house_mesh (HouseMesh.cpp) и соседние модули постройки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
- ПО ФАЙЛУ НА АЛГОРИТМ (решение пользователя 21.08): модуль держит ОДИН
  алгоритм постройки; общие руки — в HouseMeshDetail.h.
*/
/*
UPD:
- 21:08:2026 - 00:40:00: Вырезан из HouseMesh.cpp (1942 строки, девять алгоритмов в одном файле).
- 21:08:2026 - 01:35:00: РАМПА-ОСНОВАНИЕ под открытыми ступенями (open>0.5).
  Капсула (R 0.35) шире проступи (0.35 при подъёме 6/12): на 60 Гц она за кадр
  смещается на 3 см, успевает осесть между кромками соседних плит, и следующий
  подступёнок оказывается выше лимита шага — персонаж намертво садится на
  ребро (бот Вайтрана: три прогона подряд стоп в одной точке при ЛЮБОМ
  положении марша; репродукция голой капсулой в Jolt: DT=0.1 проходит,
  DT=1/60 застревает). Непрерывная наклонная опора в 2 см под линией марша
  не даёт осесть глубже; она же — ответ на жалобу «с лестницы скатываюсь,
  не стою ровно на рёбрах» (бэклог 20.08). Судья и 92 теста зелёные.
- 21:08:2026 - 13:30:00: КЛИНЬЯ ВМЕСТО ПЛИТ у открытых ступеней (open=1 и
  open=2): передняя грань наклонена под уклон марша. Рампы оказалось мало —
  живая char-трасса бота на городском марше показала вечный OnSteepGround с
  нормалью (0,0,1): капсула стоит на КРОМКЕ плиты (нормаль 14 гр., walkable,
  WalkStairs не активируется), а любое движение и прыжок гасятся о череду
  вертикальных подступёнков. У клина подступёнок — фаска 26 гр. (walkable):
  капсула скользит на следующую проступь. Ступень с фаской — честная форма
  стёртой лестницы: картинка и коллайдер остаются одним описанием.
- 21:08:2026 - 13:55:00: И ФАСОК МАЛО: капсула зажимается в V-канаве «кромка
  проступи + фаска следующего блока» (char-трасса: контакт с фаской есть —
  нормаль (0, 0.89, 0.45), — а движения нет). Открытая лестница для капсулы
  непроходима в принципе; физикой марш теперь НЕВИДИМЫЙ ПАНДУС по линии
  передних кромок (MeshPart.collider_only — симметрия mb.collider=false),
  картинка — прежние ступени. Стандартное решение индустрии; 92/92, судья
  ходит по маршам домов по пандусу.
- 27:08:2026 - 01:55:00: ПОДСТУПЁНОК СТАЛ СВОЙСТВОМ МАРША (rise), А ПАНДУС —
  ВЫБОРОМ ЛИНИИ (nosing), и оба заведены критом владельца: «лестницы стоят
  криво, прямо у входа и надо их перепрыгивать; слишком длинные они, должны
  быть круче». ЗАМЕР по выпечке (dfn_stairs_check, новый прибор): у ВСЕХ
  открытых маршей полки ходимая поверхность пандуса выходила на ПОДСТУПЁНОК
  выше обоих концов — «на входе +0.155, на выходе −0.155» в доме Житнова.
  Это и есть то, через что перепрыгивают, и это НЕ дефект уличных маршей:
  их генератор города сажает ровно с этой поправкой. Отсюда не смена
  умолчания, а свойство. Внутридомовые объявляют rise=0.28 и nosing=1.
  Плюс НИЖНЯЯ ГРАНИЦА ЧИСЛА СТУПЕНЕЙ по PLAYER_STEP_HEIGHT: с ростом
  подступёнка округление ВНИЗ на коротком марше стало способно выдать
  ступень выше той, на которую контроллер поднимается сам.
- 27:08:2026 - 22:25:00: ПОДСТУПЁНОК-ЩИТ (riser=1) У ОТКРЫТОГО МАРША. Крит владельца по
  кадрам аудита (docs/reports/big-audit-28-08.html, раздел «Лестницы»):
  «проступи — тонкие доски с просветами, подступёнков нет, перил нет, площадки
  нет — это ПОЖАРНЫЙ ТРАП, а не лестница жилого дома». Прибор этого не видел ни
  одной из пяти рук: красота не мерилась ничем. Щит закрывает просвет между
  проступями (низ — верх предыдущей доски, верх — изнанка своей, отступ 0.03
  назад от носка, чтобы свес остался читаемым).
  ПРИЗНАКОМ, А НЕ УМОЛЧАНИЕМ: у open=2 зазоры между каменными блоками —
  эталонная форма уличного марша (EXTERIOR_CATALOG.md, image copy 12), и щит
  там запрещён; у open=0 подступёнок уже есть телом короба.
  ЩИТ НЕ ИДЁТ В КОЛЛАЙДЕР, и довод — запись 21.08 двумя абзацами выше: ровно
  эта форма (вертикаль поперёк хода) и есть та ловушка капсулы, ради обхода
  которой положен невидимый пандус. Картинка получает лестницу, физика остаётся
  бит-в-бит прежней.
*/

#include "engine/world/sources/HouseMeshDetail.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>

namespace dfn::world {

namespace {


} // namespace

inline constexpr float HOUSE_STRINGER_BAND_M = 0.26f; ///< высота тетивы
inline constexpr float HOUSE_STRINGER_TH_M = 0.045f;  ///< толщина тетивы

void build_stairs(const Element& e, const ElementParams& p, glm::vec3 a, glm::vec3 b,
                         float half_w, MeshBuilder& mb, HouseMesh& mesh) {
    // Низ — тот якорь, что ниже: лестницу можно тянуть в обе стороны.
    if (b.y < a.y) {
        std::swap(a, b);
    }
    const glm::vec3 d = b - a;
    const float run = std::sqrt(d.x * d.x + d.z * d.z);
    const float rise_total = d.y;
    if (run < HOUSE_GEOM_EPS || rise_total < HOUSE_GEOM_EPS) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate, run,
                                 "лестнице нужны и длина, и высота: тяни к якорю выше"});
        return;
    }
    const glm::vec3 dir{d.x / run, 0.0f, d.z / run};
    const glm::vec3 side = glm::normalize(glm::cross(dir, glm::vec3{0.0f, 1.0f, 0.0f}));
    // ЧИСЛО СТУПЕНЕЙ: округление до целого от «подъём / подступёнок», где
    // подступёнок — свой у марша (p.rise) либо жилой по умолчанию.
    //
    // НИЖНЯЯ ГРАНИЦА ВЫВЕДЕНА, А НЕ НАЗНАЧЕНА: округление ВНИЗ на коротком
    // марше поднимает фактический подступёнок выше заказанного (при одной
    // ступени — вдвое), и он может перевалить `PLAYER_STEP_HEIGHT` — высоту,
    // на которую контроллер поднимается сам. Ступень выше неё непроходима
    // ровно так же, как стена: закрывать это глазами нечем, и потому число
    // ступеней снизу связано ТЕМ ЖЕ порогом, а не надеждой на округление.
    const float step_m = p.rise > HOUSE_GEOM_EPS ? p.rise : HOUSE_STAIR_RISE_M;
    const int by_rise = static_cast<int>(std::round(rise_total / step_m));
    const int by_step = static_cast<int>(
        std::ceil(rise_total / static_cast<float>(dfn::config::PLAYER_STEP_HEIGHT)
                  - 1e-4f));
    const int steps = std::max({1, by_rise, by_step});
    const float rise = rise_total / static_cast<float>(steps);
    const float tread = run / static_cast<float>(steps);
    const std::uint32_t quad[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < steps; ++i) {
        const glm::vec3 base = a + dir * (tread * static_cast<float>(i))
                             + glm::vec3{0.0f, rise * static_cast<float>(i), 0.0f};
        if (p.open > 1.5f) {
            // СТУПЕНЬ ИЗ КАМЕННЫХ БЛОКОВ (EXTERIOR_CATALOG.md, image copy 12):
            // 2-3 блока по ширине, зазор 2-4 см, каждый блок дышит высотой и
            // глубиной; при износе блок может ВЫВАЛИТЬСЯ (один из пяти).
            const int nblocks = half_w > 0.55f ? 3 : 2;
            const float gap_b = 0.03f;
            const float bw = (half_w * 2.0f - gap_b * (nblocks - 1))
                           / static_cast<float>(nblocks);
            const glm::vec3 top = base + glm::vec3{0.0f, rise, 0.0f};
            for (int bkk = 0; bkk < nblocks; ++bkk) {
                if (p.wear > 0.0f
                    && course_jitter(i * 29 + bkk * 7, 11) < p.wear * 0.2f) {
                    continue; // вывалившийся блок — читается провалом ступени
                }
                const float j_h = 0.012f * (course_jitter(i * 3 + bkk, 19) - 0.5f);
                const float j_d = 0.03f * course_jitter(bkk * 5 + 1, i * 7 + 2);
                const glm::vec3 s0 = top - side * half_w
                                   + side * ((bw + gap_b) * static_cast<float>(bkk))
                                   + glm::vec3{0.0f, j_h, 0.0f};
                const glm::vec3 loop[4] = {
                    s0 - dir * (0.02f + j_d), s0 + side * bw - dir * (0.02f + j_d),
                    s0 + side * bw + dir * (tread + 0.02f), s0 + dir * (tread + 0.02f)};
                // КЛИН, А НЕ ПЛИТА: низ блока смещён вниз по маршу, передняя
                // грань наклонена под уклон марша. Вертикальный подступёнок —
                // ловушка капсулы: char-трасса бота (21.08) показала вечный
                // OnSteepGround с нормалью (0,0,1) — контроллер липнет к
                // череде вертикальных граней и гасит горизонталь даже в
                // прыжке. Фаска 26 гр. walkable — капсула скользит на
                // следующую проступь. Ступень с фаской — честная форма
                // стёртой каменной лестницы, картинка и физика совпадают.
                const float chamfer = 0.16f * tread / rise;
                glm::vec3 sunk[4];
                for (int k = 0; k < 4; ++k) {
                    sunk[k] = loop[k] - glm::vec3{0.0f, 0.16f, 0.0f}
                            - dir * chamfer;
                }
                push_prism(mb, sunk, quad,
                           dir * chamfer + glm::vec3{0.0f, 0.16f, 0.0f},
                           p.tex_deg, mesh, e.id);
            }
            continue;
        }
        if (p.open > 0.5f) {
            // ОТКРЫТАЯ СТУПЕНЬ (20.08): доска на тетивах, под ней воздух.
            // Нос выступает на 3 см; износ грызёт края — ширина дышит.
            const float bite =
                p.wear * 0.06f * course_jitter(i * 13 + 7, static_cast<int>(half_w * 10));
            const float w0 = half_w - bite;
            const glm::vec3 top = base + glm::vec3{0.0f, rise, 0.0f};
            const glm::vec3 loop[4] = {top - side * w0 - dir * 0.03f,
                                       top + side * w0 - dir * 0.03f,
                                       top + side * w0 + dir * (tread + 0.02f),
                                       top - side * w0 + dir * (tread + 0.02f)};
            // Тот же клин, что у каменных блоков ниже: вертикальный торец
            // доски — та же ловушка капсулы, только высотой 5 см.
            const float chamfer = 0.05f * tread / rise;
            glm::vec3 sunk[4];
            for (int k = 0; k < 4; ++k) {
                sunk[k] = loop[k] - glm::vec3{0.0f, 0.05f, 0.0f} - dir * chamfer;
            }
            push_prism(mb, sunk, quad,
                       dir * chamfer + glm::vec3{0.0f, 0.05f, 0.0f}, p.tex_deg,
                       mesh, e.id);
            // ПОДСТУПЁНОК-ЩИТ (riser=1). Закрывает просвет между проступью i-1
            // и доской проступи i: низ на верхе предыдущей доски, верх — в
            // изнанку своей. Отступ 0.03 назад от носка оставляет свес, по
            // которому лестница читается лестницей, а не наклонной плитой.
            // ПРИ open=2 ЩИТА НЕТ И НЕ БУДЕТ: зазоры между каменными блоками —
            // эталонная форма уличного марша, а не дефект (см. HouseMesh.h).
            //
            // ЩИТ — КОСМЕТИКА, И ЭТО НЕ ЛЕНЬ, А ТА ЖЕ ЧАР-ТРАССА. Ровно эту
            // форму — ВЕРТИКАЛЬНУЮ ГРАНЬ ПОПЕРЁК ХОДА — три раунда трассы
            // бота 21.08 назвали ловушкой капсулы (вечный OnSteepGround с
            // нормалью (0,0,1)); из-за неё марш и ходится по невидимому
            // пандусу. Вернуть её В КОЛЛАЙДЕР значило бы вернуть дефект,
            // ради обхода которого пандус и положен. Тела здесь и так нет
            // ничего: щит стоит ВНУТРИ объёма марша, между проступью снизу
            // и доской сверху, и заслонять ему нечего.
            if (p.riser > 0.5f) {
                const float band = rise - 0.05f;
                if (band > HOUSE_GEOM_EPS) {
                    const float rw = std::max(w0 - 0.01f, 0.05f);
                    const glm::vec3 face[4] = {base - side * rw, base + side * rw,
                                               base + side * rw + dir * 0.04f,
                                               base - side * rw + dir * 0.04f};
                    const bool was = mb.collider;
                    mb.collider = false;
                    push_prism(mb, face, quad, glm::vec3{0.0f, band, 0.0f},
                               p.tex_deg, mesh, e.id);
                    mb.collider = was;
                }
            }
            continue;
        }
        // Короб ступени: от её пола до её верха, глубиной в одну проступь.
        const glm::vec3 loop[4] = {base - side * half_w, base + side * half_w,
                                   base + side * half_w + dir * tread,
                                   base - side * half_w + dir * tread};
        push_prism(mb, loop, quad, glm::vec3{0.0f, rise, 0.0f}, p.tex_deg, mesh, e.id);
    }
    // НЕВИДИМЫЙ ПАНДУС ПО ЛИНИИ ПЕРЕДНИХ КРОМОК. Открытые ступени для капсулы
    // непроходимы в принципе: она стоит на кромке проступи (нормаль ~14°,
    // walkable — WalkStairs не просыпается), а любое движение гасится о
    // подступёнок следующего блока; наклонные фаски вместо подступёнков
    // зажимают её в V-канаве «кромка + фаска» (три раунда char-трассы бота
    // Вайтрана, 21.08). Гладкая наклонная плоскость через верхние кромки —
    // стандартное решение индустрии; она ЧИСТО ФИЗИЧЕСКАЯ (collider_only):
    // картинка остаётся честными ступенями, ходьба идёт по пандусу 26°.
    //
    // ПО ЧЕМУ ИМЕННО ИДЁТ ПАНДУС — ЭТО ВТОРОЕ РЕШЕНИЕ, И ОНО НЕ ОДНО НА ВСЕХ
    // (правка 27.08, крит владельца «надо их перепрыгивать, чтобы пройти»).
    // Честных линий ровно две:
    //   ЗАДНИЕ ВЕРХНИЕ УГЛЫ ступеней (nosing=0, наследие): нога никогда не
    //     утопает в нарисованной ступени, но ходимая поверхность выходит на
    //     ПОДСТУПЁНОК выше и пола внизу, и площадки наверху. У уличного марша
    //     эту ступеньку снимает посадка: генератор города опускает якорь на
    //     «подступёнок − 0.02» с обоих концов и держит рядом таблицу поправок
    //     на каждый рецепт (tools/gen_city.py). Поэтому умолчание остаётся
    //     этим — смена сдвинула бы каждый уличный марш обоих городов молча.
    //   НОСКИ (nosing=1): пандус приходит ВРОВЕНЬ с полом и с площадкой, а
    //     нога утопает в ступени не глубже полуподступёнка. Внутри дома
    //     правильна только эта: там марш стоит у самой двери, и ступенька в
    //     подступёнок на входе — ровно то, через что владелец перепрыгивал.
    if (p.open > 0.5f) {
        mb.flush_part();
        mb.collider_only = true;
        const glm::vec3 lift{0.0f, p.nosing > 0.5f ? -0.16f : rise - 0.18f, 0.0f};
        const glm::vec3 lo = a + lift;
        const glm::vec3 hi = a + dir * run + glm::vec3{0.0f, rise_total, 0.0f} + lift;
        const glm::vec3 loop[4] = {lo - side * half_w, lo + side * half_w,
                                   hi + side * half_w, hi - side * half_w};
        push_prism(mb, loop, quad, glm::vec3{0.0f, 0.16f, 0.0f}, p.tex_deg, mesh,
                   e.id);
        mb.flush_part();
        mb.collider_only = false;
    }
    // ТЕТИВЫ: наклонные плиты по бокам, полосой вниз от линии марша.
    for (const float s_side : {-1.0f, 1.0f}) {
        const glm::vec3 off = side * (half_w * s_side);
        const glm::vec3 lo0 = a + off;
        const glm::vec3 hi0 = a + off + dir * run + glm::vec3{0.0f, rise_total, 0.0f};
        const glm::vec3 band{0.0f, -HOUSE_STRINGER_BAND_M, 0.0f};
        const glm::vec3 loop[4] = {lo0 + band, lo0, hi0, hi0 + band};
        // Наружу от марша, толщиной тетивы.
        glm::vec3 out_n = side * s_side;
        std::uint32_t tris[6] = {0, 1, 2, 0, 2, 3};
        if (s_side < 0.0f) {
            std::swap(tris[1], tris[2]);
            std::swap(tris[4], tris[5]);
        }
        push_prism(mb, loop, tris, out_n * HOUSE_STRINGER_TH_M, p.tex_deg, mesh, e.id);
    }
}

/// ЛЕСТНИЦА НА ЧЕТЫРЁХ ТОЧКАХ (правка 20.08: «лестница же на 4 точках
/// держится» — она деталь раздела стен, а не форма палки). Две нижние вершины
/// контура — ширина марша и его начало, две верхние — куда он приходит; всё
/// остальное считает тот же построитель, что и раньше: число ступеней из
/// высоты, ровный шаг, коробы-ступени в коллайдер, тетивы по бокам.
void build_stairs_contour(const Element& e, const ElementParams& p,
                                 std::span<const glm::vec3> pts, MeshBuilder& mb,
                                 HouseMesh& mesh) {
    if (pts.size() < 4) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate,
                                 static_cast<float>(pts.size()),
                                 "лестнице нужны четыре точки: две внизу, две наверху"});
        return;
    }
    std::vector<std::size_t> order(pts.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(),
              [&](std::size_t l, std::size_t r) { return pts[l].y < pts[r].y; });
    if (pts.size() != 4) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate,
                                 static_cast<float>(pts.size()),
                                 "лестница считает по четырём точкам: лишние не участвуют"});
    }
    const glm::vec3& lo0 = pts[order[0]];
    const glm::vec3& lo1 = pts[order[1]];
    const glm::vec3& hi0 = pts[order[order.size() - 1]];
    const glm::vec3& hi1 = pts[order[order.size() - 2]];
    const glm::vec3 a = (lo0 + lo1) * 0.5f;
    const glm::vec3 b = (hi0 + hi1) * 0.5f;
    const float half_w = std::max(glm::length(lo1 - lo0) * 0.5f, 0.15f);
    build_stairs(e, p, a, b, half_w, mb, mesh);
}


} // namespace dfn::world
