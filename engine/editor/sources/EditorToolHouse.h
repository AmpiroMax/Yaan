/*
Created: 18:08:2026 - 18:02:11
Last updated: 20:08:2026 - 01:47:30
Module: engine/editor
File: engine/editor/sources/EditorToolHouse.h

Responsibility:
- ТРИ ИНСТРУМЕНТА ПОСТРОЙКИ поверх уже написанной модели (HouseGraph) и её
  геометрии (HouseMesh): вершины, прямая («магнитный конструктор») и
  поверхность. Плюс ОБЩЕЕ состояние на троих — сам граф, выбор и история
  правок, — потому что три инструмента правят ОДНУ постройку и второй копии
  графа у них быть не может.

Key items:
- HouseSession: граф + выбор + история + перевод «мир <-> постройка». ОДНА
  ДВЕРЬ КО ВСЕМ МУТАЦИЯМ (mutate), и снимок для отмены пишется в ней, а не в
  каждом инструменте.
- HouseVertexTool: щелчок по земле / по оси прямой, протаскивание, удаление с
  ОТКАЗОМ И СПИСКОМ ДЕРЖАТЕЛЕЙ.
- HouseLineTool: одна механика с двумя исходами (отпустил на вершине —
  соединил; отпустил в пустоте — прямая с длиной и углами) и зажим длины до
  ближайшего якоря сверху/снизу.
- HouseSurfaceTool: обход по вершинам, замыкание на первой, СТРЕЛКА НОРМАЛИ ДО
  подтверждения.
- build_house_wire / append_ball / append_plumb: проволочная картинка постройки
  отрезками — ШАРИК на вершине и ПУНКТИРНЫЙ ОТВЕС от неё до земли.

WHY THIS EXISTS (пользователь, 18.08.2026, дословно): «вершины я хочу уметь
также в воздухе ставить, просто в рандомном месте... она должна рисоваться
ШАРИКОМ... надо от вершины вниз рисовать ПУНКТИРНУЮ ЛИНИЮ, которая будет
упираться в землю, и я буду видеть, над какой точкой ставлю свой объект».

ОТВЕС — ПРИБОР, А НЕ УКРАШЕНИЕ, и это единственная причина, по которой он
описан в интерфейсе, а не оставлен на усмотрение рисующего: на экране ВЫСОТА И
ДАЛЬНОСТЬ ВЫГЛЯДЯТ ОДИНАКОВО. Шарик в воздухе без отвеса не отвечает на вопрос
«над какой точкой земли он висит» — ни при каком угле камеры.

ПОЧЕМУ РЕШЕНИЯ ЛЕЖАТ ЗДЕСЬ, А РИСОВАНИЕ ПАНЕЛИ — В EditorToolHouseUi.cpp.
Правило 3: этот файл не знает ни про ImGui, ни про окно, поэтому цель
app_editor_house линкует ТОЛЬКО EditorToolHouse.cpp и спрашивает у инструментов
то, чего не покажет ни один кадр: куда смотрит нормаль ДО подтверждения, кто
держит вершину, которую не дали удалить, и на сколько зажалась длина.

Dependencies:
- Uses: EditorTool.h, EditorHistory.h, engine/world (HouseGraph, HouseFile,
  HouseMesh), glm.
- Used by: engine/app (создаёт сессию и три инструмента), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЛЮБАЯ ПРАВКА ГРАФА ИДЁТ ЧЕРЕЗ HouseSession::mutate. Второе место, которое
  правит граф мимо неё, — это второе место, обязанное помнить про снимок для
  отмены, а забыть его можно ровно один раз, и узнается об этом тогда, когда
  отмена молча пропустит шаг.
- НИ ОДНОГО ПРАВИЛА ПРО ГЕОМЕТРИЮ ЗДЕСЬ. Нормаль контура берётся у
  fit_contour_plane, направление цепочки — тем же выражением, что у
  world::surface_normal. Вторая формула нормали означала бы стрелку, которая
  показывает не туда, куда ляжет текстура.
*/
/*
UPD:
- 18:08:2026 - 18:02:11: Создан — три инструмента постройки, общее состояние с историей и
  проволочная картинка с шариками и отвесами (заказ 18.08).
- 18:08:2026 - 18:40:24: НОМЕР ЖИЗНИ ГРАФА и опознание якоря ПО МЕСТУ — по предупреждению зоны
  core про имена после отмены. Проверил её слова числом и поправил: имена
  чтение НЕ выдаёт заново, первый же круговой прогон сходится побайтово. Но
  опасность настоящая, просто на шаг дальше — СЧЁТЧИК имён откатывается вместе
  с графом: три вершины, снимок, четвёртая зовётся v4 в точке 99; отмена;
  следующая созданная снова зовётся v4, но стоит в -77. Рука, помнящая v4,
  показывает на другую точку и молчит. Отсюда revision() и stale(): всё, что
  инструмент держит по именам, бросается при откате, а выбор восстанавливается
  по координате. Контрфакт: stale() всегда false — 4 покрасневших утверждения.
- 18:08:2026 - 18:56:09: ДОВОД ПЕРЕЕХАЛ ИЗ ПЕРЕПИСКИ В КОД. Зона core починила читателя (146f484):
  имена переживают круговой прогон, и мой прежний комментарий у revision()
  («чтение выдаёт имена заново») стал неправдой — то есть выглядел бы поводом
  снять защиту. Написано прямо: починка НЕ закрывает ловушку, потому что
  счётчик имён откатывается вместе с графом, и следующая вершина после отмены
  снова получает имя v4 в другой точке (99 м против -77 м). Без этого абзаца
  следующий читатель снял бы stale() с чистой совестью.
- 18:08:2026 - 18:58:40: Фиксация оси у сессии: HouseSession::Axis (земля/вертикаль), toggle_axis, и прямая объявляет aims_without_ground — пока ось включена и рука на якоре, земля ей не нужна.
- 18:08:2026 - 19:14:22: Поверхность: черновик — обход из двух и больше якорей.
- 18:08:2026 - 19:44:10: Шарик якоря вдвое больше (0.10 -> 0.20 м); pick_vertex_ray/pick_edge_ray — цель ищется ЛУЧОМ, а не точкой на земле; у якоря pull_m вместо высоты числом, призрак Ghost с прилипанием к оси и отвесом.
- 18:08:2026 - 20:26:30: pick_element_ray (выбор любого элемента по контуру) и delete_selection (одно удаление на стену, прямую и якорь); у прямой поле snap_.
- 18:08:2026 - 21:12:40: version() — версия геометрии, растёт в mutate; revision() остался про имена. Их путаница стоила незалитого тела.
- 18:08:2026 - 22:20:15: AxisLock из трёх видов (свободно/вертикаль/вдоль прямой), cycle_axis, axis_dir, axis_label; версия спрашивается у графа.
- 18:08:2026 - 23:20:00: Сетка мира в сессии (вкл, шаг, snap_to_grid); у прямой есть черновик и отмена; три инструмента ведут взятое без проверки дальности.
- 19:08:2026 - 00:12:30: HOUSE_GHOST_COLOR (зелёный): жёлтый остаётся за тем, что уже стоит и выбрано.
- 19:08:2026 - 00:48:20: nudging() — пока правят стрелками, призрак под прицелом не рисуется, помечается двигаемый якорь.
- 19:08:2026 - 01:20:45: Объявлена draw_house_selection_panel.
- 19:08:2026 - 02:05:30: Сессия и числа руки выехали в HouseSession.h (долг 3): здесь остались три инструмента.
- 19:08:2026 - 23:58:20: Заготовка инструментов: материал/тон/форма у прямой, материал/тон/обшивка/окна у поверхности; доступ по ссылке для панели и рукава.
- 20:08:2026 - 00:58:40: Заготовка поверхности: вид кладки (fill).
- 20:08:2026 - 01:47:30: Заготовка: spin (поворот сечения), doors; форма «лестница».
*/

