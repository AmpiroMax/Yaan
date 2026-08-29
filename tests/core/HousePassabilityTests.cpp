/*
Module: tests/core
File: tests/core/HousePassabilityTests.cpp

Responsibility:
- СУДЬЯ ПРОХОДИМОСТИ ГОТОВЫХ ПОСТРОЕК (заказ 20.08: «проверить, что персонаж
  может пройти по всем комнатам, все двери открыть и вернуться обратно на
  улицу»). Капсула игрока шагает МАРШРУТОМ по дому, собранному из настоящего
  .dfh кузницы: опора считается по верхам тел коллайдера, зазор — пересечением
  с ними; дверные створки исключены, как их исключает коллайдер приложения.

Dependencies:
- Uses: dfn_world (HouseFile, HouseMesh); артефакты assets/houses/*.dfh.
- Used by: ctest (test_house_passability).

Notes:
- ЧИСЛА ИГРОКА — docs/NUMBERS.md: капсула 1.8 м, радиус 0.35 (здесь 0.3 —
  игрок не обязан тереться плечами о косяк, а тест не обязан мерить впритык).
- ТЕЛА ОЦЕНИВАЮТСЯ ГАБАРИТАМИ (AABB): дома кузницы стоят по осям, и габарит
  короба равен коробу. Дом под поворотом этот прибор мерить НЕ умеет —
  скажет об этом первым же ложным «упёрся».
- КОНТРОЛЬНОЕ ПЛЕЧО (правило 30): маршрут сквозь глухую северную стену обязан
  упереться — иначе прибор не видит стен и все зелёные маршруты ничего не
  значат.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Правки домов — в tools/forge_houses.cpp
  с перегенерацией артефактов; маршруты здесь описывают ЗАКАЗАННЫЕ комнаты.
*/

#include <doctest/doctest.h>

#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseGraph.h"
#include "engine/world/sources/HouseMesh.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace {

using dfn::world::HouseGraph;
using dfn::world::HouseMesh;

constexpr float CAPSULE_H = 1.8f;
constexpr float CAPSULE_R = 0.3f;
constexpr float STEP_UP_M = 0.35f; ///< больше подъёма ступени 0.175, меньше подоконника

struct Aabb {
    glm::vec3 lo{1e9f};
    glm::vec3 hi{-1e9f};
};

struct WalkWorld {
    std::vector<Aabb> solids;
    int door_leaves = 0;
};

WalkWorld load_house(const char* path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), path);
    std::stringstream ss;
    ss << in.rdbuf();
    HouseGraph g;
    const auto io = dfn::world::read_house(ss.str(), g);
    REQUIRE_MESSAGE(io.ok, io.why);
    const HouseMesh m = dfn::world::build_house_mesh(g);
    WalkWorld w;
    for (const auto& e : g.elements()) {
        if (g.param(e.id, "door") == "1") {
            ++w.door_leaves;
        }
    }
    for (const auto& c : m.convex) {
        // Створки в коллайдер не входят — ровно как в приложении: дверь
        // открывается, статичное тело в проёме держало бы человека всегда.
        const auto* e = g.element(c.element);
        if (e != nullptr && g.param(e->id, "door") == "1") {
            continue;
        }
        // ВЫПУКЛОЕ ТЕЛО РЕЖЕТСЯ НА ДОЛЬКИ вдоль своей длинной оси. Общий
        // габарит наклонной плиты (тетива марша, скат крыши) встал бы стеной
        // во всю высоту; границы дольки выпуклого тела достигаются на рёбрах,
        // а все рёбра — подмножество пар точек, поэтому дольки ТОЧНЫЕ.
        Aabb whole;
        for (const auto& q : c.points) {
            whole.lo = glm::min(whole.lo, q);
            whole.hi = glm::max(whole.hi, q);
        }
        const glm::vec3 ext = whole.hi - whole.lo;
        int dim = 0;
        if (ext.y > ext.x) { dim = 1; }
        if (ext.z > ext[dim]) { dim = 2; }
        constexpr int SLICES = 10;
        if (ext[dim] < 1.0f) {
            w.solids.push_back(whole);
        } else {
            for (int k = 0; k < SLICES; ++k) {
                const float c0 = whole.lo[dim] + ext[dim] * k / SLICES;
                const float c1 = whole.lo[dim] + ext[dim] * (k + 1) / SLICES;
                Aabb box;
                bool any = false;
                const auto take = [&](const glm::vec3& q) {
                    box.lo = glm::min(box.lo, q);
                    box.hi = glm::max(box.hi, q);
                    any = true;
                };
                for (const auto& q : c.points) {
                    if (q[dim] >= c0 && q[dim] <= c1) {
                        take(q);
                    }
                }
                for (std::size_t i = 0; i < c.points.size(); ++i) {
                    for (std::size_t j = i + 1; j < c.points.size(); ++j) {
                        const glm::vec3 a = c.points[i];
                        const glm::vec3 b = c.points[j];
                        const float da = a[dim];
                        const float db = b[dim];
                        if (std::fabs(db - da) < 1e-6f) {
                            continue;
                        }
                        for (const float cc : {c0, c1}) {
                            const float t = (cc - da) / (db - da);
                            if (t > 0.0f && t < 1.0f) {
                                take(a + (b - a) * t);
                            }
                        }
                    }
                }
                if (any) {
                    w.solids.push_back(box);
                }
            }
        }
    }
    return w;
}

