/*
Created: 28:08:2026 - 14:30:00
Last updated: 28:08:2026 - 14:30:00
Module: engine/app
File: engine/app/sources/AppProps.cpp

Responsibility:
- ФИЗИКА ПРЕДМЕТОВ В ЖИВОЙ ИГРЕ (заказ владельца 28.08: «всем объектам в мире
  надо задать физические свойства; хочу как в Скайриме — зажав E, поднимать
  объекты, держать, складывать друг на друга»): подъём динамических тел по
  расстановкам локации, синхронизация тело->отрисовка, распознавание
  короткого/долгого E, прицел, пружина хвата, бросок и вращение.

Key items:
- App::spawn_loose_props / clear_loose_props: предметы как тела мира.
- App::sync_loose_props: тело -> матрица дро, раз в тик, после шага физики.
- App::grab_input: короткое/долгое E, взятие, ведение, выпадение, бросок.
- App::grab_aimed / grab_hand_point / grab_prompt_key: прицел и подсказка.
- App::probe_grab: беспилотная рука приёмки (DFN_GRAB_PROBE).

Dependencies:
- Uses: PropPhysics.h (класс, вещество, масса), GrabDrive.h (арифметика руки),
  ObjectRegistry (геометрия предмета), RenderSystem (слот подвижных дро),
  platform IPhysics, engine/physics (слои).
- Used by: App (тик, вход/выход из локации).

Notes:
- ДОГОВОР ПО КЛАВИШЕ E, слово в слово как он принят с волной «сидеть и лежать»
  (28.08): за PlayerState::interact_pressed остаётся смысл «КОРОТКОЕ нажатие
  случилось», распознавание короткого и долгого кладёт ЭТА зона, и ни строки
  чужого кода не меняется. Механика такова, что, пока клавиша нажата, знать,
  коротким ли будет нажатие, НЕЛЬЗЯ, — поэтому защёлка СНИМАЕТСЯ на время
  удержания и ВОЗВРАЩАЕТСЯ на отпускании, если порог не набран. Нажатие и
  отпускание внутри одного тика (быстрый щелчок при высоком кадре) до этого
  кода не доходит вовсе: клавиша уже отпущена, защёлка цела, дверь открывается
  как вчера.
- СЧЁТЧИК УДЕРЖАНИЯ ЖИВЁТ НА СЫРОМ СОСТОЯНИИ КЛАВИШИ (IInput::is_down), а не
  на защёлке. Их волна ГАСИТ защёлку в park_posture, пока человек в позе, —
  считай мы удержание по ней, у сидящего долгое нажатие съедалось бы выходом
  из позы.
- ПОКА ЧЕЛОВЕК В ПОЗЕ, ВЗЯТЬ НЕЛЬЗЯ (их же условие): руки заняты, а капсула
  запаркована в стороне от тела — поднятый сидя предмет повис бы над ней.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона big-grab владеет этим файлом.
- АРИФМЕТИКА РУКИ — ТОЛЬКО В GrabDrive.cpp. Здесь состояние и мир; вторая
  копия пружины разошлась бы с приёмкой (правило 39), которая зовёт ту же.
*/
/*
UPD:
- 28:08:2026 - 14:30:00: Создан. Пункты 2-4 волны: тела, удержание, стеки.
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/AppInternal.h"
#include "engine/app/sources/Localization.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"
#include "engine/world/sources/Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::app {

namespace {

/// Единичный вектор взгляда по позе камеры — ТА ЖЕ формула, что у дверей и у
/// мебели. Расхождение значило бы, что подсказка горит не там, куда указывает
/// перекрестье.
[[nodiscard]] glm::vec3 look_dir(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
}

/// МЕТКА ПРЕДМЕТА В user_data ТЕЛА. Старший полуслог — признак «это подвижный
/// предмет», младший — номер в списке. Метка нужна потому, что user_data по
/// уговору носит биты EntityId, и луч, нашедший тело без метки, обязан быть
/// отличим от луча, нашедшего предмет. Выбран узор НЕСУЩЕСТВУЮЩЕЙ сущности
/// (index == UINT32_MAX — это EntityId::null()), поэтому спутать нельзя даже
/// теоретически.
constexpr std::uint64_t PROP_TAG = 0xFFFFFFFFull << 32;
[[nodiscard]] std::uint64_t prop_user_data(std::size_t index) {
    return PROP_TAG | static_cast<std::uint64_t>(index);
}
[[nodiscard]] int prop_from_user_data(std::uint64_t data) {
    return (data & PROP_TAG) == PROP_TAG ? static_cast<int>(data & 0xFFFFFFFFull) : -1;
}

} // namespace

bool App::props_enabled() {
    static const bool on = [] {
        const char* e = door_value("DFN_PROPS");
        return e == nullptr || *e == '\0' || *e != '0';
    }();
    return on;
}

// ---------------------------------------------------------------------------
// ПОДЪЁМ И СНОС
// ---------------------------------------------------------------------------

bool App::loose_prop_placement(const world::Placement& placement, const glm::vec3& origin,
                               bool interior) {
    if (!props_enabled()) {
        return false; // доза 0: прежний мир, всё едет в потоки построек
    }
    if (!prop_table_read_) {
        prop_table_read_ = true;
        prop_table_ = load_prop_table("assets/objects/furniture/PHYSICS.txt");
        std::fprintf(stderr, "[предметы] реестр физики: %zu строк\n", prop_table_.size());
    }
    const auto row = prop_table_.find(placement.object);
    if (row == prop_table_.end() || row->second.cls != PropClass::Loose) {
        return false; // не названо — значит обстановка (умолчание таблицы)
    }
    LoosePlacement lp;
    lp.object = placement.object;
    lp.position = placement.position + origin;
    lp.yaw = placement.yaw;
    lp.scale = placement.scale;
    lp.interior = interior;
    loose_placements_.push_back(std::move(lp));
    return true;
}

void App::clear_loose_props() {
    if (grabbed_ >= 0) {
        grabbed_ = -1;
        grab_lag_s_ = 0.0f;
        grab_spin_ = 0.0f;
    }
    if (physics_ != nullptr) {
        for (const LoosePropLink& link : loose_props_) {
            physics_->destroy_body(link.body);
        }
    }
    loose_props_.clear();
    if (renderer_ != nullptr) {
        render_system_.clear_loose_props(*renderer_);
    }
}

/// НАКРЫТЫЙ СТОЛ ДЛЯ ПРИЁМКИ (дверь DFN_GRAB_LAB). Заведён потому, что боевые
/// карты СЕГОДНЯ не ставят в комнаты ни одного подвижного предмета: из реестра
/// в интерьерах стоит одна кровать, а вся утварь рисуется чертежами .dfh по
/// месту. Приёмка хвата без этой двери мерила бы пустую комнату — то есть не
/// мерила бы ничего.
///
/// ЭТО НЕ КАРТА И НЕ ФАЙЛ СЦЕНЫ: пробные карты засоряют меню (правило дерева),
/// а дверь живёт ровно один прогон. Уйдёт в день, когда расстановка убранства
/// положит утварь сама.
void App::add_lab_props() {
    const char* lab = door_value("DFN_GRAB_LAB");
    if (lab == nullptr || *lab == '\0' || *lab == '0' || physics_ == nullptr) {
        return;
    }
    // ОПОРУ ИЩЕМ ПО СЦЕНЕ, А ВЫСОТУ — ЛУЧОМ. Столешницу можно было бы взять
    // числом из перечня, но стол в комнате стоит чертежом .dfh, у которого своя
    // высота, и число из перечня утопило бы посуду в столе или подвесило над
    // ним. Луч сверху вниз отвечает про ТУ поверхность, которая там есть.
    glm::vec3 spot = interior_pocket_;
    bool found = false;
    for (const world::ScenePlacedHouse& h : interior_doc_.houses) {
        if (h.file.find("furn-table") != std::string::npos) {
            spot = h.position + interior_pocket_;
            found = true;
            break;
        }
    }
    if (!found) {
        // Стола в комнате нет — ставим на пол перед точкой входа.
        spot = interior_doc_.spawn + interior_pocket_
             + glm::vec3{0.0f, 0.0f, -1.0f};
    }
    // ГДЕ У ЭТОГО СТОЛА СТОЛЕШНИЦА — СПРАШИВАЕТСЯ ЛУЧАМИ, А НЕ ЧИСЛОМ.
    // Начало координат чертежа стоит в УГЛУ его пятна, и луч, пущенный ровно
    // оттуда, проходит мимо столешницы и находит пол — что и случилось на
    // первом прогоне (нашёл «верх» в трёх сантиметрах над полом). Поэтому
    // пятно вокруг начала обходится сеткой, и берётся САМАЯ ВЫСОКАЯ
    // поверхность, лежащая на высоте стола (0.5..1.1 м над полом).
    const float floor_y = spot.y;
    float top = floor_y;
    glm::vec2 best_xz{spot.x, spot.z};
    // СЕРЕДИНА СТОЛЕШНИЦЫ — СРЕДНЕЕ ПО ВСЕМ ЕЁ ТОЧКАМ, а не первая найденная.
    // Начало координат чертежа стоит в УГЛУ пятна, и «первая точка на высоте
    // стола» — это угол столешницы: расставленная от него посуда наполовину
    // висит за краем. Замер 28.08: семь предметов из восьми оказались на полу,
    // потому что их собственный луч опоры нашёл не стол, а пол под ним.
    glm::vec2 sum{0.0f};
    int hits = 0;
    for (int gx = 0; gx <= 12; ++gx) {
        for (int gz = 0; gz <= 12; ++gz) {
            const float x = spot.x - 0.6f + static_cast<float>(gx) * 0.20f;
            const float z = spot.z - 0.6f + static_cast<float>(gz) * 0.20f;
            const platform::RayHit hit =
                physics_->raycast({x, floor_y + 2.0f, z}, {0.0f, -1.0f, 0.0f}, 4.0f,
                                  physics::LAYER_STATIC);
            if (!hit.hit) {
                continue;
            }
            const float h = hit.position.y - floor_y;
            if (h > 0.5f && h < 1.1f) {
                if (hit.position.y > top) {
                    // Нашли поверхность ВЫШЕ прежней — прежние точки были от
                    // другой мебели; счёт начинается заново.
                    top = hit.position.y;
                    sum = glm::vec2{0.0f};
                    hits = 0;
                }
                if (std::abs(hit.position.y - top) < 0.02f) {
                    sum += glm::vec2{x, z};
                    ++hits;
                }
            }
        }
    }
    if (hits > 0) {
        best_xz = sum / static_cast<float>(hits);
    }
    const bool on_table = hits > 0;
    grab_lab_on_table_ = on_table;
    if (!on_table) {
        // Столешницы не нашлось — ставим на пол у точки входа. Приёмка хвата
        // от этого не рушится: она мерит хват, а не мебель.
        const platform::RayHit floor_hit =
            physics_->raycast(spot + glm::vec3{0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 4.0f,
                              physics::LAYER_STATIC);
        top = floor_hit.hit ? floor_hit.position.y : floor_y;
    } else {
        spot.x = best_xz.x;
        spot.z = best_xz.y;
    }
    top += 0.02f;
    grab_lab_table_ = glm::vec3{best_xz.x, top, best_xz.y};
    std::fprintf(stderr,
                 "[накрытый стол] опора (%.2f %.2f %.2f), верх %.3f — %s (%.2f м над полом)\n",
                 static_cast<double>(spot.x), static_cast<double>(spot.y),
                 static_cast<double>(spot.z), static_cast<double>(top),
                 on_table ? "СТОЛЕШНИЦА" : "пол",
                 static_cast<double>(top - floor_y));
    // КАЖДЫЙ ПРЕДМЕТ САДИТСЯ НА ТО, ЧТО ПОД НИМ, И СПРАШИВАЕТ ОБ ЭТОМ СВОИМ
    // ЛУЧОМ. Общая высота столешницы на всю восьмёрку выглядела красиво ровно
    // до первого кадра: миски, которым не хватило столешницы, повисли в
    // воздухе над полом — и НЕ УПАЛИ, потому что тела рождаются спящими
    // (замер 28.08). Спящий предмет, поставленный в воздух, остаётся в
    // воздухе, и это верное поведение тела при неверной расстановке.
    const auto put = [&](const char* name, float dx, float dz, float dy) {
        const float x = spot.x + dx;
        const float z = spot.z + dz;
        const platform::RayHit under =
            physics_->raycast({x, top + 0.5f, z}, {0.0f, -1.0f, 0.0f}, 4.0f,
                              physics::LAYER_STATIC);
        LoosePlacement lp;
        lp.object = name;
        lp.position = glm::vec3{x, (under.hit ? under.position.y + 0.01f : top) + dy, z};
        lp.yaw = 0.0f;
        lp.scale = 1.0f;
        lp.interior = true;
        loose_placements_.push_back(std::move(lp));
    };
    // На столе: кувшин, два кубка, книги. Рядом на полу — три миски столбиком
    // (приёмка стека) и бутыль.
    put("furn-jug", 0.18f, 0.10f, 0.0f);
    put("furn-cup", -0.06f, 0.14f, 0.0f);
    put("furn-cup", -0.16f, 0.02f, 0.0f);
    put("furn-books", 0.04f, -0.12f, 0.0f);
    // ПИРАМИДА ИЗ ТРЁХ МИСОК — приёмка стека: миска 0.08 высотой, шаг 0.085
    // оставляет полсантиметра зазора, который физика выбирает за первый же
    // тик после пробуждения и дальше держит трением.
    put("furn-bowl", -0.30f, 0.22f, 0.000f);
    put("furn-bowl", -0.30f, 0.22f, 0.085f);
    put("furn-bowl", -0.30f, 0.22f, 0.170f);
    put("furn-bottle", 0.30f, -0.18f, 0.0f);
    // НА ПОЛУ, В РЯД — для приёмки «прошёл сквозь накрытый стол» (владелец
    // 28.08: «моё тело тоже имеет физические свойства»). На столе телом их не
    // тронешь: капсула ростом 1.8 м упирается в саму столешницу, а не в
    // посуду на ней, и замер мерил бы стол.
    put("furn-cup", 1.10f, 0.35f, 0.0f);
    put("furn-cup", 1.10f, 0.00f, 0.0f);
    put("furn-bottle", 1.10f, -0.35f, 0.0f);

    // СКОЛЬКО ЕЩЁ ПОСТАВИТЬ — ЧИСЛОМ В ДВЕРИ (DFN_GRAB_LAB=40). Бюджет кадра
    // спрашивают не про «есть ли предметы», а про «сколько их держит комната»,
    // и ответ на него — кривая, а не одна точка (правило 30: у порога должны
    // быть обе стороны). Сетка ставится на пол вокруг стола, по 0.35 м.
    const int extra = std::atoi(lab);
    if (extra > 1) {
        static const char* kinds[] = {"furn-bowl", "furn-cup", "furn-bottle", "furn-books"};
        for (int i = 0; i < extra; ++i) {
            const int gx = i % 8;
            const int gz = i / 8;
            put(kinds[static_cast<std::size_t>(i) % 4],
                -1.6f + static_cast<float>(gx) * 0.35f,
                -1.6f + static_cast<float>(gz) * 0.35f, 0.0f);
        }
    }
}

void App::spawn_loose_props() {
    clear_loose_props();
    if (!props_enabled() || physics_ == nullptr || renderer_ == nullptr) {
        return;
    }
    add_lab_props();
    if (!prop_table_read_) {
        prop_table_read_ = true;
        prop_table_ = load_prop_table("assets/objects/furniture/PHYSICS.txt");
        std::fprintf(stderr, "[предметы] реестр физики: %zu строк\n", prop_table_.size());
    }
    if (loose_placements_.empty() || prop_table_.empty()) {
        return;
    }

    // ГЕОМЕТРИЯ СЧИТАЕТСЯ ОДИН РАЗ НА ИМЯ, а не на экземпляр: в комнате стоит
    // до шести мисок одного чертежа, и строить оболочку шесть раз значило бы
    // заплатить за одно и то же шесть раз (тот же довод, что у точек позы).
    struct Recipe {
        std::vector<glm::vec3> hull;   ///< точки оболочки, местные, с масштабом
        float mass_kg = 0.0f;
        core::SubstanceId substance = core::SUBSTANCE_DEFAULT;
        std::vector<render::RenderSystem::HouseStream> streams;
        bool valid = false;
    };
    std::map<std::string, Recipe> recipes;

    std::vector<render::RenderSystem::LooseProp> draws;
    draws.reserve(loose_placements_.size());
    std::size_t bodies = 0;
    for (const LoosePlacement& place : loose_placements_) {
        // Ключ рецепта — имя И масштаб: масштаб входит и в оболочку, и в массу.
        char key[192];
        std::snprintf(key, sizeof(key), "%s@%.3f", place.object.c_str(),
                      static_cast<double>(place.scale));
        auto it = recipes.find(key);
        if (it == recipes.end()) {
            Recipe recipe;
            // ПРЕДМЕТ ЧИТАЕТСЯ С ПОЛКИ, ЕСЛИ ЕГО ТАМ ЕЩЁ НЕ БРАЛИ. Заливка
            // кладёт в scene_objects_ только то, что названо сценой, — а
            // накрытый стол приёмки приносит имена, которых в сцене нет.
            auto obj = scene_objects_.find(place.object);
            if (obj == scene_objects_.end()) {
                std::optional<render::RegistryObject> read;
                for (const std::string& shelf : gallery_shelves_) {
                    read = render::read_object(std::filesystem::path(shelf)
                                               / (place.object + ".dfo"));
                    if (read) {
                        break;
                    }
                }
                if (read) {
                    obj = scene_objects_.emplace(place.object, std::move(*read)).first;
                }
            }
            const auto row = prop_table_.find(place.object);
            if (obj != scene_objects_.end() && row != prop_table_.end()
                && !obj->second.house.empty()) {
                std::vector<glm::vec3> positions;
                std::vector<std::uint32_t> indices;
                for (const render::HouseSubmesh& sub : obj->second.house) {
                    const auto base = static_cast<std::uint32_t>(positions.size());
                    render::RenderSystem::HouseStream stream;
                    stream.surface = sub.surface;
                    stream.tone = sub.tone;
                    stream.emissive = sub.emissive;
                    stream.mesh.vertices.reserve(sub.mesh.vertices.size());
                    for (const platform::Vertex& v : sub.mesh.vertices) {
                        platform::Vertex scaled = v;
                        // МАСШТАБ ЗАПЕКАЕТСЯ В ВЕРШИНЫ, а не в матрицу: матрицу
                        // приносит физика, и вписать в неё масштаб значило бы
                        // каждый кадр восстанавливать его из тела, которое о
                        // нём не знает.
                        scaled.position = v.position * place.scale;
                        stream.mesh.vertices.push_back(scaled);
                        positions.push_back(scaled.position);
                    }
                    stream.mesh.indices.assign(sub.mesh.indices.begin(),
                                               sub.mesh.indices.end());
                    for (const std::uint32_t i : sub.mesh.indices) {
                        indices.push_back(base + i);
                    }
                    recipe.streams.push_back(std::move(stream));
                }
                const MeshBulk bulk = measure_bulk(positions, indices);
                const PropMass mass = prop_mass(bulk, row->second, place.object);
                recipe.mass_kg = mass.mass_kg;
                const core::SubstanceId sid = core::find_substance(row->second.substance);
                recipe.substance = sid == core::SUBSTANCE_NONE ? core::SUBSTANCE_DEFAULT : sid;
                recipe.hull = hull_points(positions, PROP_HULL_MAX_POINTS);
                recipe.valid = recipe.hull.size() >= 4 && !recipe.streams.empty();
            }
            it = recipes.emplace(key, std::move(recipe)).first;
        }
        const Recipe& recipe = it->second;
        if (!recipe.valid) {
            continue;
        }

        platform::DynamicBodyDesc desc;
        desc.points = recipe.hull;
        desc.position = place.position;
        desc.rotation = glm::angleAxis(place.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
        desc.mass_kg = recipe.mass_kg;
        desc.substance = recipe.substance;
        desc.layer = physics::LAYER_LOOSE;
        desc.user_data = prop_user_data(loose_props_.size());
        // РОЖДАЮТСЯ СПЯЩИМИ: комната из тридцати предметов, в которой никто
        // ничего не трогал, не должна платить ни одного тика симуляции — и это
        // же лечит классический отказ Скайрима «предметы разлетаются при входе
        // в помещение» (записка №4 ресёрчера, Б4.2).
        desc.start_asleep = true;
        const platform::PhysicsBodyHandle body = physics_->create_dynamic_body(desc);
        if (!body.valid()) {
            std::fprintf(stderr, "[предметы] %s: тело НЕ создано (масса %.2f, точек %zu)\n",
                         place.object.c_str(), static_cast<double>(recipe.mass_kg),
                         recipe.hull.size());
            continue;
        }
        LoosePropLink link;
        link.object = place.object;
        link.body = body;
        link.mass_kg = recipe.mass_kg;
        link.interior = place.interior;
        link.render_index = draws.size();
        loose_props_.push_back(std::move(link));
        render::RenderSystem::LooseProp draw;
        draw.streams = recipe.streams;
        draw.interior = place.interior;
        draws.push_back(std::move(draw));
        ++bodies;
    }
    render_system_.set_loose_props(*renderer_, std::move(draws));
    sync_loose_props(/*force=*/true);
    std::fprintf(stderr, "[предметы] подвижных тел: %zu (расстановок %zu, рецептов %zu)\n",
                 bodies, loose_placements_.size(), recipes.size());
}

