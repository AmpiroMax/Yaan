/*
Module: tests
File: tests/app/SeatApproachTests.cpp

Responsibility:
- ПОДХОД К МЕБЕЛИ: точка старта позы и сходимость к ней. Четыре утверждения,
  которых кадр предъявить не может: (1) точка старта, выведенная из НАСТОЯЩИХ
  чертежей полки, стоит вплотную к предмету и НЕ В НЁМ; (2) рыск точки старта
  у сиденья совпадает с рыском позы, то есть в тик посадки доворачивать уже
  нечего — перекоса нет по построению; (3) у лежака сторона выбирается той, на
  которой человек стоит, а не жребием; (4) ИДЕМПОТЕНТНОСТЬ: десять разных
  законных исходных поз сходятся к точке старта за отведённое время, и все
  десять кончаются посадкой.

Dependencies:
- Uses: doctest, SeatAim.cpp + FurnitureSeats.cpp + ThirdPersonRig.cpp (без
  App и без окна), dfn_world (чтение .dfh и сборка меша — та же, что рисует
  игра).
- Used by: ctest (app_seat_approach). Рабочая папка рукавов — корень
  репозитория (tests/CMakeLists.txt), поэтому чертежи читаются относительно.

Notes:
- ПОЧЕМУ ЗДЕСЬ СВОЙ ЗАГРУЗЧИК ЧЕРТЕЖА, А НЕ ОБЩИЙ С SeatAimTests. Каждый рукав
  — отдельный исполняемый файл (tests/app.cmake), и общий помощник потребовал
  бы третьей единицы трансляции ради двадцати строк. Разборщик .dfh при этом
  всё равно ОДИН на проект: обе копии зовут world::read_house.
- ЧТО ЭТОТ РУКАВ НЕ МЕРИТ. Сам ход автопилота в приложении (кто пишет оси, в
  каком месте тика, доходит ли капсула сквозь физику) меряется беспилотным
  прибором DFN_SEAT_TAKE=sit+<кадров>@<градусов> на живом бинарнике: там
  участвуют физика, ввод и разрыв в тиках между нажатием и отпусканием, а
  здесь — только тот контракт, на который автопилот опирается.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЧИСЛА ЗДЕСЬ — ЗАМЕРЫ С ЧЕРТЕЖЕЙ, а не копии таблицы: если furn-bench станет
  глубже, красным станет ЭТОТ файл, и это правильно.
*/

#include <doctest/doctest.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "engine/app/sources/FurnitureSeats.h"
#include "engine/app/sources/SeatAim.h"
#include "engine/app/sources/ThirdPersonRig.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"

using namespace dfn;
using namespace dfn::app;

