/*
Created: 23:08:2026 - 23:50:00
Last updated: 24:08:2026 - 03:20:00
Module: engine/app
File: engine/app/sources/AppInterior.cpp

Responsibility:
- ПЕРЕХОД В ИНТЕРЬЕР-ЛОКАЦИЮ И ОБРАТНО (docs/plans/INTERIORS_I15.md, волна А).
  Карман под городом, подвес экстерьера, второй слот отрисовки со своим телом
  Jolt и страховочной плитой, свет локации, точка возврата, экран загрузки.

Key items:
- App::enter_interior / App::leave_interior: переход, оба конца.
- App::upload_interior_body: тело интерьера + страховочная плита.
- App::spawn_scene_portals / clear_scene_portals: [portal] как вещь мира.
- App::present_loading_frame: один кадр экрана загрузки.

Dependencies:
- Uses: engine/world (Scene, HouseFile), engine/render (RenderSystem,
  LoadingScreen), engine/gameplay (InteriorState, Interaction), platform.
- Used by: App (двери DFN_INTERIOR, активация перехода, выход).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона lead владеет этим файлом.
- ГОРОД НЕ ВЫГРУЖАЕТСЯ. Ни один чанк, ни одно тело, ни один буфер. Выгрузка
  сделала бы выход второй загрузкой карты, а свод даёт на выход 0.05 с —
  на два порядка меньше, чем стоит открыть Вайтран.
- КАРМАН ПО Y, И ЭТО ЗАМЕР, А НЕ ВКУС. Ключ ячейки пакетирования потоков
  различает XZ только в полосе -256..1792 м (свод, «База»): карман в сторону
  попал бы в ту же ячейку, что и город, и отсечение по пирамиде перестало бы
  работать разом для обоих.
- ПОЛПУТИ ВНУТРЬ ХУЖЕ ЗАКРЫТОЙ ДВЕРИ. Всё, что может не получиться (чтение
  сцены, чтение .dfh), делается ДО первой правки состояния мира. Игрок,
  оказавшийся в кармане без пола, не может ни выйти, ни пожаловаться внятно.
*/
/*
UPD:
- 23:08:2026 - 23:50:00: Создан. И15 волна А, шаги 2-4: карман, подвес, слот,
  портал, экран загрузки, свет локации, точка возврата.
- 24:08:2026 - 03:20:00: ДЛИТЕЛЬНОСТЬ ЭКРАНА СТАЛА НАСТОЯЩЕЙ. DFN_INTERIOR_FADE
  читалась как «показывать или нет», а секунды в ней не значили ничего:
  экран жил ровно один кадр при любом значении. hold_loading_screen()
  показывает кадры, пока не выйдет срок, — и именно КАДРЫ, а не сон: спящее
  окно перестаёт отвечать ОС посреди собственного экрана загрузки.
  И СНИМОК САМОГО ЭКРАНА: он живёт между двумя кадрами штатной петли и
  потому невидим для DFN_SHOT_AFTER, который считает показанные ею кадры;
  пишется ровно один раз за прогон туда, куда прогон уже кладёт снимки.
*/

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/AppInternal.h"
#include "engine/app/sources/App.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/Interaction.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/Interior.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/world/sources/HouseFile.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace dfn::app {
namespace {

/// ГЛУБИНА КАРМАНА. Не «подальше», а число с причиной: город Вайтрана лежит в
/// полосе высот 20..90 м, самая высокая гора карты не достаёт 200 м, а
/// туман композиции кончается на 380 м. Километр вниз оставляет между
/// подвалом города и потолком локации шестикратный запас по самому дальнему
/// из этих чисел — и остаётся круглым числом, которое видно в отладчике.
constexpr float POCKET_DEPTH_M = 1000.0f;

/// СТРАХОВОЧНАЯ ПЛИТА: насколько ниже пола локации и насколько широка.
/// Пол интерьера — обычная геометрия, и дырка в ней при -1000 м означала бы
/// падение без дна: игрок летит вечно, кадр показывает чёрное, и ни один
/// прибор не скажет, что случилось. Плита превращает это в «стою на пустом
/// месте под домом» — состояние, которое видно и называемо.
constexpr float PLATE_DROP_M = 5.0f;
constexpr float PLATE_HALF_M = 128.0f;

} // namespace