// ---------------------------------------------------------------------------
// ТЕЛО -> ОТРИСОВКА
// ---------------------------------------------------------------------------

void App::sync_loose_props(bool force) {
    if (loose_props_.empty() || physics_ == nullptr) {
        return;
    }
    for (const LoosePropLink& link : loose_props_) {
        // СПЯЩЕЕ ТЕЛО НЕ СПРАШИВАЕМ: его поза не менялась с прошлого раза, а
        // тридцать спящих предметов комнаты — это тридцать бесполезных
        // запросов в кадр. Первый же толчок будит тело, и оно возвращается в
        // этот цикл само.
        //
        // ...НО ПЕРВЫЙ РАЗ СПРАШИВАЮТСЯ ВСЕ, И ЭТО НЕ МЕЛОЧЬ. Тело рождается
        // СПЯЩИМ, то есть при обычном правиле его матрица так и осталась бы
        // единичной — а единичная матрица ставит предмет в начало координат
        // мира. Замер 28.08: восемь предметов накрытого стола лежали по своим
        // местам в физике и рисовались все разом в нуле, за километр от
        // комнаты; в кадре это выглядело как «предметов нет вовсе».
        if (!force && physics_->body_asleep(link.body)) {
            continue;
        }
        const platform::BodyPose pose = physics_->body_pose(link.body);
        render_system_.set_loose_prop_transform(
            link.render_index,
            glm::translate(glm::mat4(1.0f), pose.position) * glm::mat4_cast(pose.rotation));
    }
}

