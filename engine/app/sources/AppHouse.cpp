/*
Created: 19:08:2026 - 01:40:00
Last updated: 19:08:2026 - 05:26:10
Module: engine/app
File: engine/app/sources/AppHouse.cpp

Responsibility:
- ВСЁ, ЧТО App ЗНАЕТ О ПОСТРОЙКЕ, В ОДНОМ ФАЙЛЕ: клавиши (ось, сетка, удаление,
  отмена, стрелки), сетка на земле, тело в отрисовку, коллайдер в физику,
  демо-сруб для приёмочных кадров.
- Собрано по второму аудиту (docs/AUDIT_EDITOR_TOOLS.md, долг 1): 33 прямых
  обращения App к постройке были размазаны по App.cpp и AppInput.cpp «мелочью по
  чуть-чуть» — здесь они на одном экране, и следующая механика постройки
  добавляется СЮДА, а не в run().

Dependencies:
- Uses: App.h, EditorToolHouse.h (сессия), world (HouseGraph/HouseMesh/HouseFile),
  render (MeshData), platform physics.
- Used by: dfn_app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ВСЯ ПРАВКА ГРАФА — ЧЕРЕЗ HouseSession::mutate (снимок для отмены пишется в
  ней); правка мимо неё — шаг, которого отмена не увидит.
- Тело, коллайдер и проволока читают ОДИН номер версии (HouseGraph::version).
*/
/*
UPD:
- 19:08:2026 - 01:40:00: Создан: методы постройки съехали из App.cpp и AppInput.cpp без
  изменений поведения (перенос, не переписывание).
- 19:08:2026 - 02:34:20: Цвет вершин потоков постройки белый: материал несёт плитка, тонировка затемнила бы её вдвое.
- 19:08:2026 - 03:22:40: Прицел на постройке — узлы сетки В ОБЪЁМЕ (крестики в узлах мира вокруг точки попадания): «узлы на стенах, узлы на полу».
- 19:08:2026 - 04:05:50: Потоки по mat/tone из параметров элемента; дверь — свой поток с петлёй и ВНЕ коллайдера; демо-сруб получил дверь из тёмной доски.
- 19:08:2026 - 05:26:10: Демо-сруб: вторая стена обшита с двумя окнами.
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppInternal.h"
#include "engine/app/sources/Controls.h"
#include "engine/editor/sources/EditorToolHouse.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/render/sources/RenderSystem.h"
#include "engine/world/sources/HouseMesh.h"

#include <cmath>
#include <map>
#include <cstdio>
#include <glm/geometric.hpp>

namespace dfn::app {

void App::on_axis_lock() {
    // ОСЬ КРУТИТСЯ ВОКРУГ ВЫБРАННОГО ЯКОРЯ: список того, вдоль чего можно
    // двигаться, — это прямые, приходящие ИМЕННО В НЕГО. Без выбранного якоря
    // остаются свободно и вертикаль, и это законное состояние (человек ещё
    // ничего не выбрал, а стойку вверх вести уже хочет).
    house_.cycle_axis(house_.selected_vertex());
    // СКАЗАНО ВСЛУХ, потому что фиксация оси — состояние без своей картинки на
    // кадре до первого движения мыши: молчащее переключение неотличимо от
    // непрочитанной клавиши. Подпись внизу экрана говорит то же самое, но
    // журнал остаётся и после того, как подпись сменится.
    std::fprintf(stderr, "[постройка] ось: %s\n", house_.axis_label().c_str());
}

void App::on_delete_selected() {
    const std::string why = house_.delete_selection();
    // ОТКАЗ ГОВОРИТСЯ ВСЛУХ И СО СПИСКОМ ДЕРЖАТЕЛЕЙ: молча не сработавшая
    // клавиша неотличима от сломанной, а этот сорт молчания мы разбирали
    // сегодня трижды.
    std::fprintf(stderr, "[постройка] удаление: %s\n", why.empty() ? "готово" : why.c_str());
}

void App::on_grid_toggle() {
    house_.set_grid_on(!house_.grid_on());
    std::fprintf(stderr, "[постройка] сетка %s, шаг %.2f м\n",
                 house_.grid_on() ? "включена" : "выключена",
                 static_cast<double>(house_.grid_step_m()));
}

void App::draw_editor_grid(const ToolAim& aim) {
    if (!house_.grid_on() || renderer_ == nullptr || !aim.hit) {
        return;
    }
    // ПРИЦЕЛ НА ПОСТРОЙКЕ — УЗЛЫ В ОБЪЁМЕ (заказ 19.08: «узлы на стенах, узлы
    // на полу»). Плоские отсечки по земле здесь не отвечают на вопрос «куда
    // прилипнет»: прилипание на стене живёт на высоте, а не на траве под ней.
    // Крестики по трём осям стоят в УЗЛАХ МИРА вокруг точки, где луч встретил
    // дом; куб 5x5x5 — 125 узлов, 375 линий, на порядок меньше бюджета.
    const float step = house_.grid_step_m();
    float house_t = 0.0f;
    if (house_.pick_element_ray(aim.origin, aim.direction(), HOUSE_EDGE_GRAB_M, &house_t)
            != world::NO_ELEMENT
        && house_t > 0.0f && house_t <= aim.distance_m + 0.5f) {
        const glm::vec3 hit = aim.origin + aim.direction() * house_t;
        const float tick = std::min(step * 0.12f, 0.15f);
        constexpr std::uint32_t COL = 0xFF60D8F0u;
        constexpr int HALF3 = 2;
        for (int iy = -HALF3; iy <= HALF3; ++iy) {
            for (int iz = -HALF3; iz <= HALF3; ++iz) {
                for (int ix = -HALF3; ix <= HALF3; ++ix) {
                    const glm::vec3 n{
                        std::round(hit.x / step + static_cast<float>(ix)) * step,
                        std::round(hit.y / step + static_cast<float>(iy)) * step,
                        std::round(hit.z / step + static_cast<float>(iz)) * step};
                    renderer_->debug_line(n - glm::vec3{tick, 0.0f, 0.0f},
                                          n + glm::vec3{tick, 0.0f, 0.0f}, COL);
                    renderer_->debug_line(n - glm::vec3{0.0f, tick, 0.0f},
                                          n + glm::vec3{0.0f, tick, 0.0f}, COL);
                    renderer_->debug_line(n - glm::vec3{0.0f, 0.0f, tick},
                                          n + glm::vec3{0.0f, 0.0f, tick}, COL);
                }
            }
        }
        renderer_->set_debug_lines(true);
        return;
    }
    // ОТСЕЧКИ ТОЛЬКО ВОКРУГ ПРИЦЕЛА, а не по всему миру — прямое требование:
    // «сетка везде глаза зальёт». Пятно узлов идёт за прицелом и живёт в
    // МИРОВЫХ координатах: узел там, где координата кратна шагу, и он не
    // сдвинется оттого, что человек отошёл.
    constexpr int HALF = 8; // узлов в каждую сторону от прицела
    const float cx = std::round(aim.point.x / step) * step;
    const float cz = std::round(aim.point.z / step) * step;
    // ПЕРЕКРЕСТИЯ, А НЕ СПЛОШНЫЕ ЛИНИИ. Сплошная сетка на траве читается как
    // рябь и прячет саму траву; короткие крестики в узлах говорят то же самое
    // («вот куда прилипнет»), занимая на порядок меньше пикселей.
    const float tick = std::min(step * 0.18f, 0.25f);
    constexpr std::uint32_t COL = 0xFF60D8F0u;
    for (int iz = -HALF; iz <= HALF; ++iz) {
        for (int ix = -HALF; ix <= HALF; ++ix) {
            const float x = cx + static_cast<float>(ix) * step;
            const float z = cz + static_cast<float>(iz) * step;
            const float y = chunks_.height_at({x, z}).value_or(aim.point.y) + 0.03f;
            renderer_->debug_line({x - tick, y, z}, {x + tick, y, z}, COL);
            renderer_->debug_line({x, y, z - tick}, {x, y, z + tick}, COL);
        }
    }
    renderer_->set_debug_lines(true);
}

/// ШАГ СТРЕЛКАМИ ПО ВЫБРАННОМУ ЯКОРЮ. true — что-то сдвинули.
///
/// НАПРАВЛЕНИЯ ОТ КАМЕРЫ, А НЕ ОТ МИРА: «вправо» значит вправо НА ЭКРАНЕ, иначе
/// человеку пришлось бы держать в голове, куда сейчас смотрит север. Вверх и
/// вниз — по мировой вертикали: единственное направление, которое от взгляда не
/// зависит и всегда значит одно и то же.


bool App::nudge_selected_anchor() {
    const float step = house_.grid_step_m();
    const float yaw = editor_cam_.yaw();
    const glm::vec3 fwd{std::sin(yaw), 0.0f, -std::cos(yaw)};
    const glm::vec3 right{std::cos(yaw), 0.0f, std::sin(yaw)};
    glm::vec3 by{0.0f};
    const bool shift = input_->is_down(platform::Key::LEFT_SHIFT)
                    || input_->is_down(platform::Key::RIGHT_SHIFT);
    if (input_->was_pressed(platform::Key::RIGHT)) { by += right * step; }
    if (input_->was_pressed(platform::Key::LEFT)) { by -= right * step; }
    // SHIFT+ВВЕРХ/ВНИЗ — ПО ВЫСОТЕ, без него — вперёд/назад по земле. Одна пара
    // клавиш на два направления: третьей пары стрелок на клавиатуре нет.
    if (input_->was_pressed(platform::Key::UP)) {
        by += shift ? glm::vec3{0.0f, step, 0.0f} : fwd * step;
    }
    if (input_->was_pressed(platform::Key::DOWN)) {
        by -= shift ? glm::vec3{0.0f, step, 0.0f} : fwd * step;
    }
    if (glm::length(by) < 1e-6f) {
        return false;
    }
    const world::VertexId id = house_.selected_vertex();
    // В УЗЕЛ, А НЕ НА ДЕЛЬТУ (заказ 18.08: «объекты при движении по сетке
    // должны не на дельту перемещаться, а чётко в координатах сетки жить, типа
    // 100 101 102 с шагом 1»). Разница видна на второй же нажатой стрелке:
    // якорь, стоявший в 100.37, от дельты уехал бы в 101.37 и остался бы кривым
    // навсегда. Сложение с шагом даёт направление, округление — само место.
    const glm::vec3 to = house_.snap_to_grid(house_.vertex_world(id) + by);
    (void)house_.mutate("сдвинул якорь стрелкой", [&](world::HouseGraph& g) {
        return g.move_vertex(id, house_.to_local(to));
    });
    std::fprintf(stderr, "[постройка] якорь v%u -> (%.2f %.2f %.2f), шаг %.2f м\n",
                 static_cast<unsigned>(id), static_cast<double>(to.x),
                 static_cast<double>(to.y), static_cast<double>(to.z),
                 static_cast<double>(step));
    return true;
}

void App::on_undo_redo() {
    // ОДИН ОБРАБОТЧИК НА ДВА ДЕЙСТВИЯ, потому что различает их МОДИФИКАТОР, а
    // не клавиша. Cmd обязателен: голая Z в редакторе — это буква, и если
    // однажды появится поле ввода, отмена начала бы срабатывать при наборе.
    const bool cmd = input_->is_down(platform::Key::LEFT_SUPER)
                  || input_->is_down(platform::Key::RIGHT_SUPER);
    if (!cmd) {
        return;
    }
    const bool shift = input_->is_down(platform::Key::LEFT_SHIFT)
                    || input_->is_down(platform::Key::RIGHT_SHIFT);
    // ОТМЕНЯТЬ НЕЧЕГО — ЭТО ГОВОРИТСЯ ВСЛУХ: молчащая отмена неотличима от
    // сломанной, а этот сорт молчания мы 18.08 разбирали трижды.
    const std::string state = shift ? history_.redo() : history_.undo();
    if (state.empty()) {
        std::fprintf(stderr, "[история] %s нечего\n",
                     shift ? "повторять" : "отменять");
        return;
    }
    std::fprintf(stderr, "[история] %s: %s\n", shift ? "повтор" : "отмена",
                 shift ? history_.undo_label().c_str() : history_.redo_label().c_str());
    // СНИМОК ПРИМЕНЯЕТ ТОТ, У КОГО ЕСТЬ МОДЕЛЬ. История про модель не знает и
    // знать не должна (иначе её нельзя было бы проверить без мира), поэтому
    // read_house зовётся здесь — в ЕДИНСТВЕННОМ месте, куда приходит снимок.
    if (!house_.apply_snapshot(state)) {
        std::fprintf(stderr, "[история] снимок не читается — постройка НЕ восстановлена\n");
    }
}

void App::seed_demo_house() {
    // ЧЕТЫРЕ СТОЙКИ, ПОЛ И ОДНА СТЕНА — этого хватает, чтобы кадр показал всё
    // три вида тела: брус вокруг оси, замкнутый контур с триангуляцией и
    // выдавленную вверх цепочку. Числа простые нарочно: приёмочный кадр должен
    // читаться глазом, а не сверяться с таблицей.
    using world::Anchoring;
    using world::ElementKind;
    const float S = 3.0f;   // сторона сруба, м
    const float H = 2.5f;   // высота стоек, м
    std::array<world::VertexId, 4> low{};
    std::array<world::VertexId, 4> high{};
    // СТАВИТСЯ ПЕРЕД КАМЕРОЙ, а не в нуле мира: в нуле сруб оказывался за
    // спиной и кадр показывал пустую траву при исправной геометрии — ровно тот
    // случай, когда дверь «молча ничего не сделала».
    const glm::vec3 eye = editor_cam_.position();
    const float yaw = editor_cam_.yaw();
    // ВЗЯТО ИЗ EditorCamera, а не выведено заново: у камеры yaw 0 смотрит на
    // СЕВЕР, то есть в −Z, и мой первый вывод («вперёд это +X») поставил сруб
    // сбоку от кадра при исправной геометрии.
    const glm::vec3 fwd{std::sin(yaw), 0.0f, -std::cos(yaw)};
    const glm::vec3 right{std::cos(yaw), 0.0f, std::sin(yaw)};
    const glm::vec3 base = eye + fwd * 18.0f - right * (S * 0.5f);
    std::array<glm::vec3, 4> corners{{base,
                                      base + right * S,
                                      base + right * S + fwd * S,
                                      base + fwd * S}};
    // ВЫСОТА УГЛОВ — ОТ ЗЕМЛИ, А НЕ ОТ ГЛАЗА. Первый заход взял y камеры, и
    // сруб встал полом на семь метров выше своих же стоек: пол на 27.4, верх на
    // 22.5. На кадре это выглядело как «двери нет», а не как «пол не там».
    for (glm::vec3& c : corners) {
        c.y = chunks_.height_at({c.x, c.z}).value_or(0.0f);
    }
    (void)house_.mutate("демо-сруб", [&](world::HouseGraph& g) {
        for (std::size_t i = 0; i < 4; ++i) {
            // Заземлённый якорь хранит только XZ — высоту ему даёт рельеф;
            // верхний свободен и стоит на H над той же землёй.
            const float gy = corners[i].y;
            low[i] = g.add_vertex(Anchoring::OnGround, corners[i]);
            high[i] = g.add_vertex(Anchoring::Free,
                                   {corners[i].x, gy + H, corners[i].z});
            world::ElementId post = world::NO_ELEMENT;
            (void)g.add_element(ElementKind::Line, {low[i], high[i]}, "", post);
        }
        // Обвязка поверху — четыре бруса по кругу.
        for (std::size_t i = 0; i < 4; ++i) {
            world::ElementId beam = world::NO_ELEMENT;
            (void)g.add_element(ElementKind::Line, {high[i], high[(i + 1) % 4]}, "", beam);
        }
        // Пол: замкнутый контур на четырёх нижних якорях.
        world::ElementId floor = world::NO_ELEMENT;
        if (g.add_element(ElementKind::Surface,
                          {low[0], low[1], low[2], low[3]}, "", floor).ok) {
            g.set_closed(floor, true);
            g.set_param(floor, "thickness", "0.12");
        }
        // Стена: открытая цепочка по одной стороне, выдавливается вверх — И ЭТО
        // ДВЕРЬ: кадр обязан показывать и материал (доска), и петлю в работе.
        world::ElementId wall = world::NO_ELEMENT;
        if (g.add_element(ElementKind::Surface, {low[0], low[1]}, "", wall).ok) {
            g.set_param(wall, "height", "2.5");
            g.set_param(wall, "thickness", "0.10");
            g.set_param(wall, "mat", "1");  // пилёная доска
            g.set_param(wall, "tone", "2"); // тёмная
            g.set_param(wall, "door", "1");
            g.set_param(wall, "hinge", "0");
        }
        // И ОБШИТАЯ СТЕНА С ОКНАМИ — на кадре обязано быть видно, что доски,
        // раскосы и рамы стали ГЕОМЕТРИЕЙ, а не картинкой.
        world::ElementId clad_wall = world::NO_ELEMENT;
        if (g.add_element(ElementKind::Surface, {low[1], low[2]}, "", clad_wall).ok) {
            g.set_param(clad_wall, "height", "2.5");
            g.set_param(clad_wall, "thickness", "0.10");
            g.set_param(clad_wall, "clad", "1");
            g.set_param(clad_wall, "windows", "2");
        }
        return world::GraphResult{};
    });
    std::fprintf(stderr, "[постройка] сруб: глаз (%.1f %.1f %.1f) yaw %.2f, угол (%.1f %.1f)\n",
                 static_cast<double>(eye.x), static_cast<double>(eye.y),
                 static_cast<double>(eye.z), static_cast<double>(yaw),
                 static_cast<double>(base.x), static_cast<double>(base.z));
    std::fprintf(stderr, "[постройка] дверь DFN_HOUSE_DEMO: якорей %zu, элементов %zu\n",
                 house_.graph().vertex_count(), house_.graph().element_count());
}

void App::upload_house_mesh() {
    if (renderer_ == nullptr) {
        return;
    }
    const world::HouseMesh built = world::build_house_mesh(house_.graph());
    // ПОТОК НА МАТЕРИАЛ (заказ 19.08: «выбирать текстуры для палок, стен»).
    // Материал элемента — параметры mat/tone; по умолчанию брус носит тёсаное
    // дерево, полотно — светлую штукатурку. Дверь (param door=1) уезжает в
    // СВОЙ поток с петлёй и в коллайдер НЕ входит: она качается, а статичное
    // тело в проёме держало бы человека в пустом дверном проёме.
    const glm::vec3 zero = house_.to_world({0.0f, 0.0f, 0.0f});
    const auto to_world_vertex = [&](const world::HouseVertex& v) {
        platform::Vertex pv{};
        pv.position = house_.to_world(v.pos);
        pv.normal = house_.to_world(v.normal) - zero;
        pv.uv = v.uv;
        pv.color_rgba = 0xFFFFFFFFu; // материал несёт плитка, тонировка затемнила бы её
        return pv;
    };
    const auto mat_of = [&](const world::Element& e, std::uint32_t& surface,
                            std::uint32_t& tone) {
        const bool beam = e.kind == world::ElementKind::Line;
        surface = beam ? 0u : 5u; // HewnTimber : Plaster
        tone = beam ? 1u : 0u;    // Mid : Light
        const std::string m = house_.graph().param(e.id, "mat");
        const std::string t = house_.graph().param(e.id, "tone");
        if (!m.empty()) { surface = static_cast<std::uint32_t>(std::atoi(m.c_str())) % 9u; }
        if (!t.empty()) { tone = static_cast<std::uint32_t>(std::atoi(t.c_str())) % 4u; }
    };
    std::map<std::uint64_t, render::RenderSystem::HouseStream> streams;
    std::vector<render::RenderSystem::HouseDoor> doors;
    std::vector<std::uint32_t> remap;
    house_positions_.clear();
    std::vector<std::uint32_t> collider_indices;
    for (const world::MeshPart& part : built.parts) {
        const world::Element* e = house_.graph().element(part.element);
        if (e == nullptr) {
            continue;
        }
        std::uint32_t surface = 0;
        std::uint32_t tone = 0;
        mat_of(*e, surface, tone);
        const bool is_door = house_.graph().param(e->id, "door") == "1";
        render::MeshData* into = nullptr;
        if (is_door) {
            doors.emplace_back();
            doors.back().surface = surface;
            doors.back().tone = tone;
            // ПЕТЛЯ — ВЫБРАННАЯ ПАРА СОСЕДНИХ ЯКОРЕЙ (param hinge = номер
            // ребра обхода, по кругу). Ось идёт через их мировые точки.
            const std::size_t n = e->refs.size();
            std::size_t hinge = 0;
            if (const std::string h = house_.graph().param(e->id, "hinge"); !h.empty()) {
                hinge = static_cast<std::size_t>(std::atoi(h.c_str())) % n;
            }
            doors.back().hinge_a = house_.vertex_world(e->refs[hinge]);
            doors.back().hinge_b = house_.vertex_world(e->refs[(hinge + 1) % n]);
            into = &doors.back().mesh;
        } else {
            auto& st = streams[(static_cast<std::uint64_t>(surface) << 8) | tone];
            st.surface = surface;
            st.tone = tone;
            into = &st.mesh;
        }
        remap.assign(built.vertices.size(), 0xFFFFFFFFu);
        for (std::uint32_t i = 0; i < part.index_count; ++i) {
            const std::uint32_t vi = built.indices[part.index_begin + i];
            if (remap[vi] == 0xFFFFFFFFu) {
                remap[vi] = static_cast<std::uint32_t>(into->vertices.size());
                into->vertices.push_back(to_world_vertex(built.vertices[vi]));
            }
            into->indices.push_back(remap[vi]);
        }
        if (!is_door) {
            // Коллайдер — из тех же треугольников, дверные исключены.
            for (std::uint32_t i = 0; i < part.index_count; ++i) {
                const std::uint32_t vi = built.indices[part.index_begin + i];
                collider_indices.push_back(
                    static_cast<std::uint32_t>(house_positions_.size()));
                house_positions_.push_back(house_.to_world(built.vertices[vi].pos));
            }
        }
    }
    for (const world::MeshFinding& f : built.findings) {
        std::fprintf(stderr, "[постройка] e%u: %s\n", static_cast<unsigned>(f.element),
                     f.what.c_str());
    }
    std::vector<render::RenderSystem::HouseStream> stream_list;
    for (auto& [key, st] : streams) {
        stream_list.push_back(std::move(st));
    }
    const std::size_t n_streams = stream_list.size();
    const std::size_t n_doors = doors.size();
    render_system_.set_house_mesh(*renderer_, std::move(stream_list), std::move(doors));

    // СКВОЗЬ ДОМ ХОДИТЬ НЕЛЬЗЯ (кроме дверей — они качаются). Коллайдер из тех
    // же треугольников, что и картинка: два описания одного дома разъезжаются в
    // день, когда правят одно из них.
    if (physics_ != nullptr) {
        if (house_body_.valid()) {
            physics_->destroy_body(house_body_);
            house_body_ = {};
        }
        if (!house_positions_.empty() && !collider_indices.empty()) {
            platform::TerrainMeshDesc desc;
            desc.positions = house_positions_;
            desc.indices = collider_indices;
            desc.layer = physics::LAYER_STATIC;
            house_body_ = physics_->create_terrain_mesh(desc);
            if (!house_body_.valid()) {
                std::fprintf(stderr, "[постройка] коллайдер НЕ создан — сквозь дом "
                                     "можно пройти\n");
            } else {
                glm::vec3 lo = house_positions_.front();
                glm::vec3 hi = lo;
                for (const glm::vec3& p : house_positions_) {
                    lo = glm::min(lo, p);
                    hi = glm::max(hi, p);
                }
                const glm::vec3 mid = (lo + hi) * 0.5f;
                const float span = glm::length(hi - lo) + 2.0f;
                const platform::RayHit through = physics_->raycast(
                    {lo.x - span, mid.y, mid.z}, {1.0f, 0.0f, 0.0f}, span * 2.0f,
                    physics::LAYER_STATIC);
                const platform::RayHit beside = physics_->raycast(
                    {lo.x - span, mid.y, hi.z + 50.0f}, {1.0f, 0.0f, 0.0f}, span * 2.0f,
                    physics::LAYER_STATIC);
                std::fprintf(stderr,
                             "[постройка] коллайдер: сквозь дом %s, в стороне %s\n",
                             through.hit ? "упёрся" : "ПРОШЁЛ НАСКВОЗЬ",
                             beside.hit ? "тоже упёрся (прибор врёт)" : "прошёл");
            }
        }
    }
    std::fprintf(stderr, "[постройка] тело: потоков %zu, дверей %zu\n", n_streams,
                 n_doors);
}

} // namespace dfn::app
