/*
Created: 18:08:2026 - 12:08:20
Last updated: 18:08:2026 - 19:44:10
Module: tests/app
File: tests/app/EditorToolboxTests.cpp

Responsibility:
- THE FOUR PROPERTIES THE USER ASKED FOR, AS NUMBERS. Every one of them is
  invisible in a screenshot and was, until today, held by nothing at all:
  1. a click on ANOTHER tool's settings does not change what is in hand;
  2. a click on the ACTIVE tool's icon drops it, and its preview leaves the
     world with it;
  3. R toggles the pointer mode both ways, whatever is open;
  4. a click beyond the reach ceiling does NOTHING, and one inside it acts.
  Plus the property the whole rework exists for: a press reaches EXACTLY ONE
  tool, counted at every tool.

Dependencies:
- Uses: doctest, EditorToolbox.h, EditorToolIcons.h. No ImGui, no window, no
  App — which is the point: the toolbox was written so that these questions
  could be asked at all (Rule 3, Rule 27).
- Used by: ctest (app_editor_toolbox).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- EVERY CLAIM HERE SHIPS WITH ITS CONTROL (Rule 30). "Nothing happened" is the
  expected result of half of these tests, and a test whose pass condition is
  "nothing happened" passes just as well on a toolbox that does nothing at all
  — so each of them stands beside an arm where the SAME call DOES something.
*/
/*
UPD:
- 18:08:2026 - 12:08:20: Создан — рукав ящика инструментов (заказ 18.08 и
  docs/AUDIT_EDITOR_TOOLS.md).
- 18:08:2026 - 12:51:26: пара «подпись говорит почему, мир не обещает» за пределом дальности.
  Порознь каждая половина прошла бы на сломанном коде: подпись без гашения
  оставляет зелёное кольцо там, куда щелчок не достанет, гашение без подписи —
  молчание, неотличимое от поломки. Контрфакт: снял проверку — 6 утверждений.
- 18:08:2026 - 13:08:07: седьмой значок (тропа) в списке различимости — иначе новый значок мог бы
  совпасть с чужим побайтово и никто бы не заметил.
- 18:08:2026 - 19:44:10: Три новых значка в перечне попарно-разных.
*/

#include "engine/editor/sources/EditorToolIcons.h"
#include "engine/editor/sources/EditorToolbox.h"
#include "engine/editor/sources/EditorToolsBuiltin.h"

#include <doctest/doctest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

using dfn::app::bake_tool_icon;
using dfn::app::EditorToolbox;
using dfn::app::IEditorTool;
using dfn::app::NO_TOOL;
using dfn::app::ToolAim;
using dfn::app::ToolPreview;
using dfn::app::ToolStatus;
using dfn::app::ToolIcon;
using dfn::app::ToolIdentity;
using dfn::app::ToolPreview;
using dfn::app::ToolTickReport;
using dfn::app::ToolWorld;

namespace {

/// A COUNTING TOOL. It does nothing to the world and remembers everything that
/// was asked of it — which is exactly what "did the click reach this one?" needs
/// to be a number rather than an impression.
class FakeTool final : public IEditorTool {
public:
    FakeTool(const char* id, float reach_m, bool wants_ghost)
        : id_(id), reach_m_(reach_m), ghost_(wants_ghost) {}

    [[nodiscard]] ToolIdentity identity() const override {
        return ToolIdentity{id_.c_str(), "editor.tool.height", "tool.hint.height",
                            ToolIcon::Height};
    }
    void on_press(const ToolAim& aim, ToolWorld&) override {
        ++presses;
        last_press_distance_m = aim.distance_m;
    }
    void on_drag(const ToolAim&, float, ToolWorld&) override { ++drags; }
    void on_release(ToolWorld&) override { ++releases; }
    [[nodiscard]] ToolPreview preview(const ToolAim&) const override {
        ToolPreview out;
        out.ghost = ghost_;
        out.target_probe = ghost_;
        return out;
    }
    void draw_settings() override { ++settings_draws; }
    [[nodiscard]] float max_reach_m() const override { return reach_m_; }
    void on_selected(ToolWorld&) override { ++selects; }
    void on_deselected(ToolWorld&) override { ++deselects; }