/// Верх опоры под точкой: пол, ступень, настил. Земля мира — y=0.
float support_at(const WalkWorld& w, float x, float z, float y_now) {
    // КРЕСТ ИЗ ПЯТИ ТОЧЕК: капсула опирается ПЛОЩАДЬЮ, и зазор 3 см между
    // блоками ступени (open=2) не роняет её в щель — как не роняет игрока.
    float best = 0.0f;
    const float r = 0.12f;
    const float px[5] = {x, x + r, x - r, x, x};
    const float pz[5] = {z, z, z, z + r, z - r};
    for (const Aabb& b : w.solids) {
        for (int k = 0; k < 5; ++k) {
            if (px[k] < b.lo.x || px[k] > b.hi.x || pz[k] < b.lo.z
                || pz[k] > b.hi.z) {
                continue;
            }
            if (b.hi.y <= y_now + STEP_UP_M && b.hi.y > best) {
                best = b.hi.y;
            }
            break;
        }
    }
    return best;
}

/// Капсула (столбик над коленом) против тел: пересёкся — стоп.
bool blocked_at(const WalkWorld& w, float x, float z, float y) {
    const float eps = 0.01f;
    for (const Aabb& b : w.solids) {
        const bool xz = x - CAPSULE_R < b.hi.x - eps && x + CAPSULE_R > b.lo.x + eps
                     && z - CAPSULE_R < b.hi.z - eps && z + CAPSULE_R > b.lo.z + eps;
        if (!xz) {
            continue;
        }
        if (y + STEP_UP_M < b.hi.y - eps && y + CAPSULE_H > b.lo.y + eps) {
            return true;
        }
    }
    return false;
}

struct WalkResult {
    bool ok = true;
    glm::vec3 where{0.0f};
    float peak_y = 0.0f;
    float end_y = 0.0f;
};

WalkResult walk(const WalkWorld& w, const std::vector<glm::vec2>& route) {
    WalkResult r;
    float y = support_at(w, route.front().x, route.front().y, 1.0f);
    for (std::size_t i = 0; i + 1 < route.size(); ++i) {
        const glm::vec2 a = route[i];
        const glm::vec2 b = route[i + 1];
        const float len = glm::length(b - a);
        const int steps = std::max(1, static_cast<int>(len / 0.1f));
        for (int s = 0; s <= steps; ++s) {
            const glm::vec2 p = a + (b - a) * (static_cast<float>(s) / steps);
            y = support_at(w, p.x, p.y, y);
            r.peak_y = std::max(r.peak_y, y);
            if (blocked_at(w, p.x, p.y, y)) {
                r.ok = false;
                r.where = {p.x, y, p.y};
                return r;
            }
        }
    }
    r.end_y = y;
    return r;
}

} // namespace

TEST_CASE("Г-образный: улица - комната - дверь - вторая комната - улица") {
    const WalkWorld w = load_house("assets/houses/l-house.dfh");
    REQUIRE_FALSE(w.solids.empty());
    // Двери: входная + межкомнатная.
    CHECK(w.door_leaves == 2);
    const WalkResult r = walk(w, {
        {2.0f, 10.0f}, // улица южнее входа
        {2.0f, 8.0f},  // входной проём
        {2.0f, 6.0f},  // комната западного крыла
        {2.0f, 4.0f},  // межкомнатная дверь
        {2.0f, 2.0f},  // комната северного бара
        {6.5f, 2.0f},  // восточный конец бара
        {2.0f, 2.0f},  // обратно
        {2.0f, 6.0f},
        {2.0f, 10.0f}, // снова улица
    });
    CHECK_MESSAGE(r.ok, "упёрся в (" << r.where.x << ' ' << r.where.y << ' '
                                     << r.where.z << ")");
    CHECK(r.end_y < 0.2f); // вернулся на землю

    // КОНТРОЛЬНОЕ ПЛЕЧО: сквозь глухую северную стену прибор обязан упереться.
    const WalkResult blocked = walk(w, {{2.0f, 2.0f}, {2.0f, -2.0f}});
    CHECK_FALSE(blocked.ok);
}