// ---------------------------------------------------------------------------
// ПРИЦЕЛ
// ---------------------------------------------------------------------------

int App::grab_aimed() const {
    if (loose_props_.empty() || physics_ == nullptr) {
        return -1;
    }
    const auto* cam = world_.get<components::CameraPose>(player_);
    if (cam == nullptr) {
        return -1;
    }
    const glm::vec3 eye = cam->position;
    const glm::vec3 dir = look_dir(cam->yaw, cam->pitch);
    const platform::RayHit hit =
        physics_->raycast(eye, dir, grab_tuning_.reach_m, physics::LAYER_LOOSE);
    if (!hit.hit) {
        return -1;
    }
    // СТЕНА МЕЖДУ ГЛАЗОМ И ПРЕДМЕТОМ ОТМЕНЯЕТ ПРИЦЕЛ. Луч по слою предметов не
    // видит стен вовсе — он для того и разделён по слоям, — и без этой второй
    // проверки кувшин на столе соседней комнаты «брался» бы сквозь перегородку.
    const platform::RayHit wall =
        physics_->raycast(eye, dir, hit.distance, physics::LAYER_STATIC);
    if (wall.hit && wall.distance < hit.distance - 0.02f) {
        return -1;
    }
    const int index = prop_from_user_data(hit.user_data);
    return index >= 0 && index < static_cast<int>(loose_props_.size()) ? index : -1;
}

