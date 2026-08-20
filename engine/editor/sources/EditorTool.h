/*
Created: 18:08:2026 - 11:52:10
Last updated: 20:08:2026 - 23:59:00
Module: engine/editor
File: engine/editor/sources/EditorTool.h

Responsibility:
- WHAT AN EDITOR TOOL IS, as a type. One class per tool, one contract, and the
  contract is COMPLETE: if a tool does not answer a question here, it does not
  do that thing — there is no "and the rest is written somewhere in App".

Key items:
- IEditorTool: identity, press/drag/release, preview, settings, reach,
  selected/deselected.
- ToolAim: where the crosshair meets the world, and HOW FAR — the number the
  reach ceiling is measured against.
- ToolWorld: everything a tool may do TO the world, as hooks the app fills.
  A tool never names App, never names a chunk manager, never names ImGui.
- ToolPreview: what the tool wants drawn in the world while it is in hand.
- ToolStatus: what the badge under the crosshair says right now.

WHY THIS EXISTS (docs/AUDIT_EDITOR_TOOLS.md, user 18.08.2026): «у меня
серьезная настроенность в том, что текущий код инструментов стал спагетти
кодом. У тебя нет отдельного класса под инструменты, нет четких интерфейсов
взаимодействия и прочее. Если бы они были, нельзя бы было поймать ошибки, что я
сразу два инструмента в руке держу и прочее.»

The audit measured the claim and it was right: SEVEN places asked "which tool is
it now", TWELVE `case EditorTool::` labels in two switches, ZERO classes. The
tool was a LABEL, and every behaviour hung off that label in a place that had to
remember to. Two tools at once was not a bug in one of those places — it was the
shape of the whole thing: the click was armed by «режим постановки ИЛИ открыт
список объектов», while the brush hung off the same button on its own.

THE PROPERTY THIS BUYS, and it is the only reason to move the code at all:
- Two tools in hand are IMPOSSIBLE, not merely avoided. There is one active
  pointer (EditorToolbox), the press goes to it and to nothing else, and there
  is no second place a tool could subscribe from.
- A tool cleans up after itself: on_deselected() is called from exactly one
  place — where the active pointer changes — so «деталь остаётся в руках» has
  nowhere to happen.
- Settings cannot drift from the selection: the panel draws the tool's OWN
  draw_settings(), so there is no second control setting the same state.

Dependencies:
- Uses: glm, std. NOT ImGui: the interface must be instantiable in a test with
  no window (Rule 3), and draw_settings()'s BODY is what may call ImGui — in
  engine/editor, where it is allowed.
- Used by: EditorToolbox, EditorToolsBuiltin, EditorUi's toolbar, engine/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- A NEW TOOL IS A NEW CLASS AND NOTHING ELSE. If adding one makes you edit a
  switch anywhere, that switch is the defect this file was written to delete.
- Never read "which tool is active" outside EditorToolbox. Ask the toolbox for
  the ANSWER (preview, status, reach), never for the identity to switch on.
*/
/*
UPD:
- 18:08:2026 - 11:52:10: Создан по образцу из docs/AUDIT_EDITOR_TOOLS.md — интерфейс
  инструмента, прицел с ДАЛЬНОСТЬЮ, мир как набор крючков, превью и состояние
  подписи. Заказ 18.08 целиком: «нельзя бы было поймать ошибки, что я сразу два
  инструмента в руке держу».
- 18:08:2026 - 13:08:07: ДВЕ ДОБАВКИ ПОД ТРОПЫ, обе минимальные и обе — ответы, а не ярлыки.
  ToolPreview получил ЛОМАНУЮ И УЗЛЫ: инструмент тропы рисует в мире не кольцо,
  а линию, и без этого поля App пришлось бы спрашивать «а не тропа ли сейчас в
  руке» — то самое седьмое место, ради удаления которого файл написан.
  ToolWorld получил ground_height (положить линию на землю), relief_paths /
  commit_path (прочитать и записать тропы) и last_dab (числа последнего мазка —
  счётчик вернулся из мёртвой панели кисти в настройки самой кисти).
- 18:08:2026 - 18:02:11: ДВЕ ДОБАВКИ ПОД ПОСТРОЙКУ, и обе — про то, чего рисовальщик сегодня
  сделать не может. (1) ToolPreview::accent — ВТОРОЙ список отрезков со СВОИМ
  цветом. Подсветка выбора («выбрал вершину — светятся её элементы») это ответ
  на вопрос «что я выбрал», и одним цветом на всё она не выражается: выбранное
  и невыбранное стали бы одинаковыми. (2) IEditorTool::on_confirm/on_cancel —
  у инструмента поверхности есть состояние, которое НАКАПЛИВАЕТСЯ между
  щелчками (обход по якорям), и заканчивается оно не щелчком, а отдельным
  словом. Без этих двух глаголов «Enter — создать» пришлось бы вешать на
  сравнение с именем инструмента снаружи — то самое седьмое место.
- 18:08:2026 - 18:58:40: aims_without_ground() — инструмент сам решает, куда попала мышь; для оси луч в небо законен.
- 18:08:2026 - 19:14:22: has_draft() — есть ли что подтверждать: Enter занят заметкой, и различает два действия УСЛОВИЕ, а не вторая строка в таблице.
- 18:08:2026 - 19:44:10: ToolAim::direction() — луч прицела считается ЗДЕСЬ и больше нигде; wants_wheel/on_wheel — колесо отдаётся инструменту, который его просит.
- 18:08:2026 - 20:26:30: ToolAim::in_reach — ящик сообщает обстоятельство, а обещание от факта отделяет сам инструмент.
- 18:08:2026 - 23:20:00: stroke_needs_reach — дальность судит начало работы, а не каждый шаг уже взятого.
- 19:08:2026 - 00:12:30: ToolPreview::ghost_pairs и ghost_color — призрак рисуется своей стопкой и своим цветом.
- 20:08:2026 - 00:02:30: Крючки картинок в ToolWorld: material_swatch и wall_example — «хочу не слова, а картинки».
- 20:08:2026 - 17:30:00: ToolWorld: house_assets / place_house_at_aim / remove_last_house.
- 20:08:2026 - 20:30:00: unpack_house_at_aim — распаковка постройки под прицелом.
- 20:08:2026 - 23:59:00: Крючки save_session_house и apply_style_to_draft.
*/

