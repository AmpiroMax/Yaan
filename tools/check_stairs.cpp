/*
Created: 27:08:2026 - 01:20:00
Last updated: 27:08:2026 - 23:05:00
Module: tools
File: tools/check_stairs.cpp

Responsibility:
- dfn_stairs_check: СУДЬЯ ВНУТРИДОМОВОГО МАРША. Три крита владельца (27.08)
  меряются по ВЫПЕЧКЕ, а не по коду рецепта:
    рука 1 — ПРОХОД ОТ ДВЕРИ: свободная полоса от дверного проёма вглубь дома
             в поясе от высоты шага до макушки; отдельно называется, если
             полосу занимает ТЕЛО МАРША;
    рука 2 — УКЛОН: подступёнок, проступь, угол, ход и подъём каждого марша;
    рука 3 — ПРОСВЕТ НАД ГОЛОВОЙ: капсула ставится на НОСОК каждой ступени
             (HOUSES.md §9.3) и меряется, что над ней;
    рука 4 — МАРШ ВНУТРИ ОБОЛОЧКИ: пробы марша против выпуклых тел чужих
             элементов — марш, вышедший сквозь стену наружу, называется числом;
    рука 5 — СТЫК ПАНДУСА: ходимая поверхность невидимого пандуса против пола
             внизу и настила наверху (ступенька на входе и на выходе);
    рука 6 — МИНИМАЛЬНАЯ ПРОСТУПЬ: ступень, на которую не ставится нога
             (HOUSES.md §9.6, порог 2R/3 из радиуса капсулы);
    рука 7 — НИЖНЯЯ ГРАНИЦА УКЛОНА: марш положе половины проходимой крутизны —
             это пандус со ступеньками, он съедает вдвое больше пола, чем
             стоило бы (HOUSES.md §9.6, крит владельца «слишком пологие»);
    рука 8 — ПО МАРШУ МОЖНО ПОДНЯТЬСЯ: капсула ПРОГОНОМ идёт по невидимому
             пандусу от пола внизу до площадки наверху, с замером фактического
             подъёма и просвета в каждой точке пути.

Usage:
    dfn_stairs_check <файл.dfh | каталог> [...] [--all] [--tol-head 1.9]
                     [--known <список.txt>] [--expect N|КЛАСС:N ...]

  Запускать из корня репозитория. Ненулевой выход при находках.
  Без --all печатаются только тела С НАХОДКАМИ; с --all — все марши.
  --known списывает перечисленные там находки в «известные» (см.
  tools/known_findings.h); --expect — оснастка контрольной руки.

Dependencies:
- Uses: engine/world (HouseGraph, HouseFile, HouseMesh), engine/core (Constants).
- Used by: волна лестниц 27.08, человек, будущая приёмка кузницы.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- СУДЬЯ НЕ ЗОВЁТ КАЛЬКУЛЯТОР (HOUSES.md §9.3). Формулы длины проёма здесь нет
  и быть не может: две независимые дороги к одному числу — единственное, что
  способно поймать ошибку в любой из них. Всё, что печатает этот файл,
  получено из ТРЕУГОЛЬНИКОВ и ВЫПУКЛЫХ ТЕЛ собранного меша.
- МЕРИТСЯ ОТРЕЗКАМИ, А НЕ ГАБАРИТНЫМИ ЯЩИКАМИ (§14.1): ящик наклонной тетивы
  накрывает весь марш, и первая редакция такого судьи краснела на КАЖДОМ
  исправном марше.
- МЕРКИ ГЕРОЯ — ИЗ КОНСТАНТ. В день, когда герой подрастёт, дом обязан
  перестать проходить, а не остаться зелёным.
*/
/*
UPD:
- 27:08:2026 - 01:20:00: Создан. Волна лестниц: три крита владельца («марш у
- 27:08:2026 - 11:20:22: координатор: полоса прохода мерит высоту тела В ТОЧКЕ (барицентрика), а не ящиком треугольника — длинный наклонный подкос давал ложный блок; полотно двери исключено и из ПРОСВЕТА (створка открывается, потолком не является); в «УЗКО» печатается пол y0.
  входа», «слишком полого и длинно», «голова в потолок») ни одного прибора не
  имели — приёмка прошлых заходов печатала просвет по ПЛИТАМ, а бьёт не плита.
- 27:08:2026 - 23:05:00: ТРИ НОВЫЕ РУКИ И МЕСТО В ctest (аудит «Большой мир», раздел
  «лестницы: прибор против глаза», задачи 1/3/5). Аудит назвал пять слепых
  пятен; три из них закрыты здесь: у уклона не было НИЖНЕЙ границы (половина
  крита владельца «слишком длинные и пологие» не могла быть поймана по
  построению), не было МИНИМАЛЬНОЙ ПРОСТУПИ (0.161 м постоялого двора —
  стремянка, засчитанная как лестница), и ни один прогон не предъявлял
  утверждения «по маршу можно ПОДНЯТЬСЯ» — рука 8 гонит капсулу по тому же
  невидимому пандусу, по которому ходит игрок, и печатает фактический подъём.
  Плюс каждая находка получила КЛАСС, а судья — список известных и оснастку
  ожиданий: без класса ни ctest по всей полке, ни контрольная рука на
  настоящем отвергнутом случае не собираются.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"
#include "tools/known_findings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/geometric.hpp>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using dfn::world::ConvexPart;
using dfn::world::Element;
using dfn::world::ElementId;
using dfn::world::ElementParams;
using dfn::world::HouseGraph;
using dfn::world::HouseMesh;

// МЕРКИ ГЕРОЯ — из констант, не из головы.
constexpr float CAP_R = static_cast<float>(dfn::config::PLAYER_CAPSULE_RADIUS);
constexpr float CAP_H = static_cast<float>(dfn::config::PLAYER_CAPSULE_HEIGHT);
constexpr float STEP_H = static_cast<float>(dfn::config::PLAYER_STEP_HEIGHT);
constexpr float MAX_SLOPE = static_cast<float>(dfn::config::PLAYER_MAX_SLOPE);

/// ТРЕБУЕМЫЙ ПРОСВЕТ НАД МАРШЕМ, м. Крит владельца 27.08 дословно: «нельзя
/// пройти по лестнице — сверху в потолок упираешься». 1.90 — рост капсулы 1.80
/// плюс 0.10 запаса: порог, стоящий НА РАЗРЕШЕНИИ, запаса не имеет.
float g_head_tol = 1.90f;
/// ТРЕБУЕМАЯ ШИРИНА ПРОХОДА ОТ ДВЕРИ, м (крит 1: «надо их перепрыгивать»).
constexpr float PASS_W = 0.80f;
/// НАСКОЛЬКО ВГЛУБЬ ДОМА МЕРИТСЯ ЭТОТ ПРОХОД, м.
constexpr float PASS_LEN = 3.0f;

/// МИНИМАЛЬНАЯ ПРОСТУПЬ, м = 2R/3 (рука 6). Выведена из ОПОРЫ ГЕРОЯ, а не из
/// вкуса: капсула стоит на диске диаметром 2R = 0.70. Ступень, по которой
/// ИДУТ, обязана нести этот диск не более чем на ТРЁХ проступях подряд — при
/// 2R/G > 3 нога не ставится на ступень, а перекрывает пачку реек. Это и есть
/// стремянка, и именно так выглядит настоящий отвергнутый случай: постоялый
/// двор Житнова, e116, проступь 0.161 при капсуле 0.35 (аудит «Большой мир»).
///
/// ЗАМЕРЕННОЕ РАЗДЕЛЕНИЕ (правило 30: порог стоит ВЫШЕ настоящего отвергнутого
/// и НИЖЕ настоящего принятого). По всем 194 телам полки 27.08 вечера: отвергнутые —
/// 0.161 (cornhall-inn e116) и 0.189 (frame-replica/stone-replica, демо-стенд);
/// ближайший принятый — 0.250 (city-steps6-h14r2). Порог 0.233 лежит в этом
/// разрыве: запас 0.044 над отвергнутым, 0.017 под принятым.
constexpr float TREAD_MIN = 2.0f * CAP_R / 3.0f;

/// НИЖНЯЯ ГРАНИЦА УКЛОНА (рука 7) — ПОЛОВИНА ПРОХОДИМОЙ КРУТИЗНЫ, и меряется
/// она ТАНГЕНСОМ, а не градусами. Крутизна в движке — это уклон поверхности
/// (rise/run), контроллер судит именно его (PlayerMovementWorld:
/// max_slope_radians), и половина ГРАДУСА (24.9°) не отделяет ничего.
///
/// ПОЧЕМУ ИМЕННО ПОЛОВИНА. Вдвое меньший уклон — это ВДВОЕ БОЛЬШЕ ПОЛА на тот
/// же подъём: на пределе проходимого метр подъёма стоит 0.84 м пола, при
/// половине — 1.69 м. Крит владельца 27.08 звучит дословно «слишком ДЛИННЫЕ и
/// пологие»; «вдвое длиннее, чем позволяет единственная крутизна, которую
/// герой вообще знает» — это то же самое, сказанное числом.
///
/// ЗАМЕРЕННОЕ РАЗДЕЛЕНИЕ по 77 маршам полки 27.08 вечера (только ВНУТРИДОМОВЫЕ, см.
/// has_walls ниже): отвергнутые владельцем — 26.6° (city-keep-s e5, city-mill
/// e12, city-house-l e32, уклон ровно 1:2) и названные аудитом 28.6°
/// (cornhall-granary e42), 29.1° (cornhall-house-s-old e78); принятые —
/// 33.9° (cornhall-windmill e35), 42.1°, 45.0° (перепечка волны 27.08).
/// Порог 30.6° лежит в разрыве: запас 1.5° над худшим отвергнутым, 3.3° под
/// ближайшим принятым.
const float GRADE_MAX = std::tan(MAX_SLOPE);
const float GRADE_MIN = GRADE_MAX * 0.5f;

/// ПРОСВЕТ ДЛЯ ПРОГОНА (рука 8) — РОСТ КАПСУЛЫ БЕЗ ЗАПАСА. Рука 3 судит
/// ЧЕРТЁЖ и держит 1.90 (рост плюс десятина запаса — порог, стоящий на
/// разрешении, запаса не имеет). Рука 8 отвечает на ДРУГОЙ вопрос — «прошёл
/// или не прошёл», — и на него отвечает ровно рост: 1.79 м просвета это
/// шишка, 1.81 — проход. Запас тут был бы не строгостью, а выдумкой.
constexpr float WALK_HEAD = CAP_H;

/// ШАГ ПРОГОНА ВДОЛЬ МАРША, м. Мельче капсулы и мельче самого узкого тела,
/// которым бьют по голове (балка наката 0.20-0.25): проба радиусом CAP_R,
/// поставленная через 0.10, не может проскочить между двумя балками.
constexpr float WALK_DS = 0.10f;

struct Tri {
    glm::vec3 a, b, c;
    ElementId owner = dfn::world::NO_ELEMENT;
    bool collider_only = false;
};

/// НАХОДКА НЕСЁТ КЛАСС, А НЕ ТОЛЬКО ТЕКСТ. Класс — единственное, что не
/// плывёт, пока параллельная волна двигает геометрию: по нему ведётся список
/// известных находок и по нему контрольная рука доказывает, что покраснела
/// ИМЕННО испытуемая рука, а не соседняя (tools/known_findings.h).
struct Bad {
    std::string cls;
    std::string text;
};

int g_findings = 0;
bool g_all = false;
dfn::tools::KnownFindings g_known;
std::map<std::string, long> g_by_class;
int g_known_hits = 0;

// ---------------------------------------------------------------------------
// Мелкая геометрия
// ---------------------------------------------------------------------------

/// Квадрат расстояния от точки до треугольника В ПЛАНЕ (XZ).
///
/// ВЫРОЖДЕННЫЙ В ПЛАНЕ ТРЕУГОЛЬНИК МЕРИТСЯ ОТРЕЗКОМ, И ЭТО НЕ МЕЛОЧЬ: ЛИЦО
/// ЛЮБОЙ ВЕРТИКАЛЬНОЙ СТЕНЫ — это как раз такой треугольник (все три вершины
/// на одной прямой в плане). Знаковые проверки на нём дают три нуля, «внутри»
/// оказывается ЛЮБАЯ точка мира, и первый же прогон судьи объявил дверной
/// проход занятым во всех домах разом.
[[nodiscard]] float dist2_xz(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c) {
    const auto seg = [](glm::vec2 q, glm::vec2 u, glm::vec2 v) {
        const glm::vec2 d = v - u;
        const float dd = glm::dot(d, d);
        float t = dd > 1e-12f ? glm::dot(q - u, d) / dd : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        const glm::vec2 w = q - (u + d * t);
        return glm::dot(w, w);
    };
    const float area2 = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (std::fabs(area2) < 1e-7f) {
        return std::min({seg(p, a, b), seg(p, b, c), seg(p, c, a)});
    }
    const float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    const float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    const float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    if (!(neg && pos)) {
        return 0.0f;
    }
    return std::min({seg(p, a, b), seg(p, b, c), seg(p, c, a)});
}

/// НАИМЕНЬШАЯ ВЫСОТА ТРЕУГОЛЬНИКА В КРУГЕ радиуса r вокруг xz, считая только
/// то, что ВЫШЕ пола под ногами. Возвращает false, если круг треугольника не
/// задевает или всё его тело ниже ног.
///
/// ОТРЕЗКАМИ, А НЕ ЯЩИКОМ: точки берутся ПО САМОМУ треугольнику (сетка по
/// барицентрике), поэтому наклонная тетива отвечает своей высотой НАД НОСКОМ,
/// а не высотой своего ящика.
///
/// ВЫХОД С МАРША ПОТОЛКОМ НЕ ЯВЛЯЕТСЯ (§14.1). Отсекается он не по элементу, а
/// ПО МЕСТУ: проба, лежащая ДАЛЬШЕ верха марша по ходу и не выше его верха, —
/// это площадка, на которую ступают. Отсев по элементу («плита, чей верх совпал
/// с верхом марша») скрыл бы вместе с площадкой и тот кусок настила, что лёг
/// над НИЖНИМИ ступенями, — то есть ровно искомый дефект.
[[nodiscard]] bool tri_low_above(const Tri& t, glm::vec2 xz, float r, float foot_y,
                                 glm::vec3 foot, glm::vec3 dir, float run, float head_y,
                                 float under, float half_w, float s_min, float& out_y) {
    const float top = std::max({t.a.y, t.b.y, t.c.y});
    if (top <= foot_y + under) {
        return false; // это пол под ногами, а не потолок над головой
    }
    // Дешёвый отсев по плану.
    if (dist2_xz(xz, {t.a.x, t.a.z}, {t.b.x, t.b.z}, {t.c.x, t.c.z}) > r * r) {
        return false;
    }
    constexpr int N = 8;
    bool any = false;
    float best = 1e9f;
    for (int i = 0; i <= N; ++i) {
        for (int j = 0; i + j <= N; ++j) {
            const float u = static_cast<float>(i) / static_cast<float>(N);
            const float v = static_cast<float>(j) / static_cast<float>(N);
            const glm::vec3 p = t.a * (1.0f - u - v) + t.b * u + t.c * v;
            if (p.y <= foot_y + under) {
                continue;
            }
            const float dx = p.x - xz.x;
            const float dz = p.z - xz.y;
            if (dx * dx + dz * dz > r * r) {
                continue;
            }
            const float s = glm::dot(p - foot, dir);
            // …И НАД МАРШЕМ ПО ДЛИНЕ ТОЖЕ, А НЕ ПОЗАДИ ЕГО ПОДНОЖИЯ. Тот же
            // довод, что у полосы вбок, и та же покупка: у city-house-l марш
            // начинается в 0.22 м от изнанки стены, круг капсулы на первой
            // пробе заходит в кладку назад, а в кладке на высоте 1.70 над
            // ступенью лежит перемычка окна. Нависшее ПОЗАДИ подножия — это
            // потолок комнаты, о него бьются, не поднимаясь по лестнице.
            if (s < s_min) {
                continue;
            }
            // ПОТОЛОК МЕРИТСЯ НАД МАРШЕМ, А НЕ НАД ВСЕМ КРУГОМ КАПСУЛЫ.
            // Куплено ложными находками на СТЕНЕ, к которой прижат марш:
            // круг радиуса 0.35 вокруг оси заходит в кладку на десяток
            // сантиметров, а внутри кладки всегда что-нибудь обращено вниз —
            // перемычка окна, изнанка обвязки. Игрок туда не встаёт: раньше
            // головы его останавливает плечо. То, что висит ВНЕ ширины марша,
            // обходят боком; что марш не влез в стену — отдельно судит рука 4.
            // Полуширина, большая длины марша, отключает отсев (рука 3).
            if (std::fabs(glm::dot(p - foot, glm::cross(dir,
                                                        glm::vec3{0.0f, 1.0f, 0.0f})))
                > half_w) {
                continue;
            }
            // ПЛОЩАДКА ВЫХОДА И ЕЁ КОНСТРУКЦИЯ — НЕ ПОТОЛОК, и полоса отсева
            // равна РАДИУСУ КАПСУЛЫ, потому что этого требует её форма, а не
            // снисходительность. Капсула — цилиндр: у самой кромки проёма она
            // ВСЕГДА заходит под площадку на свой радиус, у любой исправной
            // лестницы мира, — а под площадкой законно висят и ригель обвязки
            // (0.22 ниже изнанки), и накат (0.25). Носок ступени НИЖЕ них
            // ровно потому, что человек ещё не поднялся; это перешагивают, а
            // не бьются об это головой. Мерить тут — значит краснеть на
            // предпоследней ступени КАЖДОГО марша (что первый прогон и делал).
            // Запас по высоте — PLAYER_STEP_HEIGHT, и это не поблажка, а та же
            // константа контроллера: то, что в зоне выхода лежит НЕ ВЫШЕ ОДНОГО
            // ШАГА над верхом марша, — это следующая ступень, на которую
            // поднимаются, а не потолок, в который упираются. Так устроено
            // всякое крыльцо пакета: марш до 0.39, плита площадки 0.45,
            // каменная ступень порога 0.54, пол 0.57.
            if (s > run - r && p.y <= head_y + STEP_H) {
                continue;
            }
            if (p.y < best) {
                best = p.y;
                any = true;
            }
        }
    }
    out_y = best;
    return any;
}

/// ВЫСОТА ИЗНАНКИ НАД ТОЧКОЙ — ТОЧНО, А НЕ ПО СЕТКЕ. Отдельная функция, а не
/// tri_low_above, и причина замеренная: та сеет по треугольнику 45 проб
/// БАРИЦЕНТРИЧЕСКИ, то есть шаг сетки равен размеру треугольника, делённому
/// на восемь. Для ступени 0.25 м это 3 см, для НАСТИЛА КОМНАТЫ 4x4 м — полметра,
/// и круг капсулы радиусом 0.35 проваливается между пробами целиком. Контрольная
/// фикстура lowbeam.dfh (настил заведён над нижними ступенями) проходила прогон
/// «поднялся 1.75 из 1.75» при просвете 0.93 — прибор не видел плиту у себя над
/// головой, потому что не попал в неё ни одной точкой.
///
/// Здесь берётся ТА ЖЕ плоскость треугольника, но в двух честных точках: под
/// самой осью, если ось внутри треугольника, и в ближайшей к оси точке его
/// границы. Для горизонтальной плиты это точный ответ, для наклонной — с
/// точностью до наклона внутри круга.
[[nodiscard]] bool tri_height_near(const Tri& t, glm::vec2 q, float r, glm::vec3& out) {
    const glm::vec2 a{t.a.x, t.a.z};
    const glm::vec2 b{t.b.x, t.b.z};
    const glm::vec2 c{t.c.x, t.c.z};
    const float area2 = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (std::fabs(area2) < 1e-7f) {
        return false; // вырожденный в плане: это лицо стены, не изнанка
    }
    if (dist2_xz(q, a, b, c) > r * r) {
        return false;
    }
    // Ближайшая к оси точка треугольника В ПЛАНЕ: сама ось, если она внутри,
    // иначе ближайшая точка границы.
    glm::vec2 best = q;
    if (dist2_xz(q, a, b, c) > 1e-9f) {
        float bd = 1e9f;
        const glm::vec2 pts[3] = {a, b, c};
        for (int i = 0; i < 3; ++i) {
            const glm::vec2 u = pts[i];
            const glm::vec2 v = pts[(i + 1) % 3];
            const glm::vec2 d = v - u;
            const float dd = glm::dot(d, d);
            float tt = dd > 1e-12f ? glm::dot(q - u, d) / dd : 0.0f;
            tt = std::clamp(tt, 0.0f, 1.0f);
            const glm::vec2 w = u + d * tt;
            const float dist = glm::dot(q - w, q - w);
            if (dist < bd) {
                bd = dist;
                best = w;
            }
        }
    }
    const float w0 = ((b.x - best.x) * (c.y - best.y)
                    - (c.x - best.x) * (b.y - best.y)) / area2;
    const float w1 = ((c.x - best.x) * (a.y - best.y)
                    - (a.x - best.x) * (c.y - best.y)) / area2;
    out = glm::vec3{best.x, t.a.y * w0 + t.b.y * w1 + t.c.y * (1.0f - w0 - w1),
                    best.y};
    return true;
}

/// Точка ВНУТРИ выпуклой призмы коллайдера (шесть точек: низ 0-2, верх 3-5).
/// Глубина захода — расстояние до ближайшей грани, 0 снаружи.
[[nodiscard]] float convex_depth(const ConvexPart& cp, glm::vec3 p) {
    if (cp.points.size() != 6) {
        return 0.0f;
    }
    glm::vec3 mid{0.0f};
    for (const glm::vec3& q : cp.points) {
        mid += q;
    }
    mid /= 6.0f;
    const int face[5][3] = {{0, 1, 2}, {3, 4, 5}, {0, 1, 4}, {1, 2, 5}, {2, 0, 3}};
    float depth = 1e9f;
    for (const auto& fq : face) {
        const glm::vec3& a = cp.points[static_cast<std::size_t>(fq[0])];
        const glm::vec3& b = cp.points[static_cast<std::size_t>(fq[1])];
        const glm::vec3& c = cp.points[static_cast<std::size_t>(fq[2])];
        glm::vec3 n = glm::cross(b - a, c - a);
        const float len = glm::length(n);
        if (len < 1e-6f) {
            continue;
        }
        n /= len;
        if (glm::dot(mid - a, n) < 0.0f) {
            n = -n;
        }
        const float d = glm::dot(p - a, n);
        if (d <= 0.0f) {
            return 0.0f;
        }
        depth = std::min(depth, d);
    }
    return depth < 1e8f ? depth : 0.0f;
}

// ---------------------------------------------------------------------------
// Разбор одного дома
// ---------------------------------------------------------------------------

struct Flight {
    ElementId id = dfn::world::NO_ELEMENT;
    glm::vec3 foot{0.0f};  ///< середина нижней кромки
    glm::vec3 head{0.0f};  ///< середина верхней кромки
    glm::vec3 dir{0.0f};   ///< единичный, в плане
    float half_w = 0.0f;
    float run = 0.0f;
    float rise_total = 0.0f;
    int steps = 0;
    bool open = false;
};

[[nodiscard]] bool read_graph(const std::string& path, HouseGraph& g) {
    std::ifstream in(path);
    if (!in.good()) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return dfn::world::read_house(ss.str(), g).ok;
}

/// ЧИСЛО СТУПЕНЕЙ СУДЬЯ НЕ СЧИТАЕТ, А ЧИТАЕТ ПО МЕШУ: он берёт РАЗНЫЕ высоты
/// ГОРИЗОНТАЛЬНЫХ верхних граней этого элемента. Формулу движка он повторять не
/// имеет права (§9.3) — иначе обе дороги ошибутся одинаково.
///
/// «ГОРИЗОНТАЛЬНЫХ» КУПЛЕНО ПЕРВЫМ ПРОГОНОМ: без него в счёт шли лица ТЕТИВ —
/// наклонные полосы во всю длину марша, у которых верхняя высота у каждого
/// треугольника своя. Марш из пятнадцати ступеней объявлялся тридцатидвух-
/// ступенчатым, и вместе с числом уезжали проступь, подступёнок и угол.
[[nodiscard]] int steps_from_mesh(const std::vector<Tri>& own, float y_lo, float y_hi) {
    std::set<int> tops;
    for (const Tri& t : own) {
        if (t.collider_only) {
            continue;
        }
        const glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
        const float len = glm::length(n);
        if (len < 1e-8f || n.y / len < 0.98f) {
            continue; // не проступь: подступёнок, фаска, тетива, ИЗНАНКА доски
        }
        const float y = std::max({t.a.y, t.b.y, t.c.y});
        if (y < y_lo - 0.01f || y > y_hi + 0.01f) {
            continue;
        }
        tops.insert(static_cast<int>(std::lround(y * 1000.0f)));
    }
    // Уровни, отстоящие меньше чем на 2 см, — одна ступень (дрожь износа).
    int n = 0;
    int prev = -1000000;
    for (const int v : tops) {
        if (v - prev > 20) {
            ++n;
        }
        prev = v;
    }
    return n;
}

void check_file(const std::string& path) {
    HouseGraph g;
    if (!read_graph(path, g)) {
        std::printf("%s: НЕ ПРОЧИТАЛСЯ\n", path.c_str());
        ++g_findings;
        return;
    }
    const HouseMesh built = dfn::world::build_house_mesh(g);

    std::vector<Tri> tris;
    for (const dfn::world::MeshPart& part : built.parts) {
        for (std::uint32_t i = 0; i + 2 < part.index_count; i += 3) {
            Tri t;
            t.a = built.vertices[built.indices[part.index_begin + i]].pos;
            t.b = built.vertices[built.indices[part.index_begin + i + 1]].pos;
            t.c = built.vertices[built.indices[part.index_begin + i + 2]].pos;
            t.owner = part.element;
            t.collider_only = part.collider_only;
            tris.push_back(t);
        }
    }
    if (tris.empty()) {
        return;
    }

    // ---- марши: контур fill=6 либо прямая stairs=1 -------------------------
    std::vector<Flight> flights;
    for (const Element& e : g.elements()) {
        const ElementParams p = dfn::world::element_params_of(e, nullptr);
        const bool contour_stairs =
            dfn::world::fill_kind(p) == dfn::world::WallFill::Stairs && e.refs.size() >= 4;
        const bool line_stairs = p.stairs > 0.5f && e.refs.size() >= 2;
        if (!contour_stairs && !line_stairs) {
            continue;
        }
        Flight fl;
        fl.id = e.id;
        fl.open = p.open > 0.5f;
        if (contour_stairs) {
            std::vector<glm::vec3> pts;
            for (const auto& r : e.refs) {
                pts.push_back(g.resolved_local(r));
            }
            std::sort(pts.begin(), pts.end(),
                      [](const glm::vec3& l, const glm::vec3& r) { return l.y < r.y; });
            fl.foot = (pts[0] + pts[1]) * 0.5f;
            fl.head = (pts[pts.size() - 1] + pts[pts.size() - 2]) * 0.5f;
            fl.half_w = std::max(glm::length(pts[1] - pts[0]) * 0.5f, 0.15f);
        } else {
            glm::vec3 a = g.resolved_local(e.refs[0]);
            glm::vec3 b = g.resolved_local(e.refs[1]);
            if (b.y < a.y) {
                std::swap(a, b);
            }
            fl.foot = a;
            fl.head = b;
            fl.half_w = std::max(p.radius, 0.15f);
        }
        const glm::vec3 d = fl.head - fl.foot;
        fl.run = std::sqrt(d.x * d.x + d.z * d.z);
        fl.rise_total = d.y;
        if (fl.run < 1e-4f || fl.rise_total < 1e-4f) {
            continue;
        }
        fl.dir = glm::vec3{d.x / fl.run, 0.0f, d.z / fl.run};
        flights.push_back(fl);
    }
    if (flights.empty()) {
        return;
    }

    std::vector<std::string> lines;
    std::vector<Bad> bad;
    const std::string name = std::filesystem::path(path).filename().string();

    // ---- РУКА 1: ПРОХОД ОТ ДВЕРИ ------------------------------------------
    // Дверная створка (door=1) — единственное тело, которое знает, ГДЕ проём:
    // раскладка ставит его сама, и рецепт этого числа не хранит.
    std::vector<Tri> stair_tris;
    std::set<ElementId> stair_ids;
    for (const Flight& fl : flights) {
        stair_ids.insert(fl.id);
    }
    for (const Tri& t : tris) {
        if (stair_ids.count(t.owner) != 0) {
            stair_tris.push_back(t);
        }
    }
    // СТЕНА С ПРОЁМОМ САМА ПРОХОДУ НЕ МЕШАЕТ, и это не поблажка: раскладка
    // вырезает в ней дверь, а судья видит только треугольники — тело стены
    // толщиной 0.42 закрывало собой первые 0.21 м пути и объявляло проход
    // занятым во ВСЕХ домах с толстой кладкой. Створка (door=1) тоже не
    // считается: она качается и в коллайдер не входит (AppHouse).
    // ЭТО ПОСТРОЙКА ИЛИ ОТДЕЛЬНАЯ ДЕТАЛЬ? Рецепты уличных маршей и крылец
    // (city-stoop*, city-steps*) — ДЕТАЛИ: площадку наверху им даёт мостовая,
    // которую кладёт генератор города, и требовать её внутри .dfh значит
    // судить деталь по правилам дома. Признак — наличие хоть одной стены.
    bool has_walls = false;
    std::set<ElementId> door_bodies;
    for (const Element& e : g.elements()) {
        const ElementParams p = dfn::world::element_params_of(e, nullptr);
        if (p.height > 0.0f) {
            has_walls = true;
        }
        if (p.doors > 0.5f || g.param(e.id, "door") == "1") {
            door_bodies.insert(e.id);
        }
    }
    glm::vec2 plan_lo{1e9f};
    glm::vec2 plan_hi{-1e9f};
    for (const Tri& t : tris) {
        for (const glm::vec3& q : {t.a, t.b, t.c}) {
            plan_lo = glm::min(plan_lo, {q.x, q.z});
            plan_hi = glm::max(plan_hi, {q.x, q.z});
        }
    }
    const glm::vec2 plan_mid = (plan_lo + plan_hi) * 0.5f;

    for (const Element& e : g.elements()) {
        if (g.param(e.id, "door") != "1" || e.refs.size() < 2) {
            continue;
        }
        const glm::vec3 a = g.resolved_local(e.refs[0]);
        const glm::vec3 b = g.resolved_local(e.refs[1]);
        const glm::vec3 c = (a + b) * 0.5f;
        glm::vec2 ax{b.x - a.x, b.z - a.z};
        if (glm::length(ax) < 1e-4f) {
            continue;
        }
        ax = glm::normalize(ax);
        glm::vec2 n{-ax.y, ax.x};
        if (glm::dot(plan_mid - glm::vec2{c.x, c.z}, n) < 0.0f) {
            n = -n;
        }
        // ПОЛОСА МЕРИТСЯ В ПОЯСЕ ВЫШЕ ШАГА: всё, что ниже PLAYER_STEP_HEIGHT,
        // контроллер берёт сам, и ступень марша препятствием не является. То,
        // что торчит В ПОЯСЕ, — это то, что «надо перепрыгивать».
        //
        // ОТСЧЁТ ОТ ПОЛА, А НЕ ОТ ПОДОШВЫ СТВОРКИ: у части рецептов створка
        // авторизована от нуля постройки, а пол лежит на цоколе, и полоса
        // уезжала вниз на всю его высоту — судья ловил ЛЕНТУ ФУНДАМЕНТА и
        // объявлял занятой дверь, в которую входят.
        float y0 = c.y;
        {
            const glm::vec2 in = glm::vec2{c.x, c.z} + n * 0.7f;
            float best = -1e9f;
            for (const Tri& t : tris) {
                const glm::vec3 nn = glm::cross(t.b - t.a, t.c - t.a);
                const float len = glm::length(nn);
                if (len < 1e-8f || nn.y / len < 0.9f) {
                    continue;
                }
                const float y = std::max({t.a.y, t.b.y, t.c.y});
                if (y > c.y + 0.8f || y < c.y - 1.5f) {
                    continue;
                }
                if (dist2_xz(in, {t.a.x, t.a.z}, {t.b.x, t.b.z}, {t.c.x, t.c.z}) > 0.09f) {
                    continue;
                }
                best = std::max(best, y);
            }
            if (best > -1e8f) {
                y0 = best;
            }
        }
        float worst_w = 9.0f;
        float worst_d = 0.0f;
        bool by_stair = false;
        ElementId worst_by = dfn::world::NO_ELEMENT;
        for (float dd = 0.15f; dd <= PASS_LEN + 1e-3f; dd += 0.1f) {
            const glm::vec2 base = glm::vec2{c.x, c.z} + n * dd;
            // Свободная полоса ВОКРУГ ОСИ ДВЕРИ: идём в обе стороны, пока
            // упираемся, и складываем.
            float left = 0.0f;
            float right = 0.0f;
            bool stair_hit = false;
            ElementId hit_by = dfn::world::NO_ELEMENT;
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                float u = 0.0f;
                for (; u <= 1.2f; u += 0.05f) {
                    const glm::vec2 q = base + ax * (u * static_cast<float>(sgn));
                    bool blocked = false;
                    for (const Tri& t : tris) {
                        if (t.collider_only || door_bodies.count(t.owner) != 0) {
                            continue;
                        }
                        const float lo = std::min({t.a.y, t.b.y, t.c.y});
                        const float hi = std::max({t.a.y, t.b.y, t.c.y});
                        if (hi < y0 + STEP_H || lo > y0 + CAP_H) {
                            continue;
                        }
                        if (dist2_xz(q, {t.a.x, t.a.z}, {t.b.x, t.b.z},
                                     {t.c.x, t.c.z}) > 1e-6f) {
                            continue;
                        }
                        // ЯЩИК ТРЕУГОЛЬНИКА — НЕ ЕГО ВЫСОТА В ТОЧКЕ (§14.1 тем
                        // же принципом: отрезками, а не ящиком). Длинное
                        // наклонное тело — подкос столба мельницы — лежит в
                        // плане поперёк всего корпуса, его lo..hi пересекает
                        // пояс ВЕЗДЕ, а сама балка в створе двери давно под
                        // полом. Для невырожденного в плане треугольника
                        // высота берётся интерполяцией В ТОЧКЕ q; вырожденный
                        // (лицо вертикальной стены) остаётся на ящике — у него
                        // интерполяции нет, а lo..hi и есть его высота.
                        {
                            const glm::vec2 ta{t.a.x, t.a.z};
                            const glm::vec2 tb{t.b.x, t.b.z};
                            const glm::vec2 tc{t.c.x, t.c.z};
                            const float area2 = (tb.x - ta.x) * (tc.y - ta.y)
                                              - (tc.x - ta.x) * (tb.y - ta.y);
                            if (std::fabs(area2) > 1e-7f) {
                                const float w0 = ((tb.x - q.x) * (tc.y - q.y)
                                                - (tc.x - q.x) * (tb.y - q.y)) / area2;
                                const float w1 = ((tc.x - q.x) * (ta.y - q.y)
                                                - (ta.x - q.x) * (tc.y - q.y)) / area2;
                                const float w2 = 1.0f - w0 - w1;
                                const float yq = t.a.y * w0 + t.b.y * w1 + t.c.y * w2;
                                if (yq < y0 + STEP_H || yq > y0 + CAP_H) {
                                    continue;
                                }
                            }
                        }
                        blocked = true;
                        hit_by = t.owner;
                        if (stair_ids.count(t.owner) != 0) {
                            stair_hit = true;
                        }
                        break;
                    }
                    if (blocked) {
                        break;
                    }
                }
                (sgn < 0 ? left : right) = u;
            }
            const float w = left + right;
            if (w < worst_w) {
                worst_w = w;
                worst_d = dd;
                by_stair = stair_hit;
                worst_by = hit_by;
            }
        }
        char buf[256];
        if (worst_w + 1e-3f < PASS_W) {
            std::snprintf(buf, sizeof(buf),
                          "  проход от двери (%.2f, %.2f): УЗКО %.2f м на %.2f м "
                          "вглубь, мешает e%u%s [пол y0=%.2f]",
                          c.x, c.z, worst_w, worst_d,
                          static_cast<unsigned>(worst_by),
                          by_stair ? " — ЭТО ТЕЛО МАРША" : "", y0);
            bad.push_back({"ПРОХОД", buf});
        } else {
            std::snprintf(buf, sizeof(buf),
                          "  проход от двери (%.2f, %.2f): свободен, узкое место %.2f м",
                          c.x, c.z, worst_w);
            lines.emplace_back(buf);
        }
    }

    // ---- РУКИ 2..5 по каждому маршу ---------------------------------------
    for (const Flight& fl : flights) {
        std::vector<Tri> own;
        for (const Tri& t : tris) {
            if (t.owner == fl.id) {
                own.push_back(t);
            }
        }
        const int steps = steps_from_mesh(own, fl.foot.y, fl.head.y + 0.02f);
        const float rise = steps > 0 ? fl.rise_total / static_cast<float>(steps) : 0.0f;
        const float tread = steps > 0 ? fl.run / static_cast<float>(steps) : 0.0f;
        const float deg = std::atan2(fl.rise_total, fl.run) * 180.0f / 3.14159265f;

        char head[320];
        std::snprintf(head, sizeof(head),
                      "  e%u: подъём %.2f, ход %.2f, ступеней %d, подступёнок %.3f, "
                      "проступь %.3f, угол %.1f гр.",
                      static_cast<unsigned>(fl.id), fl.rise_total, fl.run, steps, rise,
                      tread, deg);
        lines.emplace_back(head);
        if (deg > MAX_SLOPE * 180.0f / 3.14159265f) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: УКЛОН %.1f гр. КРУЧЕ ПРОХОДИМОГО %.1f гр.",
                          static_cast<unsigned>(fl.id), deg,
                          MAX_SLOPE * 180.0f / 3.14159265f);
            bad.push_back({"УКЛОН", buf});
        }
        if (rise > STEP_H) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: ПОДСТУПЁНОК %.3f ВЫШЕ ШАГА %.2f",
                          static_cast<unsigned>(fl.id), rise, STEP_H);
            bad.push_back({"ПОДСТУПЁНОК", buf});
        }

        // -- РУКА 6: МИНИМАЛЬНАЯ ПРОСТУПЬ ------------------------------------
        // Порог выведен из радиуса капсулы (TREAD_MIN выше). Мерится ТА ЖЕ
        // проступь, что печатается в строке марша: ход, делённый на число
        // ступеней, СЧИТАННОЕ ПО МЕШУ, — судья и здесь не зовёт калькулятор.
        if (steps > 0 && tread + 1e-3f < TREAD_MIN) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: ПРОСТУПЬ %.3f МЕЛЬЧЕ %.3f — на такую ступень "
                          "не ставится опора героя (диск 2R = %.2f): это "
                          "стремянка, а не лестница",
                          static_cast<unsigned>(fl.id), tread, TREAD_MIN,
                          2.0f * CAP_R);
            bad.push_back({"ПРОСТУПЬ", buf});
        }

        // -- РУКА 7: НИЖНЯЯ ГРАНИЦА УКЛОНА -----------------------------------
        // ТОЛЬКО ВНУТРИ ДОМА, и это не поблажка, а та же граница, которой рука
        // 5 отличает постройку от детали: уличное крыльцо (city-stoop*,
        // city-steps*) кладёт генератор города своей таблицей посадки, у него
        // пологость — свойство мостовой, а не комнаты. Требовать от детали
        // домовой крутизны значит судить деталь по правилам дома; признак тот
        // же — наличие хоть одной стены.
        const float grade = fl.rise_total / fl.run;
        if (has_walls && grade + 1e-4f < GRADE_MIN) {
            char buf[320];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: СЛИШКОМ ПОЛОГО — уклон %.3f (%.1f гр.) ниже "
                          "половины проходимого %.3f (%.1f гр.): ход %.2f м на "
                          "подъём %.2f м, вдвое больше пола, чем стоило бы",
                          static_cast<unsigned>(fl.id), grade, deg, GRADE_MIN,
                          std::atan(GRADE_MIN) * 180.0f / 3.14159265f, fl.run,
                          fl.rise_total);
            bad.push_back({"ПОЛОГО", buf});
        }

        // -- рука 3: просвет над носком каждой ступени -----------------------
        float min_head = 1e9f;
        int min_step = 0;
        ElementId min_by = dfn::world::NO_ELEMENT;
        glm::vec3 min_at{0.0f};
        for (int k = 1; k <= steps; ++k) {
            const glm::vec3 nose = fl.foot + fl.dir * (tread * static_cast<float>(k))
                                 + glm::vec3{0.0f, rise * static_cast<float>(k), 0.0f};
            float ceil_y = 1e9f;
            ElementId who = dfn::world::NO_ELEMENT;
            for (const Tri& t : tris) {
                // Полотно двери потолком не является — оно ОТКРЫВАЕТСЯ. Марш,
                // приводящий к закрытой створке (амбар, мельница), краснел
                // просветом 0.5 у последней ступени; проход полотна и так
                // исключает (та же строка ниже), просвет забывал.
                if (t.owner == fl.id || t.collider_only ||
                    door_bodies.count(t.owner) != 0) {
                    continue;
                }
                float y = 0.0f;
                // ПОД НОГАМИ — 0.05: рука 3 стоит НА НОСКЕ, и всё, что выше
                // носка, для неё потолок. Прогон руки 8 идёт по пандусу, где
                // нарисованные ступени торчат над ходимой поверхностью, и там
                // граница другая (см. ниже).
                if (tri_low_above(t, {nose.x, nose.z}, CAP_R, nose.y, fl.foot, fl.dir,
                                  fl.run, fl.head.y, 0.05f, 1e9f, -1e9f, y)) {
                    if (y < ceil_y) {
                        ceil_y = y;
                        who = t.owner;
                    }
                }
            }
            const float clear = ceil_y - nose.y;
            if (clear < min_head) {
                min_head = clear;
                min_step = k;
                min_by = who;
                min_at = nose;
            }
        }
        if (min_head > 1e8f) {
            lines.emplace_back("    просвет: над маршем ничего нет");
        } else {
            char buf[256];
            if (min_head + 1e-3f < g_head_tol) {
                std::snprintf(buf, sizeof(buf),
                              "  e%u: ПРОСВЕТ %.3f м НА СТУПЕНИ %d (норма %.2f) — "
                              "мешает e%u, носок (%.2f, %.2f, %.2f)",
                              static_cast<unsigned>(fl.id), min_head, min_step,
                              g_head_tol, static_cast<unsigned>(min_by), min_at.x,
                              min_at.y, min_at.z);
                bad.push_back({"ПРОСВЕТ", buf});
            } else {
                std::snprintf(buf, sizeof(buf), "    просвет: %.3f м (ступень %d)",
                              min_head, min_step);
                lines.emplace_back(buf);
            }
        }

        // -- рука 4: марш внутри оболочки ------------------------------------
        float worst_in = 0.0f;
        glm::vec3 worst_at{0.0f};
        for (int k = 0; k <= steps; ++k) {
            const float s = fl.run * static_cast<float>(k) / static_cast<float>(steps > 0 ? steps : 1);
            const glm::vec3 base = fl.foot + fl.dir * s
                                 + glm::vec3{0.0f, fl.rise_total * static_cast<float>(k)
                                                       / static_cast<float>(steps > 0 ? steps : 1),
                                             0.0f};
            const glm::vec3 side = glm::normalize(
                glm::cross(fl.dir, glm::vec3{0.0f, 1.0f, 0.0f}));
            for (const float u : {-1.0f, 0.0f, 1.0f}) {
                const glm::vec3 q = base + side * (fl.half_w * u * 0.95f)
                                  + glm::vec3{0.0f, 0.4f, 0.0f};
                for (const ConvexPart& cp : built.convex) {
                    if (cp.element == fl.id) {
                        continue;
                    }
                    const ElementParams pp =
                        dfn::world::element_params_of(*g.element(cp.element), nullptr);
                    if (pp.height <= 0.0f) {
                        continue; // судим по СТЕНАМ: плита под маршем — опора
                    }
                    const float d = convex_depth(cp, q);
                    if (d > worst_in) {
                        worst_in = d;
                        worst_at = q;
                    }
                }
            }
        }
        if (worst_in > 0.05f) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "  e%u: МАРШ В СТЕНЕ на %.3f м (%.2f, %.2f, %.2f)",
                          static_cast<unsigned>(fl.id), worst_in, worst_at.x, worst_at.y,
                          worst_at.z);
            bad.push_back({"МАРШ-В-СТЕНЕ", buf});
        }

        // -- рука 5: СТУПЕНЬКА НА ВХОДЕ И НА ВЫХОДЕ ---------------------------
        // Меряется ПО ТОМУ, ПО ЧЕМУ ХОДЯТ, а не по якорям рецепта: у
        // невидимого пандуса берётся его ходимая поверхность, у комнаты и у
        // площадки — верх ближайшей горизонтальной грани в 0.45 м до подножия
        // и в 0.45 м за верхом. Якорь марша у части рецептов стоит на
        // СРЕДИННОЙ плоскости настила, и разность якорей соврала бы на
        // пол-толщины в сторону «всё хорошо».
        if (fl.open) {
            float ramp_lo = -1e9f;
            float ramp_hi = -1e9f;
            for (const Tri& t : own) {
                if (!t.collider_only) {
                    continue;
                }
                for (const glm::vec3& q : {t.a, t.b, t.c}) {
                    const float s = glm::dot(q - fl.foot, fl.dir);
                    if (s < 0.05f) {
                        ramp_lo = std::max(ramp_lo, q.y);
                    }
                    if (s > fl.run - 0.05f) {
                        ramp_hi = std::max(ramp_hi, q.y);
                    }
                }
            }
            // ПОЛ — ЭТО НЕ «САМОЕ ВЫСОКОЕ ГОРИЗОНТАЛЬНОЕ», А БЛИЖАЙШЕЕ К КОНЦУ
            // МАРША. Оба уточнения куплены ложными тревогами: «самое высокое»
            // брало верх КАМЕННОЙ ЗАВАЛИНКИ вдоль стены (на 0.39 выше пола), а
            // после сужения полосы — её же на 0.10 выше. Завалинка стоит именно
            // там, где кончается марш, и по высоте от пола не отличается ничем,
            // кроме того, что она НЕ ПОЛ; отличает её только расстояние.
            const auto surface_at = [&](glm::vec3 probe, float up, float down) {
                float best = -1e9f;
                float best_d = 1e9f;
                for (const Tri& t : tris) {
                    if (t.owner == fl.id) {
                        continue;
                    }
                    const glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
                    const float len = glm::length(n);
                    if (len < 1e-8f || n.y / len < 0.9f) {
                        continue;
                    }
                    const float y = std::max({t.a.y, t.b.y, t.c.y});
                    if (y > probe.y + up || y < probe.y - down) {
                        continue;
                    }
                    if (dist2_xz({probe.x, probe.z}, {t.a.x, t.a.z}, {t.b.x, t.b.z},
                                 {t.c.x, t.c.z}) > 0.09f) {
                        continue;
                    }
                    const float d = std::fabs(y - probe.y);
                    if (d < best_d) {
                        best_d = d;
                        best = y;
                    }
                }
                return best;
            };
            const float floor_y = surface_at(fl.foot - fl.dir * 0.45f, 0.10f, 0.6f);
            const float land_y = surface_at(fl.head + fl.dir * 0.45f, 0.20f, 0.6f);
            // НАВЕРХУ ОБЯЗАНО БЫТЬ НА ЧТО СТУПИТЬ, И ЭТО ОТДЕЛЬНАЯ НАХОДКА, а
            // не молчание. Первая редакция руки 5 просто пропускала марш, у
            // которого площадка не нашлась, — и оба замка Вайтрана прошли
            // приёмку с маршем, кончавшимся В ВОЗДУХЕ в четырёх метрах от
            // кромки верхнего пола. Прибор, у которого «нет данных» и «всё
            // хорошо» выглядят одинаково, не прибор.
            if (land_y <= -1e8f && has_walls) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                              "  e%u: МАРШ ОБРЫВАЕТСЯ — наверху (%.2f, %.2f, %.2f) "
                              "не на что ступить",
                              static_cast<unsigned>(fl.id), fl.head.x, fl.head.y,
                              fl.head.z);
                bad.push_back({"ОБРЫВ", buf});
            }
            if (ramp_lo > -1e8f && ramp_hi > -1e8f && floor_y > -1e8f && land_y > -1e8f) {
                const float d_lo = ramp_lo - floor_y;
                const float d_hi = land_y - ramp_hi;
                char buf[256];
                if (std::fabs(d_lo) > 0.06f || std::fabs(d_hi) > 0.06f) {
                    std::snprintf(buf, sizeof(buf),
                                  "  e%u: СТУПЕНЬКА У МАРША — на входе %+.3f, "
                                  "на выходе %+.3f",
                                  static_cast<unsigned>(fl.id), d_lo, d_hi);
                    bad.push_back({"СТУПЕНЬКА", buf});
                } else {
                    std::snprintf(buf, sizeof(buf),
                                  "    стык: на входе %+.3f, на выходе %+.3f", d_lo,
                                  d_hi);
                    lines.emplace_back(buf);
                }
            }
        }

        // -- РУКА 8: ПО МАРШУ МОЖНО ПОДНЯТЬСЯ ---------------------------------
        // ЭТО ЕДИНСТВЕННАЯ РУКА, КОТОРАЯ НЕ СУДИТ ЧЕРТЁЖ, А ИДЁТ. Все прочие
        // спрашивают у геометрии свойство («уклон такой-то», «просвет
        // такой-то») и складывают ответы в надежде, что из свойств следует
        // проходимость. Утверждение «на второй этаж поднимаешься» до сегодня
        // не предъявлял НИ ОДИН прогон (аудит «Большой мир», задача 5) — а именно его
        // владелец и опроверг глазами.
        //
        // ИДЁТ ПО ТОМУ, ПО ЧЕМУ ХОДИТ ИГРОК. Ходимая поверхность марша — не
        // ступени, а невидимый пандус вдоль них (HouseStairs.cpp): по открытым
        // ступеням капсула не идёт в принципе. Поэтому опора здесь берётся
        // ПРОХОДИМЫМИ гранями (наклон не круче PLAYER_MAX_SLOPE) — ровно тот
        // отбор, что делает контроллер, — а не «самым высоким телом».
        //
        // ОПОРА МЕРИТСЯ ПОД ОСЬЮ, ПОТОЛОК — ПО ВСЕМУ КРУГУ КАПСУЛЫ, и это не
        // небрежность, а форма тела: подошва касается пандуса в одной точке,
        // а макушка — полусфера радиуса R, и балка, до которой 0.30 м вбок,
        // бьёт по ней так же, как балка прямо по курсу.
        if (has_walls) {
            // Только то, что рядом с осью марша: прогон в 40 проб против всех
            // тел дома стоил бы столько же, сколько все прочие руки вместе.
            const float band = fl.half_w + CAP_R + 0.8f;
            const glm::vec2 axis_a{fl.foot.x - fl.dir.x, fl.foot.z - fl.dir.z};
            const glm::vec2 axis_b{fl.foot.x + fl.dir.x * (fl.run + 1.0f),
                                   fl.foot.z + fl.dir.z * (fl.run + 1.0f)};
            const glm::vec2 axis_lo = glm::min(axis_a, axis_b);
            const glm::vec2 axis_hi = glm::max(axis_a, axis_b);
            std::vector<Tri> near_tris;
            for (const Tri& t : tris) {
                if (door_bodies.count(t.owner) != 0) {
                    continue; // створка открывается, потолком и полом не является
                }
                // ОТСЕВ ПО ЯЩИКУ ТРЕУГОЛЬНИКА, А НЕ ПО ЕГО ВЕРШИНАМ, и это не
                // придирка: у НАСТИЛА КОМНАТЫ 4x4 все три вершины лежат по
                // углам, в двух метрах от оси марша, — а сам он висит ровно
                // над головой. Отсев по вершинам выбрасывал плиту, о которую
                // бьются, и прогон объявлял марш пройденным (фикстура
                // lowbeam.dfh).
                const glm::vec2 tlo = glm::min(glm::vec2{t.a.x, t.a.z},
                                               glm::min(glm::vec2{t.b.x, t.b.z},
                                                        glm::vec2{t.c.x, t.c.z}));
                const glm::vec2 thi = glm::max(glm::vec2{t.a.x, t.a.z},
                                               glm::max(glm::vec2{t.b.x, t.b.z},
                                                        glm::vec2{t.c.x, t.c.z}));
                if (thi.x + band >= axis_lo.x && tlo.x - band <= axis_hi.x
                    && thi.y + band >= axis_lo.y && tlo.y - band <= axis_hi.y) {
                    near_tris.push_back(t);
                }
            }
            const float walk_cos = std::cos(MAX_SLOPE);
            // Высота проходимой грани в точке плана; false — грань не под ногой
            // или стоять на ней нельзя.
            const auto surf_y = [&](const Tri& t, glm::vec2 q, float& y) {
                const glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
                const float len = glm::length(n);
                if (len < 1e-8f || std::fabs(n.y) / len < walk_cos) {
                    return false; // круче проходимого — это стена, а не опора
                }
                const glm::vec2 ta{t.a.x, t.a.z};
                const glm::vec2 tb{t.b.x, t.b.z};
                const glm::vec2 tc{t.c.x, t.c.z};
                const float area2 = (tb.x - ta.x) * (tc.y - ta.y)
                                  - (tc.x - ta.x) * (tb.y - ta.y);
                if (std::fabs(area2) < 1e-7f) {
                    return false;
                }
                if (dist2_xz(q, ta, tb, tc) > 1e-6f) {
                    return false;
                }
                const float w0 = ((tb.x - q.x) * (tc.y - q.y)
                                - (tc.x - q.x) * (tb.y - q.y)) / area2;
                const float w1 = ((tc.x - q.x) * (ta.y - q.y)
                                - (ta.x - q.x) * (tc.y - q.y)) / area2;
                y = t.a.y * w0 + t.b.y * w1 + t.c.y * (1.0f - w0 - w1);
                return true;
            };

            // ПУСК — С ПЕРВОЙ СТУПЕНИ, А НЕ С ПОЛА ПЕРЕД НЕЙ, и это решение,
            // купленное ложной находкой. Первая редакция отступала на радиус
            // капсулы назад, «где стоит игрок», — и у city-house-l эта точка
            // легла ВНУТРЬ СТЕНЫ (марш начинается в 0.22 м от её изнанки).
            // Прибор объявлял непроходимым марш, не сделав ни шагу, и мерил
            // при этом посадку марша, а не марш. ЧЕМ ЗАНЯТА ДОРОГА К
            // ПОДНОЖИЮ — вопрос руки 1 («проход от двери»), и второй ответ на
            // него здесь был бы вторым ответом на один вопрос.
            float s0 = 0.0f;
            float y = -1e9f;
            for (; s0 <= 0.3f + 1e-3f; s0 += WALK_DS) {
                const glm::vec2 q{fl.foot.x + fl.dir.x * s0, fl.foot.z + fl.dir.z * s0};
                for (const Tri& t : near_tris) {
                    float ty = 0.0f;
                    if (surf_y(t, q, ty) && ty <= fl.foot.y + STEP_H
                        && ty >= fl.foot.y - STEP_H && ty > y) {
                        y = ty;
                    }
                }
                if (y > -1e8f) {
                    break;
                }
            }
            if (y < -1e8f) {
                char nofloor[300];
                std::snprintf(nofloor, sizeof(nofloor),
                              "  e%u: У МАРША НЕТ ХОДИМОЙ ПОВЕРХНОСТИ — на "
                              "подножии (%.2f, %.2f, %.2f) не на что встать: "
                              "ни пандуса, ни проступи в пределах шага",
                              static_cast<unsigned>(fl.id), fl.foot.x, fl.foot.y,
                              fl.foot.z);
                bad.push_back({"ПОДЪЁМ", nofloor});
                continue;
            }
            // ПОДЪЁМ СЧИТАЕТСЯ ОТ ПОДНОЖИЯ МАРША, А ВЕРХ — САМАЯ ВЫСОКАЯ ТОЧКА
            // ПУТИ, и оба слова куплены ложными находками.
            //
            // «ОТ ПОДНОЖИЯ, А НЕ ОТ ПЕРВОЙ ПРОБЫ»: у марша БЕЗ невидимого
            // пандуса первая проба стоит уже на ПЕРВОЙ СТУПЕНИ, то есть на
            // подступёнок выше пола. Разность «последняя проба минус первая»
            // недосчитывала ровно один подступёнок — и прибор объявлял
            // недобором каждый исправный марш полки (замер: 2.67 вместо 2.94
            // у усадьбы, ровно 0.267 = один подступёнок).
            //
            // «САМАЯ ВЫСОКАЯ, А НЕ ПОСЛЕДНЯЯ»: прогон не встаёт на кромке
            // настила, он делает ещё полшага на площадку; если площадка лежит
            // на полступени ниже верха пандуса, последняя точка отняла бы у
            // подъёма ту самую ступеньку, которую точнее меряет рука 5.
            float y_top = y;
            const float s_end = fl.run + CAP_R + 0.10f;
            float min_clear = 1e9f;
            float clear_at = 0.0f;
            ElementId clear_by = dfn::world::NO_ELEMENT;
            glm::vec3 clear_pt{0.0f};
            bool stopped = false;
            bool stopped_by_head = false;
            float stop_s = 0.0f;
            float stop_gap = 0.0f;
            const char* stop_why = "";
            for (float s = s0; s <= s_end + 1e-3f && !stopped; s += WALK_DS) {
                const glm::vec2 q{fl.foot.x + fl.dir.x * s, fl.foot.z + fl.dir.z * s};
                float best = -1e9f;
                for (const Tri& t : near_tris) {
                    float ty = 0.0f;
                    if (!surf_y(t, q, ty)) {
                        continue;
                    }
                    if (ty > y + STEP_H + 1e-3f || ty < y - 1.0f) {
                        continue; // выше шага не перешагнуть; ниже метра — обрыв
                    }
                    best = std::max(best, ty);
                }
                if (best < -1e8f) {
                    stopped = true;
                    stop_s = s;
                    stop_gap = 0.0f;
                    stop_why = "под ногой нет проходимой опоры в пределах шага";
                    break;
                }
                y = best;
                y_top = std::max(y_top, y);
                // ТРАССА ПРОГОНА ПО ТРЕБОВАНИЮ (DFN_STAIRS_TRACE=1). Прибор,
                // объявивший марш непроходимым, обязан уметь ПОКАЗАТЬ путь: без
                // этого спор «прибор врёт / марш плох» не разрешается никак —
                // тот же довод, что у карты проходимости судьи локаций. Три
                // ложные находки этой руки (пуск в стене, недобор в один
                // подступёнок, завалинка вместо потолка) найдены именно
                // трассой, а не чтением кода.
                if (std::getenv("DFN_STAIRS_TRACE") != nullptr) {
                    std::fprintf(stderr, "    трасса e%u s=%.2f y=%.3f\n",
                                 static_cast<unsigned>(fl.id), s, y);
                }
                // Просвет над макушкой. Граница «под ногами» — ШАГ, а не 0.05:
                // на пандусе нарисованные ступени торчат над ходимой линией, и
                // всё, что не выше шага, игрок берёт ногами, а не головой.
                float ceil_y = 1e9f;
                ElementId who = dfn::world::NO_ELEMENT;
                glm::vec3 hit_pt{0.0f};
                for (const Tri& t : near_tris) {
                    if (t.owner == fl.id) {
                        // ТО, ПО ЧЕМУ ИДЁШЬ, НЕ ЯВЛЯЕТСЯ ТЕМ, ВО ЧТО УПИРАЕШЬСЯ.
                        // Без этой строки марш бил головой сам себя: круг
                        // капсулы радиусом 0.35 накрывает две-три СВОИ ЖЕ
                        // ступени впереди, их верх выше шага, и прибор объявлял
                        // непроходимым каждый марш полки, включая перепечённые.
                        continue;
                    }
                    // ПОТОЛОК — ЭТО ИЗНАНКА, а не всё, что оказалось в круге.
                    // Куплено ложной находкой на КАМЕННОЙ ЗАВАЛИНКЕ (плинте
                    // стены): её верх лежит на 0.36 м над полом у подножия
                    // марша, попадает в круг радиуса 0.35 и объявлял «макушка
                    // упирается» на первой же пробе — в доме, по которому
                    // ходят. Завалинку обходят боком; головой бьются о то,
                    // что ОБРАЩЕНО ВНИЗ: накат, ригель обвязки, подкос,
                    // настил над нижними ступенями. Боковое препятствие —
                    // предмет рук 1 и 4, а не этой.
                    {
                        const glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
                        const float len = glm::length(n);
                        if (len < 1e-8f || n.y / len > -0.2f) {
                            continue;
                        }
                    }
                    glm::vec3 hit{0.0f};
                    if (!tri_height_near(t, q, CAP_R, hit)) {
                        continue;
                    }
                    if (hit.y <= y + STEP_H) {
                        continue; // не выше шага — это берут ногами, не головой
                    }
                    const float hs = glm::dot(hit - fl.foot, fl.dir);
                    // НАД МАРШЕМ: не позади подножия и не вне его ширины (оба
                    // отсева куплены ложными находками, см. ниже по тексту).
                    if (hs < -0.05f) {
                        continue;
                    }
                    if (std::fabs(glm::dot(hit - fl.foot,
                                           glm::cross(fl.dir,
                                                      glm::vec3{0.0f, 1.0f, 0.0f})))
                        > std::max(fl.half_w, CAP_R)) {
                        continue;
                    }
                    // ПЛОЩАДКА ВЫХОДА ПОТОЛКОМ НЕ ЯВЛЯЕТСЯ — то же правило и по
                    // той же причине, что у руки 3.
                    if (hs > fl.run - CAP_R && hit.y <= fl.head.y + STEP_H) {
                        continue;
                    }
                    if (hit.y < ceil_y) {
                        ceil_y = hit.y;
                        who = t.owner;
                        hit_pt = hit;
                    }
                }
                if (ceil_y < 1e8f) {
                    const float clear = ceil_y - y;
                    if (clear < min_clear) {
                        min_clear = clear;
                        clear_at = s;
                        clear_by = who;
                        clear_pt = hit_pt;
                    }
                    if (clear + 1e-3f < WALK_HEAD) {
                        stopped = true;
                        stopped_by_head = true;
                        stop_s = s;
                        stop_gap = clear;
                        stop_why = "макушка упирается";
                        break;
                    }
                }
            }
            const float climbed = y_top - fl.foot.y;
            char buf[400];
            // ПОТЕРЯ ОПОРЫ ПОСЛЕ ВЕРХА МАРША — НЕ ДЕЛО ЭТОЙ РУКИ. Прогон
            // доходит на полшага дальше кромки настила, и там он может сойти с
            // площадки в проём — это уже устройство ПЛОЩАДКИ (рука 5) и
            // локации, а не «по маршу не подняться»: марш пройден, замер это
            // показывает числом. А вот УДАР ГОЛОВОЙ — находка всегда: крит
            // владельца дословно про него, и на последней ступени он тот же
            // самый, что на первой.
            if (stopped && !stopped_by_head && y_top + 0.10f >= fl.head.y) {
                stopped = false;
            }
            if (stopped) {
                std::snprintf(buf, sizeof(buf),
                              "  e%u: ПО МАРШУ НЕ ПОДНЯТЬСЯ — прогон встал на "
                              "%.2f м из %.2f (%s, мешает e%u в (%.2f, %.2f, "
                              "%.2f), просвет %.2f при росте %.2f), поднялся "
                              "%.2f из %.2f м",
                              static_cast<unsigned>(fl.id), stop_s - s0,
                              s_end - s0, stop_why,
                              static_cast<unsigned>(clear_by), clear_pt.x,
                              clear_pt.y, clear_pt.z, stop_gap, WALK_HEAD,
                              climbed, fl.rise_total);
                bad.push_back({"ПОДЪЁМ", buf});
            } else if (y_top + 0.10f < fl.head.y) {
                std::snprintf(buf, sizeof(buf),
                              "  e%u: ПРОГОН ДОШЁЛ, НО НЕ ПОДНЯЛСЯ — %.2f м "
                              "вместо %.2f: ходимая поверхность идёт не по маршу",
                              static_cast<unsigned>(fl.id), climbed, fl.rise_total);
                bad.push_back({"ПОДЪЁМ", buf});
            } else {
                std::snprintf(buf, sizeof(buf),
                              "    прогон: поднялся %.2f м из %.2f, узкий просвет "
                              "%.2f м на %.2f м пути (e%u)",
                              climbed, fl.rise_total,
                              min_clear > 1e8f ? 0.0f : min_clear, clear_at - s0,
                              static_cast<unsigned>(clear_by));
                lines.emplace_back(buf);
            }
        }
    }

    // ИЗВЕСТНОЕ ОТДЕЛЯЕТСЯ ОТ БОЕВОГО ЗДЕСЬ, а не в глазах читателя: известная
    // находка печатается со СВОЕЙ причиной и датой и не красит прогон
    // (tools/known_findings.h).
    std::vector<std::string> known_lines;
    std::vector<std::string> live_lines;
    for (const Bad& b : bad) {
        std::string why;
        if (g_known.take(name, b.cls, &why)) {
            ++g_known_hits;
            known_lines.push_back(b.text + "  [ИЗВЕСТНОЕ, " + why + "]");
            continue;
        }
        live_lines.push_back(b.text);
        g_by_class[b.cls] += 1;
    }
    if (live_lines.empty() && known_lines.empty() && !g_all) {
        return;
    }
    std::printf("%s\n", name.c_str());
    if (g_all) {
        for (const std::string& s : lines) {
            std::printf("%s\n", s.c_str());
        }
    }
    for (const std::string& s : known_lines) {
        std::printf("%s\n", s.c_str());
    }
    for (const std::string& s : live_lines) {
        std::printf("%s\n", s.c_str());
        ++g_findings;
    }
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> files;
    std::string known_path;
    dfn::tools::Expectations expect;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--all") {
            g_all = true;
            continue;
        }
        if (a == "--tol-head" && i + 1 < argc) {
            g_head_tol = std::strtof(argv[++i], nullptr);
            continue;
        }
        if (a == "--known" && i + 1 < argc) {
            known_path = argv[++i];
            continue;
        }
        if (a == "--expect" && i + 1 < argc) {
            expect.add(argv[++i]);
            continue;
        }
        if (std::filesystem::is_directory(a)) {
            std::vector<std::string> got;
            for (const auto& de : std::filesystem::directory_iterator(a)) {
                if (de.path().extension() == ".dfh") {
                    got.push_back(de.path().string());
                }
            }
            std::sort(got.begin(), got.end());
            files.insert(files.end(), got.begin(), got.end());
            continue;
        }
        files.push_back(a);
    }
    if (files.empty()) {
        std::fprintf(stderr,
                     "dfn_stairs_check: нечего судить\n"
                     "  dfn_stairs_check <файл.dfh | каталог> [--all] "
                     "[--tol-head 1.9] [--known список.txt] [--expect N|КЛАСС:N]\n");
        return 2;
    }
    {
        std::string err;
        if (!g_known.load(known_path, err)) {
            std::fprintf(stderr, "%s\n", err.c_str());
            return 2;
        }
    }
    for (const std::string& f : files) {
        check_file(f);
    }
    const int stale = g_known.report_stale(stdout);
    std::printf("dfn_stairs_check: тел %zu, боевых находок %d, известных %d, "
                "протухших строк списка %d\n",
                files.size(), g_findings, g_known_hits, stale);
    if (expect.any()) {
        return expect.verdict(g_findings, g_by_class, stderr);
    }
    return g_findings == 0 ? 0 : 1;
}