void App::present_loading_frame() {
    if (renderer_ == nullptr || window_ == nullptr) {
        return;
    }
    render::PixelCanvas& c = render_system_.hud();
    if (c.width() == 0 || c.height() == 0) {
        return;
    }
    loading_.draw(c);
    // БЕЗ ЭТОЙ СТРОКИ ЭКРАН НЕВИДИМ, и это не гипотеза: прогресс запекания
    // ассетов рисовался в тот же холст с 18.08 и никогда не показывался —
    // render гейтит блит на hud_visible_, а его выставляет только run().
    render_system_.set_hud_visible(true);
    window_->poll_events();
    render_system_.render(world_, *renderer_, camera_, 0.0f);
    // КАДР ЭКРАНА ЗАГРУЗКИ ИНАЧЕ НЕ ПОПАДАЕТ НИ НА ОДИН БЕСПИЛОТНЫЙ ПРОГОН.
    // Экран живёт между двумя кадрами штатной петли и потому невидим для
    // DFN_SHOT_AFTER, который считает ПОКАЗАННЫЕ ею кадры. Снимок пишется
    // ровно один раз за прогон и только туда, куда прогон уже складывает
    // снимки, — новой двери для этого не нужно.
    if (!loading_shot_ && !capture_dir_.empty() && unattended_run()) {
        loading_shot_ = true;
        (void)renderer_->save_screenshot(capture_dir_ + "/loading_000.png");
    }
}

void App::hold_loading_screen() {
    // ДЛИТЕЛЬНОСТЬ ЭКРАНА — ЭТО КАДРЫ, А НЕ ОДИН КАДР И ЗАСЫПАНИЕ. Спать
    // нельзя: окно перестаёт отвечать ОС, и рабочий стол помечает
    // приложение как зависшее посреди его же экрана загрузки (то же
    // рассуждение, что у полосы запекания ассетов).
    if (interior_fade_s_ <= 0.0f) {
        return;
    }
    const auto until = std::chrono::steady_clock::now()
                     + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                           std::chrono::duration<float>(interior_fade_s_));
    while (std::chrono::steady_clock::now() < until) {
        present_loading_frame();
    }
}

void App::upload_interior_body(const std::vector<std::uint32_t>& indices) {
    if (physics_ == nullptr) {
        return;
    }
    if (interior_body_.valid()) {
        physics_->destroy_body(interior_body_);
        interior_body_ = {};
    }
    if (interior_positions_.empty() || indices.empty()) {
        // Пустая локация — законное состояние (город без открытого дома).
        // Плита при этом тоже не нужна: падать некому.
        if (interior_plate_.valid()) {
            physics_->destroy_body(interior_plate_);
            interior_plate_ = {};
        }
        return;
    }
    platform::TerrainMeshDesc desc;
    desc.positions = interior_positions_;
    desc.indices = indices;
    desc.layer = physics::LAYER_STATIC;
    interior_body_ = physics_->create_terrain_mesh(desc);
    if (!interior_body_.valid()) {
        std::fprintf(stderr,
                     "[интерьер] коллайдер локации НЕ создан — сквозь стены "
                     "комнаты можно пройти\n");
    }
    if (interior_plate_.valid()) {
        physics_->destroy_body(interior_plate_);
        interior_plate_ = {};
    }
    platform::StaticBoxDesc plate;
    plate.center = {interior_pocket_.x, interior_pocket_.y - PLATE_DROP_M,
                    interior_pocket_.z};
    plate.half_extents = {PLATE_HALF_M, 0.5f, PLATE_HALF_M};
    plate.layer = physics::LAYER_STATIC;
    interior_plate_ = physics_->create_static_box(plate);
}

void App::clear_scene_portals(bool interior) {
    for (auto it = portals_.begin(); it != portals_.end();) {
        if (it->interior != interior) {
            ++it;
            continue;
        }
        if (world_.alive(it->entity)) {
            world_.destroy_deferred(it->entity);
        }
        it = portals_.erase(it);
    }
    world_.flush_destroyed();
    if (physics_ == nullptr
        || !world_.has_resource<gameplay::InteractableBodies>()) {
        return;
    }
    // Тела прицела снимает штатная уборка взаимодействий — она уже умеет
    // это делать по мёртвой сущности, и вторая уборка была бы вторым
    // владельцем одного ресурса.
    gameplay::reap_interactable_bodies(world_, *physics_);
}