    int presses = 0;
    int drags = 0;
    int releases = 0;
    int selects = 0;
    int deselects = 0;
    int settings_draws = 0;
    float last_press_distance_m = -1.0f;

private:
    std::string id_;
    float reach_m_ = 100.0f;
    bool ghost_ = false;
};

struct Bench {
    EditorToolbox box;
    ToolWorld world;
    std::vector<FakeTool*> tools;

    Bench() {
        // ВСЕ ТРИ ПРОСЯТ ПРЕВЬЮ, и это не мелочь: пока превью просил ровно
        // один, утверждение «после сброса превью погасло» проходило бы и на
        // ящике, который в пустой руке отвечает превью ПЕРВОГО инструмента
        // (проверено контрфактом — он оставался зелёным).
        for (const char* id : {"height", "surface", "place"}) {
            auto tool = std::make_unique<FakeTool>(id, 30.0f, true);
            tools.push_back(tool.get());
            box.add(std::move(tool));
        }
    }

    /// An aim `metres` in front of the eye that DID hit something.
    static ToolAim at(float metres, bool over_ui = false) {
        ToolAim aim;
        aim.origin = {0.0f, 2.0f, 0.0f};
        aim.point = {metres, 0.0f, 0.0f};
        aim.distance_m = metres;
        aim.hit = true;
        aim.pointer_over_ui = over_ui;
        return aim;
    }

    /// One whole click: down for a frame, up on the next.
    ToolTickReport click(const ToolAim& aim) {
        const ToolTickReport down = box.update(aim, 1.0f / 60.0f, true, world);
        (void)box.update(aim, 1.0f / 60.0f, false, world);
        return down;
    }
};

} // namespace

// ============================================================================
// 1. ЩЕЛЧОК ПО НАСТРОЙКАМ ЧУЖОГО ИНСТРУМЕНТА НЕ МЕНЯЕТ АКТИВНЫЙ
// «если у меня выбран один инструмент, я кликаю на настройки другого,
//  инструмент не меняется в руках»
// ============================================================================
TEST_CASE("настройки чужого инструмента не берут его в руку") {
    Bench b;
    b.box.click_icon(0, b.world);
    REQUIRE(b.box.active_index() == 0);

    b.box.click_settings(2);

    // РУКА ТА ЖЕ, ОКНО ЧУЖОЕ. Two different verbs, and the second one cannot
    // express the first.
    CHECK(b.box.active_index() == 0);
    CHECK(b.box.settings_index() == 2);
    CHECK(b.tools[2]->selects == 0);
    CHECK(b.tools[0]->deselects == 0);

    // И ЩЕЛЧОК ПОСЛЕ ЭТОГО УХОДИТ ВСЁ ТОМУ ЖЕ. Without this line the test would
    // pass on a toolbox whose active_index() is right and whose dispatch is
    // wrong — the index is a claim about the answer, this is a claim about the
    // behaviour.
    b.click(Bench::at(5.0f));
    CHECK(b.tools[0]->presses == 1);
    CHECK(b.tools[2]->presses == 0);
    MESSAGE("в руке " << b.box.active_index() << ", открыты настройки "
                      << b.box.settings_index() << ", нажатий у 0: "
                      << b.tools[0]->presses << ", у 2: " << b.tools[2]->presses);

    // КОНТРОЛЬ: тот же ящик ДЕЙСТВИТЕЛЬНО умеет менять инструмент — через
    // другой глагол. Без этого плеча «не сменился» проходило бы и на ящике,
    // который не умеет менять инструмент вовсе (Rule 30).
    b.box.click_icon(2, b.world);
    CHECK(b.box.active_index() == 2);
    CHECK(b.tools[0]->deselects == 1);
    CHECK(b.tools[2]->selects == 1);
}