#pragma once

#include "engine/editor/sources/EditorBrush.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <utility>
#include <vector>

namespace dfn::app {

/// The picture on the tool's button. Drawn by us, on the CPU
/// (EditorToolIcons.h) — the user asked for «картинки одинаковых размеров», and
/// equal size is a property of the baker, not of five hand-made files.
enum class ToolIcon : std::uint8_t {
    Height,   ///< кисть высоты
    Surface,  ///< кисть поверхности
    Select,   ///< выбор объекта
    Place,    ///< постройка
    Plant,    ///< посадка растительности
    Path,     ///< тропа кривой
    // ТРИ ЗНАЧКА ПОСТРОЙКИ. До сегодня три инструмента графа занимали чужие
    // картинки — вершина брала рамку выбора, прямая тропу, поверхность куб, — и
    // полоса врала: одна картинка на два разных дела. Отличаются они ФОРМОЙ, а
    // не цветом, потому что различие «точка / отрезок / натянутое полотно» и
    // есть то, чем эти три инструмента отличаются друг от друга.
    HouseVertex,  ///< якорь: шарик с отвесом
    HouseLine,    ///< прямая между двумя якорями
    HouseSurface, ///< полотно, натянутое на якоря
    Settings, ///< шестерёнка: общие параметры (не инструмент)
    Count
};

/// WHAT THIS TOOL IS, for the bar and for the badge under the crosshair.
///
/// The keys are localization keys (Rule 5) and the id is ASCII the user never
/// sees: the door DFN_EDITOR_TOOL and the key table both name tools by id, so
/// renaming a caption cannot silently move a keyboard shortcut.
struct ToolIdentity {
    const char* id = "";        ///< stable ascii, e.g. "height"
    const char* title_key = ""; ///< caption / tooltip, e.g. "editor.tool.height"
    const char* hint_key = "";  ///< what the click will do, for the badge
    ToolIcon icon = ToolIcon::Count;
};

/// WHERE THE CROSSHAIR MEETS THE WORLD, and how far away that is.
///
/// `distance_m` IS THE POINT OF THIS STRUCT (user, 18.08: «у каждого
/// инструмента необходимо сделать максимальную дальность взаимодействия с
/// окружением, я не должен уметь за 1000 км что-то строить»). The aim used to
/// be a bare vec3, so no consumer could even ASK how far it had marched, and
/// the ceiling could not be written anywhere. It is filled by the caller that
/// marches the ray, once, for every tool (Rule 32).
struct ToolAim {
    glm::vec3 origin{0.0f};   ///< the eye
    glm::vec3 point{0.0f};    ///< where the ray stopped
    float distance_m = 0.0f;  ///< |point - origin|, the reach test's subject
    bool hit = false;         ///< false = the ray met nothing (looking at sky)
    bool pointer_over_ui = false; ///< the pointer is on a panel