void App::spawn_scene_portals(const world::SceneDoc& doc, bool interior) {
    if (physics_ == nullptr) {
        return;
    }
    clear_scene_portals(interior);
    for (std::size_t i = 0; i < doc.portals.size(); ++i) {
        const world::ScenePortal& P = doc.portals[i];
        const ecs::EntityId id = world_.spawn();
        // ПЕРЕХОД НЕ РИСУЕТСЯ. Створка двери уже стоит в сцене как деталь
        // постройки; сущность здесь — только прицел и подсказка. Второй,
        // нарисованный «предмет-дверь» поверх настоящей двери — это два
        // описания одной вещи, и они разъедутся при первой правке дома.
        world_.add(id, components::Transform{.position = P.at,
                                             .rotation = {1.0f, 0.0f, 0.0f, 0.0f},
                                             .scale = glm::vec3{1.0f}});
        const std::uint64_t action =
            serialization::fnv1a64(P.to + "@" + std::to_string(i));
        world_.add(id, gameplay::Highlightable{
                           .prompt_key = interior ? "prompt.leave" : "prompt.enter",
                           .max_use_distance = std::max(P.radius_m, 0.5f)});
        world_.add(id, gameplay::Usable{.action = action,
                                        .repeatable = true,
                                        .used = false});
        platform::StaticBoxDesc box;
        box.center = P.at;
        const float r = std::max(P.radius_m, 0.4f);
        box.half_extents = {r, std::max(r, 1.0f), r};
        box.layer = physics::LAYER_INTERACTABLE;
        box.user_data = id.packed();
        const platform::PhysicsBodyHandle body = physics_->create_static_box(box);
        if (body.valid()) {
            if (!world_.has_resource<gameplay::InteractableBodies>()) {
                world_.add_resource(gameplay::InteractableBodies{});
            }
            world_.resource<gameplay::InteractableBodies>()
                .bodies[id.packed()] = body;
        }
        portals_.push_back(PortalLink{action, i, interior, id});
    }
    if (!doc.portals.empty()) {
        std::fprintf(stderr, "[интерьер] переходов на %s: %zu\n",
                     interior ? "локации" : "карте", doc.portals.size());
    }
}

void App::take_portal() {
    if (pending_portal_ == 0) {
        return;
    }
    const std::uint64_t action = pending_portal_;
    pending_portal_ = 0;
    for (const PortalLink& link : portals_) {
        if (link.action != action) {
            continue;
        }
        const world::SceneDoc& doc = link.interior ? interior_doc_ : scene_doc_;
        if (link.index >= doc.portals.size()) {
            return;
        }
        const world::ScenePortal P = doc.portals[link.index];
        if (world::portal_is_back(P)) {
            leave_interior();
        } else {
            (void)enter_interior(P.to, P.to_spawn);
        }
        return;
    }
}