#pragma once

#include "engine/editor/sources/HouseSession.h"
#include "engine/editor/sources/EditorHistory.h"
#include "engine/editor/sources/EditorTool.h"
#include "engine/world/sources/HouseGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace dfn::app {

// ---------------------------------------------------------------------------

/// Щелчок по земле ставит заземлённую вершину; с поднятой высотой — вершину В
/// ВОЗДУХЕ (Anchoring::Free) с пунктирным отвесом до земли. Щелчок по оси
/// прямой сажает вершину НА ОСЬ. Протаскивание двигает якорь, и вся привязанная
/// геометрия едет за ним сама — не обходом списка, а потому что второй копии
/// геометрии нет.
class HouseVertexTool final : public IEditorTool {
public:
    explicit HouseVertexTool(HouseSession& session) : session_(&session) {}

    [[nodiscard]] ToolIdentity identity() const override;
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) override;
    void on_release(ToolWorld& world) override;
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    void draw_settings() override;
    [[nodiscard]] bool stroke_needs_reach() const override { return false; }
    [[nodiscard]] float max_reach_m() const override { return 60.0f; }
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const override;
    void on_deselected(ToolWorld& world) override;

    /// Крючки, нужные ВНЕ щелчка: земля для отвеса и для высоты новой вершины.
    void set_world(ToolWorld* world) { world_ = world; }

    /// ГРАФ ПЕРЕЧИТАЛИ ПОД РУКОЙ (отмена/повтор), а рука ещё держит старые
    /// имена. Спрашивается ДО показа (preview/status его читают и не рисуют
    /// черновик), сбрасывается в on_press.
    [[nodiscard]] bool stale() const {
        return session_ != nullptr && session_->revision() != seen_revision_;
    }

    /// НАСКОЛЬКО ШАРИК ПОДТЯНУТ К ГЛАЗУ вдоль луча прицела, метры. Ноль —
    /// вершина садится туда, где луч встретил землю (OnGround); больше нуля —
    /// шарик едет по лучу навстречу человеку и, поскольку луч смотрит ВНИЗ,
    /// поднимается над травой (Free). Тогда у него появляется отвес.
    ///
    /// ОДНО ЧИСЛО НА ОБА СЛУЧАЯ И ДВА ОРГАНА УПРАВЛЕНИЯ НА ОДНО ЧИСЛО: колесо
    /// мыши и ползунок в панели зовут этот же метод. Так же устроена общая
    /// дальность (колесо + ползунок под шестерёнкой) — и по той же причине: два
    /// СОСТОЯНИЯ вместо одного разъезжаются молча, два органа — нет.
    ///
    /// ПОЧЕМУ НЕ «ВЫСОТА НАД ЗЕМЛЁЙ», КАК БЫЛО. Высота числом требует заранее
    /// знать, на сколько поднимать, и не двигает шарик к себе вовсе — а
    /// пользователь просил именно «приближать сферу к себе или отдалять».
    /// Подтягивание вдоль луча делает и то и другое одним движением.
    [[nodiscard]] float pull_m() const { return pull_m_; }
    void set_pull_m(float metres) {
        // Зажим здесь, а не у каждого органа управления (правило 32): и колесо,
        // и ползунок зовут этот метод, и предел обязан быть один.
        pull_m_ = std::clamp(metres, 0.0f, HOUSE_PULL_MAX_M);
    }
    [[nodiscard]] bool wants_wheel() const override { return true; }
    void on_wheel(float ticks) override;

    /// ГДЕ БУДЕТ ВЕРШИНА, если щёлкнуть сейчас, и К ЧЕМУ ОНА ПРИЛИПЛА.
    /// Спрашивают и показ, и щелчок — одним методом, чтобы призрак не мог
    /// разойтись с тем, что произойдёт.
    struct Ghost {
        glm::vec3 point{0.0f};
        float ground_y = 0.0f;
        bool air = false;               ///< висит над землёй, нужен отвес
        HouseEdgeHit on_edge;           ///< прилипла к оси прямой (или нет)
        world::VertexId over = world::NO_VERTEX; ///< под прицелом чужой якорь
    };
    [[nodiscard]] Ghost ghost(const ToolAim& aim) const;

    /// УДАЛИТЬ ВЫБРАННУЮ. false — отказ, и его причина вместе со СПИСКОМ
    /// ДЕРЖАТЕЛЕЙ лежит в refusal(): «удалять бревно нельзя давать, пока к нему
    /// что-то привязано» — но человеку надо сказать, ЧТО именно привязано.
    bool delete_selected();
    [[nodiscard]] const std::string& refusal() const { return refusal_; }
    /// Какая вершина сейчас тащится (NO_VERTEX — никакая).
    [[nodiscard]] world::VertexId dragging() const { return dragging_; }

