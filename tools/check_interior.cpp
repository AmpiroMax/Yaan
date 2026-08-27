/*
Created: 24:08:2026 - 02:00:00
Last updated: 27:08:2026 - 23:05:00
Module: tools
File: tools/check_interior.cpp

Responsibility:
- dfn_interior_check: приёмка интерьера-локации (docs/plans/INTERIORS_I15.md,
  «Приёмка (семь рук)»). Четыре руки мерят ЛОКАЦИЮ, пятая — ПАРНОСТЬ ссылок
  города и его локаций:
    рука 1 — проходимость капсулой игрока от [spawn] до мебели (сетка 0.25);
    рука 2 — мебель НА ПОЛУ (низ на полу ±0.05);
    рука 5 — переход достижим (дверь не за стеной);
    рука 7 — бюджет огня (<= 8 настоящих [light], <= 1 теневой);
    рука 6 — interior= <-> файл локации <-> обратный ^back, ни одной сироты,
             и слаги локаций уникальны (--city).

Usage:
    dfn_interior_check <локация.scene> [ещё.scene ...] [--city <город.scene>]
                       [--expect <N>]

  Запускать из корня репозитория. Ненулевой выход при находках — судья
  годится в хук и в сборку. --expect N требует РОВНО N находок: это плечо
  испорченного интерьера (свод, дельта-5), где молчание прибора неотличимо
  от неработающего прибора.

Dependencies:
- Uses: engine/world (Scene, HouseFile, HouseMesh), engine/core (Constants).
- Used by: приёмка волны А зоны И15, человек, будущий хук.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- МЕРИТСЯ ГЕОМЕТРИЯ, А НЕ ИМЕНА. Проходимость — капсула игрока против
  НАСТОЯЩИХ треугольников .dfh, пол — треугольник под предметом. Судья,
  верящий именам файлов, судит другой дом.
- МЕРКИ ГЕРОЯ БЕРУТСЯ ИЗ КОНСТАНТ, а не выписываются здесь. В день, когда
  герой подрастёт, комната обязана перестать проходить, а не остаться
  зелёной (тот же довод, что у SceneLimits в Scene.h).
- НАХОДКА НЕСЁТ ЧИСЛО И АДРЕС. «Мебель висит» — мнение; «низ бочки ниже пола
  на 0.50 м на (3.60, 1.90)» — замер, по которому можно действовать.
*/
/*
UPD:
- 24:08:2026 - 02:00:00: Создан. И15 волна А, шаг 6: приёмка руками 1/2/5/6/7 и
  оснастка испорченного интерьера.
- 27:08:2026 - 01:20:00: РУКА 6 УЗНАЛА ВТОРОЙ ЗАКОННЫЙ ВХОД (И15 волна Б). Она
  требовала строку [portal] в сцене города на каждый interior=; у волны
  болванок переход заводит сама заливка по геометрии створки, и на боевом
  Вайтране рука давала 23 находки там, где всё исправно. Теперь пара
  «sealed + элемент door=1 в чертеже» засчитывается наравне со строкой
  перехода — и это ровно та пара, которую читает движок.
- 27:08:2026 - 10:18:14: СУДЬЯ УВИДЕЛ УБРАНСТВО, ПРИШЕДШЕЕ ССЫЛКОЙ ([place]).
  Кровать переехала в реестр объектов (волна 27.08), и до этой правки судья её
  не видел вовсе: рука 1 проложила бы проход СКВОЗЬ кровать и объявила комнату
  проходимой, рука 2 промолчала бы о кровати, висящей в воздухе. Проверено
  обоими плечами на копии боевой локации: поднятая на 2 м ссылочная кровать
  даёт «РУКА 2 — furn-bed ВЫШЕ пола на 1.98 м», опущенная — ноль находок.
  Объект без секции HOUS в локации — тоже находка, а не молчание.
- 27:08:2026 - 10:34:00: МЕТКА ЗАПИСИ ВЫШЕ ИСПРАВЛЕНА НА РЕАЛЬНОЕ ВРЕМЯ (правило 16):
  стояло 15:xx при стенных 10:18 — я выбрал метку позже чужой записи 14:30 в
  AppHouse.cpp, лишь бы сошлась сверка хука, вместо того чтобы разобраться с
  чужой. Поставлено время коммита 9e2c8c9 (10:18:14). Текста записи и кода не
  трогал.
- 27:08:2026 - 23:05:00: СУДЬЯ ПОПАЛ В ctest ПО ВСЕМ 130 ЛОКАЦИЯМ, А НЕ ПО ОДНОЙ
  (волна приборов; аудит эпохи «Большой мир», задача 1). Он стоял в ctest на
  ОДНОЙ сцене из ста тридцати — и, по совпадению, ровно на той единственной,
  где находок нет: одноэтажный дом без марша. Аудит прогнал его по всем 130 и
  получил 169 находок, то есть всё, ради чего судья написан, жило ВНЕ ctest.
  Три правки, все ради этого: КАТАЛОГ как аргумент (раскрытие «*.scene» делает
  оболочка, а CMake оболочки не запускает, и список из ста тридцати имён
  устарел бы МОЛЧА); КЛАСС у каждой находки (по нему ведётся список известных —
  по тексту нельзя, замеры плывут при живой правке мебели); --known и
  --expect КЛАСС:N через общий tools/known_findings.h. Порог теста при этом
  остался прежним и честным — НОЛЬ; список известных это отсрочка с датой,
  причиной и числом, и находка сверх числа краснит прогон сразу.
*/

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"
#include "engine/world/sources/Scene.h"
#include "tools/known_findings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/geometric.hpp>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using dfn::world::SceneDoc;

