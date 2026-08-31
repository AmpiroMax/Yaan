/*
Module: engine/app
File: engine/app/sources/FurnitureSeats.cpp

Responsibility:
- Правило «самая широкая горизонтальная площадка», классификатор сиденье/лежак,
  перенос найденной площадки в мировую точку позы и ТОЧКА СТАРТА ПОЗЫ — где
  человек стоит, чтобы сесть (лечь) без перекоса (контракт в заголовке).

Dependencies:
- Uses: FurnitureSeats.h, SeatAim.h, glm.
- Used by: AppSeats.cpp, tests/app/FurnitureSeatsTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона lead владеет этим файлом.
*/

#include "engine/app/sources/FurnitureSeats.h"

#include <algorithm>
#include <cmath>
#include <map>

#include <glm/geometric.hpp>

namespace dfn::app {

float FurnSurface::long_side() const {
    return std::max(hi.x - lo.x, hi.y - lo.y);
}

float FurnSurface::short_side() const {
    return std::min(hi.x - lo.x, hi.y - lo.y);
}

glm::vec2 FurnSurface::long_axis() const {
    return (hi.x - lo.x) >= (hi.y - lo.y) ? glm::vec2{1.0f, 0.0f} : glm::vec2{0.0f, 1.0f};
}

FurnSurface furniture_surface(std::span<const glm::vec3> positions,
                              std::span<const std::uint32_t> indices) {
    struct Bin {
        double area = 0.0;
        double top_sum = 0.0; // площадь-взвешенная высота: полка не идеально ровна
        glm::vec2 lo{1.0e9f};
        glm::vec2 hi{-1.0e9f};
    };
    // Упорядоченная, не хеш: обход по возрастанию высоты одинаков на каждом
    // прогоне (правило 13.2), и равные площади разрешаются вниз, а не как
    // выйдет у хеша.
    std::map<int, Bin> bins;

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const std::uint32_t ia = indices[i];
        const std::uint32_t ib = indices[i + 1];
        const std::uint32_t ic = indices[i + 2];
        if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
            continue;
        }
        const glm::vec3 a = positions[ia];
        const glm::vec3 b = positions[ib];
        const glm::vec3 c = positions[ic];
        const glm::vec3 n = glm::cross(b - a, c - a); // длина = 2 x площадь
        const float len = glm::length(n);
        if (len < 1.0e-9f) {
            continue;
        }
        // СМОТРИТ ВВЕРХ И ПОЧТИ ПЛОСКО. На площадку садятся сверху, значит
        // нижняя грань настила (нормаль вниз) не в счёт — иначе матрас и его
        // изнанка удвоили бы площадь одной и той же полки.
        const float up = n.y / len;
        if (up < 0.98f) {
            continue;
        }
        const float y = (a.y + b.y + c.y) / 3.0f;
        Bin& bin = bins[static_cast<int>(std::floor(y / SURFACE_BIN_M))];
        const double area = 0.5 * static_cast<double>(len);
        bin.area += area;
        bin.top_sum += area * static_cast<double>(y);
        for (const glm::vec3& p : {a, b, c}) {
            bin.lo = glm::min(bin.lo, glm::vec2{p.x, p.z});
            bin.hi = glm::max(bin.hi, glm::vec2{p.x, p.z});
        }
    }

    FurnSurface out;
    for (const auto& [key, bin] : bins) {
        if (bin.area <= static_cast<double>(out.area_m2)) {
            continue;
        }
        out.found = true;
        out.area_m2 = static_cast<float>(bin.area);
        out.top_y = static_cast<float>(bin.top_sum / bin.area);
        out.lo = bin.lo;
        out.hi = bin.hi;
    }
    return out;
}

SpotKind classify_surface(const FurnSurface& s) {
    if (!s.found) {
        return SpotKind::None;
    }
    const float lng = s.long_side();
    const float shrt = s.short_side();
    if (s.top_y >= LIE_MIN_M && s.top_y <= LIE_MAX_M && lng >= LIE_MIN_LONG_M
        && shrt >= LIE_MIN_SHORT_M) {
        return SpotKind::Lie;
    }
    if (s.top_y >= SEAT_MIN_M && s.top_y <= SEAT_MAX_M && lng >= SEAT_MIN_LONG_M
        && shrt <= SEAT_MAX_SHORT_M) {
        return SpotKind::Seat;
    }
    if (s.top_y > TABLE_MIN_M && s.top_y <= TABLE_MAX_M && lng >= TABLE_MIN_LONG_M
        && shrt >= TABLE_MIN_SHORT_M) {
        return SpotKind::Table;
    }
    return SpotKind::None;
}