// ============================================================================
// 2. ЩЕЛЧОК ПО ИКОНКЕ АКТИВНОГО СБРАСЫВАЕТ ВЫБОР И ГАСИТ ПРЕВЬЮ
// «если я кликну на иконку выбранного уже инструмента, выбор сбросится, весь UI
//  дополнительный для этого пропадет, я буду просто бегать по игре»
// ============================================================================
TEST_CASE("щелчок по активному кладёт инструмент и гасит его превью") {
    Bench b;
    b.box.click_icon(2, b.world); // "place" — единственный, кто просит призрак
    REQUIRE(b.box.active_index() == 2);
    // КОНТРОЛЬНОЕ ПЛЕЧО, и оно обязано идти ПЕРВЫМ: превью сначала ЕСТЬ.
    // Иначе «превью погасло» проходит на ящике, который его никогда не зажигал.
    REQUIRE(b.box.preview(Bench::at(5.0f)).ghost);

    b.box.click_icon(2, b.world);

    CHECK(b.box.active_index() == NO_TOOL);
    CHECK(b.box.active() == nullptr);
    CHECK_FALSE(b.box.preview(Bench::at(5.0f)).ghost);
    CHECK_FALSE(b.box.preview(Bench::at(5.0f)).target_probe);
    CHECK(b.box.preview(Bench::at(5.0f)).ring_brush == nullptr);
    // ЗА СОБОЙ УБРАЛ РОВНО ОДИН РАЗ. Дважды — значит уборка живёт в двух
    // местах, и это ровно тот дефект, из-за которого деталь оставалась в руке.
    CHECK(b.tools[2]->deselects == 1);
    // И РУКА ПУСТА НЕ НА СЛОВАХ: щелчок больше никому не доходит.
    const ToolTickReport tick = b.click(Bench::at(5.0f));
    CHECK_FALSE(tick.pressed);
    CHECK(b.tools[2]->presses == 0);
    MESSAGE("после сброса: активный " << (b.box.active_index() == NO_TOOL ? -1 : 0)
                                      << ", нажатий " << b.tools[2]->presses
                                      << ", уборок " << b.tools[2]->deselects);
}

// ============================================================================
// 3. ОДИН ИНСТРУМЕНТ ЗА РАЗ — ПО УСТРОЙСТВУ
// «нельзя бы было поймать ошибки, что я сразу два инструмента в руке держу»
// ============================================================================
TEST_CASE("щелчок доходит РОВНО до одного инструмента") {
    Bench b;
    b.box.click_icon(1, b.world);
    // Настройки открыты у ТРЕТЬЕГО, что раньше и взводило второго хозяина
    // кнопки: «выбран режим постановки ИЛИ открыт список объектов».
    b.box.click_settings(2);

    b.click(Bench::at(4.0f));

    int total = 0;
    for (const FakeTool* t : b.tools) {
        total += t->presses;
    }
    CHECK(total == 1);
    CHECK(b.tools[1]->presses == 1);
    MESSAGE("нажатий всего " << total << " (у 0: " << b.tools[0]->presses << ", у 1: "
                             << b.tools[1]->presses << ", у 2: " << b.tools[2]->presses
                             << ")");

    // КОНТРОЛЬ: сумма считается не на пустом месте — второй щелчок с ДРУГИМ
    // активным даёт вторую единицу, и она приходит другому инструменту.
    b.box.click_icon(0, b.world);
    b.click(Bench::at(4.0f));
    total = 0;
    for (const FakeTool* t : b.tools) {
        total += t->presses;
    }
    CHECK(total == 2);
    CHECK(b.tools[0]->presses == 1);
}

