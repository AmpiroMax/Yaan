/*
Created: 19:08:2026 - 01:40:00
Last updated: 19:08:2026 - 02:34:20
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
    // ОТСЕЧКИ ТОЛЬКО ВОКРУГ ПРИЦЕЛА, а не по всему миру — прямое требование:
    // «сетка везде глаза зальёт». Пятно узлов идёт за прицелом и живёт в
    // МИРОВЫХ координатах: узел там, где координата кратна шагу, и он не
    // сдвинется оттого, что человек отошёл.
    const float step = house_.grid_step_m();
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
        // Стена: открытая цепочка по одной стороне, выдавливается вверх.
        world::ElementId wall = world::NO_ELEMENT;
        if (g.add_element(ElementKind::Surface, {low[0], low[1]}, "", wall).ok) {
            g.set_param(wall, "height", "2.5");
            g.set_param(wall, "thickness", "0.10");
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
    render::MeshData out;
    out.vertices.reserve(built.vertices.size());
    out.indices = built.indices;
    // В МИРОВЫЕ КООРДИНАТЫ ЗДЕСЬ. Граф живёт в координатах постройки (так дом
    // переносится целиком и копируется файлом), а слот рисуется единичной
    // матрицей; перенос делает тот, кто знает про сессию.
    const glm::vec3 zero = house_.to_world({0.0f, 0.0f, 0.0f});
    for (const world::HouseVertex& v : built.vertices) {
        platform::Vertex pv{};
        pv.position = house_.to_world(v.pos);
        // НОРМАЛЬ ПОВОРАЧИВАЕТСЯ, НО НЕ ПЕРЕНОСИТСЯ: это направление, а не
        // точка. Разность двух переведённых точек — самый дешёвый способ
        // спросить у сессии её поворот, не заводя второго знания о нём.
        pv.normal = house_.to_world(v.normal) - zero;
        pv.uv = v.uv;
        // ЦВЕТ — ВРЕМЕННАЯ ЗАМЕНА МАТЕРИАЛУ, и он назван вслух именно так.
        // Программа «prop» рисует освещённую геометрию с цветом вершины и без
        // текстуры; пока стиля (.dfstyle) в отрисовке нет, белое тело читается
        // как пластик. Брус и полотно красятся по-разному, чтобы на кадре было
        // видно, где каркас, а где стена.
        pv.color_rgba = 0xFFFFFFFFu;
        out.vertices.push_back(pv);
    }
    // ДВА ПОТОКА ПО МАТЕРИАЛУ: каркас и полотна. Материал приходит текстурой
    // draw-вызова, поэтому одним потоком брус и штукатурка носили бы одну
    // шкуру. Индексы перенумеровываются, а вершины копируются: отдать целый
    // буфер вершин обоим потокам значило бы залить его в видеопамять дважды.
    render::MeshData beams;
    render::MeshData panels;
    std::vector<std::uint32_t> remap(out.vertices.size(), 0xFFFFFFFFu);
    const auto take = [&](render::MeshData& into, std::uint32_t vi) {
        if (remap[vi] == 0xFFFFFFFFu) {
            remap[vi] = static_cast<std::uint32_t>(into.vertices.size());
            into.vertices.push_back(out.vertices[vi]);
        }
        into.indices.push_back(remap[vi]);
    };
    for (const world::MeshPart& part : built.parts) {
        const world::Element* e = house_.graph().element(part.element);
        if (e == nullptr) {
            continue;
        }
        const bool is_beam = e->kind == world::ElementKind::Line;
        render::MeshData& into = is_beam ? beams : panels;
        // ЦВЕТ ВЕРШИНЫ — МНОЖИТЕЛЬ ПОВЕРХ ПЛИТКИ, и у обоих потоков он БЕЛЫЙ:
        // материал теперь несёт текстура draw-вызова (fs_prop сэмплит с 19.08),
        // а тонировка поверх плитки затемнила бы её вдвое. Тёплый и светлый
        // цвета, которыми потоки различались, пока плитки не читались, ушли
        // вместе с причиной их существования.
        constexpr std::uint32_t WHITE = 0xFFFFFFFFu;
        const std::uint32_t col = WHITE;
        (void)is_beam;
        for (std::uint32_t i = 0; i < part.index_count; ++i) {
            const std::uint32_t vi = out.indices[part.index_begin + i];
            if (vi < out.vertices.size()) {
                out.vertices[vi].color_rgba = col;
            }
        }
        std::fill(remap.begin(), remap.end(), 0xFFFFFFFFu);
        for (std::uint32_t i = 0; i < part.index_count; ++i) {
            take(into, out.indices[part.index_begin + i]);
        }
    }
    // НАХОДКИ ГОВОРЯТСЯ ВСЛУХ: неплоский контур и вырожденный элемент — это то,
    // что человек увидит как дыру в стене, и молчание здесь стоило бы ему
    // получаса поисков.
    for (const world::MeshFinding& f : built.findings) {
        std::fprintf(stderr, "[постройка] e%u: %s\n", static_cast<unsigned>(f.element),
                     f.what.c_str());
    }
    render_system_.set_house_mesh(*renderer_, beams, panels);
    // СКВОЗЬ ДОМ ХОДИТЬ НЕЛЬЗЯ. Коллайдер строится ИЗ ТЕХ ЖЕ ТРЕУГОЛЬНИКОВ, что
    // и картинка, и это не экономия, а требование: два независимых описания
    // одного дома разъезжаются в тот день, когда правят одно из них, — и
    // человек упирается в воздух там, где стены нет.
    //
    // Тело пересобирается целиком на каждое изменение постройки. Дорого это
    // станет на большом городе, и тогда пересборку надо будет резать по
    // элементам; сегодня дом — один, а неверная физика видна сразу.
    if (physics_ != nullptr) {
        if (house_body_.valid()) {
            physics_->destroy_body(house_body_);
            house_body_ = {};
        }
        house_positions_.clear();
        house_positions_.reserve(out.vertices.size());
        for (const platform::Vertex& v : out.vertices) {
            house_positions_.push_back(v.position);
        }
        if (!house_positions_.empty() && !out.indices.empty()) {
            platform::TerrainMeshDesc desc;
            desc.positions = house_positions_;
            desc.indices = out.indices;
            desc.layer = physics::LAYER_STATIC;
            house_body_ = physics_->create_terrain_mesh(desc);
            if (!house_body_.valid()) {
                std::fprintf(stderr, "[постройка] коллайдер НЕ создан — сквозь дом "
                                     "можно пройти\n");
            } else {
                // ЛУЧ СКВОЗЬ СОБСТВЕННОЕ ТЕЛО — ПРОВЕРКА, А НЕ УКРАШЕНИЕ.
                // «Тело создано» и «в него можно упереться» — разные
                // утверждения: у выродившихся треугольников форма создаётся, а
                // столкновений не даёт. Луч пускается через середину коробки
                // построенного, и рядом печатается КОНТРОЛЬ — тот же луч в
                // стороне от неё. Совпали ответы — прибор ничего не меряет.
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
    std::fprintf(stderr,
                 "[постройка] тело: каркас %zu треугольников, полотна %zu\n",
                 beams.triangle_count(), panels.triangle_count());
}

} // namespace dfn::app