// МЕРКИ ГЕРОЯ — из констант, не из головы (см. шапку).
constexpr float CAPSULE_R = static_cast<float>(dfn::config::PLAYER_CAPSULE_RADIUS);
constexpr float CAPSULE_H = static_cast<float>(dfn::config::PLAYER_CAPSULE_HEIGHT);
constexpr float STEP_H = static_cast<float>(dfn::config::PLAYER_STEP_HEIGHT);
// ШАГ СЕТКИ ПРОХОДИМОСТИ. Свод называл 0.25 м; ЗАМЕР НА ПИЛОТЕ ПОТРЕБОВАЛ
// 0.05, и это правило 50, а не вкус.
//
// Проба ставится в ЦЕНТР ячейки, поэтому сетка разрешает не ширину прохода, а
// ширину полосы ДОПУСТИМЫХ ЦЕНТРОВ, а она равна «зазор минус диаметр
// капсулы». В настоящем доме у рынка проход между столом и кроватью 0.80 м
// при капсуле 0.70 — законный проход, по которому игрок ходит, — но полоса
// центров всего 0.10 м. Сетка 0.25 её не видела вовсе; сетка 0.10 клала
// центры ровно на её края (2.70 и 2.80 при допуске 0.35 — касание, то есть
// «занято»), и вердикт держался на знаке сравнения. 0.05 ставит центр
// ВНУТРИ полосы (2.725, запас 0.025) и перестаёт зависеть от округления.
// Прибор с шагом крупнее разрешаемого зазора не меряет его, а алиасит.
constexpr float GRID_M = 0.05f;
// ДОПУСК «МЕБЕЛЬ НА ПОЛУ» — 0.05 м по своду.
constexpr float FLOOR_TOL_M = 0.05f;
// БЮДЖЕТ ОГНЯ — 8 настоящих ламп и 1 теневая на локацию (свод).
constexpr std::size_t LIGHT_BUDGET = 8;
constexpr std::size_t SHADOW_BUDGET = 1;

struct Tri {
    glm::vec3 a, b, c;
};

/// ГДЕ ИСКАТЬ ЗАПЕЧЁННЫЕ ПРЕДМЕТЫ. Полка мебели — своя, и она названа здесь
/// одна: судья локации судит УБРАНСТВО, а деревья и детали набора в комнате
/// не стоят. Ключ --objects добавляет полки тому, у кого они свои.
std::vector<std::string> g_shelves{"assets/objects/furniture"};

const std::vector<std::string>& object_shelves() { return g_shelves; }

struct Item {
    std::string file;
    glm::vec2 lo{0.0f};   ///< след в плане
    glm::vec2 hi{0.0f};
    glm::vec2 center{0.0f};
    float bottom_y = 0.0f;
    /// ПОДОШВА ПРЕДМЕТА — НАЧАЛО ЕГО ЧЕРТЕЖА, а не низ его треугольников, и
    /// это замер: кровать, бочка и стол авторизованы от y=0 вверх, а у очага
    /// топка УТОПЛЕНА — его меш уходит на 0.45 м ниже начала НАМЕРЕННО.
    /// Судья по низу меша объявил бы каждый очаг города закопанным.
    float origin_y = 0.0f;
    bool furniture = false;
    std::vector<Tri> tris; ///< СВОИ треугольники — чтобы опора считалась БЕЗ них
};

/// НАХОДКА НЕСЁТ КЛАСС И ТЕЛО, А НЕ ТОЛЬКО ТЕКСТ (27.08). Текст несёт замеры,
/// а замеры плывут каждый час, пока волна мебели двигает предметы; по классу
/// же ведётся список известных находок и по нему контрольная рука доказывает,
/// что покраснела ИМЕННО испытуемая рука (tools/known_findings.h).
struct Finding {
    std::string body; ///< сцена или город — ключ списка известных
    std::string cls;  ///< класс: Р1-НЕ-ДОЙТИ, Р2-ВЫШЕ, Р6-СИРОТА, …
    std::string text;
};
std::vector<Finding> findings;

void finding(const std::string& body, const char* cls, const std::string& text) {
    findings.push_back({body, cls, text});
}

[[nodiscard]] bool read_graph(const std::string& path, dfn::world::HouseGraph& g) {
    std::ifstream in(path);
    if (!in.good()) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return dfn::world::read_house(ss.str(), g).ok;
}

/// Мировая точка детали: местный +X при yaw уходит в (cos, -sin) — конвенция
/// сцены, та же формула, что в App и в генераторах.
[[nodiscard]] glm::vec3 place(glm::vec3 l, glm::vec3 pos, float yaw) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return pos + glm::vec3{l.x * c + l.z * s, l.y, -l.x * s + l.z * c};
}