// ============================================================================
// 4. R ПЕРЕКЛЮЧАЕТ РЕЖИМ УКАЗАТЕЛЯ В ОБЕ СТОРОНЫ И НИ ОТ ЧЕГО НЕ ЗАВИСИТ
// «надо нажать на R и также нажать R чтобы выйти из этого режима»
// ============================================================================
TEST_CASE("R переключает режим указателя в обе стороны, что бы ни было открыто") {
    Bench b;
    CHECK_FALSE(b.box.pointer_mode()); // изначально мышь у камеры

    b.box.toggle_pointer_mode();
    CHECK(b.box.pointer_mode());
    b.box.toggle_pointer_mode();
    CHECK_FALSE(b.box.pointer_mode());

    // ТЕПЕРЬ ТО ЖЕ САМОЕ, НО С ОТКРЫТЫМИ НАСТРОЙКАМИ И ИНСТРУМЕНТОМ В РУКЕ.
    // Это и есть «не зависит от того, открыто ли что-то»: раньше курсор
    // освобождался догадкой о том, где висит указатель, и залипал.
    b.box.click_icon(0, b.world);
    b.box.click_settings(1);
    b.box.toggle_pointer_mode();
    CHECK(b.box.pointer_mode());
    CHECK(b.box.settings_index() == 1); // окно не тронуто
    CHECK(b.box.active_index() == 0);   // рука не тронута
    b.box.toggle_pointer_mode();
    CHECK_FALSE(b.box.pointer_mode());

    // И РЕЖИМ УКАЗАТЕЛЯ ОТДАЁТ ЩЕЛЧОК ИНТЕРФЕЙСУ, а не миру — иначе «режим»
    // был бы словом без последствий.
    b.box.toggle_pointer_mode();
    const ToolTickReport blocked = b.click(Bench::at(5.0f));
    CHECK(blocked.blocked);
    CHECK(b.tools[0]->presses == 0);
    // КОНТРОЛЬ: тот же щелчок вне режима указателя доходит.
    b.box.toggle_pointer_mode();
    const ToolTickReport acted = b.click(Bench::at(5.0f));
    CHECK(acted.pressed);
    CHECK(b.tools[0]->presses == 1);
}

// ============================================================================
// 5. ДАЛЬНОСТЬ: «я не должен уметь за 1000 км что-то строить»
// ============================================================================
TEST_CASE("щелчок дальше потолка не делает ничего, ближе — делает") {
    Bench b;
    b.box.click_icon(0, b.world);
    b.box.set_reach_ceiling_m(20.0f);
    REQUIRE(b.box.active_reach_m() == doctest::Approx(20.0f));

    // ДАЛЬШЕ ПОТОЛКА — НИЧЕГО. Не «почти ничего»: ноль нажатий.
    const ToolTickReport far_tick = b.click(Bench::at(1000.0f));
    CHECK(far_tick.out_of_reach);
    CHECK_FALSE(far_tick.pressed);
    CHECK(b.tools[0]->presses == 0);

    // БЛИЖЕ — ДЕЛАЕТ. Это контроль: без него «ничего не произошло» прошло бы
    // на инструменте, который не работает вовсе.
    const ToolTickReport near_tick = b.click(Bench::at(5.0f));
    CHECK(near_tick.pressed);
    CHECK(b.tools[0]->presses == 1);
    CHECK(b.tools[0]->last_press_distance_m == doctest::Approx(5.0f));
    MESSAGE("потолок " << b.box.active_reach_m() << " м: с 1000 м нажатий "
                       << 0 << ", с 5 м — " << b.tools[0]->presses);

    // ГРАНИЦА ПРИНАДЛЕЖИТ ПОТОЛКУ, и она названа: ровно 20 м работает, 20.5 нет.
    b.tools[0]->presses = 0;
    CHECK(b.click(Bench::at(20.0f)).pressed);
    CHECK_FALSE(b.click(Bench::at(20.5f)).pressed);
    CHECK(b.tools[0]->presses == 1);

    // ПОТОЛОК ОБЩИЙ, НО ИНСТРУМЕНТ МОЖЕТ БЫТЬ КОРОТКОРУКИМ. Меньшее из двух —
    // и проверяется обе стороны, иначе min() и max() неразличимы.
    auto shorty = std::make_unique<FakeTool>("shorty", 6.0f, false);
    FakeTool* shorty_raw = shorty.get();
    const std::size_t i = b.box.add(std::move(shorty));
    b.box.click_icon(i, b.world);
    CHECK(b.box.active_reach_m() == doctest::Approx(6.0f));
    CHECK_FALSE(b.click(Bench::at(10.0f)).pressed);
    CHECK(b.click(Bench::at(4.0f)).pressed);
    CHECK(shorty_raw->presses == 1);

    // И ПОТОЛОК ВЫИГРЫВАЕТ, КОГДА ОН НИЖЕ.
    b.box.set_reach_ceiling_m(3.0f);
    CHECK(b.box.active_reach_m() == doctest::Approx(3.0f));
    CHECK_FALSE(b.click(Bench::at(4.0f)).pressed);
    CHECK(shorty_raw->presses == 1);
}

