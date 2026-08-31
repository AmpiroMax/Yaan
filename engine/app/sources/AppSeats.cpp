/*
Module: engine/app
File: engine/app/sources/AppSeats.cpp

Responsibility:
- СИДЕТЬ И ЛЕЖАТЬ (обязательство эпохи «Большой мир», заказ владельца 28.08):
  сбор точек позы по залитой локации, прицел радиус+взгляд у мебели, ПОДХОД к
  точке старта по клавише E, парковка капсулы и камера из глаза позы.

Key items:
- App::spawn_furniture_seats / clear_furniture_seats: точки как вещь мира.
- App::filter_seat_hover: подсказка только когда СМОТРИМ на предмет — и
  ЗАМОРОЖЕННАЯ на время, пока лежит клавиша E (там же назван замер
  «садится не каждый раз»).
- App::take_seat: нажатие E начинает ПОДХОД, а не позу.
- App::begin_approach / approach_step / cancel_approach: короткий автопилот к
  точке старта позы — ходьба теми же намерениями, что пишет клавиатура,
  доворот, и только потом поза.
- App::enter_posture / leave_posture: вход в позу и выход из неё.
- App::park_posture / posture_camera: движение выключено, глаз — из позы.
- App::posture_trace_step: прибор перехода (DFN_POSTURE_TRACE).
- App::probe_seats: беспилотный прибор прицела (DFN_SEAT_PROBE).

Dependencies:
- Uses: FurnitureSeats.h, SeatAim.h, engine/anim (Posture/BodyDrive),
  engine/gameplay (Highlightable/Usable/PlayerState), engine/world
  (HouseMesh), engine/render (ObjectRegistry), platform IPhysics.
- Used by: App (тик, вход/выход из локации).

Notes:
- ОТКУДА БЕРЁТСЯ ГЕОМЕТРИЯ. Мебель локации приходит двумя путями и оба здесь
  сведены к ОДНОМУ правилу (FurnitureSeats.h): секция [house] несёт чертёж
  .dfh (граф уже прочитан в interior_houses_, меш собирается тем же
  build_house_mesh, что и для картинки), секция [place] несёт объект реестра
  (.dfo, куски постройки уже лежат в scene_objects_). Второго правила «где у
  кровати матрас» нет и быть не должно — предмет один, ответ один.
- ПОЧЕМУ ТОЧКА ВЫХОДА — ЭТО МЕСТО, ГДЕ ЧЕЛОВЕК СТОЯЛ. Вычисленная точка «рядом
  с мебелью» требует доказательства, что она не в стене, не в соседней лавке и
  не за краем пола, — а доказать это дёшево нечем (у дверей на такое
  доказательство ушла отдельная волна и перепись 130 локаций). Точка, с
  которой человек НАЖАЛ, уже проверена его собственными ногами.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона lead владеет этим файлом.
- ОДИН ОТВЕТ НА «ЦЕЛЮСЬ ЛИ Я»: и подсказка, и клавиша спрашивают seat_aim()
  через seat_aim_now(). Второй ответ — тот самый дефект, за который двери
  заплатили жалобой владельца.
*/

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/AppInternal.h"
#include "engine/app/sources/App.h"
#include "engine/app/sources/Localization.h"
#include "engine/app/sources/ThirdPersonRig.h"

#include "engine/anim/sources/Body.h"
#include "engine/core/components/sources/Components.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/CameraBoom.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"
#include "engine/world/sources/Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace dfn::app {

namespace {

/// СКОЛЬКО ХОДА ХВАТАЕТ, ЧТОБЫ СЧЕСТЬ «ОН ПОШЁЛ» и поднять из позы. Не ноль:
/// у джойстика и у мёртвой зоны мыши бывает шум, и вставать от шума — это
/// поза, из которой нельзя посидеть.
constexpr float POSTURE_WALK_OUT = 0.30f;

/// Кадры беспилотной руки (DFN_SEAT_TAKE): подвод и выдержка в позе.
constexpr std::uint64_t SEAT_TAKE_APPROACH_FRAMES = 15;
/// СКОЛЬКО СТОЯТЬ ПЕРЕД НАЖАТИЕМ, кадров. Не ноль и не «на всякий случай»:
/// подвод ТЕЛЕПОРТИРУЕТ капсулу, а телепорт — это приземление, и живой слой
/// отвечает на него провалом (land_dip). Нажать E в тот же тик значило бы
/// начать переход из проваленного таза, и след перехода показал бы подъём на
/// 2.5 см там, где человек просто стоит. Замерено этим самым следом.
constexpr std::uint64_t SEAT_TAKE_SETTLE_FRAMES = 30;
constexpr std::uint64_t SEAT_TAKE_HOLD_FRAMES = 480;

/// НА СКОЛЬКО РУКА ПРИБОРА ВСТАЁТ ДАЛЬШЕ ТОЧКИ СТАРТА, м. Не ноль и не
/// «побольше»: ноль означал бы, что подход нечего проходить, а мерить
/// подход, которого не было, нельзя (правило 47). 0.55 м — это дальше точки
/// старта на 0.20 м (лавка: 0.175 + 0.35 = 0.525 против 0.175 + 0.55) и всё
/// ещё внутри радиуса руки SEAT_REACH_M 1.2 м, то есть подсказка обязана
/// гореть, а автопилот — иметь что пройти.
constexpr float SEAT_TAKE_STANDOFF_M = 0.55f;

/// Единичный вектор взгляда по позе камеры — та же формула, что у дверей
/// (door_aim_now) и у InteractionSystem::view_direction. Расхождение здесь
/// значило бы, что подсказка горит не там, куда указывает перекрестье.
/// ОТКРЫТА ЛИ ДВЕРЬ СЛЕДА (DFN_POSTURE_TRACE). Один ответ на весь файл: и
/// след перехода, и след подхода — один прибор, и включаться они обязаны
/// вместе, иначе «поза не поехала» и «человек не дошёл» придётся ловить
/// разными прогонами.
[[nodiscard]] bool posture_trace_on() {
    static const bool on = [] {
        const char* v = door_value("DFN_POSTURE_TRACE");
        return v != nullptr && v[0] != '\0' && v[0] != '0';
    }();
    return on;
}

[[nodiscard]] glm::vec3 look_dir(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
}

} // namespace

bool App::seats_enabled() {
    static const bool on = [] {
        const char* e = door_value("DFN_SEAT");
        return e == nullptr || *e == '\0' || *e != '0';
    }();
    return on;
}

bool App::seat_approach_enabled() {
    static const bool on = [] {
        const char* e = door_value("DFN_SEAT_APPROACH");
        return e == nullptr || *e == '\0' || *e != '0';
    }();
    return on;
}

// ---------------------------------------------------------------------------
// СБОР ТОЧЕК ПОЗЫ
// ---------------------------------------------------------------------------