    /// ДОСТАЁТ ЛИ ЩЕЛЧОК ДО ЭТОЙ ТОЧКИ. Заполняет ящик, читает показ.
    ///
    /// Раньше ящик просто НЕ ЗВАЛ preview() за пределом дальности, и правило
    /// было верным ровно наполовину: призрак — обещание щелчка, и за пределом
    /// его показывать нельзя. Но у инструментов постройки в той же картинке
    /// едет ВСЯ ПОСТРОЙКА, а она не обещание, а факт, — и дом исчезал, стоило
    /// отойти («когда я удаляюсь от якорей и линий, они исчезают, хотя я хочу
    /// чтобы рисовались», 18.08).
    ///
    /// Поэтому решает теперь инструмент: он один знает, где у него обещание, а
    /// где то, что уже стоит в мире.
    bool in_reach = true;

    /// НАПРАВЛЕНИЕ ВЗГЛЯДА, единичное. Считается ЗДЕСЬ и больше нигде: второй
    /// способ узнать, куда смотрит человек (например, по камере), разъехался бы
    /// с первым в тот день, когда прицел сместят от центра экрана.
    ///
    /// Луч нужен всем, кто ищет цель НЕ НА ЗЕМЛЕ: точка прицела лежит там, где
    /// луч встретил грунт, и висящий в воздухе якорь рядом с ней не окажется
    /// никогда.
    [[nodiscard]] glm::vec3 direction() const {
        const glm::vec3 d = point - origin;
        const float len = glm::length(d);
        // Вырожденный случай назван, а не поделён на ноль: прицел, совпавший с
        // глазом, направления не задаёт вовсе.
        return len > 1e-6f ? d / len : glm::vec3{0.0f, 0.0f, 1.0f};
    }
};

/// WHAT THE TOOL WANTS DRAWN IN THE WORLD while it is in hand. Everything here
/// disappears when the tool is put down, and that is not a courtesy: the user
/// asked for «если я кликну на иконку выбранного уже инструмента, выбор
/// сбросится, весь UI дополнительный для этого пропадет».
struct ToolPreview {
    bool ghost = false;        ///< the part in hand, drawn where it would stand
    bool target_probe = false; ///< «что под прицелом» pass (delete / select)
    /// ПРИЗРАК — СВОЯ СТОПКА И СВОЙ ЦВЕТ, отрезками (пары точек).
    ///
    /// Пользователь 18.08 прислал кадр: «якорей рисуется жёлтых 2». Их и было
    /// два — выбранный якорь и призрак будущего, — и оба жёлтые, потому что
    /// ехали в одной стопке подсветки. Цвет обязан отвечать на вопрос «это уже
    /// стоит или только встанет», иначе человек ищет глазами, какой из двух
    /// шариков настоящий.
    const std::vector<glm::vec3>* ghost_pairs = nullptr;
    std::uint32_t ghost_color = 0;