TEST_CASE("прицел, не встретивший ничего, вне досягаемости любого инструмента") {
    Bench b;
    b.box.click_icon(0, b.world);
    ToolAim sky = Bench::at(3.0f);
    sky.hit = false;
    CHECK_FALSE(b.box.in_reach(sky));
    CHECK_FALSE(b.click(sky).pressed);
    // КОНТРОЛЬ: тот же прицел, но встретивший землю, — принимается.
    CHECK(b.box.in_reach(Bench::at(3.0f)));
}

// ============================================================================
// 6. ЗНАЧКИ ОДИНАКОВОГО РАЗМЕРА И РАЗЛИЧИМЫЕ
// «текст кнопок сверху замени на картинки одинаковых размеров»
// ============================================================================
TEST_CASE("значки инструментов: один размер, непустые, попарно разные") {
    constexpr int PX = 32;
    const ToolIcon ICONS[] = {ToolIcon::Height,      ToolIcon::Surface,
                              ToolIcon::Select,      ToolIcon::Place,
                              ToolIcon::Plant,       ToolIcon::Path,
                              ToolIcon::HouseVertex, ToolIcon::HouseLine,
                              ToolIcon::HouseSurface, ToolIcon::Settings};
    std::vector<std::vector<std::uint8_t>> baked;
    for (const ToolIcon icon : ICONS) {
        std::vector<std::uint8_t> rgba;
        REQUIRE(bake_tool_icon(icon, PX, rgba));
        // ОДИНАКОВЫЙ РАЗМЕР — СВОЙСТВО ПЕКАРЯ, а не обещание про шесть файлов.
        CHECK(rgba.size() == static_cast<std::size_t>(PX) * PX * 4u);
        std::size_t inked = 0;
        for (std::size_t i = 3; i < rgba.size(); i += 4) {
            inked += rgba[i] > 0 ? 1u : 0u;
        }
        // НЕПУСТОЙ: пустая картинка проходит проверку размера безупречно.
        CHECK(inked > 0);
        MESSAGE("значок " << static_cast<int>(icon) << ": закрашено " << inked
                          << " из " << (PX * PX));
        baked.push_back(std::move(rgba));
    }
    // ПОПАРНО РАЗНЫЕ: два инструмента с одной картинкой — это та самая
    // путаница, ради устранения которой картинки и заказаны.
    for (std::size_t i = 0; i < baked.size(); ++i) {
        for (std::size_t j = i + 1; j < baked.size(); ++j) {
            CHECK(baked[i] != baked[j]);
        }
    }
    // КОНТРОЛЬ ПРИБОРА: «не картинка» обязана быть отказом, иначе сравнение
    // выше сравнивало бы шесть одинаковых пустых буферов и молчало.
    std::vector<std::uint8_t> none;
    CHECK_FALSE(bake_tool_icon(ToolIcon::Count, PX, none));
}

TEST_CASE("размер значка — аргумент, а не константа внутри рисунка") {
    for (const int px : {16, 32, 48}) {
        std::vector<std::uint8_t> rgba;
        REQUIRE(bake_tool_icon(ToolIcon::Place, px, rgba));
        CHECK(rgba.size() == static_cast<std::size_t>(px) * px * 4u);
        std::size_t inked = 0;
        for (std::size_t i = 3; i < rgba.size(); i += 4) {
            inked += rgba[i] > 0 ? 1u : 0u;
        }
        // Доля закрашенного держится: рисунок в ДОЛЯХ иконки, а не в пикселях.
        const double share = static_cast<double>(inked) / (px * px);
        CHECK(share > 0.15);
        CHECK(share < 0.85);
        MESSAGE("значок «постройка» при " << px << " px: закрашено "
                                          << (100.0 * share) << "%");
    }
}