private:
    [[nodiscard]] float ground_at(glm::vec2 xz, float fallback_y) const;

    HouseSession* session_ = nullptr;
    ToolWorld* world_ = nullptr;
    float pull_m_ = 0.0f;
    world::VertexId dragging_ = world::NO_VERTEX;
    /// Насколько выбранная вершина стояла НАД землёй в момент захвата. Тащим по
    /// XZ, а высоту держим над рельефом: иначе якорь, сидевший в двух метрах
    /// над склоном, при сдвиге на метр молча уходил бы в грунт.
    float drag_lift_m_ = 0.0f;
    std::string drag_before_;
    std::string refusal_;
    mutable HouseWire wire_;
    mutable std::vector<glm::vec3> ghost_pairs_;
    /// Номер жизни графа, при котором рука набрала то, что держит. Разошёлся —
    /// значит между двумя щелчками произошла отмена, и всё, что рука помнит по
    /// именам, надо забыть, а не разыгрывать заново на чужих вершинах.
    std::uint32_t seen_revision_ = 0;
};

// ---------------------------------------------------------------------------
// 8 — ПРЯМАЯ («магнитный конструктор»)
// ---------------------------------------------------------------------------

/// Зажим длины: до ближайшего якоря ВПЕРЁД по лучу или НАЗАД к началу.
/// «Механика клипа длины прямой до ближайшего сверху / снизу на выбор якоря»
/// (пользователь, 18.08).
enum class HouseClamp : std::uint8_t {
    None = 0, ///< длина ровно из жеста
    Above,    ///< до ближайшего якоря ДАЛЬШЕ текущего конца
    Below,    ///< до ближайшего якоря БЛИЖЕ текущего конца
};