    /// ЛОМАНАЯ, КОТОРУЮ ИНСТРУМЕНТ ХОЧЕТ ВИДЕТЬ В МИРЕ, уже положенная на
    /// землю, или null. Указатель на СОБСТВЕННЫЙ буфер инструмента, как и
    /// ring_brush: копия здесь была бы вектором в кадр на пустом месте.
    ///
    /// ПОЧЕМУ ЭТО ЗДЕСЬ, А НЕ В App: тропа рисуется линией, а не кольцом, и
    /// единственная альтернатива — вопрос «какой инструмент сейчас в руке» в
    /// коде отрисовки. Ровно от этого вопроса файл и избавлялся.
    const std::vector<glm::vec3>* polyline = nullptr;
    /// Узлы, которые человек может схватить, — рисуются крестиками. Отдельно от
    /// ломаной: точки поставлены рукой, а ломаная посчитана, и путать их на
    /// экране значит показывать, будто кривая состоит из его точек.
    const std::vector<glm::vec3>* handles = nullptr;
    /// 0xAABBGGRR для обеих. Ноль — «цвет по умолчанию решает рисующий».
    std::uint32_t line_color = 0;
    /// ВТОРОЙ СПИСОК ОТРЕЗКОВ, ПАРАМИ, СО СВОИМ ЦВЕТОМ — подсветка. Тот же
    /// формат, что у handles, и рисуется тем же кодом; разделены они не по
    /// форме, а по СМЫСЛУ: accent отвечает на вопрос «что выбрано», и ответ на
    /// него обязан отличаться от остального на глаз. Один список с одним цветом
    /// показал бы выбранную стену ровно так же, как невыбранную.
    const std::vector<glm::vec3>* accent = nullptr;
    std::uint32_t accent_color = 0;
    /// THE BRUSH THE RING IS DRAWN FOR, or null for no ring. A pointer to the
    /// tool's OWN brush rather than a radius: brush_outline() bisects the rim
    /// out of brush_weight(), and handing over a number here would let the ring
    /// promise a boundary the brush does not honour (EditorBrush.h says so).
    const TerrainBrush* ring_brush = nullptr;
};

/// WHAT THE BADGE UNDER THE CROSSHAIR SAYS. `key` is a localization key;
/// `text` overrides it when the tool has a sentence of its own (the judge's
/// refusal, for instance, which is already a translated string).
struct ToolStatus {
    const char* key = "";
    std::string text;
    bool ready = true; ///< false = the click would do nothing, and says why
};

/// EVERYTHING A TOOL MAY DO TO THE WORLD, and nothing else is reachable.
///
/// Hooks rather than an interface App implements, for the reason the palette
/// and the brush already use hooks: App owns a window and cannot be
/// instantiated in a test, but a struct of std::function can be filled by a
/// four-line fake — which is how the reach ceiling and the exclusivity get
/// measured at all.
///
/// Every hook may be empty. A tool that calls an empty hook does nothing, which
/// is the correct behaviour for a tool wired into a world that cannot do that
/// thing yet — and it is why the toolbox can be tested with a world that
/// implements two hooks out of ten.
struct ToolWorld {
    // -- ground ---------------------------------------------------------------
    /// One dab of `brush` at `centre`, for `dt_s` of holding. THE BRUSH COMES
    /// FROM THE TOOL, so the settings the user moved and the ground he digs
    /// cannot be two different brushes — which is what one shared TerrainBrush
    /// with a `mode` field shown as two tools had become.
    std::function<bool(const TerrainBrush& brush, glm::vec2 centre, float dt_s)> terrain_dab;