// ============================================================================
// 7. МЕЛОЧИ, КОТОРЫЕ ЛОМАЮТСЯ МОЛЧА
// ============================================================================
TEST_CASE("протяжка идёт тому же инструменту, а отпускание — ровно одно") {
    Bench b;
    b.box.click_icon(0, b.world);
    const ToolAim aim = Bench::at(5.0f);
    b.box.update(aim, 0.016f, true, b.world);
    b.box.update(aim, 0.016f, true, b.world);
    b.box.update(aim, 0.016f, true, b.world);
    b.box.update(aim, 0.016f, false, b.world);
    b.box.update(aim, 0.016f, false, b.world);
    CHECK(b.tools[0]->presses == 1);
    CHECK(b.tools[0]->drags == 2);
    CHECK(b.tools[0]->releases == 1);
}

TEST_CASE("мазок, начатый на панели, не начинается вовсе") {
    Bench b;
    b.box.click_icon(0, b.world);
    // Кнопка нажата, когда указатель НА ПАНЕЛИ...
    b.box.update(Bench::at(5.0f, true), 0.016f, true, b.world);
    // ...и уехала с панели, не отпускаясь: копать всё равно нельзя.
    b.box.update(Bench::at(5.0f), 0.016f, true, b.world);
    CHECK(b.tools[0]->presses == 0);
    CHECK(b.tools[0]->drags == 0);
    // КОНТРОЛЬ: отпустил, нажал заново вне панели — работает.
    b.box.update(Bench::at(5.0f), 0.016f, false, b.world);
    b.box.update(Bench::at(5.0f), 0.016f, true, b.world);
    CHECK(b.tools[0]->presses == 1);
}

TEST_CASE("смена инструмента прерывает недоделанный мазок ровно один раз") {
    Bench b;
    b.box.click_icon(0, b.world);
    b.box.update(Bench::at(5.0f), 0.016f, true, b.world);
    REQUIRE(b.tools[0]->presses == 1);
    REQUIRE(b.tools[0]->releases == 0);
    b.box.click_icon(1, b.world);
    CHECK(b.tools[0]->releases == 1);
    CHECK(b.tools[0]->deselects == 1);
    // И НОВЫЙ ИНСТРУМЕНТ НЕ УНАСЛЕДОВАЛ НАЖАТУЮ КНОПКУ: держать её дальше не
    // значит копать новым.
    b.box.update(Bench::at(5.0f), 0.016f, true, b.world);
    CHECK(b.tools[1]->drags == 0);
    CHECK(b.tools[1]->presses == 0);
}

TEST_CASE("ESC закрывает настройки и говорит, было ли что закрывать") {
    Bench b;
    CHECK_FALSE(b.box.close_settings()); // нечего закрывать -> дальше в паузу
    b.box.click_settings(1);
    CHECK(b.box.settings_open());
    CHECK(b.box.close_settings());
    CHECK_FALSE(b.box.settings_open());
    // Шестерёнка — то же окно и тот же ESC.
    b.box.click_gear();
    CHECK(b.box.common_settings_open());
    CHECK(b.box.close_settings());
    CHECK_FALSE(b.box.common_settings_open());
}

TEST_CASE("потолок дальности зажат в разумные пределы") {
    Bench b;
    b.box.set_reach_ceiling_m(100000.0f);
    CHECK(b.box.reach_ceiling_m() == doctest::Approx(dfn::app::EDITOR_REACH_MAX_M));
    b.box.set_reach_ceiling_m(-5.0f);
    CHECK(b.box.reach_ceiling_m() == doctest::Approx(dfn::app::EDITOR_REACH_MIN_M));
}