namespace {

constexpr float PI = 3.14159265358979f;

/// ЗАЗОР МЕЖДУ ПОДОШВАМИ И ГАБАРИТОМ ПРЕДМЕТА, м: отступ коробки прицела плюс
/// радиус капсулы. Не копия — это ТЕ ЖЕ два слагаемых, которыми считает
/// posture_start, выписанные здесь, чтобы рукав назвал ожидаемое число сам.
constexpr float STANDOFF_M =
    SEAT_AIM_PAD_M + static_cast<float>(config::PLAYER_CAPSULE_RADIUS);

struct Piece {
    std::vector<glm::vec3> pos;
    std::vector<std::uint32_t> idx;
    glm::vec3 lo{0.0f};
    glm::vec3 hi{0.0f};
    bool ok = false;
};

/// ЧИТАЕТ ЧЕРТЁЖ ПОЛКИ И СОБИРАЕТ ЕГО ТЕМ ЖЕ ПОСТРОИТЕЛЕМ, ЧТО И ИГРА.
[[nodiscard]] Piece load(const std::string& name) {
    Piece p;
    std::ifstream in("assets/houses/" + name + ".dfh");
    if (!in) {
        return p;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    world::HouseGraph g;
    if (!world::read_house(ss.str(), g).ok) {
        return p;
    }
    const world::HouseMesh built = world::build_house_mesh(g);
    p.pos.reserve(built.vertices.size());
    p.lo = glm::vec3{1.0e9f};
    p.hi = glm::vec3{-1.0e9f};
    for (const world::HouseVertex& v : built.vertices) {
        p.pos.push_back(v.pos);
        p.lo = glm::min(p.lo, v.pos);
        p.hi = glm::max(p.hi, v.pos);
    }
    p.idx = built.indices;
    p.ok = !p.idx.empty();
    return p;
}

/// Точка позы по чертежу, поставленному в `origin` с поворотом `yaw`.
[[nodiscard]] FurnitureSpot spot_of(const Piece& p, const glm::vec3& origin, float yaw) {
    const FurnSurface s = furniture_surface(p.pos, p.idx);
    FurnitureSpot spot = furniture_spot(s, classify_surface(s), origin, yaw, p.lo, p.hi);
    spot.aim.stand_m = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    spot.source = "чертёж";
    return spot;
}

/// РАССТОЯНИЕ ОТ ТОЧКИ ДО ГАБАРИТА ПРЕДМЕТА ПО ГОРИЗОНТАЛИ. Тем же приёмом,
/// что и seat_aim: местные координаты, зажим в полугабарит.
[[nodiscard]] float gap_to_body(const SeatAim& aim, const glm::vec3& world) {
    const glm::vec3 e = seat_aim_local(aim, world);
    const float dx = std::max(0.0f, std::fabs(e.x) - aim.half.x);
    const float dz = std::max(0.0f, std::fabs(e.z) - aim.half.z);
    return std::sqrt(dx * dx + dz * dz);
}

/// ПРОГОН АВТОПИЛОТА НА ЧИСТЫХ ФУНКЦИЯХ: ходьба к точке и доворот, теми же
/// величинами и теми же порогами, какими это делает App::approach_step.
/// Возвращает время до посадки, отрицательное — не дошёл.
struct Walk {
    float seconds = 0.0f;
    bool sat = false;
    float yaw_left = 0.0f; ///< остаток доворота в тик посадки, рад
    float body_travel_m = 0.0f; ///< сколько ПОЕДЕТ тело за переход (см. ниже)
};

[[nodiscard]] Walk walk_to(const PostureStart& start, const FurnitureSpot& spot,
                           glm::vec3 at, float yaw) {
    Walk w;
    const float dt = static_cast<float>(config::SIM_DT);
    const float rate = static_cast<float>(config::BODY_TURN_RATE);
    const float speed = static_cast<float>(config::WALK_SPEED);
    while (w.seconds <= SEAT_APPROACH_TIMEOUT_S) {
        const glm::vec3 d{start.at.x - at.x, 0.0f, start.at.z - at.z};
        const float left = glm::length(d);
        if (left > SEAT_ARRIVE_M) {
            yaw = turn_body_toward(yaw, std::atan2(d.x, -d.z), dt, rate);
            // ШАГ ИДЁТ ТУДА, КУДА СМОТРИТ ТЕЛО, а не сразу к цели: оси пишутся
            // вперёд (0,1), и локомоция везёт человека по его собственному
            // рыску. Иначе рукав мерил бы телепорт по прямой.
            at += glm::vec3{std::sin(yaw), 0.0f, -std::cos(yaw)} * speed * dt;
        } else if (std::fabs(shortest_arc(yaw, start.yaw)) > SEAT_ALIGN_RAD) {
            yaw = turn_body_toward(yaw, start.yaw, dt, rate);
        } else {
            w.sat = true;
            w.yaw_left = std::fabs(shortest_arc(yaw, start.yaw));
            w.body_travel_m = glm::length(glm::vec3{spot.floor_at.x - at.x, 0.0f,
                                                    spot.floor_at.z - at.z});
            return w;
        }
        w.seconds += dt;
    }
    return w;
}

} // namespace

TEST_CASE("точка старта лавки: вплотную к телу, но не в нём") {
    const Piece p = load("furn-bench");
    REQUIRE(p.ok);
    const FurnitureSpot spot = spot_of(p, glm::vec3{0.0f}, 0.0f);
    REQUIRE(spot.kind == SpotKind::Seat);
    const PostureStart st = posture_start(spot, glm::vec3{0.0f, 0.0f, -3.0f});
    REQUIRE(st.valid);
    // ЗАЗОР ДО ТЕЛА ВЫВЕДЕН, А НЕ ПОДОБРАН: капсула касается того, что её
    // останавливает, и не входит в него — до КОРОБКИ ПРИЦЕЛА, которой мебель стоит в физике: она шире габарита
    // на SEAT_AIM_PAD_M, и упирается капсула именно в неё.
    const float gap = gap_to_body(spot.aim, st.at);
    INFO("зазор до габарита ", gap, " м при радиусе капсулы ",
         config::PLAYER_CAPSULE_RADIUS, " и отступе коробки ", SEAT_AIM_PAD_M);
    CHECK(gap == doctest::Approx(STANDOFF_M).epsilon(0.01));
    // ...И ЭТО НЕ «ГДЕ-ТО РЯДОМ»: точка стоит на оси площадки, то есть ровно
    // перед серединой лавки, а не у её конца.
    const glm::vec3 local = seat_aim_local(spot.aim, st.at);
    CHECK(std::fabs(local.x - seat_aim_local(spot.aim, spot.floor_at).x) < 0.01f);
    // Стоит на полу предмета: и он, и человек стоят на одном полу.
    CHECK(st.at.y == doctest::Approx(spot.floor_at.y));
    // ЛАВКА 1.60x0.35 ПРИ ТЕЛЕ 1.60x0.35: полугабарит поперёк 0.175, значит
    // от середины до подошв 0.175 + 0.10 + 0.35 = 0.625 м. Это же число —
    // путь, который проедет корень тела за переход, и меньше него он быть не
    // может.
    const float from_centre =
        glm::length(glm::vec3{st.at.x - spot.floor_at.x, 0.0f, st.at.z - spot.floor_at.z});
    INFO("от середины площадки до подошв ", from_centre, " м");
    CHECK(from_centre == doctest::Approx(0.625f).epsilon(0.06));
}

TEST_CASE("рыск точки старта = рыск позы: доворачивать в тик посадки нечего") {
    const Piece p = load("furn-bench");
    REQUIRE(p.ok);
    // ПО ВОСЬМИ ПОСАДКАМ ЧЕРТЕЖА, а не по одной: рыск предмета входит и в
    // точку старта, и в позу, и совпадение обязано быть тождеством, а не
    // совпадением при yaw = 0.
    for (int k = 0; k < 8; ++k) {
        const float yaw = static_cast<float>(k) * PI / 4.0f;
        const FurnitureSpot spot = spot_of(p, glm::vec3{1.0f, 2.0f, 3.0f}, yaw);
        REQUIRE(spot.kind == SpotKind::Seat);
        const PostureStart st = posture_start(spot, spot.floor_at + spot.facing * 2.0f);
        REQUIRE(st.valid);
        // ЭТО ТА ЖЕ ФОРМУЛА, ЧТО СТАВИТ drive->posture_yaw в App::enter_posture.
        const float pose_yaw = std::atan2(spot.facing.x, -spot.facing.z);
        INFO("посадка ", yaw, ": старт ", st.yaw, ", поза ", pose_yaw);
        CHECK(std::fabs(shortest_arc(st.yaw, pose_yaw)) < 1.0e-4f);
    }
}

TEST_CASE("дуговая лавка: та же мера на пятне 3.21x0.66") {
    const Piece p = load("furn-bench-arc");
    REQUIRE(p.ok);
    const FurnitureSpot spot = spot_of(p, glm::vec3{0.0f}, 0.0f);
    REQUIRE(spot.kind == SpotKind::Seat);
    const PostureStart st = posture_start(spot, glm::vec3{0.0f, 0.0f, -3.0f});
    REQUIRE(st.valid);
    CHECK(gap_to_body(spot.aim, st.at) == doctest::Approx(STANDOFF_M).epsilon(0.01));
    // ДЛИННАЯ ЛАВКА НЕ ОТОДВИГАЕТ ЧЕЛОВЕКА ДАЛЬШЕ. Ровно за этим и заведена
    // опорная функция коробки: полугабарит ВДОЛЬ подхода у дуговой лавки тот
    // же, что у прямой, хотя сама она вчетверо длиннее.
    const float from_centre =
        glm::length(glm::vec3{st.at.x - spot.floor_at.x, 0.0f, st.at.z - spot.floor_at.z});
    INFO("дуговая: от середины до подошв ", from_centre, " м");
    CHECK(from_centre < 1.0f);
}

TEST_CASE("кровать: подходят БОКОМ, и стороной, на которой стоят") {
    const Piece p = load("furn-bed");
    REQUIRE(p.ok);
    const FurnitureSpot spot = spot_of(p, glm::vec3{0.0f}, 0.0f);
    REQUIRE(spot.kind == SpotKind::Lie);
    // Сторона — поперёк `facing` (куда уходит голова), а не вдоль него.
    const glm::vec3 side =
        glm::normalize(glm::cross(spot.facing, glm::vec3{0.0f, 1.0f, 0.0f}));
    const PostureStart a = posture_start(spot, spot.floor_at + side * 2.0f);
    const PostureStart b = posture_start(spot, spot.floor_at - side * 2.0f);
    REQUIRE(a.valid);
    REQUIRE(b.valid);
    CHECK(glm::dot(a.facing, side) > 0.99f);
    CHECK(glm::dot(b.facing, side) < -0.99f);
    // В ИЗГОЛОВЬЕ НЕ ПОДХОДЯТ ни с одной стороны.
    CHECK(std::fabs(glm::dot(a.facing, spot.facing)) < 1.0e-3f);
    CHECK(std::fabs(glm::dot(b.facing, spot.facing)) < 1.0e-3f);
    // И ОБЕ ТОЧКИ СТОЯТ ВПЛОТНУЮ К ТЕЛУ. Тело кровати (1.15x2.05) шире
    // матраса (0.97x1.90), и мера берётся у тела: капсула упирается в раму.
    CHECK(gap_to_body(spot.aim, a.at) == doctest::Approx(STANDOFF_M).epsilon(0.01));
    CHECK(gap_to_body(spot.aim, b.at) == doctest::Approx(STANDOFF_M).epsilon(0.01));
}

TEST_CASE("опорная функция коробки: с торца дальше, чем с бока") {
    const Piece p = load("furn-bed");
    REQUIRE(p.ok);
    const FurnitureSpot spot = spot_of(p, glm::vec3{0.0f}, 0.0f);
    const glm::vec3 side =
        glm::normalize(glm::cross(spot.facing, glm::vec3{0.0f, 1.0f, 0.0f}));
    const float head = seat_aim_support(spot.aim, spot.facing);
    const float flank = seat_aim_support(spot.aim, side);
    INFO("кровать: с торца ", head, " м, с бока ", flank, " м");
    // Чертёж furn-bed: тело 1.15x2.05, значит 1.02 против 0.57.
    CHECK(head == doctest::Approx(1.02f).epsilon(0.08));
    CHECK(flank == doctest::Approx(0.57f).epsilon(0.08));
    CHECK(head > flank * 1.5f);
    // ЗНАК НАПРАВЛЕНИЯ НЕ ВАЖЕН: у коробки обе стороны одинаковы.
    CHECK(seat_aim_support(spot.aim, -spot.facing) == doctest::Approx(head));
}

TEST_CASE("функция чистая: тот же ответ, и предмет не тронут") {
    const Piece p = load("furn-bench");
    REQUIRE(p.ok);
    FurnitureSpot spot = spot_of(p, glm::vec3{4.0f, 1.0f, -2.0f}, 0.7f);
    const glm::vec3 from{4.0f, 1.0f, -4.0f};
    const glm::vec3 facing_before = spot.facing;
    const glm::vec3 floor_before = spot.floor_at;
    const PostureStart a = posture_start(spot, from);
    const PostureStart b = posture_start(spot, from);
    CHECK(a.at == b.at);
    CHECK(a.yaw == b.yaw);
    CHECK(a.facing == b.facing);
    CHECK(spot.facing == facing_before);
    CHECK(spot.floor_at == floor_before);
    // Не сиденье и не лежак — ответа нет, и это НЕ точка (0,0,0).
    spot.kind = SpotKind::Table;
    CHECK_FALSE(posture_start(spot, from).valid);
}

TEST_CASE("ИДЕМПОТЕНТНОСТЬ: десять законных исходных поз — десять посадок") {
    const Piece p = load("furn-bench");
    REQUIRE(p.ok);
    const FurnitureSpot spot = spot_of(p, glm::vec3{0.0f}, 0.0f);
    REQUIRE(spot.kind == SpotKind::Seat);

    // ДЕСЯТЬ РАЗНЫХ РУК, А НЕ ДЕСЯТЬ ОДИНАКОВЫХ (правило 47): десять раз
    // повторить один прогон значит проверить, что арифметика повторяема, —
    // а спрошено, надёжна ли посадка. Руки разведены по ДВУМ величинам,
    // которыми живая посадка и отличается прогон от прогона: с какой стороны
    // человек стоит и куда он в этот миг смотрит.
    int sat = 0;
    float worst_s = 0.0f;
    float worst_travel = 0.0f;
    for (int k = 0; k < 10; ++k) {
        const float around = -1.2f + 0.24f * static_cast<float>(k); // рад, +-70°
        const float radius = 0.9f + 0.2f * static_cast<float>(k % 5); // 0.9..1.7 м
        const glm::vec3 dir{std::sin(around), 0.0f, -std::cos(around)};
        const glm::vec3 at = spot.floor_at + dir * radius;
        // Смотрит на лавку — так он и нажал бы E.
        const float yaw = std::atan2(-dir.x, dir.z);
        const PostureStart st = posture_start(spot, at);
        REQUIRE(st.valid);
        const float to_start =
            glm::length(glm::vec3{st.at.x - at.x, 0.0f, st.at.z - at.z});
        INFO("рука ", k, ": стоит в ", radius, " м под ", around,
             " рад, до точки старта ", to_start, " м");
        // Все десять — внутри потолка автопилота, иначе рука мерила бы отказ.
        CHECK(to_start <= SEAT_APPROACH_MAX_M);
        const Walk w = walk_to(st, spot, at, yaw);
        CHECK(w.sat);
        if (w.sat) {
            ++sat;
            worst_s = std::max(worst_s, w.seconds);
            worst_travel = std::max(worst_travel, w.body_travel_m);
            // ОСТАТОК ДОВОРОТА — НЕ БОЛЬШЕ ОДНОГО ТИКА штатного разворота:
            // ровно за этим порог и выведен (SEAT_ALIGN_RAD).
            CHECK(w.yaw_left <= SEAT_ALIGN_RAD);
        }
    }
    INFO("самый долгий подход ", worst_s, " с при потолке ", SEAT_APPROACH_TIMEOUT_S,
         "; самый длинный проезд тела ", worst_travel, " м");
    CHECK(sat == 10);
    CHECK(worst_s < SEAT_APPROACH_TIMEOUT_S);
    // ПРОЕЗД ТЕЛА ЗА ПЕРЕХОД — ЭТО РАССТОЯНИЕ ОТ ПОДОШВ ДО ЗЕМЛИ ПОЗЫ
    // (anim::body_root_for смешивает эти две точки). После подхода он равен
    // мере точки старта у ВСЕХ десяти рук, а не тому, кто где нажал: это и
    // есть «телепорт убран» — проезд стал свойством предмета.
    CHECK(worst_travel < 0.70f);
}