    /// КАРТИНКА МАТЕРИАЛА НАБОРА (surface, tone, px) -> текстура интерфейса.
    /// Заказ 19.08 дословно: «я всегда хочу видеть примеры, причём не слова, а
    /// картинки», «список словами не нагляден». Печёт и кэширует App — у него
    /// и лист набора, и EditorUi с загрузкой текстур; инструмент только
    /// показывает. 0 — картинки нет, панель падает в подпись словами.
    std::function<std::uint64_t(int surface, int tone, int px)> material_swatch;
    /// КАРТИНКА-ПРИМЕР ЗАПОЛНЕНИЯ СТЕНЫ: 0 гладкая, 1 фахверк, 2 фахверк с
    /// окнами. Те же права и та же причина, что у material_swatch.
    std::function<std::uint64_t(int variant, int px)> wall_example;
    /// The stroke ended: spend its budget and rebuild what it dirtied.
    std::function<void()> finish_stroke;
    /// ПОКАЖИ МОИ СОБСТВЕННЫЕ НАСТРОЙКИ. Зовёт инструмент — и это единственный
    /// способ их открыть изнутри мира.
    ///
    /// Заведено по разбору жалобы 18.08: «когда я сажать пытаюсь, мне
    /// открывается меню инструмента посадки, бредовое поведение». Открывал их
    /// App, условием «есть выбранная расстановка и настройки закрыты» — и НЕ
    /// спрашивал, чей сейчас ход, поэтому поведение, написанное для инструмента
    /// ВЫБОРА («ткнул в объект — вот его свойства»), срабатывало у всех. Тот же
    /// дефект, что и остальные за день: условие называло ОТВЕТ, а не ВЛАДЕЛЬЦА.
    /// Теперь настройки открывает тот, кто этого хочет, из своего же on_press —
    /// и другому инструменту нечего забыть.
    std::function<void()> open_own_settings;
    /// «Ровно»: put one [pad] into the composition. The pad is COMPUTED by the
    /// tool (flatten_pad(), EditorBrush.h) — this only carries it.
    std::function<void(const world::ScenePad& pad)> add_pad;
    /// The finished ground at a world XZ — hand edits included. A tool that
    /// draws anything ON the ground needs it, and a tool that guesses sea level
    /// instead draws its line under the hill (that defect is in this repo's
    /// history, in the brush ring).
    std::function<float(glm::vec2 world_xz)> ground_height;
    /// ЧИСЛА ПОСЛЕДНЕГО МАЗКА: сколько отсчётов сдвинулось и на сколько.
    /// Показываются в настройках самой кисти — кисть, которая молча перестала
    /// бить (прицел за подгруженным кольцом), выглядит ровно как кисть,
    /// наведённая не туда, и эти два числа различают их с одного взгляда.
    std::function<void(int& samples, float& worst_m)> last_dab;

    // -- ready-made buildings (20.08) ----------------------------------------
    /// Библиотека готовых построек: имена .dfh из assets/houses (без пути и
    /// расширения). Пустой список законен — библиотека может быть пуста.
    std::function<std::vector<std::string>()> house_assets;
    /// Поставить постройку ПОД ПРИЦЕЛ: файл из библиотеки, поворот в градусах.
    /// Пишет секцию [house] сцены и тут же поднимает дом (меш + коллайдер).
    std::function<void(const std::string& name, float yaw_deg)> place_house_at_aim;
    /// Убрать ПОСЛЕДНЮЮ поставленную постройку (отмена не через историю
    /// графа: сцена — другой документ; сказано в панели).
    std::function<void()> remove_last_house;
    /// РАСПАКОВАТЬ постройку под прицелом в сессию (20.08: «должна быть
    /// возможность выбирать стены, якоря — сейчас стоит как проп»): граф
    /// вливается в редактируемый, запись [house] снимается со сцены.
    std::function<void()> unpack_house_at_aim;
    /// СОХРАНИТЬ ТЕКУЩУЮ ПОСТРОЙКУ СЕССИИ в библиотеку (assets/houses/<имя>.dfh),
    /// с нормировкой координат к нулю — файл кладётся на любую карту.
    std::function<void(const std::string& name)> save_session_house;
    /// Применить стиль (пары отделки) к ЗАГОТОВКАМ инструментов постройки.
    std::function<void(const std::vector<std::pair<std::string, std::string>>&)>
        apply_style_to_draft;