glm::vec3 App::grab_hand_point() const {
    const auto* cam = world_.get<components::CameraPose>(player_);
    if (cam == nullptr) {
        return glm::vec3{0.0f};
    }
    const glm::vec3 dir = look_dir(cam->yaw, cam->pitch);
    return cam->position + dir * grab_tuning_.carry_m
         - glm::vec3{0.0f, grab_tuning_.carry_drop_m, 0.0f};
}

std::uint64_t App::grab_prompt_key() const {
    if (grabbed_ >= 0) {
        return serialization::fnv1a64("prompt.carry");
    }
    return grab_aimed() >= 0 ? serialization::fnv1a64("prompt.take") : 0;
}

// ---------------------------------------------------------------------------
// ВЗЯТЬ, ВЕСТИ, ОТПУСТИТЬ
// ---------------------------------------------------------------------------

void App::grab_take(int index) {
    if (index < 0 || index >= static_cast<int>(loose_props_.size()) || physics_ == nullptr) {
        return;
    }
    grabbed_ = index;
    grab_lag_s_ = 0.0f;
    grab_spin_ = 0.0f;
    const LoosePropLink& link = loose_props_[static_cast<std::size_t>(index)];
    // ВЕС СНИМАЕТСЯ НА ВРЕМЯ НОШЕНИЯ, а сила — нет. Пружина, которой пришлось
    // бы ещё и держать вес, провисала бы тем сильнее, чем тяжелее предмет, —
    // то есть вес выражался бы дважды и оба раза криво. Он и так выражен
    // потолком силы: тяжёлое тащится, лёгкое летит.
    physics_->set_body_gravity_factor(link.body, 0.0f);
    physics_->activate_body(link.body);
    std::fprintf(stderr, "[хват] взял %s (%.2f кг)\n", link.object.c_str(),
                 static_cast<double>(link.mass_kg));
}

