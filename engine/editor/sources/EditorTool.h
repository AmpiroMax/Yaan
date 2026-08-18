/*
Created: 18:08:2026 - 11:52:10
Last updated: 18:08:2026 - 13:08:07
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
*/

#pragma once

#include "engine/editor/sources/EditorBrush.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
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
};

/// WHAT THE TOOL WANTS DRAWN IN THE WORLD while it is in hand. Everything here
/// disappears when the tool is put down, and that is not a courtesy: the user
/// asked for «если я кликну на иконку выбранного уже инструмента, выбор
/// сбросится, весь UI дополнительный для этого пропадет».
struct ToolPreview {
    bool ghost = false;        ///< the part in hand, drawn where it would stand
    bool target_probe = false; ///< «что под прицелом» pass (delete / select)
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

    /// Do the arrow keys turn the part in hand for this tool? Asked instead of
    /// «is the object list open», which is what armed two owners of one button.
    [[nodiscard]] virtual bool wants_part_rotation() const { return false; }
};

} // namespace dfn::app