    // -- paths ----------------------------------------------------------------
    /// Тропы, которые уже проведены. Указатель, а не копия: инструмент читает
    /// их каждый кадр, чтобы нарисовать и чтобы найти узел под прицелом.
    /// Пустой ответ (nullptr) законен и значит «мир троп не держит».
    std::function<const std::vector<world::ReliefPath>*()> relief_paths;
    /// ЕДИНСТВЕННАЯ ДВЕРЬ К ЗАПИСИ ТРОПЫ, и она одна на три действия нарочно:
    /// `index` == npos — добавить, `path` == nullptr — удалить, иначе заменить.
    /// Три отдельных крючка означали бы три места, которые обязаны помнить про
    /// перепечку канала и про пометку чанков.
    ///
    /// ВОЗВРАЩАЕТ ИНДЕКС ТРОПЫ ПОСЛЕ ДЕЙСТВИЯ (npos после удаления). Без него
    /// добавивший тропу инструмент вычислял бы её номер из длины чужого
    /// вектора — то есть держал бы у себя копию правила о том, как мир
    /// нумерует тропы.
    std::function<std::size_t(std::size_t index, const world::ReliefPath* path)>
        commit_path;

    // -- parts ---------------------------------------------------------------
    /// Place what the hand is holding where the ghost stands. False = refused.
    std::function<bool()> place_part;
    /// Remove whatever the target probe found. False = nothing there.
    std::function<bool()> delete_target;
    /// Select whatever the target probe found (fills the properties column).
    /// TRUE = что-то выбрано. Возвращает признак, а не void, потому что от него
    /// зависит второе действие: свойства открываются, только если есть что
    /// показать. Пустой щелчок по траве, распахивающий пустую колонку, — это
    /// шум, а не отклик.
    std::function<bool()> select_target;
    /// Is something under the crosshair right now?
    std::function<bool()> has_target;
    /// May the held part be placed? On refusal, `reason` carries the judge's
    /// own sentence — one verdict rendered twice, never two verdicts.
    std::function<bool(std::string& reason)> ghost_ready;
    /// Drop the held part's preview mesh. Called from on_deselected().
    std::function<void()> clear_ghost;

    // -- plants ---------------------------------------------------------------
    /// One dab of planting at `centre` with THIS brush; returns how many stood
    /// up. The brush travels with the call for the same reason the terrain
    /// brush does: it belongs to the tool, and the app must not have to know
    /// which tool is asking (it used to reach for the concrete class).
    std::function<int(const PlantBrush& brush, glm::vec2 centre)> plant_dab;
};

/// A TOOL. One class per tool, and the class is the only place its behaviour
/// lives.
class IEditorTool {
public:
    IEditorTool() = default;
    virtual ~IEditorTool() = default;
    IEditorTool(const IEditorTool&) = delete;
    IEditorTool& operator=(const IEditorTool&) = delete;

    /// WHAT IT IS: the bar's caption, its icon, its badge key.
    [[nodiscard]] virtual ToolIdentity identity() const = 0;

    /// WHAT IT DOES ON A CLICK AND ON A DRAG. One owner of the button: the
    /// toolbox calls exactly one tool, and only when the click is in reach and
    /// the pointer is not on a panel.
    virtual void on_press(const ToolAim& aim, ToolWorld& world) = 0;
    virtual void on_drag(const ToolAim& aim, float dt_s, ToolWorld& world) = 0;
    virtual void on_release(ToolWorld& world) = 0;

    /// WHAT IT DRAWS IN THE WORLD while it is in hand.
    [[nodiscard]] virtual ToolPreview preview(const ToolAim& aim) const = 0;

    /// ITS OWN SETTINGS, and nobody else's. Called inside a window the toolbar
    /// owns — plain ImGui inside is the whole API. Called only for the tool
    /// whose triangle was pressed, which is what makes «меню настройки этого И
    /// ТОЛЬКО этого инструмента» true by construction.
    virtual void draw_settings() = 0;

    /// HOW FAR IT REACHES on its own. The toolbox takes the smaller of this and
    /// the common ceiling, so a tool may be shorter-armed than the ceiling but
    /// can never be longer.
    [[nodiscard]] virtual float max_reach_m() const = 0;

    /// PICKED UP / PUT DOWN. Called from exactly one place (EditorToolbox), so
    /// «деталь остаётся в руках» has nowhere left to happen.
    virtual void on_selected(ToolWorld& world) { (void)world; }
    virtual void on_deselected(ToolWorld& world) { (void)world; }

