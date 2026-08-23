/*
Created: 19:08:2026 - 01:40:00
Last updated: 23:08:2026 - 18:11:50
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
- 20:08:2026 - 18:40:00: demo_swing — только выбранный элемент сессии; мох гуще (0.85, нижние 1.4 м).
- 20:08:2026 - 22:40:00: scene_index заполняется загрузкой; remap по штампу вместо переинициализации (квадратичный разгон).
- 21:08:2026 - 01:50:00: Слой грязи и мха v2: пятна по шуму (не градиент), налёт на горизонталях, тёмная грязь у земли — калибровка после трёх слепых приёмок.
- 21:08:2026 - 02:45:00: Мох v4: жёсткий высотный ноль (бонус горизонталей красил стену доверху), пятна мельче и контрастнее; грязь у земли полметра и темнее.
- 21:08:2026 - 14:35:00: Потребитель MeshPart.collider_only: часть уходит в
  коллайдер построек и НЕ уходит в рендер-потоки (невидимый пандус лестниц).
- 22:08:2026 - 14:50:00: Контрольная рука прибора проходимости называет точку,
  нормаль и entity того, во что упёрлась, вместо голого «прибор врёт»: на
  городской карте (131 дом + гора + деревья) пустой прямой на hi.y+5 может
  просто не существовать, и это другой диагноз, чем сломанный raycast.
  Названная точка тут же раскрыла корень: оба луча стартовали ЗА краем мира
  (span габарита города ~350 м) и первым делом били 200-метровую стену
  барьера world_edge_ на x=-4 — «сквозь дом упёрся» и «прибор врёт» были
  одним попаданием в барьер. Оба плеча зажаты внутрь world_bounds_xz().
- 22:08:2026 - 17:45:00: AO построек — печётся по отпечатку меша (кэш: 434 экземпляра из
  ~20-30 уникальных .dfh), ложится в альфу вершин ПОСЛЕ мха/грязи. Дверь
  дозы DFN_HOUSE_AO, 0 = прибитые 255 бит-в-бит.
- 22:08:2026 - 18:40:00: пространственная часть ключа потока (ячейка 32 м по первой вершине
  части): «материал|тон» сливал город в ~15 мешей размером с карту, и
  отсечение не могло отвергнуть ничего по построению.
- 23:08:2026 - 07:20:00: пятна построек для травы собираются при заливке (габарит по вершинам,
- 23:08:2026 - 18:11:50: самосветные части (glow) уходят отдельными потоками с DrawParams.emissive; дверь DFN_HOUSE_GLOW (0 = прежние освещённые потоки бит-в-бит).
  усадка 0.45 — габарит несёт свес, трава у стены снаружи законна).
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppDoors.h"
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
#include <unordered_map>
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

    // КЭШ НЕБЕСНОЙ ВИДИМОСТИ ПО ОТПЕЧАТКУ МЕША. 434 постройки города — это
    // ~20-30 уникальных .dfh: печь AO на каждый ЭКЗЕМПЛЯР значило бы платить
    // в двадцать раз больше за побайтово тот же ответ (build_house_mesh
    // детерминирован по построению — см. его заголовок). Отпечаток — счётчики
    // плюс FNV по первым вершинам; живёт от загрузки до загрузки карты.
    static std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> ao_cache;
    ao_cache.clear();
    const auto ao_of = [](const world::HouseMesh& built)
        -> const std::vector<std::uint8_t>& {
        std::uint64_t h = 1469598103934665603ull;
        const auto mix = [&h](std::uint64_t v) {
            h = (h ^ v) * 1099511628211ull;
        };
        mix(built.vertices.size());
        mix(built.indices.size());
        const size_t n = std::min<size_t>(built.vertices.size(), 64);
        for (size_t i = 0; i < n; ++i) {
            const auto& p = built.vertices[i].pos;
            mix(static_cast<std::uint64_t>(std::llround(p.x * 512.0f)));
            mix(static_cast<std::uint64_t>(std::llround(p.y * 512.0f)));
            mix(static_cast<std::uint64_t>(std::llround(p.z * 512.0f)));
        }
        auto it = ao_cache.find(h);
        if (it == ao_cache.end()) {
            it = ao_cache.emplace(h, world::bake_house_sky_visibility(built)).first;
        }
        return it->second;
    };

    // ПЯТНА ПОСТРОЕК ДЛЯ ТРАВЫ («былинки сквозь пол», владелец 23.08): трава
    // сеялась, не зная о домах — пад кладёт травяную землю ровно под полом.
    // Собираются здесь, где габарит уже считается; усадка 0.45 внутрь, чтобы
    // не съесть законную траву у стены снаружи (габарит несёт свес кровли).
    std::vector<glm::vec4> ground_exclusions;

    const auto append_graph = [&](const world::HouseGraph& graph,
                                  const auto& to_world) {
        const world::HouseMesh built = world::build_house_mesh(graph);
        const std::vector<std::uint8_t>& sky_vis = ao_of(built);
        const glm::vec3 zero = to_world(glm::vec3{0.0f});
        {
            glm::vec2 lo{1e9f};
            glm::vec2 hi{-1e9f};
            for (const world::HouseVertex& hv : built.vertices) {
                const glm::vec3 wp = to_world(hv.pos);
                lo = glm::min(lo, {wp.x, wp.z});
                hi = glm::max(hi, {wp.x, wp.z});
            }
            const glm::vec2 half = (hi - lo) * 0.5f - glm::vec2{0.45f};
            if (half.x > 0.5f && half.y > 0.5f) {
                const glm::vec2 c = (lo + hi) * 0.5f;
                ground_exclusions.push_back({c.x, c.y, half.x, half.y});
            }
        }
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
        bool part_organic = false;
        const auto to_world_vertex = [&](const world::HouseVertex& v,
                                         std::uint8_t vis) {
            platform::Vertex pv{};
            pv.position = to_world(v.pos);
            pv.normal = to_world(v.normal) - zero;
            pv.uv = v.uv;
            pv.color_rgba = part_color;
            // ЗАПЕЧЁННАЯ НЕБЕСНАЯ ВИДИМОСТЬ — в альфу вершины: канал был
            // прибит к 255, dfn_surface_light уже умножает на него ambient
            // (sky_vis). Ни одной правки шейдера (22.08, «постройки плоские»:
            // откос ворот был той же яркости, что фасад). DFN_HOUSE_AO —
            // доза: 0 возвращает прибитые 255 бит-в-бит, дробная ослабляет
            // затемнение пропорционально.
            static const float ao_dose = [] {
                const char* e = door_value("DFN_HOUSE_AO");
                return (e != nullptr && *e != '\0')
                           ? std::strtof(e, nullptr)
                           : 1.0f;
            }();
            const std::uint32_t a = static_cast<std::uint32_t>(std::lround(
                255.0f - std::clamp(ao_dose, 0.0f, 1.0f)
                             * (255.0f - static_cast<float>(vis))));
            // СЛОЙ ГРЯЗИ И МХА (калибровка 21.08: равномерный градиент тонул
            // в свету — три приёмки кадров его не увидели). Три правила из
            // EXTERIOR_CATALOG.md:
            //  - мох ПЯТНАМИ по шуму, гуще к земле (не ровной заливкой);
            //  - налёт на ГОРИЗОНТАЛЯХ (верхние грани держат воду);
            //  - тёмная грязь у самой земли — всегда, сильнее с износом.
            {
                const auto hash01 = [](float x, float z) {
                    const std::int32_t ix = static_cast<std::int32_t>(std::floor(x * 2.7f));
                    const std::int32_t iz = static_cast<std::int32_t>(std::floor(z * 2.7f));
                    std::uint32_t h = static_cast<std::uint32_t>(ix * 73856093)
                                    ^ static_cast<std::uint32_t>(iz * 19349663);
                    h = (h ^ (h >> 13)) * 0x85ebca6bu;
                    return static_cast<float>((h >> 16) & 0xFFu) / 255.0f;
                };
                const float low =
                    std::clamp(1.0f - (v.pos.y - gmin_y) / 1.3f, 0.0f, 1.0f);
                const float spots = hash01(pv.position.x * 2.0f + pv.position.y,
                                           pv.position.z * 2.0f - pv.position.y);
                // Мох: пятно живёт там, где шум перевалил порог. ЖЁСТКИЙ
                // высотный ноль (калибровка №2: бонус горизонталей у верхней
                // грани КАЖДОГО кирпича закрашивал стену доверху — ковёр
                // вместо пятен): выше 1.3 м мха нет совсем; горизонталь
                // усиливает пятно, но не рождает его.
                float moss = 0.0f;
                if (part_wear > 0.0f && part_organic && low > 0.01f) {
                    const float need = 0.92f - 0.45f * low;
                    if (spots > need) {
                        moss = std::min(1.0f, part_wear * (spots - need) * 8.0f);
                        if (pv.normal.y > 0.55f) {
                            moss = std::min(1.0f, moss * 1.6f);
                        }
                    }
                }
                // Грязь у земли: полметра, сильнее и темнее (была «самая
                // яркая полоса кадра» — светлый тон завалинки съедал эффект).
                const float dirt =
                    part_organic
                        ? std::clamp(1.0f - (v.pos.y - gmin_y) / 0.5f, 0.0f, 1.0f)
                              * (0.55f + 0.4f * part_wear)
                        : 0.0f;
                if (moss > 0.01f || dirt > 0.01f) {
                    const auto ch = [&](int shift, float moss_t, float dirt_mul) {
                        float c = static_cast<float>((part_color >> shift) & 0xFFu);
                        c = c * (1.0f - moss) + moss_t * 255.0f * moss;
                        c *= 1.0f - dirt * (1.0f - dirt_mul);
                        return static_cast<std::uint32_t>(
                                   std::lround(std::clamp(c, 0.0f, 255.0f)))
                            << shift;
                    };
                    // BGR; мох тёмно-зелёный, грязь буро-тёмная.
                    pv.color_rgba = 0xFF000000u | ch(16, 0.30f, 0.52f)
                                  | ch(8, 0.52f, 0.47f) | ch(0, 0.30f, 0.40f);
                }
            }
            // Альфа — ПОСЛЕ всех перекрасок: и чистый part_color, и слой
            // мха/грязи несут запечённую видимость, а не свои 0xFF.
            pv.color_rgba = (pv.color_rgba & 0x00FFFFFFu) | (a << 24);
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
        // Штамп вместо переинициализации: remap.assign на каждую часть давал
        // квадратичный разгон (аудит #3, находка 7) — дом с паркетом и кровлей
        // это десятки тысяч вершин на сотни частей при каждой правке.
        std::vector<std::uint32_t> remap(built.vertices.size(), 0xFFFFFFFFu);
        std::vector<std::uint32_t> remap_stamp(built.vertices.size(), 0xFFFFFFFFu);
        std::uint32_t part_no = 0;
        for (const world::MeshPart& part : built.parts) {
            ++part_no;
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
            // ОРГАНИКА — ТОЛЬКО НА МЕЛКОЙ ГРАНУЛЯЦИИ (калибровка глаз 21.08:
            // на пластине стены угловые вершины размазывали мох ЗАЛИВКОЙ по
            // всей грани). Кирпич, венец, дранка — да; пластина — нет.
            part_organic = false;
            if (part_wear > 0.0f) {
                glm::vec3 plo{1e9f};
                glm::vec3 phi{-1e9f};
                for (std::uint32_t i = 0; i < part.index_count; ++i) {
                    const auto& q = built.vertices[built.indices[part.index_begin + i]].pos;
                    plo = glm::min(plo, q);
                    phi = glm::max(phi, q);
                }
                part_organic = glm::length(phi - plo) < 1.4f;
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
            // ЧИСТО ФИЗИЧЕСКАЯ ЧАСТЬ (пандус лестницы): в коллайдер ниже,
            // в картинку — нет. Симметрия mb.collider=false (21.08).
            if (part.collider_only) {
                for (std::uint32_t i = 0; i < part.index_count; ++i) {
                    const std::uint32_t vi = built.indices[part.index_begin + i];
                    collider_indices.push_back(
                        static_cast<std::uint32_t>(house_positions_.size()));
                    house_positions_.push_back(to_world(built.vertices[vi].pos));
                }
                continue;
            }
            render::MeshData* into = nullptr;
            if (is_door) {
                doors.emplace_back();
                doors.back().surface = surface;
                doors.back().tone = tone;
                // Качается только дверь, ВЫБРАННАЯ в сессии редактирования —
                // показать петлю; остальные (и все двери готовых домов)
                // стоят закрытыми.
                doors.back().demo_swing = (&graph == &house_.graph())
                                       && house_.selected_element() == e->id;
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
                // ПРОСТРАНСТВЕННАЯ ЧАСТЬ КЛЮЧА (22.08). Ключ «материал|тон»
                // сливал все 462 постройки города в ~15 мешей размером с
                // карту, и отсечение по пирамиде не могло отвергнуть ничего
                // по построению: профиль 59905 кадров показал 1.91 млн
                // треугольников в кадре с разбросом 1% независимо от
                // направления взгляда. Ячейка 32 м — компромисс: мельче —
                // растут вызовы (каждая ячейка ещё и кастер теней), крупнее
                // — отсечению снова нечего делать. Ячейка берётся по ПЕРВОЙ
                // вершине части: часть моста между ячейками уйдёт в одну из
                // них целиком, габарит потока это честно учтёт.
                const glm::vec3 anchor =
                    to_world(built.vertices[built.indices[part.index_begin]].pos);
                constexpr float STREAM_CELL_M = 32.0f;
                const auto cell = [](float v) {
                    return static_cast<std::uint64_t>(std::clamp(
                        static_cast<int>(std::floor(v / STREAM_CELL_M)) + 8, 0,
                        63));
                };
                const std::uint64_t cell_key =
                    (cell(anchor.x) << 6) | cell(anchor.z);
                // САМОСВЕТНАЯ ЧАСТЬ — СВОЙ ПОТОК (24.08, владелец: «свет
                // должен гореть всегда на любом удалении»): пламя и стекло
                // фонаря (glow=1 рецепта) рисуются без освещения и горят
                // независимо от бюджета точечных светов. Дверь-доза
                // DFN_HOUSE_GLOW: 0 — флаг игнорируется, части живут в
                // прежних освещённых потоках бит-в-бит.
                static const bool glow_on = [] {
                    const char* e = door_value("DFN_HOUSE_GLOW");
                    return (e == nullptr || *e == '\0')
                        || std::strtof(e, nullptr) > 0.5f;
                }();
                const bool part_glow = glow_on && part.emissive;
                auto& st = streams[(cell_key << 16)
                                   | (static_cast<std::uint64_t>(surface) << 8)
                                   | (part_glow ? (1ull << 15) : 0ull) | tone];
                st.surface = surface;
                st.tone = tone;
                st.emissive = part_glow;
                into = &st.mesh;
            }
            for (std::uint32_t i = 0; i < part.index_count; ++i) {
                const std::uint32_t vi = built.indices[part.index_begin + i];
                if (remap_stamp[vi] != part_no) {
                    remap_stamp[vi] = part_no;
                    remap[vi] = static_cast<std::uint32_t>(into->vertices.size());
                    into->vertices.push_back(
                        to_world_vertex(built.vertices[vi], sky_vis[vi]));
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
    render_system_.set_ground_exclusions(std::move(ground_exclusions));

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
                // ОБА ЛУЧА ЖИВУТ ВНУТРИ КОРОБКИ МИРА. На карте города габарит
                // 131 дома даёт span ~350 м, и «lo.x - span» оказывался за
                // краем мира — а край закрыт четырьмя невидимыми стенами
                // высотой 200 м (AppWorld, world_edge_). Оба плеча прибора
                // первым делом упирались в западную стену границы на x=-4 и
                // дальше не летели: «сквозь дом упёрся» и «в стороне тоже
                // упёрся (прибор врёт)» были ОДНИМ И ТЕМ ЖЕ попаданием в
                // барьер (найдено 22.08 после того, как рука назвала точку).
                const glm::vec4 wbz = chunks_.world_bounds_xz();
                const float ray_x0 = std::max(lo.x - span, wbz.x + 1.0f);
                const float ray_x1 = std::min(lo.x + span, wbz.z - 1.0f);
                const float ray_len = std::max(ray_x1 - ray_x0, 1.0f);
                // Луч целится СКВОЗЬ ПЕРВУЮ ВЕРШИНУ коллайдера, а не в
                // середину габарита: с несколькими домами на карте середина
                // попадает в коридор между ними, и прибор кричал бы про дыру,
                // которой нет (найдено дымом карты «Стройка», 20.08).
                const glm::vec3 aim = house_positions_.front();
                const platform::RayHit through = physics_->raycast(
                    {ray_x0, aim.y, aim.z}, {1.0f, 0.0f, 0.0f}, ray_len,
                    physics::LAYER_STATIC);
                // Контрольное плечо — НАД всем построенным и в стороне: на
                // высоте вершины луч в стороне цеплял рельеф за полкой. Отход
                // в сторону тоже зажат в мир — за краем стоит барьер.
                const glm::vec3 beside_org{ray_x0, hi.y + 5.0f,
                                           std::min(hi.z + 50.0f, wbz.w - 1.0f)};
                const platform::RayHit beside = physics_->raycast(
                    beside_org, {1.0f, 0.0f, 0.0f}, ray_len,
                    physics::LAYER_STATIC);
                // Контрольная рука НАЗЫВАЕТ, во что упёрлась: голое «прибор
                // врёт» на карте города (22.08) не отвечало, врёт ли рука
                // из-за прибора или из-за того, что на городской карте с
                // горой и деревьями попросту не существует пустой прямой на
                // высоте hi.y+5 — а это разные диагнозы с разной ценой.
                if (beside.hit) {
                    std::fprintf(stderr,
                                 "[постройка] коллайдер: сквозь дом %s; контроль в "
                                 "стороне УПЁРСЯ на (%.1f, %.1f, %.1f) — луч из "
                                 "(%.1f, %.1f, %.1f), нормаль (%.2f, %.2f, %.2f), "
                                 "entity %llu — рука не чиста, замер сквозь дом "
                                 "этой строкой НЕ доказан\n",
                                 through.hit ? "упёрся" : "ПРОШЁЛ НАСКВОЗЬ",
                                 static_cast<double>(beside.position.x),
                                 static_cast<double>(beside.position.y),
                                 static_cast<double>(beside.position.z),
                                 static_cast<double>(beside_org.x),
                                 static_cast<double>(beside_org.y),
                                 static_cast<double>(beside_org.z),
                                 static_cast<double>(beside.normal.x),
                                 static_cast<double>(beside.normal.y),
                                 static_cast<double>(beside.normal.z),
                                 static_cast<unsigned long long>(beside.user_data));
                } else {
                    std::fprintf(stderr,
                                 "[постройка] коллайдер: сквозь дом %s, в стороне "
                                 "прошёл\n",
                                 through.hit ? "упёрся" : "ПРОШЁЛ НАСКВОЗЬ");
                }
            }
        }
    }
    std::fprintf(stderr, "[постройка] тело: потоков %zu, дверей %zu (готовых домов %zu)\n",
                 n_streams, n_doors, placed_houses_.size());
}

void App::load_scene_houses() {
    placed_houses_.clear();
    for (std::size_t hi = 0; hi < scene_doc_.houses.size(); ++hi) {
        const world::ScenePlacedHouse& H = scene_doc_.houses[hi];
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
        ph.scene_index = hi;
        placed_houses_.push_back(std::move(ph));
    }
    if (!placed_houses_.empty() || !scene_doc_.houses.empty()) {
        std::fprintf(stderr, "[постройка] готовых домов на карте: %zu из %zu\n",
                     placed_houses_.size(), scene_doc_.houses.size());
        upload_house_mesh();
    }
}

} // namespace dfn::app