struct HouseClampHit {
    bool found = false;
    float length_m = 0.0f;
    world::VertexId at = world::NO_VERTEX;
};

/// §5.5 замысла: якоря, чья проекция на луч лежит в пределах допуска по
/// расстоянию ДО ОСИ, отдельно вперёд и назад от текущего конца. Перебор, а не
/// индекс: вершин в постройке сотни, и индекс здесь стоил бы дороже ответа.
[[nodiscard]] HouseClampHit house_clamp_length(const HouseSession& s, world::VertexId from,
                                               glm::vec3 dir_world, float raw_length_m,
                                               float axis_tol_m, HouseClamp mode);

/// Нажал в якорь и потянул. Отпустил НА якоре — якоря соединились; отпустил в
/// пустоте — прямая с длиной и углами из жеста, которые дальше правятся
/// числами. ОДНА МЕХАНИКА, ДВА ИСХОДА: два инструмента здесь означали бы два
/// разных способа провести одно и то же бревно.
class HouseLineTool final : public IEditorTool {
public:
    explicit HouseLineTool(HouseSession& session) : session_(&session) {}

    [[nodiscard]] ToolIdentity identity() const override;
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) override;
    void on_release(ToolWorld& world) override;
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    void draw_settings() override;
    /// Пока ось зафиксирована и рука уже на якоре — земля не нужна.
    [[nodiscard]] bool aims_without_ground() const override {
        return session_ != nullptr && from_ != world::NO_VERTEX
            && !session_->axis().free();
    }
    /// Набранное у прямой — это якорь, от которого её тянут: ESC обязан его
    /// отпустить, иначе следующая прямая молча начнётся от старого.
    [[nodiscard]] bool has_draft() const override { return from_ != world::NO_VERTEX; }
    void on_cancel(ToolWorld& world) override;
    [[nodiscard]] bool stroke_needs_reach() const override { return false; }
    [[nodiscard]] float max_reach_m() const override { return 60.0f; }
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const override;
    void on_deselected(ToolWorld& world) override;

    void set_world(ToolWorld* world) { world_ = world; }

    /// ГРАФ ПЕРЕЧИТАЛИ ПОД РУКОЙ (отмена/повтор), а рука ещё держит старые
    /// имена. Спрашивается ДО показа (preview/status его читают и не рисуют
    /// черновик), сбрасывается в on_press.
    [[nodiscard]] bool stale() const {
        return session_ != nullptr && session_->revision() != seen_revision_;
    }

    /// Якорь, из которого тянут (NO_VERTEX — рука пуста).
    [[nodiscard]] world::VertexId anchor() const { return from_; }

    /// Что получилось последним — для проверок и для панели.
    [[nodiscard]] world::ElementId last_element() const { return last_; }
    [[nodiscard]] HouseClamp& clamp_mode() { return clamp_; }
    [[nodiscard]] HouseClamp clamp_mode() const { return clamp_; }
    [[nodiscard]] float& radius_m() { return radius_m_; }
    [[nodiscard]] std::string& style() { return style_; }
    [[nodiscard]] const std::string& refusal() const { return refusal_; }
    /// Конец призрака с уже применённым зажимом — то, что увидит глаз, и то,
    /// что станет прямой. Одно выражение на обоих, иначе призрак соврёт.
    [[nodiscard]] glm::vec3 ghost_end() const;
    /// Куда зажалось в этом кадре (found == false — зажим не сработал).
    [[nodiscard]] const HouseClampHit& clamp_hit() const { return clamp_hit_; }