TEST_CASE("инструменты адресуются по имени, а не по номеру в чужом файле") {
    Bench b;
    CHECK(b.box.index_of("place") == 2);
    CHECK(b.box.index_of("нет такого") == NO_TOOL);
}

// ЗА ПРЕДЕЛОМ ДАЛЬНОСТИ ЧЕЛОВЕК ВИДИТ ПОЧЕМУ, А МИР МОЛЧИТ (заказ 18.08:
// «сейчас не понятно могу ли я рисовать / строить из-за расстояния, нужно
// индикатор добавить, что далеко цель»).
//
// Держится ПАРА, и порознь каждая половина проходила бы на сломанном коде:
// подпись без гашения превью оставляет зелёное кольцо там, куда щелчок не
// достанет, а гашение без подписи — молчание, неотличимое от «инструмент
// сломался». Обе половины живут в ящике, а не в инструментах: потолок общий,
// значит и объяснение общее.
TEST_CASE("за пределом дальности: подпись говорит почему, мир не обещает") {
    Bench b;
    b.box.set_reach_ceiling_m(10.0f);
    b.box.click_icon(0, b.world);

    // В ПРЕДЕЛАХ: инструмент отвечает сам, и превью у него есть.
    const ToolAim near_aim = Bench::at(3.0f);
    REQUIRE(b.box.in_reach(near_aim));
    const ToolStatus near_st = b.box.status(near_aim);
    CHECK(near_st.ready);
    CHECK(std::string_view(near_st.key) != "tool.hint.too_far");

    // ЗА ПРЕДЕЛОМ: подпись НЕ готова, называет причину и НЕСЁТ ОБА ЧИСЛА.
    // Числа проверяются потому, что «далеко» не говорит, насколько подойти, а
    // «46 m > 10 m» говорит — и ловит случай, когда предел ниже, чем думает
    // человек.
    const ToolAim far_aim = Bench::at(46.0f);
    REQUIRE_FALSE(b.box.in_reach(far_aim));
    const ToolStatus far_st = b.box.status(far_aim);
    CHECK_FALSE(far_st.ready);
    CHECK(std::string_view(far_st.key) == "tool.hint.too_far");
    CHECK(far_st.text.find("46") != std::string::npos);
    CHECK(far_st.text.find("10") != std::string::npos);

    // И МИР НИЧЕГО НЕ ОБЕЩАЕТ: ни призрака, ни кольца, ни прохода по цели.
    const ToolPreview far_pv = b.box.preview(far_aim);
    CHECK_FALSE(far_pv.ghost);
    CHECK_FALSE(far_pv.target_probe);
    CHECK(far_pv.ring_brush == nullptr);
}

// ДОЛГ, НАЗВАННЫЙ ВСЛУХ (18.08). Двух проверок здесь НЕТ, и не потому, что о
// них забыли: «свои настройки открывает только инструмент выбора» и «посадка
// сама берёт первую породу» живут в SelectTool и PlantTool, а те тянут ImGui
// ради draw_settings — эта же цель намеренно линкует только то, в чём окна нет
// (правило 3). Обе правки проверены ЗАПУСКОМ и строкой в stderr, что слабее
// рукава и так и записано.
// Чтобы долг закрылся, у инструментов надо отделить РЕШЕНИЕ от РИСОВАНИЯ так
// же, как это уже сделано у палитры (EditorPaletteFamily.cpp вынесен из цели
// ровно за это). Сегодня файл занят другой работой — тропы, — и лезть туда
// значит переписывать под чужой рукой.
// НАСТРОЙКИ ОТКРЫВАЕТ ТОТ, КОМУ ОНИ НУЖНЫ (жалоба 18.08: «когда я сажать
// пытаюсь, мне открывается меню инструмента посадки, бредовое поведение»).
//
// Решение жило в App.cpp условием «есть выбранная расстановка и настройки
// закрыты» — и НЕ спрашивало, чей сейчас ход, поэтому срабатывало у всех.
// App.cpp владеет окном и не проверяется, поэтому поймать это мог только
// человек за игрой. Здесь у того же решения есть прибор.