void App::grab_release(bool thrown) {
    if (grabbed_ < 0 || physics_ == nullptr) {
        grabbed_ = -1;
        return;
    }
    const LoosePropLink& link = loose_props_[static_cast<std::size_t>(grabbed_)];
    physics_->set_body_gravity_factor(link.body, 1.0f);
    if (thrown) {
        const auto* cam = world_.get<components::CameraPose>(player_);
        const glm::vec3 dir =
            cam != nullptr ? look_dir(cam->yaw, cam->pitch) : glm::vec3{0.0f, 0.0f, -1.0f};
        const float speed = throw_speed(link.mass_kg, grab_tuning_);
        physics_->set_body_velocity(link.body, dir * speed, glm::vec3{0.0f});
        std::fprintf(stderr, "[хват] бросил %s: %.2f м/с\n", link.object.c_str(),
                     static_cast<double>(speed));
    } else {
        // ОТПУЩЕННОЕ НЕ ЗАМИРАЕТ В ВОЗДУХЕ и не улетает: скорость гасится, но
        // тело остаётся разбуженным, чтобы оно упало на то, что под ним.
        physics_->set_body_velocity(link.body, glm::vec3{0.0f}, glm::vec3{0.0f});
        std::fprintf(stderr, "[хват] отпустил %s\n", link.object.c_str());
    }
    physics_->activate_body(link.body);
    grabbed_ = -1;
    grab_lag_s_ = 0.0f;
}

void App::grab_input(float dt) {
    if (!props_enabled() || physics_ == nullptr || input_ == nullptr) {
        return;
    }
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    if (ps == nullptr) {
        return;
    }
    probe_grab();

    // --- ДОГОВОР ПО E: короткое остаётся чужим, долгое становится нашим ---
    // Сырое состояние клавиши ПЛЮС рука прибора: у беспилотного прогона нет
    // пальцев, а мерить он обязан ту же механику, что и человек.
    const bool down = input_->is_down(platform::Key::E) || grab_probe_key_;
    if (down && ps->interact_pressed) {
        // Клавиша ЕЩЁ нажата, а защёлка уже взведена: значит нажатие только
        // началось и коротким оно ещё не является. Берём защёлку на хранение.
        ps->interact_pressed = false;
        grab_short_held_ = true;
    }
    const GrabPress press = grab_press(grab_hold_, down, dt, grab_tuning_);
    if (!down && grab_short_held_) {
        // Отпустили. Если порога не набрали — нажатие было КОРОТКИМ, и защёлка
        // возвращается ровно с тем смыслом, который за ней остался.
        if (!grab_hold_.consumed) {
            ps->interact_pressed = true;
        }
        grab_short_held_ = false;
    }

    // --- ВЗЯТЬ / ОТПУСТИТЬ ---
    if (grabbed_ < 0) {
        // ПОКА ЧЕЛОВЕК В ПОЗЕ, ВЗЯТЬ НЕЛЬЗЯ (условие волны «сидеть и лежать»):
        // руки заняты, а капсула запаркована в стороне от тела.
        if (press == GrabPress::Long && !in_posture()) {
            grab_take(grab_aimed());
        }
        return;
    }

    // БРОСОК ПРОВЕРЯЕТСЯ РАНЬШЕ ОТПУСКАНИЯ, и порядок здесь имеет значение:
    // человек, бросая, отпускает E и жмёт ЛКМ почти одновременно, и в тик, где
    // случилось и то и другое, «отпустил на месте» съело бы бросок. Первый
    // прогон прибора поймал это на себе: рука жала ЛКМ ровно в тот тик, в
    // котором отпускала клавишу, и в отчёте вместо броска стояло «отпустил».
    if (input_->was_pressed(platform::MouseButton::LEFT) || grab_probe_click_) {
        grab_release(true);
        return;
    }
    if (!down) {
        // Отпускание клавиши — отпустить на месте.
        grab_release(false);
        return;
    }

    // --- ВРАЩЕНИЕ КОЛЕСОМ ---
    const float wheel = input_->scroll_delta().y;
    if (std::abs(wheel) > 0.01f) {
        grab_spin_ += wheel * grab_tuning_.spin_rate * dt * 10.0f;
    }

    // --- ПРУЖИНА ---
    const LoosePropLink& link = loose_props_[static_cast<std::size_t>(grabbed_)];
    const platform::BodyPose pose = physics_->body_pose(link.body);
    const glm::vec3 hand = grab_hand_point();
    const float distance = glm::length(hand - pose.position);
    if (grab_slipped(distance, dt, grab_lag_s_, grab_tuning_)) {
        // ЗАКЛИНИЛО — РУКА СДАЁТСЯ. Без этого игрок либо таскает мир за собой,
        // либо предмет уходит сквозь стену (записка №4, Б4.4).
        std::fprintf(stderr, "[хват] выпал: отстал на %.2f м\n",
                     static_cast<double>(distance));
        grab_release(false);
        return;
    }
    const glm::vec3 velocity = grab_velocity(pose.position, physics_->body_velocity(link.body),
                                             hand, link.mass_kg, dt, grab_tuning_);
    // УГЛОВАЯ СКОРОСТЬ — ТОЛЬКО ТА, ЧТО ЗАДАЛ ЧЕЛОВЕК КОЛЕСОМ. Пружина по
    // повороту здесь не заведена намеренно: предмет, который сам
    // разворачивается «как надо», перестаёт быть предметом и становится
    // иконкой в руке, а заказ прямо говорит «как в Скайриме».
    physics_->set_body_velocity(link.body, velocity,
                                glm::vec3{0.0f, grab_spin_, 0.0f});
    grab_spin_ *= 0.85f; // доворот затухает, иначе предмет крутится вечно
}

