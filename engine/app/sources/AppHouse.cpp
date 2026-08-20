/*
Created: 19:08:2026 - 01:40:00
Last updated: 20:08:2026 - 17:30:00
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
- 20:08:2026 - 00:02:30: App печёт свотчи материалов из листа набора и составные примеры заполнения (штукатурка + брус + проёмы), кэш по ключу.
- 20:08:2026 - 00:58:40: Бакеты уважают материал куска; свотчи и карточки светятся по листу нормалей («плоские текстуры» 20.08); карточки кирпича и блоков; демо-сруб получил кирпичную стену.
- 20:08:2026 - 12:10:00: Краска элемента — вершинный цвет поверх плитки материала.
- 20:08:2026 - 15:30:00: upload_house_mesh вливает готовые постройки карты (append_graph на граф) в общие потоки и ОДИН коллайдер; load_scene_houses; проба коллайдера целится сквозь вершину, контроль — над рельефом.
- 20:08:2026 - 17:30:00: Износ: мох по нижнему метру вершинным цветом, wear>=0.7 уводит тон в выветренный ряд.
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppInternal.h"
#include "engine/app/sources/Controls.h"
#include "engine/editor/sources/EditorToolHouse.h"
#include "engine/editor/sources/EditorUi.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/render/sources/PartsAtlas.h"
#include "engine/render/sources/RenderSystem.h"
#include "engine/world/sources/HouseFile.h"
#include "engine/world/sources/HouseMesh.h"
#include "engine/world/sources/Scene.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
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
        // И КИРПИЧНАЯ СТЕНА — кладка кусочками с перевязкой на кадре.
        world::ElementId brick_wall = world::NO_ELEMENT;
        if (g.add_element(ElementKind::Surface, {low[2], low[3]}, "", brick_wall).ok) {
            g.set_param(brick_wall, "height", "2.5");
            g.set_param(brick_wall, "thickness", "0.10");
            g.set_param(brick_wall, "fill", "2");
            g.set_param(brick_wall, "windows", "1");
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

/// СВОТЧ СВЕТИТСЯ ПО ЛИСТУ НОРМАЛЕЙ. Плоское альбедо человек прочитал как
/// «нет объёмных вариантов, только плоские текстуры» (20.08) — и был прав:
/// в мире борозды даёт пер-пиксельный рельеф, а картинка в меню его не несла,
/// то есть ОБЕЩАЛА другой материал. Свет фиксированный, сверху-слева, как у
/// эталонных кадров каталога деталей.
static void shade_by_normal(std::uint8_t* dst, const std::uint8_t* albedo,
                            const std::uint8_t* normal) {
    const float nx = static_cast<float>(normal[0]) / 127.5f - 1.0f;
    const float ny = static_cast<float>(normal[1]) / 127.5f - 1.0f;
    const float nz = static_cast<float>(normal[2]) / 127.5f - 1.0f;
    // L = normalize(-0.45, -0.55, 0.70): сверху-слева, к зрителю.
    const float diff = std::max(nx * -0.45f + ny * -0.55f + nz * 0.70f, 0.0f);
    const float lit = 0.35f + 0.80f * diff;
    for (int c = 0; c < 3; ++c) {
        dst[c] = static_cast<std::uint8_t>(
            std::min(255.0f, static_cast<float>(albedo[c]) * lit));
    }
    dst[3] = 255;
}

std::uint64_t App::house_material_swatch(int surface, int tone, int px) {
    // ПЕЧЁТСЯ ИЗ ТОГО ЖЕ ЛИСТА, ЧТО НОСЯТ СТЕНЫ: свотч, нарисованный отдельно,
    // разошёлся бы с материалом в мире в первый же день правки листа.
    const std::uint64_t key = (static_cast<std::uint64_t>(surface) << 32)
                            | (static_cast<std::uint64_t>(tone) << 16)
                            | static_cast<std::uint64_t>(px);
    if (const auto it = house_swatches_.find(key); it != house_swatches_.end()) {
        return it->second;
    }
    const std::uint32_t side = static_cast<std::uint32_t>(std::max(px, 16));
    const render::PartsAtlas sheet = render::generate_parts_atlas(side);
    const render::PartsAtlas normals = render::generate_parts_normal_atlas(side);
    std::vector<std::uint8_t> tile(static_cast<std::size_t>(side) * side * 4u);
    const std::uint32_t x0 = static_cast<std::uint32_t>(surface) * side;
    const std::uint32_t y0 = static_cast<std::uint32_t>(tone) * side;
    for (std::uint32_t y = 0; y < side; ++y) {
        const std::size_t row = (static_cast<std::size_t>(y0 + y) * sheet.width + x0) * 4u;
        for (std::uint32_t x = 0; x < side; ++x) {
            shade_by_normal(&tile[(static_cast<std::size_t>(y) * side + x) * 4u],
                            &sheet.pixels[row + x * 4u], &normals.pixels[row + x * 4u]);
        }
    }
    const std::uint64_t tex = editor_ui_.make_texture(side, side, tile.data());
    house_swatches_.emplace(key, tex);
    return tex;
}

std::uint64_t App::house_wall_example(int variant, int px) {
    // ПРИМЕР ЗАПОЛНЕНИЯ — КАРТИНКА, СОБРАННАЯ ИЗ ПЛИТОК НАБОРА: штукатурка
    // фоном, брус досками и раскосом, тёмные проёмы окон. Это иллюстрация
    // ПРАВИЛА, а не рендер конкретной стены: правило («как будет заполнено»)
    // человек выбирает до того, как стена существует.
    const std::uint64_t key = 0xF000000000000000ull
                            | (static_cast<std::uint64_t>(variant) << 16)
                            | static_cast<std::uint64_t>(px);
    if (const auto it = house_swatches_.find(key); it != house_swatches_.end()) {
        return it->second;
    }
    const std::uint32_t w = static_cast<std::uint32_t>(std::max(px, 32));
    const std::uint32_t h = w * 2u / 3u;
    const render::PartsAtlas sheet = render::generate_parts_atlas(64);
    const render::PartsAtlas normals = render::generate_parts_normal_atlas(64);
    const auto sheet_off = [&](std::uint32_t surface, std::uint32_t tone, std::uint32_t x,
                               std::uint32_t y) {
        const std::uint32_t sx = surface * 64u + (x % 64u);
        const std::uint32_t sy = tone * 64u + (y % 64u);
        return (static_cast<std::size_t>(sy) * sheet.width + sx) * 4u;
    };
    std::vector<std::uint8_t> img(static_cast<std::size_t>(w) * h * 4u);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            // Фон — штукатурка (светлая).
            std::size_t off = sheet_off(5, 0, x, y);
            if (variant == 3 || variant == 4) {
                // КЛАДКА: ряды с перевязкой; шов тёмный. Кирпич мельче, блок
                // крупнее; материал — глина или камень из того же листа.
                const std::uint32_t uh = variant == 3 ? h / 8u : h / 4u;
                const std::uint32_t ul = variant == 3 ? w / 6u : w / 3u;
                const std::uint32_t seam = std::max(w / 48u, 1u);
                const std::uint32_t row = y / uh;
                const std::uint32_t xo = x + (row % 2u) * (ul / 2u);
                const bool in_seam = (y % uh) < seam || (xo % ul) < seam;
                std::uint8_t* dst = &img[(static_cast<std::size_t>(y) * w + x) * 4u];
                if (in_seam) {
                    dst[0] = 30; dst[1] = 27; dst[2] = 24; dst[3] = 255;
                } else {
                    const std::size_t moff =
                        sheet_off(variant == 3 ? 4u : 3u, 1, x, y);
                    shade_by_normal(dst, &sheet.pixels[moff], &normals.pixels[moff]);
                }
                continue;
            }
            bool timber = false;
            if (variant >= 1) {
                // Доски: рамка по краю и стойки каждые ~w/4; раскос — диагональ.
                const std::uint32_t step = w / 4u;
                const std::uint32_t bar = std::max(w / 16u, 2u);
                timber = x < bar || x >= w - bar || y < bar || y >= h - bar
                      || (x % step) < bar
                      || (x > y && x - y < bar * 2u); // раскос
            }
            bool window = false;
            if (variant >= 2) {
                // Два тёмных проёма между стойками.
                const std::uint32_t step = w / 4u;
                const std::uint32_t wx0 = step + w / 20u;
                const std::uint32_t wx1 = 2u * step - w / 20u;
                const std::uint32_t wx2 = 2u * step + w / 20u;
                const std::uint32_t wx3 = 3u * step - w / 20u;
                const std::uint32_t wy0 = h / 3u;
                const std::uint32_t wy1 = 2u * h / 3u;
                window = y >= wy0 && y < wy1
                      && ((x >= wx0 && x < wx1) || (x >= wx2 && x < wx3));
            }
            if (timber) {
                off = sheet_off(0, 1, x, y); // тёсаный брус, средний
            }
            std::uint8_t* dst = &img[(static_cast<std::size_t>(y) * w + x) * 4u];
            if (window) {
                dst[0] = 24; dst[1] = 20; dst[2] = 16; dst[3] = 255;
            } else {
                shade_by_normal(dst, &sheet.pixels[off], &normals.pixels[off]);
            }
        }
    }
    const std::uint64_t tex = editor_ui_.make_texture(w, h, img.data());
    house_swatches_.emplace(key, tex);
    return tex;
}

void App::upload_house_mesh() {
    if (renderer_ == nullptr) {
        return;
    }
    // ПОТОК НА МАТЕРИАЛ (заказ 19.08: «выбирать текстуры для палок, стен»).
    // Материал элемента — параметры mat/tone; по умолчанию брус носит тёсаное
    // дерево, полотно — светлую штукатурку. Дверь (param door=1) уезжает в
    // СВОЙ поток с петлёй и в коллайдер НЕ входит: она качается, а статичное
    // тело в проёме держало бы человека в пустом дверном проёме.
    //
    // СЮДА ЖЕ ВЛИВАЮТСЯ ГОТОВЫЕ ПОСТРОЙКИ КАРТЫ (20.08, секция [house]):
    // один набор потоков и ОДИН коллайдер на всё построенное — у картинки и
    // физики нет второй истории, которая могла бы разъехаться.
    std::map<std::uint64_t, render::RenderSystem::HouseStream> streams;
    std::vector<render::RenderSystem::HouseDoor> doors;
    std::vector<std::uint32_t> collider_indices;
    house_positions_.clear();

    const auto append_graph = [&](const world::HouseGraph& graph,
                                  const auto& to_world) {
        const world::HouseMesh built = world::build_house_mesh(graph);
        const glm::vec3 zero = to_world(glm::vec3{0.0f});
        // НИЗ ПОСТРОЙКИ — для мха износа: зелёный налёт живёт в первом метре
        // от самой низкой точки, как сырость от земли.
        float gmin_y = 1e9f;
        for (const auto& v : built.vertices) {
            gmin_y = std::min(gmin_y, v.pos.y);
        }
        // КРАСКА ЭЛЕМЕНТА — вершинный цвет: плитка материала умножается на
        // него в шейдере, 0xFFFFFFFF (без краски) оставляет её как есть.
        std::uint32_t part_color = 0xFFFFFFFFu;
        float part_wear = 0.0f;
        const auto to_world_vertex = [&](const world::HouseVertex& v) {
            platform::Vertex pv{};
            pv.position = to_world(v.pos);
            pv.normal = to_world(v.normal) - zero;
            pv.uv = v.uv;
            pv.color_rgba = part_color;
            // МОХ ИЗНОСА: вершинный цвет тянется к зелёному налёту тем
            // сильнее, чем ниже вершина (первый метр) и чем старше элемент.
            if (part_wear > 0.0f) {
                const float base = std::clamp(
                    1.0f - (v.pos.y - gmin_y) / 0.9f, 0.0f, 1.0f);
                const float k = part_wear * base * 0.55f;
                if (k > 0.01f) {
                    const auto ch = [&](int shift, float target) {
                        const float c =
                            static_cast<float>((part_color >> shift) & 0xFFu);
                        const float m = c * (1.0f - k) + target * 255.0f * k;
                        return static_cast<std::uint32_t>(std::lround(m)) << shift;
                    };
                    pv.color_rgba = 0xFF000000u | ch(16, 0.52f) | ch(8, 0.72f)
                                  | ch(0, 0.58f); // BGR: мшисто-зелёный
                }
            }
            return pv;
        };
        const auto paint_of = [&](const world::Element& e) -> std::uint32_t {
            const std::string c = graph.param(e.id, "paint");
            if (c.empty()) {
                return 0xFFFFFFFFu;
            }
            const int idx =
                std::clamp(std::atoi(c.c_str()), 0, world::HOUSE_PAINT_COUNT - 1);
            const glm::vec3 rgb = world::HOUSE_PAINT_RGB[idx];
            const auto b = [](float f) {
                return static_cast<std::uint32_t>(std::lround(f * 255.0f));
            };
            return 0xFF000000u | (b(rgb.z) << 16) | (b(rgb.y) << 8) | b(rgb.x);
        };
        const auto mat_of = [&](const world::Element& e, std::uint32_t& surface,
                                std::uint32_t& tone) {
            const bool beam = e.kind == world::ElementKind::Line;
            surface = beam ? 0u : 5u; // HewnTimber : Plaster
            tone = beam ? 1u : 0u;    // Mid : Light
            const std::string m = graph.param(e.id, "mat");
            const std::string t = graph.param(e.id, "tone");
            if (!m.empty()) {
                surface = static_cast<std::uint32_t>(std::atoi(m.c_str())) % 9u;
            }
            if (!t.empty()) {
                tone = static_cast<std::uint32_t>(std::atoi(t.c_str())) % 4u;
            }
            // СИЛЬНЫЙ ИЗНОС УВОДИТ ТОН В ВЫВЕТРЕННЫЙ РЯД атласа: серость и
            // лишайник нарисованы там, а не выдумываются шейдером.
            const std::string w = graph.param(e.id, "wear");
            if (!w.empty() && std::strtof(w.c_str(), nullptr) >= 0.7f) {
                tone = 3u; // Weathered
            }
        };
        std::vector<std::uint32_t> remap;
        for (const world::MeshPart& part : built.parts) {
            const world::Element* e = graph.element(part.element);
            if (e == nullptr) {
                continue;
            }
            std::uint32_t surface = 0;
            std::uint32_t tone = 0;
            mat_of(*e, surface, tone);
            part_color = paint_of(*e);
            {
                const std::string w = graph.param(e->id, "wear");
                part_wear = w.empty() ? 0.0f : std::strtof(w.c_str(), nullptr);
            }
            // КУСОК КЛАДКИ НЕСЁТ СВОЙ МАТЕРИАЛ: доска фахверка — брус,
            // кирпич — глина, блок — камень. Элементный остаётся у пластины.
            if (part.mat_override >= 0) {
                surface = static_cast<std::uint32_t>(part.mat_override) % 9u;
            }
            if (part.tone_override >= 0) {
                tone = static_cast<std::uint32_t>(part.tone_override) % 4u;
            }
            const bool is_door = graph.param(e->id, "door") == "1";
            render::MeshData* into = nullptr;
            if (is_door) {
                doors.emplace_back();
                doors.back().surface = surface;
                doors.back().tone = tone;
                // ПЕТЛЯ — ВЫБРАННАЯ ПАРА СОСЕДНИХ ЯКОРЕЙ (param hinge — номер
                // ребра обхода, по кругу). Ось идёт через их мировые точки.
                const std::size_t n = e->refs.size();
                std::size_t hinge = 0;
                if (const std::string h = graph.param(e->id, "hinge"); !h.empty()) {
                    hinge = static_cast<std::size_t>(std::atoi(h.c_str())) % n;
                }
                doors.back().hinge_a = to_world(graph.resolved_local(e->refs[hinge]));
                doors.back().hinge_b =
                    to_world(graph.resolved_local(e->refs[(hinge + 1) % n]));
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
                    house_positions_.push_back(to_world(built.vertices[vi].pos));
                }
            }
        }
        for (const world::MeshFinding& f : built.findings) {
            std::fprintf(stderr, "[постройка] e%u: %s\n",
                         static_cast<unsigned>(f.element), f.what.c_str());
        }
    };

    append_graph(house_.graph(),
                 [&](glm::vec3 local) { return house_.to_world(local); });
    for (const PlacedHouse& ph : placed_houses_) {
        // Поворот вокруг вертикали по конвенции сцены: местный +X при yaw
        // уходит в (cos, -sin) — та же формула, что у расстановок деталей.
        const float c = std::cos(ph.yaw);
        const float sn = std::sin(ph.yaw);
        append_graph(ph.graph, [&, c, sn](glm::vec3 l) {
            return ph.pos + glm::vec3{l.x * c + l.z * sn, l.y, -l.x * sn + l.z * c};
        });
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
                const float span = glm::length(hi - lo) + 2.0f;
                // Луч целится СКВОЗЬ ПЕРВУЮ ВЕРШИНУ коллайдера, а не в
                // середину габарита: с несколькими домами на карте середина
                // попадает в коридор между ними, и прибор кричал бы про дыру,
                // которой нет (найдено дымом карты «Стройка», 20.08).
                const glm::vec3 aim = house_positions_.front();
                const platform::RayHit through = physics_->raycast(
                    {lo.x - span, aim.y, aim.z}, {1.0f, 0.0f, 0.0f}, span * 2.0f,
                    physics::LAYER_STATIC);
                // Контрольное плечо — НАД всем построенным и в стороне: на
                // высоте вершины луч в стороне цеплял рельеф за полкой.
                const platform::RayHit beside = physics_->raycast(
                    {lo.x - span, hi.y + 5.0f, hi.z + 50.0f}, {1.0f, 0.0f, 0.0f},
                    span * 2.0f, physics::LAYER_STATIC);
                std::fprintf(stderr,
                             "[постройка] коллайдер: сквозь дом %s, в стороне %s\n",
                             through.hit ? "упёрся" : "ПРОШЁЛ НАСКВОЗЬ",
                             beside.hit ? "тоже упёрся (прибор врёт)" : "прошёл");
            }
        }
    }
    std::fprintf(stderr, "[постройка] тело: потоков %zu, дверей %zu (готовых домов %zu)\n",
                 n_streams, n_doors, placed_houses_.size());
}

void App::load_scene_houses() {
    placed_houses_.clear();
    for (const world::ScenePlacedHouse& H : scene_doc_.houses) {
        std::ifstream in(H.file);
        if (!in.good()) {
            std::fprintf(stderr, "[постройка] [house] %s: файл не открылся — дом "
                                 "ПРОПУЩЕН\n",
                         H.file.c_str());
            continue;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        PlacedHouse ph;
        const world::HouseIoResult io = world::read_house(ss.str(), ph.graph);
        if (!io.ok) {
            std::fprintf(stderr, "[постройка] [house] %s:%d: %s — дом ПРОПУЩЕН\n",
                         H.file.c_str(), io.line, io.why.c_str());
            continue;
        }
        ph.pos = H.position;
        ph.yaw = H.yaw;
        placed_houses_.push_back(std::move(ph));
    }
    if (!placed_houses_.empty() || !scene_doc_.houses.empty()) {
        std::fprintf(stderr, "[постройка] готовых домов на карте: %zu из %zu\n",
                     placed_houses_.size(), scene_doc_.houses.size());
        upload_house_mesh();
    }
}

} // namespace dfn::app