bool App::enter_interior(const std::string& scene_path,
                         const std::string& spawn_name) {
    if (renderer_ == nullptr || physics_ == nullptr || !world_.alive(player_)) {
        return false;
    }
    if (!world_.has_resource<gameplay::InteriorState>()) {
        world_.add_resource(gameplay::InteriorState{});
    }
    auto& state = world_.resource<gameplay::InteriorState>();
    if (state.depth() >= gameplay::INTERIOR_MAX_DEPTH) {
        // ПРЕДЕЛ — КОНСТАНТА, А НЕ УСТРОЙСТВО (владелец 24.08). Состояние
        // умеет стек любой глубины; сегодня разрешена одна ступень, и отказ
        // звучит вслух, а не проглатывается.
        std::fprintf(stderr,
                     "[интерьер] вложенность %zu уже на пределе (%zu) — вход в "
                     "%s ОТКАЗАН\n",
                     state.depth(), gameplay::INTERIOR_MAX_DEPTH,
                     scene_path.c_str());
        return false;
    }

    const auto t0 = std::chrono::steady_clock::now();
    static const bool load_log = [] {
        const char* e = door_value("DFN_LOAD_LOG");
        return e != nullptr && *e != '\0' && *e != '0';
    }();
    loading_.set_log(load_log);
    loading_.begin("Вход", scene_path);
    loading_.set_expected(5);
    if (interior_fade_s_ > 0.0f) {
        present_loading_frame();
    }

    // ---- ВСЁ, ЧТО МОЖЕТ НЕ ПОЛУЧИТЬСЯ, ДЕЛАЕТСЯ ДО ПРАВКИ МИРА ----
    world::SceneDoc doc;
    std::string serr;
    if (!world::read_scene(scene_path, doc, serr)) {
        std::fprintf(stderr, "[интерьер] %s: %s — вход ОТМЕНЁН\n",
                     scene_path.c_str(), serr.c_str());
        loading_.hide();
        return false;
    }
    loading_.stage("сцена локации прочитана");
    // ГДЕ КАРМАН. По центру города, чтобы дальность звука, тумана и всего
    // остального, что меряется от начала мира, осталась той же величины.
    const glm::vec4 wb = chunks_.world_bounds_xz();
    interior_pocket_ = {(wb.x + wb.z) * 0.5f, -POCKET_DEPTH_M,
                        (wb.y + wb.w) * 0.5f};

    const bool resident = (interior_resident_ == scene_path);
    if (!resident) {
        std::vector<PlacedHouse> houses;
        for (std::size_t hi = 0; hi < doc.houses.size(); ++hi) {
            const world::ScenePlacedHouse& H = doc.houses[hi];
            std::ifstream in(H.file);
            if (!in.good()) {
                std::fprintf(stderr,
                             "[интерьер] [house] %s: файл не открылся — вход "
                             "ОТМЕНЁН\n",
                             H.file.c_str());
                loading_.hide();
                return false;
            }
            std::stringstream ss;
            ss << in.rdbuf();
            PlacedHouse ph;
            const world::HouseIoResult io = world::read_house(ss.str(), ph.graph);
            if (!io.ok) {
                std::fprintf(stderr, "[интерьер] [house] %s:%d: %s — вход ОТМЕНЁН\n",
                             H.file.c_str(), io.line, io.why.c_str());
                loading_.hide();
                return false;
            }
            ph.pos = H.position + interior_pocket_;
            ph.yaw = H.yaw;
            ph.scene_index = hi;
            houses.push_back(std::move(ph));
        }
        interior_houses_ = std::move(houses);
        loading_.stage("постройки локации прочитаны");
        upload_house_mesh(/*interior_only=*/true);
        loading_.stage("геометрия и коллайдер локации");
        interior_resident_ = scene_path;
    }

    // ---- С ЭТОГО МЕСТА МИР МЕНЯЕТСЯ ----
    if (!city_lights_saved_) {
        city_lights_ = render_system_.scene_lights();
        city_lights_saved_ = true;
    }
    std::vector<render::RenderSystem::ExtraLight> lamps;
    for (const world::SceneLight& L : doc.lights) {
        if (L.radius_m <= 0.0f) {
            continue;
        }
        render::RenderSystem::ExtraLight e;
        e.position = L.position + interior_pocket_;
        e.color = L.color;
        e.radius_m = L.radius_m;
        e.casts_shadow = L.casts_shadow;
        // ГЕЙТ ПО НЕБЕСНОЙ ВИДИМОСТИ ЗДЕСЬ ЛИШНИЙ: снаружи локации нечему
        // течь, а сам гейт погасил бы очаг в комнате без окон.
        e.interior = false;
        e.room_center_xz = L.room_center;
        e.room_half_xz = L.room_half;
        e.softness = L.softness;
        e.flicker = L.flicker;
        lamps.push_back(e);
    }
    render_system_.set_scene_lights(std::move(lamps));
    render_system_.set_transient_lights({});

    if (!city_sky_saved_) {
        city_sun_color_ = render_system_.environment().sun_color;
        city_ambient_ = render_system_.environment().ambient_color;
        city_sky_saved_ = true;
    }
    // СОЛНЦЕ В НОЛЬ. У локации нет неба, и оставленное солнце светило бы
    // сквозь кладку из направления, которого внутри не существует.
    render_system_.environment().sun_color = glm::vec3{0.0f};
    const float amb = doc.air.set && doc.air.ambient >= 0.0f ? doc.air.ambient
                                                             : 0.10f;
    render_system_.environment().ambient_color = glm::vec3{amb};
    if (doc.air.set && doc.air.fog_end_m > 0.0f) {
        render_system_.set_air_override(doc.air.fog_start_m, doc.air.fog_end_m,
                                        doc.air.cloud_cover);
    }

    loading_.stage("свет локации");
    // ТОЧКА ВОЗВРАТА — ГДЕ ИГРОК СТОИТ СЕЙЧАС. Не точка активации: игрок,
    // высаженный в дверном проёме, немедленно активирует ту же дверь снова.
    gameplay::InteriorReturn back;
    back.from_scene = state.inside() ? state.scene_path : gallery_scene_;
    if (const auto* tr = world_.get<components::Transform>(player_)) {
        back.position = tr->position;
    }
    if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
        back.yaw = ps->yaw;
    }
    state.stack.push_back(back);
    state.scene_path = scene_path;
    state.door_open = true;

    glm::vec3 spawn = interior_pocket_;
    float spawn_yaw = 0.0f;
    bool found = false;
    for (const world::SceneSpawn& S : doc.spawns) {
        if (!spawn_name.empty() && S.name != spawn_name) {
            continue;
        }
        spawn = S.position + interior_pocket_;
        spawn_yaw = S.yaw;
        found = true;
        break;
    }
    if (!found && doc.has_spawn) {
        spawn = doc.spawn + interior_pocket_;
        spawn_yaw = doc.spawn_yaw;
        found = true;
    }
    if (!found) {
        // НЕТ ТОЧКИ ВХОДА — ЭТО ДЕФЕКТ ЛОКАЦИИ, а не повод молча высадить в
        // начало координат: пол комнаты может там не быть.
        std::fprintf(stderr,
                     "[интерьер] %s: ни [spawn] «%s», ни заголовочного spawn — "
                     "игрок встал в начало кармана\n",
                     scene_path.c_str(), spawn_name.c_str());
    }
    if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
        physics_->teleport_character(ps->character, spawn);
        ps->yaw = spawn_yaw;
        ps->vertical_velocity = 0.0f;
    }
    if (auto* tr = world_.get<components::Transform>(player_)) {
        tr->position = spawn;
    }
    if (auto* pt = world_.get<components::PreviousTransform>(player_)) {
        pt->position = spawn;
    }

    spawn_scene_portals(doc, /*interior=*/true);
    interior_doc_ = std::move(doc);
    render_system_.set_world_suspended(true);
    state.visited = true;
    loading_.stage("переход и точка входа");

    loading_.finish();
    interior_enter_ms_ =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "[интерьер] ВХОД %s за %.1f ms%s\n", scene_path.c_str(),
                 interior_enter_ms_, resident ? " (геометрия уже была залита)" : "");
    hold_loading_screen();
    loading_.hide();
    render_system_.set_hud_visible(false);
    return true;
}