/// Квадрат расстояния от точки до треугольника В ПЛАНЕ (XZ).
[[nodiscard]] float dist2_point_tri_xz(glm::vec2 p, glm::vec2 a, glm::vec2 b,
                                       glm::vec2 c) {
    const auto seg = [](glm::vec2 q, glm::vec2 u, glm::vec2 v) {
        const glm::vec2 d = v - u;
        const float dd = glm::dot(d, d);
        float t = dd > 1e-12f ? glm::dot(q - u, d) / dd : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        const glm::vec2 w = q - (u + d * t);
        return glm::dot(w, w);
    };
    // Внутри треугольника — ноль.
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

/// Верх пола под точкой: самый высокий треугольник, лежащий НИЖЕ головы.
[[nodiscard]] bool floor_under(const std::vector<Tri>& tris, glm::vec2 xz,
                               float head_y, float& out_y) {
    bool any = false;
    float best = -1e9f;
    for (const Tri& t : tris) {
        const glm::vec2 a{t.a.x, t.a.z};
        const glm::vec2 b{t.b.x, t.b.z};
        const glm::vec2 c{t.c.x, t.c.z};
        if (dist2_point_tri_xz(xz, a, b, c) > 1e-6f) {
            continue;
        }
        const float y = std::max({t.a.y, t.b.y, t.c.y});
        if (y <= head_y && y > best) {
            best = y;
            any = true;
        }
    }
    out_y = best;
    return any;
}

struct Report {
    int grid_w = 0;
    int grid_h = 0;
    glm::vec2 origin{0.0f};
    std::vector<uint8_t> free_cell;
    std::vector<uint8_t> reached;
};

[[nodiscard]] int cell_of(float v, float origin) {
    return static_cast<int>(std::floor((v - origin) / GRID_M));
}

bool dump_map = false;

void check_scene_file(const std::string& path) {
    SceneDoc doc;
    std::string err;
    if (!dfn::world::read_scene(path, doc, err)) {
        finding(path, "СЦЕНА", path + ": сцена не прочиталась — " + err);
        return;
    }

    // ---- ГЕОМЕТРИЯ. Оболочка и мебель разделяются по ФАЙЛУ рецепта: имя
    // furn-* — это контракт кузницы убранства, и он же используется
    // конвейером города. Отдельного признака в формате нет намеренно: он был
    // бы вторым ответом на тот же вопрос.
    std::vector<Tri> shell;
    std::vector<Item> items;
    for (const dfn::world::ScenePlacedHouse& H : doc.houses) {
        dfn::world::HouseGraph g;
        if (!read_graph(H.file, g)) {
            finding(path, "ЧЕРТЁЖ", path + ": [house] " + H.file + " не прочитался");
            continue;
        }
        const dfn::world::HouseMesh built = dfn::world::build_house_mesh(g);
        Item it;
        it.file = H.file;
        it.furniture =
            std::filesystem::path(H.file).filename().string().rfind("furn-", 0) == 0;
        glm::vec2 lo{1e9f};
        glm::vec2 hi{-1e9f};
        float bottom = 1e9f;
        std::vector<Tri> mine;
        for (const dfn::world::MeshPart& part : built.parts) {
            // СТВОРКА БЕЗ portal=1 НЕ ЗАГОРАЖИВАЕТ — ровно как в движке
            // (AppHouse): она качается и в коллайдер не входит. Судья,
            // считающий её стеной, объявил бы каждую дверь тупиком.
            const bool is_door = g.param(part.element, "door") == "1";
            const bool portal_leaf = is_door && g.param(part.element, "portal") == "1";
            if (is_door && !portal_leaf) {
                continue;
            }
            for (std::uint32_t i = 0; i + 2 < part.index_count; i += 3) {
                Tri t;
                t.a = place(built.vertices[built.indices[part.index_begin + i]].pos,
                            H.position, H.yaw);
                t.b = place(built.vertices[built.indices[part.index_begin + i + 1]].pos,
                            H.position, H.yaw);
                t.c = place(built.vertices[built.indices[part.index_begin + i + 2]].pos,
                            H.position, H.yaw);
                mine.push_back(t);
                for (const glm::vec3& p : {t.a, t.b, t.c}) {
                    lo = glm::min(lo, {p.x, p.z});
                    hi = glm::max(hi, {p.x, p.z});
                    bottom = std::min(bottom, p.y);
                }
            }
        }
        if (mine.empty()) {
            continue;
        }
        it.lo = lo;
        it.hi = hi;
        it.center = (lo + hi) * 0.5f;
        it.bottom_y = bottom;
        it.origin_y = H.position.y;
        it.tris = std::move(mine);
        if (!it.furniture) {
            shell.insert(shell.end(), it.tris.begin(), it.tris.end());
        }
        items.push_back(std::move(it));
    }

    // ---- УБРАНСТВО, ПРИШЕДШЕЕ ССЫЛКОЙ НА ОБЪЕКТ РЕЕСТРА ([place], 27.08).
    // Предмет, чьё тело запечено один раз (assets/objects/furniture/*.dfo),
    // стоит в локации ИМЕНЕМ, а не чертежом. Судья обязан видеть его тем же
    // предметом: кровать, которую он не видит, — это кровать, сквозь которую
    // он проложит проход к очагу и объявит комнату проходимой.
    //
    // ПОДОШВА ЗДЕСЬ — НИЗ МЕША, а не начало чертежа: у запечённого предмета
    // чертежа уже нет, его начало и есть точка постановки. Для кровати это
    // одно и то же число (её тело идёт от y=0 вверх); для утопленного очага
    // разница была бы существенной — и очаг ссылкой пока не ставится.
    for (const dfn::world::Placement& P : doc.placements) {
        std::optional<dfn::render::RegistryObject> obj;
        for (const std::string& shelf : object_shelves()) {
            obj = dfn::render::read_object(std::filesystem::path(shelf)
                                           / (P.object + ".dfo"));
            if (obj) {
                break;
            }
        }
        if (!obj) {
            finding(path, "ОБЪЕКТ",
                    path + ": [place] объект \"" + P.object
                    + "\" не найден ни на одной полке — судить нечем");
            continue;
        }
        Item it;
        it.file = P.object;
        it.furniture = true; // ссылкой ставится убранство; оболочка — чертежом
        glm::vec2 lo{1e9f};
        glm::vec2 hi{-1e9f};
        float bottom = 1e9f;
        std::vector<Tri> mine;
        for (const dfn::render::HouseSubmesh& sub : obj->house) {
            for (std::size_t i = 0; i + 2 < sub.mesh.indices.size(); i += 3) {
                Tri t;
                t.a = place(sub.mesh.vertices[sub.mesh.indices[i]].position,
                            P.position, P.yaw);
                t.b = place(sub.mesh.vertices[sub.mesh.indices[i + 1]].position,
                            P.position, P.yaw);
                t.c = place(sub.mesh.vertices[sub.mesh.indices[i + 2]].position,
                            P.position, P.yaw);
                mine.push_back(t);
                for (const glm::vec3& p : {t.a, t.b, t.c}) {
                    lo = glm::min(lo, {p.x, p.z});
                    hi = glm::max(hi, {p.x, p.z});
                    bottom = std::min(bottom, p.y);
                }
            }
        }
        if (mine.empty()) {
            // НЕ МОЛЧА: объект без запечённой постройки в локации — это либо
            // дерево, поставленное в комнату, либо предмет, испечённый мимо
            // секции HOUS. И то и другое человек обязан увидеть.
            finding(path, "ОБЪЕКТ",
                    path + ": [place] объект \"" + P.object
                    + "\" не несёт запечённой постройки (секция HOUS пуста)");
            continue;
        }
        it.lo = lo;
        it.hi = hi;
        it.center = (lo + hi) * 0.5f;
        it.bottom_y = bottom;
        it.origin_y = P.position.y;
        it.tris = std::move(mine);
        items.push_back(std::move(it));
    }

    if (shell.empty()) {
        finding(path, "СЦЕНА", path + ": в локации нет оболочки — судить нечего");
        return;
    }

    // ---- ТОЧКА ВХОДА
    glm::vec3 spawn{0.0f};
    bool has_spawn = false;
    if (!doc.spawns.empty()) {
        spawn = doc.spawns.front().position;
        has_spawn = true;
    } else if (doc.has_spawn) {
        spawn = doc.spawn;
        has_spawn = true;
    }
    if (!has_spawn) {
        // ЧЕТВЁРТОЕ УМОЛЧАНИЕ ВСЕГДА ДЕФЕКТ (свод, правило границы): нет
        // входа — в локацию нельзя попасть, и это не «пока не сделали».
        finding(path, "СЦЕНА", path + ": ни одной точки входа ([spawn]) — войти некуда");
        return;
    }

    // ---- СЕТКА ПРОХОДИМОСТИ (рука 1)
    glm::vec2 lo{1e9f};
    glm::vec2 hi{-1e9f};
    for (const Tri& t : shell) {
        for (const glm::vec3& p : {t.a, t.b, t.c}) {
            lo = glm::min(lo, {p.x, p.z});
            hi = glm::max(hi, {p.x, p.z});
        }
    }
    lo -= glm::vec2{1.0f};
    hi += glm::vec2{1.0f};
    Report R;
    R.origin = lo;
    R.grid_w = static_cast<int>(std::ceil((hi.x - lo.x) / GRID_M)) + 1;
    R.grid_h = static_cast<int>(std::ceil((hi.y - lo.y) / GRID_M)) + 1;
    R.free_cell.assign(static_cast<std::size_t>(R.grid_w * R.grid_h), 0);
    R.reached.assign(R.free_cell.size(), 0);

    // Всё, во что можно упереться: оболочка И убранство. Собирается из уже
    // разобранных предметов, а не вторым разбором графов: второй разбор был
    // бы вторым ответом на вопрос «где стоит стол» (правило 39).
    std::vector<Tri> solid;
    for (const Item& it : items) {
        solid.insert(solid.end(), it.tris.begin(), it.tris.end());
    }

    // ПОЛОСА ПРЕПЯТСТВИЙ ОТСЧИТЫВАЕТСЯ ОТ ПОЛА ПОД ЭТОЙ ЯЧЕЙКОЙ, и оба слова
    // куплены отказами.
    //
    // «ОТ ПОЛА, А НЕ ОТ НОГ СПАВНА»: точка входа авторизуется НАД полом (у
    // пилота на 0.08 м), и полоса от неё уезжала вверх на этот зазор — матрас
    // кровати 0.50 при шаге 0.45 попадал в щель между двумя отсчётами и
    // объявлялся переступаемым, то есть кровать поперёк прохода переставала
    // быть препятствием.
    // «ПОД ЭТОЙ ЯЧЕЙКОЙ, А НЕ ОДНО ЧИСЛО НА КОМНАТУ»: половая доска дощатого
    // пола лежит с пазами, и ровно под спавном пилота одиночная проба нашла
    // не верх доски (0.12), а дно паза (0.02). Полоса опустилась на дециметр,
    // лавки высотой 0.54 стали стенами, и прибор объявил недостижимыми угол
    // с полкой и очаг — в комнате, по которой игрок ходит. Одна проба на
    // комнату — это ставка на то, что она попадёт не в паз; карта высот пола
    // ставки не делает, и заодно готова к локации со ступенью.
    std::vector<float> floor_h(R.free_cell.size(), spawn.y);
    for (const Tri& t : shell) {
        const float top = std::max({t.a.y, t.b.y, t.c.y});
        if (top > spawn.y + 0.5f) {
            continue; // это уже не пол, а то, что над головой
        }
        const glm::vec2 a{t.a.x, t.a.z};
        const glm::vec2 b{t.b.x, t.b.z};
        const glm::vec2 c{t.c.x, t.c.z};
        const glm::vec2 tlo = glm::min(a, glm::min(b, c));
        const glm::vec2 thi = glm::max(a, glm::max(b, c));
        const int x0 = std::max(0, cell_of(tlo.x, lo.x));
        const int x1 = std::min(R.grid_w - 1, cell_of(thi.x, lo.x) + 1);
        const int z0 = std::max(0, cell_of(tlo.y, lo.y));
        const int z1 = std::min(R.grid_h - 1, cell_of(thi.y, lo.y) + 1);
        for (int gz = z0; gz <= z1; ++gz) {
            for (int gx = x0; gx <= x1; ++gx) {
                const glm::vec2 p{lo.x + (static_cast<float>(gx) + 0.5f) * GRID_M,
                                  lo.y + (static_cast<float>(gz) + 0.5f) * GRID_M};
                if (dist2_point_tri_xz(p, a, b, c) > 1e-6f) {
                    continue;
                }
                float& h = floor_h[static_cast<std::size_t>(gz * R.grid_w + gx)];
                h = std::max(h, top);
            }
        }
    }
    // Пазы доски всё же остаются ямками в один-два сэмпла: пол ячейки берётся
    // как МАКСИМУМ по ней и её соседям — половица шире паза, и сосед всегда
    // стоит на доске. Это сглаживание ПОЛА, а не препятствий: препятствия
    // по-прежнему мерятся каждое своё.
    {
        std::vector<float> smooth = floor_h;
        for (int gz = 0; gz < R.grid_h; ++gz) {
            for (int gx = 0; gx < R.grid_w; ++gx) {
                float h = floor_h[static_cast<std::size_t>(gz * R.grid_w + gx)];
                for (int dz2 = -1; dz2 <= 1; ++dz2) {
                    for (int dx2 = -1; dx2 <= 1; ++dx2) {
                        const int nx = gx + dx2;
                        const int nz = gz + dz2;
                        if (nx < 0 || nz < 0 || nx >= R.grid_w || nz >= R.grid_h) {
                            continue;
                        }
                        h = std::max(h,
                            floor_h[static_cast<std::size_t>(nz * R.grid_w + nx)]);
                    }
                }
                smooth[static_cast<std::size_t>(gz * R.grid_w + gx)] = h;
            }
        }
        floor_h.swap(smooth);
    }
    float band_lo = spawn.y + STEP_H;    // для печати карты
    float band_hi = spawn.y + CAPSULE_H;
    // ОТ ТРЕУГОЛЬНИКА К ЯЧЕЙКАМ, А НЕ ОТ ЯЧЕЙКИ КО ВСЕМ ТРЕУГОЛЬНИКАМ.
    // Комната — это ~6 тысяч треугольников на ~20 тысяч ячеек; прямой перебор
    // стоил бы сто миллионов проверок за прогон, и приёмку перестали бы
    // запускать. Каждый треугольник помечает только свой раздутый габарит.
    std::fill(R.free_cell.begin(), R.free_cell.end(), 1);
    for (const Tri& t : solid) {
        const float ty_lo = std::min({t.a.y, t.b.y, t.c.y});
        const float ty_hi = std::max({t.a.y, t.b.y, t.c.y});
        const glm::vec2 a{t.a.x, t.a.z};
        const glm::vec2 b{t.b.x, t.b.z};
        const glm::vec2 c{t.c.x, t.c.z};
        const glm::vec2 tlo = glm::min(a, glm::min(b, c)) - glm::vec2{CAPSULE_R};
        const glm::vec2 thi = glm::max(a, glm::max(b, c)) + glm::vec2{CAPSULE_R};
        const int x0 = std::max(0, cell_of(tlo.x, lo.x));
        const int x1 = std::min(R.grid_w - 1, cell_of(thi.x, lo.x) + 1);
        const int z0 = std::max(0, cell_of(tlo.y, lo.y));
        const int z1 = std::min(R.grid_h - 1, cell_of(thi.y, lo.y) + 1);
        for (int gz = z0; gz <= z1; ++gz) {
            for (int gx = x0; gx <= x1; ++gx) {
                const std::size_t n = static_cast<std::size_t>(gz * R.grid_w + gx);
                if (R.free_cell[n] == 0) {
                    continue;
                }
                const float f = floor_h[n];
                if (ty_hi < f + STEP_H || ty_lo > f + CAPSULE_H) {
                    continue; // ниже шага — переступается; выше — над головой
                }
                const glm::vec2 p{lo.x + (static_cast<float>(gx) + 0.5f) * GRID_M,
                                  lo.y + (static_cast<float>(gz) + 0.5f) * GRID_M};
                if (dist2_point_tri_xz(p, a, b, c) <= CAPSULE_R * CAPSULE_R) {
                    R.free_cell[n] = 0;
                }
            }
        }
    }

    const int sx = cell_of(spawn.x, lo.x);
    const int sz = cell_of(spawn.z, lo.y);
    if (sx < 0 || sz < 0 || sx >= R.grid_w || sz >= R.grid_h
        || R.free_cell[static_cast<std::size_t>(sz * R.grid_w + sx)] == 0) {
        finding(path, "Р1-ВХОД-ЗАНЯТ",
                path + ": точка входа (" + std::to_string(spawn.x) + ", "
                + std::to_string(spawn.z) + ") сама непроходима — капсула "
                "игрока в неё не встаёт");
        return;
    }
    std::vector<int> stack{sz * R.grid_w + sx};
    R.reached[static_cast<std::size_t>(stack.front())] = 1;
    while (!stack.empty()) {
        const int cur = stack.back();
        stack.pop_back();
        const int cx = cur % R.grid_w;
        const int cz = cur / R.grid_w;
        const int dx[4] = {1, -1, 0, 0};
        const int dz[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; ++k) {
            const int nx = cx + dx[k];
            const int nz = cz + dz[k];
            if (nx < 0 || nz < 0 || nx >= R.grid_w || nz >= R.grid_h) {
                continue;
            }
            const std::size_t n = static_cast<std::size_t>(nz * R.grid_w + nx);
            if (R.free_cell[n] == 0 || R.reached[n] != 0) {
                continue;
            }
            R.reached[n] = 1;
            stack.push_back(static_cast<int>(n));
        }
    }

    if (dump_map) {
        // КАРТА ПРОХОДИМОСТИ — глазами. Прибор, объявивший комнату
        // непроходимой, обязан уметь ПОКАЗАТЬ, где он видит стену: спор
        // «прибор врёт / комната тесна» иначе не разрешается никак.
        std::fprintf(stdout,
                     "# карта проходимости %s (шаг %.2f м, начало %.2f %.2f, "
                     "полоса препятствий %.2f..%.2f):\n",
                     path.c_str(), static_cast<double>(GRID_M),
                     static_cast<double>(lo.x), static_cast<double>(lo.y),
                     static_cast<double>(band_lo), static_cast<double>(band_hi));
        for (int gz = 0; gz < R.grid_h; ++gz) {
            std::string row;
            for (int gx = 0; gx < R.grid_w; ++gx) {
                const std::size_t n = static_cast<std::size_t>(gz * R.grid_w + gx);
                row += R.reached[n] != 0 ? '.' : (R.free_cell[n] != 0 ? 'o' : '#');
            }
            std::fprintf(stdout, "%s\n", row.c_str());
        }
    }

    const auto reachable_near = [&](glm::vec2 p, float radius) {
        const int span = static_cast<int>(std::ceil(radius / GRID_M)) + 1;
        const int cx = cell_of(p.x, lo.x);
        const int cz = cell_of(p.y, lo.y);
        for (int dz2 = -span; dz2 <= span; ++dz2) {
            for (int dx2 = -span; dx2 <= span; ++dx2) {
                const int nx = cx + dx2;
                const int nz = cz + dz2;
                if (nx < 0 || nz < 0 || nx >= R.grid_w || nz >= R.grid_h) {
                    continue;
                }
                if (R.reached[static_cast<std::size_t>(nz * R.grid_w + nx)] != 0) {
                    return true;
                }
            }
        }
        return false;
    };

    // РУКА 1: до каждого предмета убранства можно ДОЙТИ. Мерится подход к
    // следу, а не сам след: внутрь бочки игрок и не должен вставать.
    for (const Item& it : items) {
        if (!it.furniture) {
            continue;
        }
        const float reach = 0.5f * glm::length(it.hi - it.lo) + CAPSULE_R + GRID_M;
        if (!reachable_near(it.center, reach)) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "%s: РУКА 1 — до «%s» на (%.2f, %.2f) от [spawn] не "
                          "дойти: путь перекрыт",
                          path.c_str(),
                          std::filesystem::path(it.file).stem().string().c_str(),
                          static_cast<double>(it.center.x),
                          static_cast<double>(it.center.y));
            finding(path, "Р1-НЕ-ДОЙТИ", buf);
        }
    }

    // РУКА 2: мебель НА ПОЛУ.
    for (const Item& it : items) {
        if (!it.furniture) {
            continue;
        }
        // ОПОРА — ПОЛ ИЛИ ВЕРХ ДРУГОГО ПРЕДМЕТА, и это не поблажка: пламя
        // очага стоит на очаге, свеча на столе, и судья, знающий только пол,
        // объявил бы висящим каждый огонь в игре. Тот же довод, что у
        // Scene.h: «опора = земля ИЛИ верх другого члена группы».
        std::vector<Tri> support = shell;
        for (const Item& other : items) {
            if (&other == &it || !other.furniture) {
                continue;
            }
            support.insert(support.end(), other.tris.begin(), other.tris.end());
        }
        float floor_y = 0.0f;
        // ОПОРА ИЩЕТСЯ НЕ ВЫШЕ САМОГО ПРЕДМЕТА. Без этой границы «полом» под
        // очагом становилось пламя, СТОЯЩЕЕ НА НЁМ, и очаг оказывался «ниже
        // пола на 0.45 м» — прибор мерил предмет относительно того, что этот
        // предмет держит.
        bool have_floor =
            floor_under(support, it.center, it.origin_y + FLOOR_TOL_M, floor_y);
        if (!have_floor) {
            // ВКОПАННЫЙ ПРЕДМЕТ ОБЯЗАН ПОЛУЧИТЬ ЧИСЛО, А НЕ «ПОЛА НЕТ ВОВСЕ».
            // Ниже своего пола не нашлось ничего именно потому, что предмет
            // утоплен; тогда опора ищется выше — и находка называет глубину,
            // по которой можно действовать, вместо загадки.
            have_floor = floor_under(support, it.center,
                                     it.origin_y + CAPSULE_H, floor_y);
        }
        if (!have_floor) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "%s: РУКА 2 — под «%s» на (%.2f, %.2f) нет пола вовсе",
                          path.c_str(),
                          std::filesystem::path(it.file).stem().string().c_str(),
                          static_cast<double>(it.center.x),
                          static_cast<double>(it.center.y));
            finding(path, "Р2-НЕТ-ПОЛА", buf);
            continue;
        }
        const float delta = it.origin_y - floor_y;
        if (std::fabs(delta) > FLOOR_TOL_M) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "%s: РУКА 2 — «%s» на (%.2f, %.2f) %s пола на %.2f м",
                          path.c_str(),
                          std::filesystem::path(it.file).stem().string().c_str(),
                          static_cast<double>(it.center.x),
                          static_cast<double>(it.center.y),
                          delta < 0.0f ? "НИЖЕ" : "ВЫШЕ",
                          static_cast<double>(std::fabs(delta)));
            // КЛАСС РАЗЛИЧАЕТ ВЕРХ И НИЗ: «висит над полом» и «утоплен в
            // пол» чинятся в разных местах, и складывать их в один
            // счёт списка известных значило бы прятать одно за другим.
            finding(path, delta < 0.0f ? "Р2-НИЖЕ" : "Р2-ВЫШЕ", buf);
        }
    }

    // РУКА 5: переход достижим. Дверь, до которой нельзя дойти, — это
    // локация, из которой нельзя выйти.
    if (doc.portals.empty()) {
        finding(path, "Р5-НЕТ-ВЫХОДА",
                path + ": РУКА 5 — ни одного [portal]: из локации нет выхода");
    }
    for (std::size_t i = 0; i < doc.portals.size(); ++i) {
        const dfn::world::ScenePortal& P = doc.portals[i];
        if (!reachable_near({P.at.x, P.at.z}, P.radius_m + CAPSULE_R)) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "%s: РУКА 5 — переход %zu на (%.2f, %.2f) недостижим "
                          "от [spawn]: дверь за стеной",
                          path.c_str(), i, static_cast<double>(P.at.x),
                          static_cast<double>(P.at.z));
            finding(path, "Р5-НЕДОСТИЖИМ", buf);
        }
    }

    // РУКА 7: бюджет огня.
    std::size_t lit = 0;
    std::size_t shadows = 0;
    for (const dfn::world::SceneLight& L : doc.lights) {
        if (L.radius_m <= 0.0f) {
            continue;
        }
        ++lit;
        shadows += L.casts_shadow ? 1u : 0u;
    }
    if (lit > LIGHT_BUDGET) {
        char buf[200];
        std::snprintf(buf, sizeof(buf),
                      "%s: РУКА 7 — настоящих [light] %zu, бюджет %zu",
                      path.c_str(), lit, LIGHT_BUDGET);
        finding(path, "Р7-ЛАМП", buf);
    }
    if (shadows > SHADOW_BUDGET) {
        char buf[200];
        std::snprintf(buf, sizeof(buf),
                      "%s: РУКА 7 — теневых ламп %zu, бюджет %zu",
                      path.c_str(), shadows, SHADOW_BUDGET);
        finding(path, "Р7-ТЕНЕЙ", buf);
    }
}