// ---------------------------------------------------------------------------
// ПРИБОР
// ---------------------------------------------------------------------------

void App::probe_grab() {
    // БЕСПИЛОТНАЯ РУКА НА КЛАВИШЕ, а не прямой вызов grab_take(): прямой вызов
    // мерил бы половину механики — мимо прицела, мимо порога удержания, мимо
    // подсказки. Ровно та ошибка, за которую заплатил беспилотный замер выхода
    // из дома 27.08. Поэтому рука ЖМЁТ КЛАВИШУ (grab_probe_key_ подмешивается
    // к сырому состоянию там же, где читается настоящая), а не зовёт механику.
    static const char* recipe = door_value("DFN_GRAB_PROBE");
    if (recipe == nullptr || *recipe == '\0') {
        return;
    }
    if (loose_props_.empty()) {
        return;
    }
    ++grab_probe_seen_;
    const std::uint64_t f = grab_probe_seen_;

    // РАСПИСАНИЕ РУКИ, кадрами. Порог удержания — 0.25 с, то есть 15 кадров
    // при 60: между «нажал» и «взял» обязано пройти больше, иначе прибор
    // измерил бы не механику, а своё расписание.
    constexpr std::uint64_t LOOK1 = 30;    // встать (прицел — через 10 кадров)
    constexpr std::uint64_t PRESS1 = 48;   // нажать E (после прицела)
    constexpr std::uint64_t SHOT_TAKE = 75;// кадр «взял»
    constexpr std::uint64_t CARRY = 150;   // кадр «несу»
    constexpr std::uint64_t PUT = 240;     // отпустить (поставить)
    constexpr std::uint64_t SHOT_PUT = 300;// кадр «поставил»
    constexpr std::uint64_t LOOK2 = 320;   // снова встать и прицелиться
    constexpr std::uint64_t PRESS2 = 338;  // нажать E второй раз
    constexpr std::uint64_t THROW = 380;   // ЛКМ — бросок
    constexpr std::uint64_t SHOT_THROW = 440;

    const auto shot = [&](const char* name) {
        if (renderer_ == nullptr) {
            return;
        }
        std::filesystem::create_directories("docs/acceptance");
        (void)renderer_->save_screenshot(std::string("docs/acceptance/") + name + ".png");
    };
    // КОГО БЕРЁМ. Имя приходит дозой (DFN_GRAB_PROBE=furn-jug): «ближайший»
    // на накрытом столе — это то, что стоит с краю, и прогон от прогона он
    // разный. Приёмка, у которой подопытный меняется сам собой, ничего не
    // измеряет дважды.
    const std::string wanted = (recipe[0] == '1' && recipe[1] == '\0') ? std::string{}
                                                                       : std::string{recipe};
    const auto nearest = [&]() -> int {
        if (!wanted.empty()) {
            for (std::size_t i = 0; i < loose_props_.size(); ++i) {
                if (loose_props_[i].object == wanted) {
                    return static_cast<int>(i);
                }
            }
        }
        const auto* cam = world_.get<components::CameraPose>(player_);
        if (cam == nullptr) {
            return -1;
        }
        int best = -1;
        float best_d = 1e9f;
        for (std::size_t i = 0; i < loose_props_.size(); ++i) {
            const float d = glm::length(
                physics_->body_pose(loose_props_[i].body).position - cam->position);
            if (d < best_d) {
                best_d = d;
                best = static_cast<int>(i);
            }
        }
        return best;
    };
    // ШАГ ПЕРВЫЙ: ВСТАТЬ. Ноги ставятся НА ПОЛ (луч вниз), а не на высоту
    // предмета: предмет стоит на столе, и «встать на его уровне» означало бы
    // висеть в воздухе — а капсула, отпущенная в воздухе, падает и уезжает.
    const auto probe_stand = [&](int index) {
        auto* ps = world_.get<gameplay::PlayerState>(player_);
        const auto* cam = world_.get<components::CameraPose>(player_);
        if (ps == nullptr || cam == nullptr || index < 0) {
            return;
        }
        const glm::vec3 at =
            physics_->body_pose(loose_props_[static_cast<std::size_t>(index)].body).position;
        const glm::vec3 flat{at.x - cam->position.x, 0.0f, at.z - cam->position.z};
        const glm::vec3 dir = glm::length(flat) > 1e-3f ? glm::normalize(flat)
                                                        : glm::vec3{0.0f, 0.0f, -1.0f};
        const glm::vec3 stand = at - dir * 0.90f;
        const platform::RayHit floor_hit =
            physics_->raycast({stand.x, at.y + 1.0f, stand.z}, {0.0f, -1.0f, 0.0f}, 4.0f,
                              physics::LAYER_STATIC);
        const float feet = floor_hit.hit ? floor_hit.position.y + 0.02f : at.y;
        // СТОЙКА ЗАПОМИНАЕТСЯ: во второй заход предмет уже не там, где стоял,
        // и вычисленная от него новая стойка легко оказывается по ту сторону
        // столешницы — луч упирается в её кромку, и «взять» не случается
        // (замер 28.08: прицел -1 при честных 1.23 м). Место, с которого
        // получилось однажды, — уже проверенное место.
        if (grab_probe_stand_.x == 0.0f && grab_probe_stand_.z == 0.0f) {
            grab_probe_stand_ = glm::vec3{stand.x, feet, stand.z};
        }
        physics_->teleport_character(ps->character, glm::vec3{stand.x, feet, stand.z});
        std::fprintf(stderr,
                     "[прибор хвата] кадр %llu: встал у %s, ноги на %.3f, предмет на %.3f\n",
                     static_cast<unsigned long long>(f),
                     loose_props_[static_cast<std::size_t>(index)].object.c_str(),
                     static_cast<double>(feet), static_cast<double>(at.y));
    };
    // ШАГ ВТОРОЙ: ПРИЦЕЛИТЬСЯ, И ТОЛЬКО ПОСЛЕ ТОГО, КАК КАПСУЛА УЖЕ СТОИТ.
    // Рыск и тангаж считаются от НАСТОЯЩЕГО глаза этого тика: угол, посчитанный
    // до телепорта, смотрит из точки, в которой человека уже нет, — и первый
    // прогон этого прибора промахнулся мимо кувшина ровно так (тангаж -0.55
    // при нужных -1.03).
    const auto probe_aim = [&](int index) {
        auto* ps = world_.get<gameplay::PlayerState>(player_);
        const auto* cam = world_.get<components::CameraPose>(player_);
        if (ps == nullptr || cam == nullptr || index < 0) {
            return;
        }
        const glm::vec3 at =
            physics_->body_pose(loose_props_[static_cast<std::size_t>(index)].body).position;
        const glm::vec3 to = at - cam->position;
        const float flat_len = std::sqrt(to.x * to.x + to.z * to.z);
        ps->yaw = std::atan2(to.x, -to.z);
        ps->pitch = std::atan2(to.y, std::max(flat_len, 1e-3f));
        if (f % 20 == 0) {
            std::fprintf(stderr,
                         "[прибор хвата] кадр %llu: прицел рыск %.3f тангаж %.3f, до "
                         "предмета %.2f м, луч нашёл %d\n",
                         static_cast<unsigned long long>(f), static_cast<double>(ps->yaw),
                         static_cast<double>(ps->pitch),
                         static_cast<double>(glm::length(to)), grab_aimed());
        }
    };

    // --- ОТДЕЛЬНЫЙ РЕЦЕПТ: ПРОЙТИ СКВОЗЬ НАКРЫТЫЙ СТОЛ (DFN_GRAB_PROBE=push).
    // Отдельный потому, что стойка у него другая: здесь человек не целится, а
    // ИДЁТ, и мерится не хват, а толчок телом.
    if (wanted == "push") {
        static std::vector<glm::vec3> before;
        auto* ps = world_.get<gameplay::PlayerState>(player_);
        if (ps == nullptr) {
            return;
        }
        // Предметы на полу — те, что стоят ниже полуметра над своим полом.
        const auto on_floor = [&]() {
            std::vector<std::size_t> out;
            for (std::size_t i = 0; i < loose_props_.size(); ++i) {
                if (physics_->body_pose(loose_props_[i].body).position.y
                    < grab_lab_table_.y - 0.30f) {
                    out.push_back(i);
                }
            }
            return out;
        };
        if (f == 30) {
            const auto items = on_floor();
            if (items.empty()) {
                std::fprintf(stderr, "[прибор толчка] на полу нет предметов — нечего мерить\n");
                return;
            }
            glm::vec3 mid{0.0f};
            for (const std::size_t i : items) {
                const glm::vec3 at = physics_->body_pose(loose_props_[i].body).position;
                mid += at;
                before.push_back(at);
            }
            mid /= static_cast<float>(items.size());
            // Встать в двух метрах и лицом на ряд: идти будем прямо на него.
            const glm::vec3 stand = mid + glm::vec3{0.0f, 0.0f, 2.0f};
            const platform::RayHit floor_hit =
                physics_->raycast(stand + glm::vec3{0.0f, 1.5f, 0.0f}, {0.0f, -1.0f, 0.0f},
                                  4.0f, physics::LAYER_STATIC);
            physics_->teleport_character(
                ps->character,
                glm::vec3{stand.x, floor_hit.hit ? floor_hit.position.y + 0.02f : mid.y,
                          stand.z});
            ps->yaw = std::atan2(mid.x - stand.x, -(mid.z - stand.z));
            ps->pitch = -0.35f;
            std::fprintf(stderr, "[прибор толчка] встал в 2 м от ряда из %zu предметов\n",
                         items.size());
        }
        if (f == 55) {
            ps->pitch = -0.45f; // взгляд опущен на ряд, а не в стену за ним
            shot("grab-5-push-before");
        }
        // ИДЁТ СКВОЗЬ РЯД И ОСТАНАВЛИВАЕТСЯ. Длина прогулки — часть замера, и
        // это надо знать, читая число: пока человек идёт, он ГОНИТ лёгкое
        // перед собой, и «сдвинулся на 5.7 м» при трёхсекундной ходьбе мерит
        // прогулку, а не толчок. 0.75 с — ровно чтобы пройти ряд насквозь.
        // Что потолок силы при этом РАБОТАЕТ, доказывает не эта рука, а
        // рукав: бутыль 1.159 м против бочки 200 кг 0.000 м на одном проходе
        // (tests/sim/LoosePropTests.cpp).
        if (f > 60 && f < 105) {
            ps->move_axes = glm::vec2{0.0f, 1.0f}; // идти вперёд
        }
        // ОБЕРНУТЬСЯ И ПОСМОТРЕТЬ, ЧТО ОСТАЛОСЬ. Кадр «после», снятый по ходу
        // движения, показывает стену: посуда осталась ПОЗАДИ. Кадр обязан
        // показывать предмет замера, иначе он не доказательство (правило 27).
        if (f == 180) {
            const auto items = on_floor();
            const auto* cam = world_.get<components::CameraPose>(player_);
            if (!items.empty() && cam != nullptr) {
                glm::vec3 mid{0.0f};
                for (const std::size_t i : items) {
                    mid += physics_->body_pose(loose_props_[i].body).position;
                }
                mid /= static_cast<float>(items.size());
                const glm::vec3 to = mid - cam->position;
                const float flat = std::sqrt(to.x * to.x + to.z * to.z);
                ps->yaw = std::atan2(to.x, -to.z);
                ps->pitch = std::atan2(to.y, std::max(flat, 1e-3f));
            }
        }
        if (f == 200) {
            const auto items = on_floor();
            float worst = 0.0f;
            for (std::size_t k = 0; k < items.size() && k < before.size(); ++k) {
                const glm::vec3 at = physics_->body_pose(loose_props_[items[k]].body).position;
                const float d = glm::length(at - before[k]);
                worst = std::max(worst, d);
                std::fprintf(stderr, "[прибор толчка] %s сдвинулся на %.3f м\n",
                             loose_props_[items[k]].object.c_str(), static_cast<double>(d));
            }
            std::fprintf(stderr, "[прибор толчка] наибольшее смещение %.3f м\n",
                         static_cast<double>(worst));
            shot("grab-6-push-after");
        }
        return;
    }

    if (f == LOOK1) {
        probe_stand(nearest());
    }
    if (f == LOOK2) {
        auto* ps = world_.get<gameplay::PlayerState>(player_);
        if (ps != nullptr) {
            physics_->teleport_character(ps->character, grab_probe_stand_);
            std::fprintf(stderr, "[прибор хвата] кадр %llu: вернулся на проверенную стойку\n",
                         static_cast<unsigned long long>(f));
        }
    }
    // ПРИЦЕЛ ДЕРЖИТСЯ, А НЕ СТАВИТСЯ ОДНАЖДЫ. Капсула, поставленная у стола,
    // ещё несколько тиков выталкивается из него телом стола, и угол,
    // посчитанный в момент постановки, к моменту нажатия смотрит мимо: замер
    // 28.08 показал промах при верном расстоянии 1.30 м. Человек в этой
    // ситуации просто продолжает смотреть на предмет — рука делает то же.
    if (grabbed_ < 0 && ((f > LOOK1 + 5 && f < PUT) || (f > LOOK2 + 5 && f < THROW))) {
        probe_aim(nearest());
    }
    // ПЕРЕНЕСТИ И ПОСТАВИТЬ НА СТОЛ. Пока предмет в руках, рука ВОДИТ взглядом
    // (рыск гуляет на четверть радиана) — так предмет виден идущим вдоль стола,
    // а не висящим на месте, — а перед тем как отпустить, разворачивается на
    // СЕРЕДИНУ СТОЛЕШНИЦЫ, чтобы «поставить» значило «поставить на стол», а не
    // «выронить туда, куда смотрел».
    if (grabbed_ >= 0 && f > SHOT_TAKE && f < PUT) {
        auto* ps = world_.get<gameplay::PlayerState>(player_);
        const auto* cam = world_.get<components::CameraPose>(player_);
        if (ps != nullptr && cam != nullptr) {
            if (f < PUT - 40 || !grab_lab_on_table_) {
                ps->yaw += 0.006f;
            } else {
                const glm::vec3 to = grab_lab_table_ - cam->position;
                const float flat = std::sqrt(to.x * to.x + to.z * to.z);
                ps->yaw = std::atan2(to.x, -to.z);
                ps->pitch = std::atan2(to.y + 0.25f, std::max(flat, 1e-3f));
            }
        }
    }
    // КЛАВИША ДЕРЖИТСЯ ОТРЕЗКАМИ, а не «нажимается»: механика меряет ВРЕМЯ
    // удержания, и рука обязана давать ей это время.
    grab_probe_key_ = (f >= PRESS1 && f < PUT) || (f >= PRESS2 && f < THROW);
    grab_probe_click_ = (f == THROW);
    if (f == SHOT_TAKE) {
        std::fprintf(stderr, "[прибор хвата] взято=%d, прицел=%d\n", grabbed_, grab_aimed());
        shot("grab-1-taken");
    }
    if (f == CARRY && grabbed_ >= 0) {
        const LoosePropLink& link = loose_props_[static_cast<std::size_t>(grabbed_)];
        const glm::vec3 at = physics_->body_pose(link.body).position;
        std::fprintf(stderr,
                     "[прибор хвата] несёт %s (%.2f кг): (%.2f %.2f %.2f), отстал %.3f м\n",
                     link.object.c_str(), static_cast<double>(link.mass_kg),
                     static_cast<double>(at.x), static_cast<double>(at.y),
                     static_cast<double>(at.z),
                     static_cast<double>(glm::length(grab_hand_point() - at)));
        shot("grab-2-carry");
    }
    // СМАХНУТЬ КУБКИ КУВШИНОМ (владелец 28.08: «толкать и двигать ДРУГИМИ
    // объектами, когда у меня в руках что-то есть»). Рука ВЕДЁТ кувшин на
    // кубки — то есть двигает не кубки, а руку, — и всё остальное делает
    // пружина: держимое остаётся полноценным динамическим телом и расталкивает
    // соседей само, без единой строки про кубки.
    // ЧТО ЗДЕСЬ БЫЛО И ПОЧЕМУ ЕГО НЕТ. Сюда просился шаг «смахнуть кувшином
    // кубки со стола», и он трижды дал ровно 0.000 м смещения. Причина
    // измерена, а не додумана, и она НЕ в хвате:
    //   * прицел прямо в кубок опускает предмет на carry_drop (0.15 м), то
    //     есть ведёт его В столешницу, где он и застревает;
    //   * до кубка на СЕРЕДИНЕ стола рука не достаёт: она несёт предмет в
    //     0.90 м перед глазом, а до середины 1.40, и ближе стол не пускает;
    //   * до кубка на ПОЛУ — тем более: глаз выше пола на 1.7 м.
    // Иначе говоря, СМАХНУТЬ МОЖНО ТО, ДО ЧЕГО ДОТЯГИВАЕШЬСЯ, и это верное
    // поведение, а не дефект: через стол ничего не смахивают, обходят. Само
    // свойство «предмет в руках толкает другие» доказано ЧИСЛАМИ с контролем
    // в tests/sim/LoosePropTests.cpp («предмет в руках толкает другие»: кубок
    // уехал на 0.840 м, тот же кадр без ведения — 0.000). Живым кадром эта
    // волна предъявляет ТОЛЧОК ТЕЛОМ (рецепт push), где расстояний хватает.
    if (f == SHOT_PUT) {
        for (const LoosePropLink& link : loose_props_) {
            const platform::BodyPose p = physics_->body_pose(link.body);
            std::fprintf(stderr, "[прибор хвата] %s: (%.3f %.3f %.3f) спит=%d\n",
                         link.object.c_str(), static_cast<double>(p.position.x),
                         static_cast<double>(p.position.y), static_cast<double>(p.position.z),
                         static_cast<int>(physics_->body_asleep(link.body)));
        }
        shot("grab-3-put");
    }
    if (f == SHOT_THROW) {
        shot("grab-4-thrown");
        std::fprintf(stderr, "[прибор хвата] готово\n");
    }
}

} // namespace dfn::app