void App::leave_interior() {
    if (!world_.has_resource<gameplay::InteriorState>()) {
        return;
    }
    auto& state = world_.resource<gameplay::InteriorState>();
    if (!state.inside() || !world_.alive(player_) || physics_ == nullptr) {
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const gameplay::InteriorReturn back = state.stack.back();
    state.stack.pop_back();
    state.scene_path = state.inside() ? state.stack.back().from_scene
                                      : std::string{};
    state.door_open = false;

    // ГЕОМЕТРИЯ ЛОКАЦИИ ОСТАЁТСЯ ЗАЛИТОЙ. Выход — это переключение флага и
    // телепорт; свод даёт на него 0.05 с, а перезаливка слота стоит на два
    // порядка больше. Повторный вход в тот же дом по той же причине почти
    // бесплатен.
    render_system_.set_world_suspended(false);
    clear_scene_portals(/*interior=*/true);

    if (city_lights_saved_) {
        render_system_.set_scene_lights(city_lights_);
    }
    if (city_sky_saved_) {
        render_system_.environment().sun_color = city_sun_color_;
        render_system_.environment().ambient_color = city_ambient_;
        city_sky_saved_ = false;
    }
    if (scene_doc_.air.set) {
        render_system_.set_air_override(scene_doc_.air.fog_start_m,
                                        scene_doc_.air.fog_end_m,
                                        scene_doc_.air.cloud_cover);
    } else {
        render_system_.clear_air_override();
    }

    if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
        physics_->teleport_character(ps->character, back.position);
        ps->yaw = back.yaw;
        ps->vertical_velocity = 0.0f;
    }
    if (auto* tr = world_.get<components::Transform>(player_)) {
        tr->position = back.position;
    }
    if (auto* pt = world_.get<components::PreviousTransform>(player_)) {
        pt->position = back.position;
    }

    interior_leave_ms_ =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "[интерьер] ВЫХОД за %.2f ms\n", interior_leave_ms_);
}

} // namespace dfn::app