    /// WHAT THE BADGE SAYS NOW. Default: the identity's hint. A tool with a
    /// state worth reporting (nothing under the crosshair, judge refused)
    /// overrides it — which is what deleted the five-case switch in App.
    [[nodiscard]] virtual ToolStatus status(const ToolAim& aim) const {
        (void)aim;
        return ToolStatus{identity().hint_key, {}, true};
    }

    /// ГОТОВО / БРОСИТЬ. Для инструмента, у которого есть состояние, копящееся
    /// между щелчками (обход по якорям у поверхности): такой набор кончается не
    /// щелчком по миру, а отдельным словом. По умолчанию оба ничего не делают —
    /// у инструмента без накопления нечего подтверждать.
    /// ЕСТЬ ЛИ ЧЕРНОВИК, КОТОРЫЙ ПОДТВЕРЖДАЮТ. Отдельный вопрос от on_confirm,
    /// потому что Enter в приложении уже занят быстрой заметкой: клавиша одна,
    /// а различает их УСЛОВИЕ — так же, как Shift различает отмену и повтор.
    /// Инструмент без черновика обязан отвечать false, иначе он молча съедал бы
    /// чужую клавишу.
    [[nodiscard]] virtual bool has_draft() const { return false; }

    /// КОЛЕСО МЫШИ ОТДАЁТСЯ ИНСТРУМЕНТУ, КОГДА ОН ЕГО ПРОСИТ. По умолчанию оно
    /// принадлежит общей дальности взаимодействия (ползунок под шестерёнкой), и
    /// это остаётся правдой для всех, кто здесь молчит.
    ///
    /// Просит его тот, у кого колесо решает задачу, которую иначе решить нечем:
    /// у якоря это ПОДТЯГИВАНИЕ ШАРИКА К СЕБЕ вдоль луча прицела — единственный
    /// способ поставить вершину в воздух, не выдумывая ей высоту числом в
    /// панели («не могу ставить его в воздухе... не могу приближать сферу к
    /// себе или отдалять», 18.08).
    [[nodiscard]] virtual bool wants_wheel() const { return false; }
    /// Щелчки колеса за кадр: вверх положительные.
    virtual void on_wheel(float ticks) { (void)ticks; }

    virtual void on_confirm(ToolWorld& world) { (void)world; }
    virtual void on_cancel(ToolWorld& world) { (void)world; }

    /// Do the arrow keys turn the part in hand for this tool? Asked instead of
    /// «is the object list open», which is what armed two owners of one button.
    /// ИНСТРУМЕНТ САМ РЕШАЕТ, КУДА ПОПАЛА МЫШЬ — земля ему сейчас не нужна.
    ///
    /// Обычное правило ящика: луч, не встретивший ничего, целью не считается,
    /// иначе щелчок по небу строил бы в воздухе. Но у инструмента с
    /// ФИКСИРОВАННОЙ ОСЬЮ точка берётся не с земли, а с прямой через якорь —
    /// и там луч в небо совершенно законен: именно так и тянут стойку ВВЕРХ.
    /// Пользователь 18.08 показал кадром обратное поведение — прямая легла на
    /// траву, потому что вверх её вести было нечем.
    ///
    /// Потолок дальности этим НЕ снимается: он судится по точке, которую
    /// вернул сам инструмент (ghost), а не по тому, куда улетел луч.
    /// СУДИТСЯ ЛИ ДАЛЬНОСТЬ НА КАЖДОМ ШАГЕ ШТРИХА, или только на нажатии.
    ///
    /// true у кистей: там каждый шаг — новый укус земли, и укус за сто метров
    /// от руки не должен случаться. false у всех, кто ВЕДЁТ уже взятое:
    /// схваченный якорь не выпадает из руки оттого, что луч прицела ушёл в
    /// небо (жалоба 18.08 — «тяну якорь вверх, движение обрывается»).
    [[nodiscard]] virtual bool stroke_needs_reach() const { return true; }

    [[nodiscard]] virtual bool aims_without_ground() const { return false; }

    [[nodiscard]] virtual bool wants_part_rotation() const { return false; }
};

} // namespace dfn::app