private:
    void update_end(const ToolAim& aim);

    HouseSession* session_ = nullptr;
    ToolWorld* world_ = nullptr;
    world::VertexId from_ = world::NO_VERTEX;
    world::ElementId last_ = world::NO_ELEMENT;
    glm::vec3 raw_end_{0.0f};
    /// К какому якорю прилип конец (NO_VERTEX — ни к какому). Находится лучом
    /// во время протаскивания и используется на отпускании: два вопроса об
    /// одном и том же дали бы два разных ответа.
    world::VertexId snap_ = world::NO_VERTEX;
    HouseClampHit clamp_hit_;
    HouseClamp clamp_ = HouseClamp::None;
    float radius_m_ = 0.12f;
    /// ЗАГОТОВКА: материал, тон и форма СЛЕДУЮЩЕЙ прямой. Пишутся в params
    /// созданного элемента; выбранный элемент правится своим блоком панели.
    // Доступ рукаву и панели — по ссылке, как у radius_m_.
public:
    [[nodiscard]] int& draft_mat() { return mat_; }
    [[nodiscard]] int& draft_tone() { return tone_; }
    [[nodiscard]] int& draft_form() { return form_; }

private:
    int mat_ = 0;   ///< PartSurface ordinal (0 = тёсаный брус)
    int tone_ = 1;  ///< PartTone ordinal (1 = средний)
    int form_ = 0;  ///< 0 круг, 1 квадрат, 2 шести-, 3 восьмигранник, 4 лестница
    float spin_deg_ = 0.0f; ///< поворот сечения вокруг оси (angle_z)
    std::string style_ = "oak";
    std::string refusal_;
    mutable HouseWire wire_;
    mutable std::vector<glm::vec3> ghost_;
    /// Номер жизни графа, при котором рука набрала то, что держит. Разошёлся —
    /// значит между двумя щелчками произошла отмена, и всё, что рука помнит по
    /// именам, надо забыть, а не разыгрывать заново на чужих вершинах.
    std::uint32_t seen_revision_ = 0;
};

// ---------------------------------------------------------------------------
// 9 — ПОВЕРХНОСТЬ
// ---------------------------------------------------------------------------

/// Щелчки по вершинам В ПОРЯДКЕ ОБХОДА, затем подтверждение. Замкнул на первой
/// вершине — контур (пол, потолок, скат); оставил цепочкой — стена с высотой.
///
/// ПОРЯДОК ЗАДАЁТ НОРМАЛЬ, поэтому стрелка нормали рисуется и называется ДО
/// подтверждения. Без неё дизайнер узнаёт о том, что обошёл контур не в ту
/// сторону, по текстуре — то есть после сборки, отделки и постановки дома.
class HouseSurfaceTool final : public IEditorTool {
public:
    explicit HouseSurfaceTool(HouseSession& session) : session_(&session) {}