TEST_CASE("П-образный: все комнаты двух этажей, обе лестницы, двор, улица") {
    const WalkWorld w = load_house("assets/houses/u-house.dfh");
    REQUIRE_FALSE(w.solids.empty());
    // Створки: 2 входа + двор + 4 межкомнатных (по двери на крыло на этаж).
    CHECK(w.door_leaves == 7);
    // К МАРШАМ — С ТОРЦА, через двери — ПО ЦЕНТРУ проёма: капсула честная,
    // и бок лестницы для неё такая же стена, как и для игрока.
    const WalkResult r = walk(w, {
        {2.1f, 12.0f}, // улица
        {2.1f, 10.0f}, // главный вход, западное крыло
        {2.5f, 7.0f},  // комната западного крыла, 1 этаж
        {2.1f, 4.0f},  // дверь в бар
        {2.1f, 2.0f},  // бар, западный конец
        {7.0f, 2.0f},  // бар, середина
        {7.0f, 4.0f},  // дверь во двор
        {7.0f, 6.5f},  // ДВОР
        {7.0f, 4.0f},  // назад в бар
        {7.0f, 2.0f},
        {11.9f, 2.0f}, // бар, восточный конец
        {12.0f, 4.0f}, // дверь в восточное крыло
        {11.4f, 7.0f}, // комната восточного крыла, 1 этаж
        {11.4f, 9.45f}, // на юг вдоль комнаты, мимо бока марша
        {13.0f, 9.45f}, // подножие восточного марша (с торца)
        {13.0f, 5.15f}, // ВВЕРХ по маршу на 2 этаж
        {13.0f, 4.5f}, // площадка
        {11.0f, 4.6f},
        {11.0f, 6.5f}, // комната-балкон восточного крыла, 2 этаж
        {11.0f, 4.6f},
        {12.0f, 4.6f}, // к центру проёма
        {12.0f, 3.4f}, // дверь в бар 2 этажа — насквозь по центру
        {7.0f, 2.0f},  // бар, 2 этаж
        {2.0f, 3.4f},
        {2.0f, 4.0f},  // дверь в западное крыло 2 этажа
        {3.0f, 6.5f},  // комната-балкон западного крыла, 2 этаж
        {2.5f, 4.6f},
        {1.0f, 4.5f},  // площадка западного марша
        {1.0f, 5.15f}, // ВНИЗ по маршу
        {1.0f, 9.45f},
        {2.1f, 9.45f}, // к выходу
        {2.1f, 10.0f}, // главный вход изнутри
        {2.1f, 12.0f}, // улица
    });
    CHECK_MESSAGE(r.ok, "упёрся в (" << r.where.x << ' ' << r.where.y << ' '
                                     << r.where.z << ")");
    CHECK(r.peak_y > 2.7f);  // на втором этаже БЫЛ
    CHECK(r.end_y < 0.2f);   // и вернулся на землю

    // КОНТРОЛЬ: сквозь наружную западную стену не пройти.
    const WalkResult blocked = walk(w, {{2.0f, 2.0f}, {-2.0f, 2.0f}});
    CHECK_FALSE(blocked.ok);
}

TEST_CASE("повторы демки: в каждый дом можно войти, дверь-створка есть") {
    for (const char* path : {"assets/houses/log-replica.dfh",
                             "assets/houses/frame-replica.dfh",
                             "assets/houses/stone-replica.dfh"}) {
        CAPTURE(path);
        const WalkWorld w = load_house(path);
        CHECK(w.door_leaves >= 1);
        // Дверь у всех трёх в середине южной стены (z = 4): улица - середина
        // дома - улица.
        const float cx = path == std::string("assets/houses/log-replica.dfh")
                             ? 3.0f
                             : 4.5f;
        const WalkResult r = walk(w, {{cx, 6.0f}, {cx, 2.0f}, {cx, 6.0f}});
        CHECK_MESSAGE(r.ok, path << ": упёрся в (" << r.where.x << ' '
                                 << r.where.y << ' ' << r.where.z << ")");
    }
}