/// РУКА 6: парность interior= <-> файл локации <-> ^back, и уникальность
/// слагов. Мерится по ГОРОДУ: сирота с той стороны отсюда не видна.
void check_city(const std::string& city_path) {
    SceneDoc city;
    std::string err;
    if (!dfn::world::read_scene(city_path, city, err)) {
        finding(city_path, "СЦЕНА", city_path + ": город не прочитался — " + err);
        return;
    }
    std::map<std::string, std::size_t> by_interior;
    for (const dfn::world::ScenePlacedHouse& H : city.houses) {
        if (H.interior.empty()) {
            continue; // оболочка с дверью-декорацией — ЗАКОННОЕ умолчание
        }
        by_interior[H.interior]++;
        if (!std::filesystem::exists(H.interior)) {
            finding(city_path, "Р6-НЕТ-ФАЙЛА",
                    city_path + ": РУКА 6 — interior=" + H.interior
                    + " указывает в никуда (файла нет)");
            continue;
        }
        SceneDoc loc;
        std::string lerr;
        if (!dfn::world::read_scene(H.interior, loc, lerr)) {
            finding(city_path, "Р6-НЕ-ЧИТАЕТСЯ",
                    city_path + ": РУКА 6 — " + H.interior + " не читается: " + lerr);
            continue;
        }
        bool has_back = false;
        for (const dfn::world::ScenePortal& P : loc.portals) {
            has_back = has_back || dfn::world::portal_is_back(P);
        }
        if (!has_back) {
            finding(H.interior, "Р6-НЕТ-ОБРАТНОГО",
                    H.interior + ": РУКА 6 — нет обратного перехода (to = ^back): "
                    "войти можно, выйти нельзя");
        }
        bool addressed = false;
        for (const dfn::world::ScenePortal& P : city.portals) {
            addressed = addressed || P.to == H.interior;
        }
        if (!addressed) {
            // ВТОРОЙ ЗАКОННЫЙ СПОСОБ ВОЙТИ (И15 волна Б). Строка [portal] в
            // сцене города — не единственный вход: у запечатанной постройки
            // (sealed) переход заводит САМА заливка, по мировым координатам
            // дверного полотна её чертежа. Так и должно быть — где дверь,
            // знает только геометрия, а вторая, генераторная запись той же
            // точки была бы вторым ответом на один вопрос (правило 39).
            // Приёмка обязана мерить ту же пару: sealed + створка в чертеже.
            dfn::world::HouseGraph g;
            bool leaf = false;
            if (H.sealed && read_graph(H.file, g)) {
                for (const dfn::world::Element& e : g.elements()) {
                    leaf = leaf || g.param(e.id, "door") == "1";
                }
            }
            addressed = leaf;
            if (!addressed) {
                finding(city_path, "Р6-НЕТ-ВХОДА",
                        city_path + ": РУКА 6 — у дома есть interior="
                        + H.interior
                        + ", но войти некуда: ни [portal] города туда не "
                          "ведёт, ни у самой постройки нет пары "
                          "«sealed + створка door=1»");
            }
        }
    }
    // СИРОТА С ДРУГОЙ СТОРОНЫ: портал ведёт туда, где никакой дом не объявлял
    // внутренности.
    for (const dfn::world::ScenePortal& P : city.portals) {
        if (dfn::world::portal_is_back(P)) {
            continue;
        }
        if (by_interior.find(P.to) == by_interior.end()) {
            finding(city_path, "Р6-СИРОТА",
                    city_path + ": РУКА 6 — [portal] ведёт в " + P.to
                    + ", но ни один [house] не объявлял его своим interior=");
        }
    }
    // УНИКАЛЬНОСТЬ СЛАГОВ: два дома, делящих одну локацию, — это два дома с
    // одной кроватью, и сохранение не сможет их различить.
    for (const auto& [file, count] : by_interior) {
        if (count > 1) {
            finding(city_path, "Р6-СЛАГ",
                    city_path + ": РУКА 6 — локацию " + file + " объявили своей "
                    + std::to_string(count) + " дома: слаг НЕ уникален");
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> scenes;
    std::string city;
    std::string known_path;
    dfn::tools::Expectations expect;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--city") == 0 && i + 1 < argc) {
            city = argv[++i];
        } else if (std::strcmp(argv[i], "--map") == 0) {
            dump_map = true;
        } else if (std::strcmp(argv[i], "--known") == 0 && i + 1 < argc) {
            known_path = argv[++i];
        } else if (std::strcmp(argv[i], "--expect") == 0 && i + 1 < argc) {
            expect.add(argv[++i]);
        } else if (std::filesystem::is_directory(argv[i])) {
            // КАТАЛОГ ЦЕЛИКОМ — РАДИ ctest, И ЭТО НЕ УДОБСТВО. Раскрытие
            // «*.scene» делает ОБОЛОЧКА, а CMake оболочки не запускает: без
            // этой ветки add_test пришлось бы писать список из ста тридцати
            // имён — то есть список, который устареет в день, когда город
            // получит сто тридцать первую локацию, и устареет МОЛЧА.
            std::vector<std::string> got;
            for (const auto& de : std::filesystem::directory_iterator(argv[i])) {
                if (de.path().extension() == ".scene") {
                    got.push_back(de.path().string());
                }
            }
            std::sort(got.begin(), got.end());
            scenes.insert(scenes.end(), got.begin(), got.end());
        } else {
            scenes.emplace_back(argv[i]);
        }
    }
    if (scenes.empty() && city.empty()) {
        std::fprintf(stderr,
                     "usage: dfn_interior_check <локация.scene | каталог ...> "
                     "[--city <город.scene>] [--known список.txt] "
                     "[--expect N|КЛАСС:N] [--map]\n");
        return 2;
    }
    dfn::tools::KnownFindings known;
    {
        std::string err;
        if (!known.load(known_path, err)) {
            std::fprintf(stderr, "%s\n", err.c_str());
            return 2;
        }
    }
    for (const std::string& s : scenes) {
        check_scene_file(s);
    }
    if (!city.empty()) {
        check_city(city);
    }
    // ИЗВЕСТНОЕ ОТДЕЛЯЕТСЯ ОТ БОЕВОГО ЗДЕСЬ (tools/known_findings.h): пока
    // геометрию чинит соседняя волна, прибору нужен ТРЕТИЙ ответ между
    // «чисто» и «красный», иначе его либо выключат, либо перестанут читать.
    long live = 0;
    long known_hits = 0;
    std::map<std::string, long> by_class;
    for (const Finding& f : findings) {
        std::string why;
        if (known.take(f.body, f.cls, &why)) {
            ++known_hits;
            std::fprintf(stdout, "%s  [ИЗВЕСТНОЕ, %s]\n", f.text.c_str(),
                         why.c_str());
            continue;
        }
        std::fprintf(stdout, "%s\n", f.text.c_str());
        ++live;
        by_class[f.cls] += 1;
    }
    const int stale = known.report_stale(stdout);
    std::fprintf(stdout,
                 "локаций %zu, боевых находок %ld, известных %ld, протухших "
                 "строк списка %d\n",
                 scenes.size(), live, known_hits, stale);
    if (expect.any()) {
        // ПЛЕЧО ИСПОРЧЕННОГО ИНТЕРЬЕРА (свод, дельта-5). На верной локации у
        // этих рук ТОЛЬКО отрицательное плечо — находок ноль по построению, —
        // и молчание неотличимо от неработающего прибора. Оснастка требует
        // РОВНО столько находок, сколько в неё заложено дефектов: и меньше, и
        // БОЛЬШЕ — отказ прибора.
        return expect.verdict(live, by_class, stderr);
    }
    return live == 0 ? 0 : 1;
}
