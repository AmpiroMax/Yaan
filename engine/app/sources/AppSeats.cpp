/*
Created: 28:08:2026 - 12:20:00
Last updated: 28:08:2026 - 12:20:00
Module: engine/app
File: engine/app/sources/AppSeats.cpp

Responsibility:
- СИДЕТЬ И ЛЕЖАТЬ (обязательство эпохи «Большой мир», заказ владельца 28.08):
  сбор точек позы по залитой локации, прицел радиус+взгляд у мебели, клавиша E
  туда и обратно, парковка капсулы и камера из глаза позы.

Key items:
- App::spawn_furniture_seats / clear_furniture_seats: точки как вещь мира.
- App::filter_seat_hover: подсказка только когда СМОТРИМ на предмет.
- App::take_seat / leave_posture: вход в позу и выход из неё.
- App::park_posture / posture_camera: движение выключено, глаз — из позы.
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
/*
UPD:
- 28:08:2026 - 12:20:00: Создан. Пункты 1, 3, 4 заказа: мета у существующих
  тел мебели, взаимодействие E, парковка капсулы и камера позы.
*/

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/AppInternal.h"
#include "engine/app/sources/App.h"
#include "engine/app/sources/Localization.h"

#include "engine/anim/sources/Body.h"
#include "engine/core/components/sources/Components.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
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

namespace dfn::app {

namespace {

/// СКОЛЬКО ХОДА ХВАТАЕТ, ЧТОБЫ СЧЕСТЬ «ОН ПОШЁЛ» и поднять из позы. Не ноль:
/// у джойстика и у мёртвой зоны мыши бывает шум, и вставать от шума — это
/// поза, из которой нельзя посидеть.
constexpr float POSTURE_WALK_OUT = 0.30f;

/// Кадры беспилотной руки (DFN_SEAT_TAKE): подвод и выдержка в позе.
constexpr std::uint64_t SEAT_TAKE_APPROACH_FRAMES = 15;
constexpr std::uint64_t SEAT_TAKE_HOLD_FRAMES = 240;

/// Единичный вектор взгляда по позе камеры — та же формула, что у дверей
/// (door_aim_now) и у InteractionSystem::view_direction. Расхождение здесь
/// значило бы, что подсказка горит не там, куда указывает перекрестье.
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
        return;
    }
    auto& hover = world_.resource<components::HoverTarget>();
    if (hover.prompt_key == 0 || !world_.alive(hover.entity)) {
        return;
    }
    for (const SeatLink& link : seats_) {
        if (link.entity != hover.entity) {
            continue;
        }
        // ЛУЧ ПОПАЛ В ТЕЛО — И ЭТОГО МАЛО, ровно как у дверей: коробка прицела
        // шире предмета на SEAT_AIM_PAD_M, и попадание в её кромку из-за спины
        // зажигало бы «Сесть» у человека, стоящего к лавке спиной.
        if (!seat_aim_now(link).ok) {
            hover = components::HoverTarget{};
        }
        return;
    }
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
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    auto* drive = world_.get<anim::BodyDrive>(player_);
    const auto* tr = world_.get<components::Transform>(player_);
    if (ps == nullptr || drive == nullptr || tr == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < seats_.size(); ++i) {
        if (seats_[i].action != action) {
            continue;
        }
        const FurnitureSpot& spot = seats_[i].spot;
        posture_exit_ = tr->position;
        posture_exit_yaw_ = ps->yaw;
        active_seat_ = static_cast<int>(i);

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
        return;
    }
}

void App::leave_posture() {
    if (!in_posture()) {
        return;
    }
    active_seat_ = -1;
    if (auto* drive = world_.get<anim::BodyDrive>(player_)) {
        // ЗАЯВКА СНИМАЕТСЯ, А БЛЕНДЕР ОСТАЁТСЯ: он доедет до нуля сам, за
        // POSTURE_BLEND_TIME_S, и всё это время земля и рыск позы обязаны
        // оставаться на месте — иначе тело прыгнет к капсуле одним кадром.
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
}

// ---------------------------------------------------------------------------
// БЕСПИЛОТНАЯ РУКА (DFN_SEAT_TAKE)
// ---------------------------------------------------------------------------

void App::drive_seat_take() {
    static const std::string want = [] {
        const char* e = door_value("DFN_SEAT_TAKE");
        return std::string(e != nullptr ? e : "");
    }();
    if (want.empty() || seat_take_stage_ >= 2 || mode_ != AppMode::Playing) {
        return;
    }
    if (seat_take_frames_ == 0) {
        seat_take_lie_ = want.rfind("lie", 0) == 0;
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
    // блендер ДОЕХАВШИМ (POSTURE_BLEND_TIME_S = 0.18 с), а кадры считаются
    // штуками, не секундами: при 180 кадрах в секунду 0.18 с — это 32 кадра.
    // 240 даёт запас и на медленную машину, и на кадр, снятый позже.
    seat_take_frames_ = SEAT_TAKE_HOLD_FRAMES;
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    if (ps == nullptr) {
        std::fprintf(stderr, "[поза] дверь DFN_SEAT_TAKE: игрока нет\n");
        seat_take_stage_ = 2;
        return;
    }
    if (seat_take_stage_ == 1) {
        // ВСТАТЬ — ТОЙ ЖЕ КЛАВИШЕЙ. park_posture() снимет защёлку и поднимет.
        std::fprintf(stderr, "[поза] дверь DFN_SEAT_TAKE: жму E, чтобы встать\n");
        ps->interact_pressed = true;
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
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const glm::vec3 n = kind == SpotKind::Lie
                            ? glm::normalize(glm::cross(best->spot.facing, up))
                            : best->spot.facing;
    const float depth = kind == SpotKind::Lie ? best->spot.aim.half.x
                                              : best->spot.aim.half.z;
    const glm::vec3 stand = best->spot.floor_at - n * (depth + 0.55f);
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
                 "%.2f м, прицел %s; жму E\n",
                 want.c_str(), best->spot.source.c_str(),
                 static_cast<double>(hit.distance_m),
                 hit.ok ? "ГОРИТ" : "МОЛЧИТ — позы не будет");
    ps->interact_pressed = true;
    seat_take_stage_ = 1;
}

} // namespace dfn::app