namespace {

/// Местное направление в мировое по конвенции сцен: местный +X уходит в
/// (cos, 0, -sin), местный +Z — в (sin, 0, cos).
[[nodiscard]] glm::vec3 to_world_dir(const glm::vec2& local_xz, float yaw) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return {local_xz.x * c + local_xz.y * s, 0.0f, -local_xz.x * s + local_xz.y * c};
}

} // namespace

FurnitureSpot furniture_spot(const FurnSurface& s, SpotKind kind,
                             const glm::vec3& origin, float yaw,
                             const glm::vec3& body_lo, const glm::vec3& body_hi) {
    FurnitureSpot spot;
    spot.kind = kind;
    if (kind == SpotKind::None || !s.found) {
        return spot;
    }
    const glm::vec2 mid = s.centre();
    const glm::vec3 mid_world = origin + to_world_dir(mid, yaw);
    // ПОЛ ПРЕДМЕТА — ЭТО ПОСАДКА ЕГО ЧЕРТЕЖА, а не низ габарита: у чертежей
    // полки нуль стоит на полу комнаты, и именно на этом полу стоит человек,
    // который сейчас на предмет садится.
    spot.floor_at = glm::vec3{mid_world.x, origin.y, mid_world.z};
    spot.surface_m = s.top_y;

    // ОСЬ ПРЕДМЕТА. У лежака голова уходит вдоль ДЛИННОЙ оси, у сиденья
    // взгляд идёт ПОПЕРЁК неё — и вторую из двух сторон выбирает уже комната
    // (seat_facing), потому что сама лавка о столе и стене ничего не знает.
    const glm::vec2 lng = s.long_axis();
    const glm::vec2 cross{lng.y, lng.x};
    spot.facing = glm::normalize(to_world_dir(kind == SpotKind::Lie ? lng : cross, yaw));

    // ГАБАРИТ ТЕЛА ДЛЯ ПРИЦЕЛА — ВЕСЬ ПРЕДМЕТ, а не его площадка: «смотрю на
    // кровать» обязано зажигаться и от изголовья тоже.
    const glm::vec3 bmid = (body_lo + body_hi) * 0.5f;
    spot.aim.centre = origin + to_world_dir(glm::vec2{bmid.x, bmid.z}, yaw)
                    + glm::vec3{0.0f, bmid.y, 0.0f};
    spot.aim.half = glm::max((body_hi - body_lo) * 0.5f, glm::vec3{0.05f});
    spot.aim.yaw = yaw;
    spot.aim.reach_m = SEAT_REACH_M;
    return spot;
}