    [[nodiscard]] ToolIdentity identity() const override;
    void on_press(const ToolAim& aim, ToolWorld& world) override;
    void on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) override;
    void on_release(ToolWorld& world) override;
    [[nodiscard]] ToolPreview preview(const ToolAim& aim) const override;
    void draw_settings() override;
    [[nodiscard]] bool stroke_needs_reach() const override { return false; }
    [[nodiscard]] float max_reach_m() const override { return 60.0f; }
    [[nodiscard]] ToolStatus status(const ToolAim& aim) const override;
    void on_deselected(ToolWorld& world) override;
    /// ENTER — СОЗДАТЬ. Тот же глагол, что у кнопки в панели, и он один: два
    /// пути к созданию поверхности означали бы два места, помнящих про
    /// closed и про числа.
    /// Обход из двух и больше якорей — уже поверхность, которую можно
    /// подтвердить. Один якорь — ещё не черновик: это просто выбор.
    [[nodiscard]] bool has_draft() const override { return refs_.size() >= 2; }
    void on_confirm(ToolWorld& world) override;
    void on_cancel(ToolWorld& world) override;

    void set_world(ToolWorld* world) { world_ = world; }

    /// ГРАФ ПЕРЕЧИТАЛИ ПОД РУКОЙ (отмена/повтор), а рука ещё держит старые
    /// имена. Спрашивается ДО показа (preview/status его читают и не рисуют
    /// черновик), сбрасывается в on_press.
    [[nodiscard]] bool stale() const {
        return session_ != nullptr && session_->revision() != seen_revision_;
    }

    [[nodiscard]] const std::vector<world::VertexId>& refs() const { return refs_; }
    [[nodiscard]] bool closed() const { return closed_; }
    /// ЗАМЫСЕЛ «ЭТО КОНТУР», а не факт замыкания. Его ставит и щелчок по первому
    /// якорю, и кнопка «замкнуть и создать» — но читает его СТРЕЛКА НОРМАЛИ, и
    /// потому он обязан быть виден ДО подтверждения: у обхода из четырёх точек
    /// два будущих (пол и стена), и лицо у них смотрит в РАЗНЫЕ стороны.
    /// Без этого переключателя стрелка показывала бы одно из двух наугад.
    [[nodiscard]] bool& closing() { return closed_; }
    [[nodiscard]] bool& flipped() { return flipped_; }
    [[nodiscard]] float& height_m() { return height_m_; }
    [[nodiscard]] float& thickness_m() { return thickness_m_; }
    [[nodiscard]] float& tex_deg() { return tex_deg_; }
    [[nodiscard]] std::string& style() { return style_; }
    [[nodiscard]] world::ElementId last_element() const { return last_; }
    [[nodiscard]] const std::string& refusal() const { return refusal_; }

    /// Убрать последний якорь обхода / бросить обход целиком.
    void undo_last();
    void clear_draft();
    /// СОЗДАТЬ. false — отказ, причина в refusal().
    bool confirm(ToolWorld& world);

    /// КУДА СМОТРИТ ЛИЦО, пока контур ещё набирается. false — вершин мало или
    /// они на одной прямой. Считается тем же способом, что и у готового
    /// элемента (world::surface_normal): контур — наилучшая по МНК плоскость,
    /// цепочка — поперечина к первому отрезку.
    [[nodiscard]] bool draft_normal(glm::vec3& out) const;

private:
    HouseSession* session_ = nullptr;
    ToolWorld* world_ = nullptr;
    std::vector<world::VertexId> refs_;
    bool closed_ = false;
    bool flipped_ = false;
    float height_m_ = 2.5f;
    /// ЗАГОТОВКА: материал, тон, обшивка и окна СЛЕДУЮЩЕЙ поверхности.
public:
    [[nodiscard]] int& draft_mat() { return mat_; }
    [[nodiscard]] int& draft_tone() { return tone_; }
    [[nodiscard]] bool& draft_clad() { return clad_; }
    [[nodiscard]] int& draft_fill() { return fill_; }
    [[nodiscard]] int& draft_windows() { return windows_; }

private:
    int mat_ = 5;   ///< штукатурка
    int tone_ = 0;  ///< светлый
    bool clad_ = false;
    int windows_ = 0;
    int fill_ = 0; ///< 0 гладкая/фахверк, 2 кирпич, 3 блоки
    int doors_ = 0; ///< дверные проёмы в кладке
    float thickness_m_ = 0.20f;
    float tex_deg_ = 0.0f;
    std::string style_ = "plank";
    world::ElementId last_ = world::NO_ELEMENT;
    std::string refusal_;
    mutable HouseWire wire_;
    mutable std::vector<glm::vec3> draft_line_;
    /// Номер жизни графа, при котором рука набрала то, что держит. Разошёлся —
    /// значит между двумя щелчками произошла отмена, и всё, что рука помнит по
    /// именам, надо забыть, а не разыгрывать заново на чужих вершинах.
    std::uint32_t seen_revision_ = 0;
};

} // namespace dfn::app