void App::spawn_furniture_seats() {
    clear_furniture_seats();
    if (!seats_enabled() || physics_ == nullptr) {
        return;
    }

    // МЕШ ЧЕРТЕЖА СЧИТАЕТСЯ ОДИН РАЗ НА ИМЯ. В комнате стоит до шести
    // одинаковых лавок, и собирать одну и ту же геометрию шесть раз значило бы
    // платить за побайтово тот же ответ (тот же довод, что у кэша AO в
    // upload_house_mesh). Упорядоченная карта — обход детерминирован.
    struct Measured {
        FurnSurface surface;
        SpotKind kind = SpotKind::None;
        glm::vec3 lo{0.0f};
        glm::vec3 hi{0.0f};
        bool ok = false;
    };
    std::map<std::string, Measured> measured;

    const auto measure = [&](const std::string& key,
                             std::span<const glm::vec3> pos,
                             std::span<const std::uint32_t> idx) -> const Measured& {
        auto it = measured.find(key);
        if (it != measured.end()) {
            return it->second;
        }
        Measured m;
        if (!pos.empty() && !idx.empty()) {
            m.surface = furniture_surface(pos, idx);
            m.kind = classify_surface(m.surface);
            glm::vec3 lo{1.0e9f};
            glm::vec3 hi{-1.0e9f};
            for (const glm::vec3& p : pos) {
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }
            m.lo = lo;
            m.hi = hi;
            m.ok = true;
        }
        return measured.emplace(key, m).first->second;
    };

    // --- ЧТО ГДЕ СТОИТ: две секции композиции, одно правило ------------------
    struct Piece {
        Measured m;
        glm::vec3 origin{0.0f};
        float yaw = 0.0f;
        std::string name;
    };
    std::vector<Piece> pieces;

    for (const PlacedHouse& ph : interior_houses_) {
        if (ph.scene_index >= interior_doc_.houses.size()) {
            continue;
        }
        const world::ScenePlacedHouse& H = interior_doc_.houses[ph.scene_index];
        const std::string key = std::filesystem::path(H.file).stem().string();
        // ОБОЛОЧКА ЛОКАЦИИ НЕ МЕБЕЛЬ. Пол комнаты — это тоже «самая широкая
        // горизонтальная площадка», и без этой строки в каждом доме появился
        // бы лежак размером с комнату. Признак берётся у ЗАПИСИ композиции
        // (sealed / есть ли у неё внутренность), а не по имени файла.
        if (H.sealed || !H.interior.empty()) {
            continue;
        }
        const world::HouseMesh built = world::build_house_mesh(ph.graph);
        std::vector<glm::vec3> pos;
        pos.reserve(built.vertices.size());
        for (const world::HouseVertex& v : built.vertices) {
            pos.push_back(v.pos);
        }
        const Measured& m = measure(key, pos, built.indices);
        if (m.kind != SpotKind::None) {
            pieces.push_back(Piece{m, ph.pos, ph.yaw, key});
        }
    }

    for (const world::Placement& p : interior_doc_.placements) {
        const auto obj = scene_objects_.find(p.object);
        if (obj == scene_objects_.end() || obj->second.house.empty()) {
            continue;
        }
        std::vector<glm::vec3> pos;
        std::vector<std::uint32_t> idx;
        for (const render::HouseSubmesh& sub : obj->second.house) {
            const auto base = static_cast<std::uint32_t>(pos.size());
            for (const platform::Vertex& v : sub.mesh.vertices) {
                pos.push_back(v.position);
            }
            for (const std::uint32_t i : sub.mesh.indices) {
                idx.push_back(base + i);
            }
        }
        const Measured& m = measure(p.object, pos, idx);
        if (m.kind != SpotKind::None) {
            pieces.push_back(Piece{m, p.position + interior_pocket_, p.yaw, p.object});
        }
    }

    // --- СЕРЕДИНА КОМНАТЫ И СТОЛЫ: обстановка, по которой сидящий
    //     разворачивается (см. seat_facing) -----------------------------------
    glm::vec3 room_lo{1.0e9f};
    glm::vec3 room_hi{-1.0e9f};
    for (const glm::vec3& v : interior_positions_) {
        room_lo = glm::min(room_lo, v);
        room_hi = glm::max(room_hi, v);
    }
    const glm::vec3 room_centre = interior_positions_.empty()
                                      ? interior_pocket_
                                      : (room_lo + room_hi) * 0.5f;
    std::vector<glm::vec3> tables;
    for (const Piece& piece : pieces) {
        if (piece.m.kind == SpotKind::Table) {
            tables.push_back(furniture_spot(piece.m.surface, SpotKind::Table,
                                            piece.origin, piece.yaw, piece.m.lo,
                                            piece.m.hi).floor_at);
        }
    }

    std::size_t n_seat = 0;
    std::size_t n_lie = 0;
    for (const Piece& piece : pieces) {
        if (piece.m.kind != SpotKind::Seat && piece.m.kind != SpotKind::Lie) {
            continue;
        }
        FurnitureSpot spot = furniture_spot(piece.m.surface, piece.m.kind,
                                            piece.origin, piece.yaw,
                                            piece.m.lo, piece.m.hi);
        spot.source = piece.name;
        // РОСТ ЧЕЛОВЕКА ОТДАЁТСЯ ПРИЦЕЛУ ЗДЕСЬ: SeatAim.h не знает ни одной
        // константы мира (как и DoorAim.h), и второй копии PLAYER_EYE_HEIGHT
        // в нём не будет.
        spot.aim.stand_m = static_cast<float>(config::PLAYER_EYE_HEIGHT);
        if (spot.kind == SpotKind::Seat) {
            spot.facing = seat_facing(spot.floor_at, spot.facing, tables, room_centre);
            ++n_seat;
        } else {
            ++n_lie;
        }

        const ecs::EntityId id = world_.spawn();
        world_.add(id, components::Transform{.position = spot.floor_at,
                                             .rotation = {1.0f, 0.0f, 0.0f, 0.0f},
                                             .scale = glm::vec3{1.0f}});
        // ДЕЙСТВИЕ ИМЕНУЕТСЯ ТОЧКОЙ, а не номером записи, — тот же довод, что
        // у створок дома: номер живёт внутри прогона, координата — свойство
        // комнаты.
        const std::uint64_t action = serialization::fnv1a64(
            "seat@" + std::to_string(static_cast<int>(spot.floor_at.x * 100.0f)) + ","
            + std::to_string(static_cast<int>(spot.floor_at.y * 100.0f)) + ","
            + std::to_string(static_cast<int>(spot.floor_at.z * 100.0f)));
        world_.add(id, gameplay::Highlightable{
                           .prompt_key = spot.kind == SpotKind::Lie ? "prompt.lie"
                                                                    : "prompt.sit",
                           // Радиус решает ПРИЦЕЛ, и только он: 0 отдаёт штатную
                           // дальность, а вердикт выносит seat_aim (один ответ).
                           .max_use_distance = 0.0f});
        world_.add(id, gameplay::Usable{.action = action,
                                        .repeatable = true,
                                        .used = false});
        platform::StaticBoxDesc box;
        box.center = spot.aim.centre;
        box.rotation = glm::angleAxis(spot.aim.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
        box.half_extents = spot.aim.half + glm::vec3{SEAT_AIM_PAD_M};
        box.layer = physics::LAYER_INTERACTABLE;
        box.user_data = id.packed();
        const platform::PhysicsBodyHandle body = physics_->create_static_box(box);
        if (body.valid()) {
            if (!world_.has_resource<gameplay::InteractableBodies>()) {
                world_.add_resource(gameplay::InteractableBodies{});
            }
            world_.resource<gameplay::InteractableBodies>().bodies[id.packed()] = body;
        }
        SeatLink link;
        link.action = action;
        link.entity = id;
        link.spot = std::move(spot);
        seats_.push_back(std::move(link));
    }

    if (!seats_.empty()) {
        const FurnitureSpot& first = seats_.front().spot;
        std::fprintf(stderr,
                     "[поза] точек на локации: %zu (сидений %zu, лежаков %zu); "
                     "первая %s на (%.2f %.2f %.2f), площадка %.3f м\n",
                     seats_.size(), n_seat, n_lie, first.source.c_str(),
                     static_cast<double>(first.floor_at.x),
                     static_cast<double>(first.floor_at.y),
                     static_cast<double>(first.floor_at.z),
                     static_cast<double>(first.surface_m));
    }
}

void App::clear_furniture_seats() {
    if (in_posture()) {
        leave_posture();
    }
    if (physics_ != nullptr && world_.has_resource<gameplay::InteractableBodies>()) {
        auto& bodies = world_.resource<gameplay::InteractableBodies>().bodies;
        for (const SeatLink& link : seats_) {
            const auto it = bodies.find(link.entity.packed());
            if (it != bodies.end()) {
                physics_->destroy_body(it->second);
                bodies.erase(it);
            }
        }
    }
    for (const SeatLink& link : seats_) {
        if (world_.alive(link.entity)) {
            world_.destroy(link.entity);
        }
    }
    seats_.clear();
    pending_seat_ = 0;
    approach_seat_ = -1;
    approach_s_ = 0.0f;
    aim_held_seat_ = -1;
}

// ---------------------------------------------------------------------------
// ПРИЦЕЛ
// ---------------------------------------------------------------------------

SeatAimHit App::seat_aim_now(const SeatLink& link) const {
    SeatAimHit miss;
    const auto* cam = world_.get<components::CameraPose>(player_);
    if (cam == nullptr) {
        return miss;
    }
    return seat_aim(link.spot.aim, cam->position, look_dir(cam->yaw, cam->pitch));
}

void App::filter_seat_hover() {
    if (seats_.empty() || !world_.has_resource<components::HoverTarget>()) {
        aim_held_seat_ = -1;
        return;
    }
    auto& hover = world_.resource<components::HoverTarget>();

    // --- 1. ЖИВОЙ ПРИЦЕЛ: ЧТО ПОД ПЕРЕКРЕСТЬЕМ ПРЯМО СЕЙЧАС ---------------
    int seen = -1;
    if (hover.prompt_key != 0 && world_.alive(hover.entity)) {
        for (std::size_t i = 0; i < seats_.size(); ++i) {
            if (seats_[i].entity != hover.entity) {
                continue;
            }
            // ЛУЧ ПОПАЛ В ТЕЛО — И ЭТОГО МАЛО, ровно как у дверей: коробка
            // прицела шире предмета на SEAT_AIM_PAD_M, и попадание в её кромку
            // из-за спины зажигало бы «Сесть» у стоящего к лавке спиной.
            if (seat_aim_now(seats_[i]).ok) {
                seen = static_cast<int>(i);
            } else {
                hover = components::HoverTarget{};
            }
            break;
        }
    }

    // --- 2. ПРИЦЕЛ ЗАМИРАЕТ, ПОКА КЛАВИША ЛЕЖИТ --------------------------
    // ЗДЕСЬ ЖИВЁТ «САДИТСЯ НЕ КАЖДЫЙ РАЗ», и это НЕ гонка, а РАЗРЫВ В ТИКАХ,
    // который можно предъявить числом. Короткое нажатие E приложение
    // возвращает не в тот тик, в котором нажали, а в тот, в котором ОТПУСТИЛИ:
    // AppProps::grab_input делит клавишу на короткое и долгое и держит защёлку
    // всё время, пока палец лежит (GrabDrive.h). Между этими двумя тиками рука
    // на мыши продолжает вести перекрестье — и вердикт выносится по прицелу
    // ОТПУСКАНИЯ, а не нажатия. Замер этого рукава: подвели к лавке, нажали и
    // за время нажатия увели взгляд на N градусов — при N >= 20 посадка
    // пропадала, при N < 20 случалась, то есть исход решал жребий руки.
    //
    // ПОЧЕМУ ЗАМОРОЗКА, А НЕ «ЗАПОМНИТЬ НАЖАТИЕ». Второй ответ на «целюсь ли
    // я» запрещён шапкой этого файла: подсказка и клавиша обязаны читать ОДИН
    // HoverTarget. Поэтому чинится не клавиша, а прицел — и ровно на то время,
    // пока лежит клавиша, которая по нему сработает.
    const bool down_now = (input_ != nullptr && input_->is_down(platform::Key::E))
                       || grab_probe_key_;
    // ...И ТИК ОТПУСКАНИЯ ТОЖЕ, и это не «на всякий случай», а РОВНО ТОТ ТИК,
    // РАДИ КОТОРОГО ЗАМОРОЗКА ЗАВЕДЕНА: защёлка возвращается ИМЕННО НА
    // ОТПУСКАНИИ (grab_input), а к этому мигу клавиша уже не лежит. Замерено
    // на себе: заморозка «пока клавиша лежит» дала те же 2 посадки из 10, что
    // и без неё, потому что гасла ровно за один тик до того, как её спросили.
    const bool key_down = seat_approach_enabled() && (down_now || aim_key_was_down_);
    aim_key_was_down_ = down_now;
    if (!key_down || seen >= 0) {
        aim_held_seat_ = seen;
        return;
    }
    if (aim_held_seat_ < 0
        || static_cast<std::size_t>(aim_held_seat_) >= seats_.size()) {
        return;
    }
    const SeatLink& held = seats_[static_cast<std::size_t>(aim_held_seat_)];
    // ДЕРЖИМ, ПОКА ОН В РАДИУСЕ РУКИ, а не «пока клавиша лежит»: отвернуться
    // законно, УЙТИ — нет. Иначе человек, зажавший E и убежавший в соседнюю
    // комнату, сел бы там на лавку, которой не видит.
    if (!world_.alive(held.entity) || !seat_aim_now(held).in_reach) {
        aim_held_seat_ = -1;
        return;
    }
    // ...И ТОЛЬКО ЕСЛИ ПОД ПЕРЕКРЕСТЬЕМ НЕ ОКАЗАЛОСЬ ЧЕГО-ТО ДРУГОГО: живая
    // цель главнее замороженной, иначе замок на двери стал бы недоступен тому,
    // кто стоит у лавки.
    if (hover.prompt_key != 0) {
        return;
    }
    const gameplay::InteractionOffer offer = gameplay::offer_for(world_, held.entity);
    if (offer.verb == gameplay::InteractionVerb::None) {
        aim_held_seat_ = -1;
        return;
    }
    hover.entity = held.entity;
    hover.verb = static_cast<std::uint8_t>(offer.verb);
    hover.prompt_key = offer.prompt_key;
}

// ---------------------------------------------------------------------------
// ВХОД В ПОЗУ И ВЫХОД ИЗ НЕЁ
// ---------------------------------------------------------------------------

void App::take_seat() {
    if (pending_seat_ == 0) {
        return;
    }
    const std::uint64_t action = pending_seat_;
    pending_seat_ = 0;
    if (in_posture()) {
        leave_posture();
        return;
    }
    // ПОВТОРНОЕ E НА ПОДХОДЕ — ОТМЕНА, а не второй подход. Та же клавиша, тем
    // же смыслом, что и у сидящего: E — это переключатель намерения, и
    // намерение, которое нельзя отменить, человек отменяет ногами.
    if (approach_seat_ >= 0) {
        cancel_approach("повторное нажатие E");
        return;
    }
    for (std::size_t i = 0; i < seats_.size(); ++i) {
        if (seats_[i].action == action) {
            if (seat_approach_enabled()) {
                begin_approach(i);
            } else {
                enter_posture(i);
            }
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// ПОДХОД К ТОЧКЕ СТАРТА (заказ владельца 28.08, пункт 4)
// ---------------------------------------------------------------------------

void App::begin_approach(std::size_t index) {
    if (index >= seats_.size()) {
        return;
    }
    const auto* tr = world_.get<components::Transform>(player_);
    if (tr == nullptr) {
        return;
    }
    const FurnitureSpot& spot = seats_[index].spot;
    const PostureStart start = posture_start(spot, tr->position);
    if (!start.valid) {
        std::fprintf(stderr, "[поза] %s: точки старта у этой меты нет — не сажусь\n",
                     spot.source.c_str());
        return;
    }
    const glm::vec3 d{start.at.x - tr->position.x, 0.0f, start.at.z - tr->position.z};
    const float walk_m = glm::length(d);
    if (walk_m > SEAT_APPROACH_MAX_M) {
        // ВСЛУХ, А НЕ МОЛЧА. Дверь, которая молча ничего не делает, для
        // игрока неотличима от сломанной (тот же довод, что у DFN_SHOT_AFTER).
        std::fprintf(stderr,
                     "[поза] %s: до точки старта %.2f м, автопилот ведёт не "
                     "дальше %.2f м — E не срабатывает, подойдите ближе\n",
                     spot.source.c_str(), static_cast<double>(walk_m),
                     static_cast<double>(SEAT_APPROACH_MAX_M));
        return;
    }
    approach_seat_ = static_cast<int>(index);
    approach_start_ = start;
    approach_s_ = 0.0f;
    approach_stall_s_ = 0.0f;
    approach_best_m_ = walk_m;
    approach_stall_said_ = false;
    std::fprintf(stderr,
                 "[поза] ПОДХОД к %s: точка старта (%.2f %.2f %.2f), рыск %.3f, "
                 "идти %.2f м\n",
                 spot.source.c_str(), static_cast<double>(start.at.x),
                 static_cast<double>(start.at.y), static_cast<double>(start.at.z),
                 static_cast<double>(start.yaw), static_cast<double>(walk_m));
}

void App::cancel_approach(const char* why) {
    if (approach_seat_ < 0) {
        return;
    }
    const std::string name = static_cast<std::size_t>(approach_seat_) < seats_.size()
                                 ? seats_[static_cast<std::size_t>(approach_seat_)]
                                       .spot.source
                                 : std::string("?");
    approach_seat_ = -1;
    const float spent = approach_s_;
    approach_s_ = 0.0f;
    if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
        ps->move_axes = glm::vec2{0.0f};
    }
    // С ЧИСЛАМИ, А НЕ ПРОСТО «БРОШЕН»: у отказа обязана быть измеримая
    // причина, иначе «упёрся» и «не туда шёл» читаются одинаково (правило 30).
    const auto* tr = world_.get<components::Transform>(player_);
    const auto* ps2 = world_.get<gameplay::PlayerState>(player_);
    const float left = tr != nullptr
                           ? glm::length(glm::vec3{approach_start_.at.x - tr->position.x,
                                                   0.0f,
                                                   approach_start_.at.z - tr->position.z})
                           : -1.0f;
    const float yaw_left =
        ps2 != nullptr ? std::fabs(shortest_arc(ps2->yaw, approach_start_.yaw)) : -1.0f;
    std::fprintf(stderr,
                 "[поза] подход к %s брошен: %s (оставалось %.3f м при пороге "
                 "%.3f, доворота %.3f рад при пороге %.3f, шли %.2f с)\n",
                 name.c_str(), why, static_cast<double>(left),
                 static_cast<double>(SEAT_ARRIVE_M), static_cast<double>(yaw_left),
                 static_cast<double>(SEAT_ALIGN_RAD), static_cast<double>(spent));
}

void App::approach_step(float dt) {
    if (approach_seat_ < 0) {
        return;
    }
    if (in_posture() || static_cast<std::size_t>(approach_seat_) >= seats_.size()) {
        cancel_approach("список точек сменился");
        return;
    }
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    const auto* tr = world_.get<components::Transform>(player_);
    if (ps == nullptr || tr == nullptr) {
        cancel_approach("игрока нет");
        return;
    }
    approach_s_ += dt;
    if (approach_s_ > SEAT_APPROACH_TIMEOUT_S) {
        cancel_approach("не дошёл за отведённое время (упёрся?)");
        return;
    }

    const glm::vec3 d{approach_start_.at.x - tr->position.x, 0.0f,
                      approach_start_.at.z - tr->position.z};
    const float left = glm::length(d);
    // ЧЕЛОВЕК ГЛАВНЕЕ АВТОПИЛОТА — НО «ПЕРЕДУМАЛ» ЗНАЧИТ ПОЙТИ ПРОЧЬ, А НЕ
    // ПРОСТО ДЕРЖАТЬ КЛАВИШУ. Живой игрок подходит к лавке НА ХОДУ и жмёт E,
    // не отпуская W; отмена по «есть ввод» отменяла бы каждую вторую посадку
    // человека, идущего к лавке. Поэтому судит ЗНАК: ввод, у которого
    // проекция на направление к точке отрицательна, — это уход. У стоящего
    // (дошёл или упёрся) направления уже нет, и тогда любой ввод — уход.
    // Порог тот же и по той же причине, что у выхода из позы: POSTURE_WALK_OUT.
    if (glm::length(ps->move_axes) > POSTURE_WALK_OUT || ps->jump_pressed) {
        const glm::vec3 fwd{std::sin(ps->yaw), 0.0f, -std::cos(ps->yaw)};
        const glm::vec3 rgt{std::cos(ps->yaw), 0.0f, std::sin(ps->yaw)};
        const glm::vec3 want = fwd * ps->move_axes.y + rgt * ps->move_axes.x;
        if (ps->jump_pressed || left <= SEAT_ARRIVE_M || glm::dot(want, d) <= 0.0f) {
            cancel_approach("игрок пошёл сам");
            return;
        }
    }
    // УПЁРСЯ — ЗНАЧИТ ПРИШЁЛ, НАСКОЛЬКО ЭТА КОМНАТА ПОЗВОЛЯЕТ. Не поблажка, а
    // ЗАМЕР: у лавки, приставленной к столу (а так они и стоят — seat_facing
    // разворачивает сидящего ЛИЦОМ К СТОЛУ), точка старта лежит В СТОЛЕ.
    // Замерено на x112z271: стол на z=2.50, лавка на z=2.00, между их телами
    // 0.10 м — капсула радиусом 0.35 туда не входит и встаёт на 0.10 м раньше
    // точки, а автопилот без этой ветки толкался в стол все 4.15 с и бросал
    // подход (числа его же следа). Место, дальше которого человека не пускают,
    // и ЕСТЬ место, с которого на эту лавку садятся: доворот и поза идут
    // оттуда, и посадка не зависит от того, поместился он или нет.
    if (left <= approach_best_m_ - SEAT_STALL_EPS_M) {
        approach_best_m_ = left;
        approach_stall_s_ = 0.0f;
    } else {
        approach_stall_s_ += dt;
    }
    const bool stalled = approach_stall_s_ >= SEAT_STALL_S;
    if (left > SEAT_ARRIVE_M && !stalled) {
        // ИДЁМ — И ИМЕННО ВВОДОМ. Оси и рыск это ТЕ ЖЕ намерения, которые
        // пишет клавиатура и очередь стенда (AppStand.h): дальше работает
        // настоящая локомоция, настоящая физика и настоящая походка. Автопилот
        // на Transform телепортировал бы человека мимо всего этого — ровно то,
        // что владелец велел убрать.
        ps->yaw = turn_body_toward(ps->yaw, std::atan2(d.x, -d.z), dt,
                                   static_cast<float>(config::BODY_TURN_RATE));
        ps->move_axes = glm::vec2{0.0f, 1.0f};
        if (posture_trace_on() && approach_trace_left_ == 0) {
            approach_trace_left_ = 60;
            std::fprintf(stderr,
                         "[поза-подход] t=%.2f (%.3f %.3f %.3f) рыск %.3f -> %.3f, "
                         "осталось %.3f м, скорость %.2f м/с\n",
                         static_cast<double>(approach_s_),
                         static_cast<double>(tr->position.x),
                         static_cast<double>(tr->position.y),
                         static_cast<double>(tr->position.z),
                         static_cast<double>(ps->yaw),
                         static_cast<double>(approach_start_.yaw),
                         static_cast<double>(left),
                         static_cast<double>(ps->stride_speed));
        }
        if (approach_trace_left_ > 0) {
            --approach_trace_left_;
        }
        ps->jog = false;
        ps->run = false;
        ps->crouch_held = false;
        return;
    }
    // ДОШЁЛ: СТОИМ И ДОВОРАЧИВАЕМСЯ. Доворот отдельной фазой, а не вместе с
    // ходьбой, потому что рыск точки старта смотрит ОТ предмета, а шёл человек
    // К предмету: развернуться на ходу значило бы уйти с точки.
    ps->move_axes = glm::vec2{0.0f};
    if (stalled && left > SEAT_ARRIVE_M && !approach_stall_said_) {
        approach_stall_said_ = true;
        std::fprintf(stderr,
                     "[поза] упёрся в %.3f м от точки старта (порог %.3f) — "
                     "сажусь отсюда: ближе эта комната не пускает\n",
                     static_cast<double>(left), static_cast<double>(SEAT_ARRIVE_M));
    }
    if (std::fabs(shortest_arc(ps->yaw, approach_start_.yaw)) > SEAT_ALIGN_RAD) {
        ps->yaw = turn_body_toward(ps->yaw, approach_start_.yaw, dt,
                                   static_cast<float>(config::BODY_TURN_RATE));
        return;
    }
    ps->yaw = approach_start_.yaw;
    const std::size_t index = static_cast<std::size_t>(approach_seat_);
    approach_seat_ = -1;
    approach_s_ = 0.0f;
    enter_posture(index);
}

void App::enter_posture(std::size_t index) {
    if (index >= seats_.size()) {
        return;
    }
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    auto* drive = world_.get<anim::BodyDrive>(player_);
    const auto* tr = world_.get<components::Transform>(player_);
    if (ps == nullptr || drive == nullptr || tr == nullptr) {
        return;
    }
    {
        const std::size_t i = index;
        const FurnitureSpot& spot = seats_[i].spot;
        posture_exit_ = tr->position;
        posture_exit_yaw_ = ps->yaw;
        active_seat_ = static_cast<int>(i);
        // НАЧАЛО СТРЕЛЫ ТРЕТЬЕГО ЛИЦА — НАД ПРЕДМЕТОМ, А НЕ НАД ПЛОЩАДКОЙ.
        // Берётся ВЕРХ САМОГО ПРЕДМЕТА (тот же габарит, которым целятся), а
        // не высота настила: у furn-bed матрас на 0.50, а столбики изголовья
        // — на 1.14, и щуп, поднятый над матрасом, начинал бы внутри столбика.
        // Плюс радиус щупа с отступом: сфера обязана начинать СВОБОДНОЙ.
        posture_perch_ = glm::vec3{
            spot.floor_at.x,
            spot.aim.centre.y + spot.aim.half.y + cam_boom_desc_.probe_radius
                + cam_boom_desc_.margin,
            spot.floor_at.z};
        posture_perch_valid_ = true;
        // ПОТОЛОК ТАНГАЖА — ТОЛЬКО ЛЕЖАЩЕМУ, и по названной причине: это его
        // ПОЗА поворачивает взгляд в потолок, а стрела уходит назад по
        // взгляду, то есть вниз, в матрас. У сидящего взгляд горизонтален и
        // стрела ведёт себя как у стоящего — навязывать ему взгляд под ноги
        // значило бы отнять комнату у всякого, кто присел в трактире.
        posture_pitch_cap_ = spot.kind == SpotKind::Lie
                                 ? gameplay::POSTURE_BOOM_PITCH_MAX
                                 : static_cast<float>(config::CAMERA_PITCH_LIMIT);

        drive->posture = spot.kind == SpotKind::Lie ? anim::Posture::Lie
                                                    : anim::Posture::Sit;
        drive->posture_ground = spot.floor_at;
        drive->posture_height_m = spot.surface_m;
        drive->posture_yaw =
            spot.kind == SpotKind::Lie
                ? anim::lie_yaw_for_head_dir(spot.facing.x, spot.facing.z)
                : std::atan2(spot.facing.x, -spot.facing.z);
        // ВЗГЛЯД РАЗВОРАЧИВАЕТСЯ ВМЕСТЕ С ТЕЛОМ, и дальше он СВОБОДЕН: голову
        // сидящему никто не держит. Лёжа взгляд ещё и поднимается в потолок —
        // человек, легший на спину, смотрит вверх, и не сделать этого значило
        // бы отдать ему первый кадр позы в стену у кровати.
        ps->yaw = drive->posture_yaw;
        // ТАНГАЖ ТОЖЕ СБРАСЫВАЕТСЯ, И У ОБЕИХ ПОЗ ПО СВОЕЙ ПРИЧИНЕ. Садясь,
        // человек смотрит НА ЛАВКУ, то есть вниз под 60°, — оставить ему этот
        // угол значило бы отдать первый кадр позы собственным коленям. Лёгши
        // на спину, он смотрит В ПОТОЛОК, и не поднять взгляд значило бы
        // отдать первый кадр стене у кровати. Дальше взгляд свободен: голову
        // сидящему никто не держит.
        ps->pitch = spot.kind == SpotKind::Lie
                        ? std::min(static_cast<float>(config::CAMERA_PITCH_LIMIT) * 0.65f,
                                   1.0f)
                        : 0.0f;
        // ...и беспилотная рука вправе довернуть его дальше (DFN_SEAT_TAKE
        // с двоеточием): взгляд в позе свободен, значит и прогон вправе им
        // распорядиться. 0 — руки не было, и умолчания выше остаются.
        if (seat_take_pitch_deg_ != 0.0f) {
            ps->pitch = seat_take_pitch_deg_ * 3.14159265358979f / 180.0f;
        }
        ps->yaw += seat_take_yaw_deg_ * 3.14159265358979f / 180.0f;
        std::fprintf(stderr,
                     "[поза] %s на %s: точка (%.2f %.2f %.2f), площадка %.3f м, "
                     "рыск %.3f; выход в (%.2f %.2f %.2f)\n",
                     spot.kind == SpotKind::Lie ? "ЛЁГ" : "СЕЛ", spot.source.c_str(),
                     static_cast<double>(spot.floor_at.x),
                     static_cast<double>(spot.floor_at.y),
                     static_cast<double>(spot.floor_at.z),
                     static_cast<double>(spot.surface_m),
                     static_cast<double>(drive->posture_yaw),
                     static_cast<double>(posture_exit_.x),
                     static_cast<double>(posture_exit_.y),
                     static_cast<double>(posture_exit_.z));
    }
}

void App::leave_posture() {
    if (!in_posture()) {
        return;
    }
    active_seat_ = -1;
    if (auto* drive = world_.get<anim::BodyDrive>(player_)) {
        // ЗАЯВКА СНИМАЕТСЯ, А БЛЕНДЕР ОСТАЁТСЯ: он доедет до нуля сам, за
        // anim::posture_transit_s(позы), и всё это время земля, рыск и ПОЗА
        // (drive.posture_shown) обязаны оставаться на месте — иначе тело
        // прыгнет к капсуле одним кадром, а с кровати встанет через сидячую.
        drive->posture = anim::Posture::None;
    }
    if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
        if (physics_ != nullptr) {
            physics_->teleport_character(ps->character, posture_exit_);
        }
        ps->yaw = posture_exit_yaw_;
        ps->vertical_velocity = 0.0f;
    }
    std::fprintf(stderr, "[поза] ВСТАЛ в (%.2f %.2f %.2f)\n",
                 static_cast<double>(posture_exit_.x),
                 static_cast<double>(posture_exit_.y),
                 static_cast<double>(posture_exit_.z));
}

// ---------------------------------------------------------------------------
// ПАРКОВКА И КАМЕРА
// ---------------------------------------------------------------------------

void App::park_posture() {
    // ПОДХОД ЖИВЁТ В ЭТОМ ЖЕ МЕСТЕ ТИКА, И ЭТО НЕ УДОБСТВО. Он ПИШЕТ
    // намерения (оси и рыск), а превращает их в перемещение player_pre_step —
    // значит слот ровно один: после того, как ввод накоплен, и до того, как
    // он потрачен. Тот же слот, в котором поза намерения ГАСИТ. Отдельный
    // вызов из App.cpp был бы вторым описанием одного и того же места.
    approach_step(static_cast<float>(timestep_.step_dt()));
    if (!in_posture()) {
        return;
    }
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    if (ps == nullptr) {
        return;
    }
    // ВЫХОД ПО ЛЮБОМУ ИЗ ТРЁХ: повторное E, прыжок, шаг. Клавиша E гасится
    // ЗДЕСЬ (защёлка), иначе тот же тик отдал бы её player_actions_step, и
    // человек встал бы и сел обратно за один кадр.
    const bool wants_out = ps->interact_pressed || ps->jump_pressed
                        || glm::length(ps->move_axes) > POSTURE_WALK_OUT;
    ps->interact_pressed = false;
    ps->jump_pressed = false;
    if (wants_out) {
        leave_posture();
        return;
    }
    // ДВИЖЕНИЕ ВЫКЛЮЧЕНО, А ВРЕМЯ ИДЁТ: тик мира не трогаем, гасим только
    // намерения. Гасить сам шаг физики значило бы остановить всё, что в мире
    // движется, ради одного сидящего.
    ps->move_axes = glm::vec2{0.0f};
    ps->crouch_held = false;
    ps->vertical_velocity = 0.0f;
    // КАПСУЛА ПАРКУЕТСЯ ТЕЛЕПОРТОМ НА ТОЧКУ ВЫХОДА, каждый тик. Контроллер
    // здесь КИНЕМАТИЧЕСКИЙ (platform::IPhysics: капсулу двигаем мы, а не
    // решатель), поэтому «выключить» его нечем и не нужно: положение,
    // переписываемое каждый тик, и есть парковка. Точка — та, где человек
    // стоял, когда сел: она заведомо не в теле мебели и не в стене.
    if (physics_ != nullptr) {
        physics_->teleport_character(ps->character, posture_exit_);
    }
}

void App::posture_trace_step(float dt) {
    if (!posture_trace_on()) {
        return;
    }
    const auto* drive = world_.get<anim::BodyDrive>(player_);
    if (drive == nullptr) {
        return;
    }
    const bool moving = drive->posture_blend > 0.0f
                     || drive->posture != anim::Posture::None;
    if (!moving) {
        posture_trace_t_ = 0.0;
        return;
    }
    posture_trace_t_ += static_cast<double>(dt);
    // ВЫСОТА ТАЗА СЧИТАЕТСЯ ТОЙ ЖЕ ПАРОЙ, КОТОРОЙ ТЕЛО НАРИСОВАНО, а не
    // отдельной формулой: земля корня плюс стоячая высота бедра плюс смещение
    // таза — ровно то, что forward_kinematics и прибавляет.
    const auto* tr = world_.get<components::Transform>(player_);
    const glm::vec3 stand = tr != nullptr ? tr->position : glm::vec3{0.0f};
    const anim::LocalPose pose = anim::evaluate_body_pose(body_rig_, *drive);
    const anim::BodyRoot root = anim::body_root_for(*drive, stand);
    const float pelvis_y = root.ground.y
                         + body_rig_.proportions.standing_hip_height()
                         + pose.pelvis_offset.y;
    const float eye_y = drive->eye_valid
                            ? drive->eye_point.y
                            : stand.y + static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const anim::PostureTransit w =
        anim::posture_transit(anim::drawn_posture(*drive), drive->posture_blend);
    std::fprintf(stderr,
                 "[поза-след] t=%.4f blend=%.4f take=%.4f drop=%.4f plan=%.4f "
                 "recline=%.4f таз=%.4f глаз=%.4f\n",
                 posture_trace_t_, static_cast<double>(drive->posture_blend),
                 static_cast<double>(w.take), static_cast<double>(w.drop),
                 static_cast<double>(w.plan), static_cast<double>(w.recline),
                 static_cast<double>(pelvis_y), static_cast<double>(eye_y));
}

void App::posture_camera() {
    const auto* drive = world_.get<anim::BodyDrive>(player_);
    auto* cam = world_.get<components::CameraPose>(player_);
    if (drive == nullptr || cam == nullptr || !drive->eye_valid) {
        return;
    }
    // ГЛАЗ БЕРЁТСЯ У ТЕЛА, А НЕ СЧИТАЕТСЯ ЗАНОВО. update_bodies опубликовал
    // его из ТОЙ ЖЕ позы и того же корня, которыми только что поставлены
    // сегменты, — камера и тело не могут разойтись, потому что число одно.
    // Направление остаётся игроку: голову сидящему никто не держит, и третье
    // лицо разворачивает стрелу вокруг этой же точки (App.cpp).
    cam->position = drive->eye_point;
}

// ---------------------------------------------------------------------------
// ПРИБОР
// ---------------------------------------------------------------------------

void App::probe_seats() {
    if (seats_.empty()) {
        std::fprintf(stderr, "[поза] прибор: точек нет\n");
        return;
    }
    std::size_t ok_front = 0;
    std::size_t ok_side = 0;
    std::size_t ok_back = 0;
    std::size_t ok_far = 0;
    for (const SeatLink& link : seats_) {
        const SeatAim& aim = link.spot.aim;
        // ЧЕТЫРЕ РУКИ, и три из них ОБЯЗАНЫ ПРОВАЛИТЬСЯ (правило 30): прибор,
        // у которого все руки зелёные, не мерит ничего.
        const glm::vec3 up{0.0f, 1.0f, 0.0f};
        const glm::vec3 n = link.spot.kind == SpotKind::Lie
                                ? glm::normalize(glm::cross(link.spot.facing, up))
                                : link.spot.facing;
        const glm::vec3 eye = aim.centre - n * (aim.half.z + 0.6f)
                            + glm::vec3{0.0f, 1.2f, 0.0f};
        if (seat_aim(aim, eye, glm::normalize(aim.centre - eye)).ok) {
            ++ok_front;
        }
        const glm::vec3 aside = aim.centre + glm::cross(n, up) * 3.0f;
        if (seat_aim(aim, eye, glm::normalize(aside - eye)).ok) {
            ++ok_side;
        }
        if (seat_aim(aim, eye, glm::normalize(eye - aim.centre)).ok) {
            ++ok_back;
        }
        const glm::vec3 far_eye = aim.centre - n * (aim.half.z + 3.0f)
                                + glm::vec3{0.0f, 1.2f, 0.0f};
        if (seat_aim(aim, far_eye, glm::normalize(aim.centre - far_eye)).ok) {
            ++ok_far;
        }
    }
    std::fprintf(stderr,
                 "[поза] прибор по %zu точкам: смотрю в предмет %zu, мимо %zu, "
                 "спиной %zu, издали %zu (ждём %zu/0/0/0)\n",
                 seats_.size(), ok_front, ok_side, ok_back, ok_far, seats_.size());
    // ...И ТОЧКА СТАРТА КАЖДОЙ, потому что подход меряется ею, а не прицелом:
    // «подсказка горит» и «есть куда встать» — два разных утверждения, и
    // прибор, печатающий только первое, второго не измеряет.
    const auto* ptr = world_.get<components::Transform>(player_);
    const glm::vec3 from = ptr != nullptr ? ptr->position : glm::vec3{0.0f};
    for (const SeatLink& link : seats_) {
        const PostureStart st = posture_start(link.spot, from);
        std::fprintf(stderr,
                     "[поза] %s: старт (%.2f %.2f %.2f), рыск %.3f, от игрока "
                     "%.2f м%s\n",
                     link.spot.source.c_str(), static_cast<double>(st.at.x),
                     static_cast<double>(st.at.y), static_cast<double>(st.at.z),
                     static_cast<double>(st.yaw),
                     static_cast<double>(glm::length(
                         glm::vec3{st.at.x - from.x, 0.0f, st.at.z - from.z})),
                     st.valid ? "" : " — ТОЧКИ НЕТ");
    }
}

// ---------------------------------------------------------------------------
// БЕСПИЛОТНАЯ РУКА (DFN_SEAT_TAKE)
// ---------------------------------------------------------------------------

void App::drive_seat_take() {
    static const std::string want = [] {
        const char* e = door_value("DFN_SEAT_TAKE");
        return std::string(e != nullptr ? e : "");
    }();
    if (want.empty() || seat_take_stage_ >= 3 || mode_ != AppMode::Playing) {
        return;
    }
    // --- КЛАВИША ЛЕЖИТ: КАЖДЫЙ КАДР, ДО СЧЁТЧИКА ФАЗ ----------------------
    // Держать её через фазовый счётчик нельзя: он пропускает кадры, а палец
    // на клавише кадров не пропускает.
    if (seat_take_hold_left_ > 0) {
        auto* hps = world_.get<gameplay::PlayerState>(player_);
        // ВЗГЛЯД УХОДИТ, ПОКА ПАЛЕЦ ЛЕЖИТ — это и есть рука на мыши, которую
        // мерит рукав: живой человек, нажав E, взгляд не замораживает.
        if (hps != nullptr && seat_take_hold_frames_ > 0) {
            hps->pitch += seat_take_hand_turn_deg_ * 3.14159265358979f / 180.0f
                        / static_cast<float>(seat_take_hold_frames_);
        }
        --seat_take_hold_left_;
        if (seat_take_hold_left_ == 0) {
            grab_probe_key_ = false; // ОТПУСТИЛ: здесь защёлка и возвращается
            std::fprintf(stderr, "[поза] дверь DFN_SEAT_TAKE: отпустил E\n");
        }
        return;
    }
    if (seat_take_frames_ == 0) {
        seat_take_lie_ = want.rfind("lie", 0) == 0;
        // РУКА, КОТОРАЯ ДЕРЖИТ (DFN_SEAT_TAKE=sit+24@30): 24 кадра лежит
        // клавиша, за них взгляд уходит на 30°. Именно этой парой меряется
        // «садится не каждый раз», и обе половины названы числом, потому что
        // жребий живой руки числом не назван (правило 30).
        if (const std::size_t plus = want.find('+'); plus != std::string::npos) {
            char* rest = nullptr;
            seat_take_hold_frames_ =
                std::strtoull(want.c_str() + plus + 1, &rest, 10);
            if (rest != nullptr && *rest == '@') {
                seat_take_hand_turn_deg_ = std::strtof(rest + 1, nullptr);
            }
            std::fprintf(stderr,
                         "[поза] дверь DFN_SEAT_TAKE: рука держит E %llu кадров, "
                         "уводя взгляд на %.1f°\n",
                         static_cast<unsigned long long>(seat_take_hold_frames_),
                         static_cast<double>(seat_take_hand_turn_deg_));
        }
        // ДОВОРОТ ВЗГЛЯДА ПОСЛЕ ПОСАДКИ (DFN_SEAT_TAKE=lie:-35), градусы.
        // Нужен ровно затем же, зачем DFN_INTERIOR_TURN: в позе взгляд
        // СВОБОДЕН, и третье лицо разворачивает стрелу вокруг него — у
        // лежащего, глядящего в потолок, стрела уходит вниз в кровать, и
        // коллизия честно прижимает её к голове. Кадр «как это выглядит со
        // стороны» умеет снять только рука на мыши, и это она.
        if (const std::size_t colon = want.find(':'); colon != std::string::npos) {
            char* rest = nullptr;
            seat_take_pitch_deg_ = std::strtof(want.c_str() + colon + 1, &rest);
            if (rest != nullptr && *rest == ',') {
                seat_take_yaw_deg_ = std::strtof(rest + 1, nullptr);
            }
            std::fprintf(stderr,
                         "[поза] дверь DFN_SEAT_TAKE: доворот взгляда %.1f° "
                         "тангаж, %.1f° рыск\n",
                         static_cast<double>(seat_take_pitch_deg_),
                         static_cast<double>(seat_take_yaw_deg_));
        }
        // ПОДВОД — 15 КАДРОВ, и это не «побольше на всякий случай»: точки
        // заводятся на входе в локацию, а вход показывает свой экран загрузки,
        // и жать раньше значило бы жать в пустой список.
        seat_take_frames_ = SEAT_TAKE_APPROACH_FRAMES;
    }
    ++seat_take_seen_;
    if (seat_take_seen_ < seat_take_frames_) {
        return;
    }
    seat_take_seen_ = 0;
    // СИДЕТЬ ДОЛЬШЕ, ЧЕМ ИДЁТ ПЕРЕХОД, И ЗАМЕТНО. Снимок позы обязан застать
    // блендер ДОЕХАВШИМ (anim::posture_transit_s: 0.60 с сесть, 0.90 с лечь),
    // а кадры считаются штуками, не секундами: при 230 кадрах в секунду 0.90 с
    // — это 207 кадров (замер этого прогона). 480 даёт запас и на медленную
    // машину, и на кадр, снятый позже; серия ФАЗ перехода снимается не здесь,
    // а меньшими значениями DFN_SHOT_AFTER (приёмка, seat-poses.md).
    seat_take_frames_ = SEAT_TAKE_HOLD_FRAMES;
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    if (ps == nullptr) {
        std::fprintf(stderr, "[поза] дверь DFN_SEAT_TAKE: игрока нет\n");
        seat_take_stage_ = 3;
        return;
    }
    if (seat_take_stage_ == 2) {
        // ВСТАТЬ — ТОЙ ЖЕ КЛАВИШЕЙ. park_posture() снимет защёлку и поднимет.
        std::fprintf(stderr, "[поза] дверь DFN_SEAT_TAKE: жму E, чтобы встать\n");
        ps->interact_pressed = true;
        seat_take_stage_ = 3;
        return;
    }
    if (seat_take_stage_ == 1) {
        // ПОДВОД БЫЛ РАНЬШЕ, ТЕПЕРЬ НАЖАТИЕ. Разделены намеренно, см.
        // SEAT_TAKE_SETTLE_FRAMES: телепорт подвода — это приземление, и
        // переход, начатый в тот же тик, стартует из проваленного таза.
        std::fprintf(stderr, "[поза] дверь DFN_SEAT_TAKE: жму E\n");
        ps->interact_pressed = true;
        if (seat_take_hold_frames_ > 0) {
            // ...И ДЕРЖУ, КАК ДЕРЖИТ РУКА. Обе половины нажатия сразу: сырое
            // состояние клавиши (grab_probe_key_ — та же рука, которую читает
            // grab_input) и защёлка. Приложение заберёт защёлку себе на всё
            // время лежания и вернёт её на отпускании — ровно то, что делает
            // живой палец.
            grab_probe_key_ = true;
            seat_take_hold_left_ = seat_take_hold_frames_;
        }
        seat_take_frames_ = SEAT_TAKE_HOLD_FRAMES;
        seat_take_stage_ = 2;
        return;
    }

    const SpotKind kind = seat_take_lie_ ? SpotKind::Lie : SpotKind::Seat;
    const SeatLink* best = nullptr;
    float best_d = 1.0e9f;
    const auto* tr = world_.get<components::Transform>(player_);
    const glm::vec3 from = tr != nullptr ? tr->position : glm::vec3{0.0f};
    for (const SeatLink& link : seats_) {
        if (link.spot.kind != kind) {
            continue;
        }
        const float d = glm::length(link.spot.floor_at - from);
        if (d < best_d) {
            best_d = d;
            best = &link;
        }
    }
    if (best == nullptr) {
        std::fprintf(stderr,
                     "[поза] дверь DFN_SEAT_TAKE=%s: подходящей точки на "
                     "локации нет (точек всего %zu)\n",
                     want.c_str(), seats_.size());
        seat_take_stage_ = 2;
        return;
    }

    // ПОДВОД: встать перед предметом со стороны ПОДХОДА и развернуться на него.
    // У сиденья сторона подхода — та, куда смотрит сидящий (лицом к лавке);
    // у лежака — бок кровати, потому что в изголовье не подходят.
    // ...И СО СТОРОНЫ ПОДХОДА, а не с противоположной. Знак здесь стоял
    // минусом и ставил руку ЗА лавку — там, куда сидящий смотрит спиной; в
    // комнате это стена. Сторона берётся у той же меты, что и точка старта
    // (posture_start), и рука встаёт ДАЛЬШЕ неё на SEAT_TAKE_STANDOFF_M,
    // чтобы подходу было что пройти: прибор, ставящий руку ровно в точку
    // старта, мерил бы подход, которого не было (правило 47).
    const PostureStart hand_start = posture_start(best->spot, from);
    const float depth = seat_aim_support(best->spot.aim, hand_start.facing);
    const glm::vec3 stand =
        best->spot.floor_at + hand_start.facing * (depth + SEAT_TAKE_STANDOFF_M);
    if (physics_ != nullptr) {
        physics_->teleport_character(ps->character, stand);
    }
    if (auto* t = world_.get<components::Transform>(player_)) {
        t->position = stand;
    }
    if (auto* pt = world_.get<components::PreviousTransform>(player_)) {
        pt->position = stand;
    }
    const glm::vec3 eye = stand + glm::vec3{0.0f,
                                            static_cast<float>(config::PLAYER_EYE_HEIGHT),
                                            0.0f};
    const glm::vec3 to = best->spot.aim.centre - eye;
    ps->yaw = std::atan2(to.x, -to.z);
    ps->pitch = std::atan2(to.y, glm::length(glm::vec2{to.x, to.z}));
    const SeatAimHit hit = seat_aim(best->spot.aim, eye, glm::normalize(to));
    std::fprintf(stderr,
                 "[поза] дверь DFN_SEAT_TAKE=%s: подвёл к %s, до габарита "
                 "%.2f м, прицел %s; стою %llu кадров и жму E\n",
                 want.c_str(), best->spot.source.c_str(),
                 static_cast<double>(hit.distance_m),
                 hit.ok ? "ГОРИТ" : "МОЛЧИТ — позы не будет",
                 static_cast<unsigned long long>(SEAT_TAKE_SETTLE_FRAMES));
    seat_take_frames_ = SEAT_TAKE_SETTLE_FRAMES;
    seat_take_stage_ = 1;
}

} // namespace dfn::app