PostureStart posture_start(const FurnitureSpot& spot, const glm::vec3& from) {
    PostureStart out;
    if (spot.kind != SpotKind::Seat && spot.kind != SpotKind::Lie) {
        return out;
    }
    glm::vec3 n{spot.facing.x, 0.0f, spot.facing.z};
    if (glm::length(n) < 1.0e-4f) {
        return out;
    }
    n = glm::normalize(n);
    if (spot.kind == SpotKind::Lie) {
        // К ЛЕЖАКУ ПОДХОДЯТ БОКОМ. `facing` у лежака — куда уходит ГОЛОВА, а
        // в изголовье не подходят: там столбики, а у кровати в комнате — ещё
        // и стена. Сторон две, и выбирает между ними ТА, НА КОТОРОЙ ЧЕЛОВЕК
        // УЖЕ СТОИТ: занятую стеной сторону он занять не мог.
        const glm::vec3 side = glm::normalize(glm::cross(n, glm::vec3{0.0f, 1.0f, 0.0f}));
        const glm::vec3 to_man{from.x - spot.floor_at.x, 0.0f, from.z - spot.floor_at.z};
        n = glm::dot(to_man, side) >= 0.0f ? side : -side;
    }
    // ОТ СЕРЕДИНЫ ГАБАРИТА, А НЕ ОТ СЕРЕДИНЫ ПЛОЩАДКИ: касается предмета
    // капсула, а предмет — это его тело (у кровати рама и столбики шире
    // матраса). Разность двух середин вдоль подхода вычитается, чтобы точка
    // легла ровно на нужном расстоянии от ТЕЛА, оставшись на оси площадки.
    const float support = seat_aim_support(spot.aim, n);
    const float pad_offset = (spot.floor_at.x - spot.aim.centre.x) * n.x
                           + (spot.floor_at.z - spot.aim.centre.z) * n.z;
    // ...И ЧЕРЕЗ КОРОБКУ ПРИЦЕЛА, А НЕ ЧЕРЕЗ ГОЛЫЙ ГАБАРИТ. Капсулу
    // останавливает не чертёж, а ТЕЛО, которым мебель стоит в физике, — а
    // оно шире габарита на SEAT_AIM_PAD_M (App::spawn_furniture_seats кладёт
    // коробку прицела именно так). ЗАМЕРЕНО, а не выведено на бумаге: точка,
    // посчитанная без этого слагаемого, оказалась НЕДОСТИЖИМОЙ — человек
    // упирался на 0.10 м раньше неё (замер лавки в x112z271: подошвы встали
    // на z=258.800 при точке 258.700), и автопилот бросал подход по таймауту.
    const float step = support + SEAT_AIM_PAD_M
                     + static_cast<float>(config::PLAYER_CAPSULE_RADIUS)
                     - pad_offset;
    out.valid = true;
    out.at = glm::vec3{spot.floor_at.x + n.x * step, spot.floor_at.y,
                       spot.floor_at.z + n.z * step};
    out.facing = n;
    // ТА ЖЕ ФОРМУЛА РЫСКА, ЧТО У ПОЗЫ СИДЯЩЕГО (take_seat: atan2(facing.x,
    // -facing.z)). Совпадение не случайно, оно и есть починка перекоса: у
    // сиденья `n` == `spot.facing`, значит после доворота защёлке позы
    // ДОВОРАЧИВАТЬ УЖЕ НЕЧЕГО и рывка на 180° в тик посадки не остаётся.
    out.yaw = std::atan2(n.x, -n.z);
    return out;
}

glm::vec3 seat_facing(const glm::vec3& seat_xz, const glm::vec3& cross,
                      std::span<const glm::vec3> tables,
                      const glm::vec3& room_centre) {
    const glm::vec3 axis = glm::length(cross) > 1.0e-4f ? glm::normalize(cross)
                                                        : glm::vec3{0.0f, 0.0f, -1.0f};
    // 1. САМЫЙ БЛИЗКИЙ СТОЛ РЕШАЕТ. У стола садятся лицом к столу — это про
    // трактир, ратушу и всякий зал, где лавки стоят парами вокруг столешницы.
    float best = TABLE_NEAR_M;
    const glm::vec3* chosen = nullptr;
    for (const glm::vec3& t : tables) {
        const glm::vec3 d{t.x - seat_xz.x, 0.0f, t.z - seat_xz.z};
        const float dist = glm::length(d);
        if (dist < best) {
            best = dist;
            chosen = &t;
        }
    }
    if (chosen != nullptr) {
        const glm::vec3 d{chosen->x - seat_xz.x, 0.0f, chosen->z - seat_xz.z};
        // ПРОЕКЦИЯ НА ПОПЕРЕЧНУЮ ОСЬ, а не сам вектор: сидящий разворачивается
        // поперёк лавки, а не наискось к ней. Стол ровно на оси лавки (проекция
        // нулевая) — вырожденный случай, и тогда решает комната, а не он.
        const float side = glm::dot(d, axis);
        if (std::fabs(side) > 1.0e-3f) {
            return side > 0.0f ? axis : -axis;
        }
    }
    // 2. ИНАЧЕ — ЛИЦОМ В КОМНАТУ. Лавка у стены: с одной стороны стена, с
    // другой комната, и середина комнаты — самый дешёвый способ их различить,
    // не спрашивая физику.
    const glm::vec3 to_room{room_centre.x - seat_xz.x, 0.0f, room_centre.z - seat_xz.z};
    const float side = glm::dot(to_room, axis);
    return side >= 0.0f ? axis : -axis;
}

} // namespace dfn::app
