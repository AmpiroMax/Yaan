/*
Module: engine/app
File: engine/app/sources/AppEditorWiring.cpp

Responsibility:
- ОБЪЯВЛЕНИЕ РЕДАКТОРА: единственный метод App::wire_editor_panels() — полка
  деталей, крючки панелей (мерка, картинка, выбор), весь ToolWorld и девять
  инструментов в порядке клавиш 1..9.

Dependencies:
- Uses: App.h, EditorPaletteThumb.h (кэш картинок), EditorToolPath.h (тропа),
  Localization.h, ContentHash.h, world/Scene.h.
- Used by: dfn_app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЭТО ТОТ ЖЕ КЛАСС, ДРУГОЙ ФАЙЛ (тот же приём, что у AppHouse.cpp и AppWorld.cpp):
  объявление остаётся в App.h, сюда уезжает только ТЕЛО.
- ЗДЕСЬ НЕТ И НЕ ДОЛЖНО БЫТЬ НИ ОДНОГО `void App::on_*(`. Обработчики клавиш
  живут в AppInput.cpp и AppHouse.cpp, и tests/app/ActionRoutesTests.cpp требует
  РОВНО ОДНО определение на эти два файла — третий файл с обработчиком не сделал
  бы тест красным, он сделал бы его слепым.
- ДЕСЯТЫЙ ИНСТРУМЕНТ ДОБАВЛЯЕТСЯ СТРОКОЙ СЮДА, а не веткой в run().
*/

#include "engine/app/sources/App.h"
#include "engine/world/sources/HouseFile.h"

#include "engine/app/sources/Localization.h"
// Картинки деталей в меню объектов. Включено ЗДЕСЬ, а не в App.h: имя знает
// только эта функция, и App.h остаётся самым широким заголовком дерева.
#include "engine/editor/sources/EditorPaletteThumb.h"
#include "engine/editor/sources/EditorToolPath.h"

#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/world/sources/Scene.h"

#include <fstream>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>

namespace dfn::app {

void App::wire_editor_panels() {
    // ONCE, AND NOT ON A KEYPRESS. The panels used to be declared inside the B
    // handler, so the object menu did not exist until somebody pressed B — and
    // neither did its chip on the toolbar, which is the one place the user is
    // supposed to be able to see what the editor can do. A tool whose menu is
    // invisible until you already know the shortcut is a tool for whoever
    // wrote it.
    if (palette_wired_) {
        return;
    }
    palette_wired_ = true;

    // ОДИН СПИСОК ДЕТАЛЕЙ НА ДВУХ ПОТРЕБИТЕЛЕЙ (правило 32). Здесь их было ДВА:
    // панель наполнялась вот этим вызовом при первом кадре редактора, а рука
    // строителя — своим, отдельным, и ТОЛЬКО когда список открывали клавишей B.
    // Открыв тот же список фишкой на панели инструментов, человек получал
    // непустое меню и ПУСТУЮ руку: выбор искал имя среди нуля групп, не находил
    // и молча ничего не делал. Пользователь описал это точно: «не могу выбирать
    // объекты, только один брус выбрал, остальные не выбираются» — держалась та
    // деталь, что попала в руку раньше, а все последующие щелчки уходили в
    // никуда. Полка читается ОДИН раз и кладётся обоим.
    build_groups_ = build_palette(gallery_objects_dir_);
    std::vector<std::string> names;
    for (const BuildGroup& g : build_groups_) {
        names.insert(names.end(), g.names.begin(), g.names.end());
    }
    palette_.set_parts(std::move(names));
    palette_.set_map_id(gallery_scene_);
    (void)palette_.load_state("editor_palette.state");
    PaletteHooks hooks;
    // ТА ЖЕ МЕРКА, ЧТО У ПРИЗРАКА И У СУДЬИ: панель не заводит второй линейки
    // (правило 32).
    hooks.measure = [this](const std::string& name, PartMeasure& out) {
        BuildJudgeCtx ctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
        const render::ObjectExtent* e = build_extent(&ctx, name);
        if (e == nullptr) {
            return false;
        }
        out.width_m = e->hi.x - e->lo.x;
        out.depth_m = e->hi.y - e->lo.y;
        out.height_m = e->top - e->bottom;
        const auto obj = scene_objects_.find(name);
        out.triangles = obj == scene_objects_.end()
                            ? 0
                            : static_cast<int>((obj->second.wood.indices.size()
                                                + obj->second.bark.indices.size()) / 3);
        out.known = true;
        return true;
    };
    // КАРТИНКА ДЕТАЛИ. Крючок thumbnail существовал с 17.08 и не был подключён
    // никем — панель спрашивала картинку каждый кадр, получала 0 и рисовала
    // подпись, то есть пользователь видел 2412 названий и ни одного предмета
    // («у объектов в меню объектов нет всё ещё предпросмотра», 18.08).
    //
    // ПОЧЕМУ КЭШ — СТАТИК ФУНКЦИИ, А НЕ ПОЛЕ App. Эта функция выполняется РОВНО
    // ОДИН РАЗ (palette_wired_), а зона правки — App.cpp; поле потребовало бы
    // App.h, который сейчас правят другие. Живёт до конца процесса, деструктор
    // текстур не трогает (их отдаёт clear(), и звать её после гибели интерфейса
    // нельзя) — то есть на выходе они просто уходят вместе с контекстом.
    static ThumbCache thumbs;
    thumbs.set_bake([this](const std::string& name, int px, std::vector<std::uint8_t>& rgba) {
        // ТА ЖЕ ЗАГРУЗКА, ЧТО У ПРИЗРАКА (правило 32): build_extent приносит
        // деталь с полки в scene_objects_, если её там ещё нет. Второй свой
        // читатель .dfo разъехался бы с первым в тот день, когда полка сменит
        // имя, и разъехался бы молча.
        BuildJudgeCtx ctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
        if (build_extent(&ctx, name) == nullptr) {
            return false;
        }
        const auto it = scene_objects_.find(name);
        if (it == scene_objects_.end()) {
            return false;
        }
        return bake_object_thumbnail(it->second, px, rgba);
    });
    thumbs.set_upload([this](int px, const std::uint8_t* rgba) {
        return static_cast<std::uint64_t>(editor_ui_.make_texture(
            static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(px), rgba));
    });
    thumbs.set_drop([this](std::uint64_t texture) {
        editor_ui_.drop_texture(static_cast<EditorTexture>(texture));
    });
    hooks.thumbnail = [](const std::string& name, int px) {
        return static_cast<EditorTexture>(thumbs.get(name, px));
    };
    hooks.begin_frame = []() { thumbs.begin_frame(); };
    hooks.on_pick = [this](const std::string& name) {
        // Рука берёт то, что выбрали: призрак меняется в тот же кадр.
        for (std::size_t g = 0; g < build_groups_.size(); ++g) {
            const auto& v = build_groups_[g].names;
            const auto it = std::find(v.begin(), v.end(), name);
            if (it != v.end()) {
                build_group_ = g;
                build_item_ = static_cast<std::size_t>(it - v.begin());
                return;
            }
        }
        // ПРОМАХ ГОВОРИТ ВСЛУХ. Молчащий промах и есть то, из-за чего этот
        // отказ дожил до пользователя: щелчок по детали не делал НИЧЕГО и не
        // оставлял следа, так что и жалоба, и разбор начинались с «меню вроде
        // работает». Меню и рука обязаны видеть одну полку; если нет — это
        // видно с первой строки.
        std::fprintf(stderr,
                     "[build] выбрана деталь «%s», которой нет в полке руки "
                     "(групп %zu). Меню и рука читают РАЗНЫЕ списки.\n",
                     name.c_str(), build_groups_.size());
    };
    PropsHooks ph;
    ph.apply = [this]() { return apply_selection_edit(); };
    ph.remove = [this]() {
        if (selected_ < scene_doc_.placements.size()) {
            build_target_ = selected_;
            (void)build_delete();
            selected_ = static_cast<std::size_t>(-1);
            props_.object.clear();
        }
    };

    // ЧТО ИНСТРУМЕНТ МОЖЕТ СДЕЛАТЬ С МИРОМ — один набор крючков на всех, взятый
    // ящиком по ссылке. Раньше на этом месте объявлялись ТРИ ПАНЕЛИ, каждая со
    // своей фишкой на полосе, а действия инструментов лежали россыпью по run():
    // список объектов взводил щелчок, кисть висела на той же кнопке отдельно, а
    // посадка вызывалась из обработчика ПОСТРОЙКИ по Shift. Теперь у кнопки
    // ровно один хозяин — активный инструмент, — и это свойство устройства, а не
    // соблюдённая договорённость (docs/audits/AUDIT_EDITOR_TOOLS.md).
    ToolWorld& tw = editor_ui_.tool_world();
    // ПАНЕЛЬ «СВОЙСТВА» — отдельная докнутая (UX-переделка 20.08): свойства
    // выбранного видны при ЛЮБОМ инструменте и не раздувают панели построек.
    {
        EditorPanel props;
        props.id = "house.properties";
        props.title_key = "editor.panel.properties";
        props.side = EditorPanelSide::Right;
        props.extent_px = 400.0f;
        props.open = true;
        props.draw = [this] {
            draw_house_properties_panel(house_, &editor_ui_.tool_world());
        };
        editor_ui_.add_panel(std::move(props));
    }
    tw.terrain_dab = [this](const TerrainBrush& brush, glm::vec2 centre, float dt_s) {
        return apply_terrain_dab(brush, centre, dt_s);
    };
    tw.finish_stroke = [this]() { finish_stroke(); };
    tw.add_pad = [this](const world::ScenePad& pad) {
        // A pad enters the world through the generation parameters, which are
        // fixed at map load — so it lands in the composition and takes effect on
        // the next load. SAYING SO is the whole point: a mode that silently does
        // nothing is the complaint this began with.
        scene_doc_.pads.push_back(pad);
        scene_dirty_ = true;
        std::fprintf(stderr, "[кисть] «ровно»: [pad] на (%.1f, %.1f) записан в "
                             "композицию — земля примет его при следующей "
                             "загрузке карты\n",
                     static_cast<double>(pad.center.x), static_cast<double>(pad.center.y));
    };
    tw.place_part = [this]() {
        if (!build_place()) {
            return false;
        }
        std::fprintf(stderr, "[build] поставлено %s (%.2f %.2f %.2f)\n",
                     scene_doc_.placements.back().object.c_str(),
                     static_cast<double>(scene_doc_.placements.back().position.x),
                     static_cast<double>(scene_doc_.placements.back().position.y),
                     static_cast<double>(scene_doc_.placements.back().position.z));
        return true;
    };
    tw.delete_target = [this]() { return build_delete(); };
    tw.has_target = [this]() { return build_target_ < scene_doc_.placements.size(); };
    tw.clear_ghost = [this]() { clear_build_ghost(); };
    tw.ghost_ready = [this](std::string& reason) {
        if (!build_ghost_.valid()) {
            reason.clear(); // "деталь не выбрана" — the tool says which
            return false;
        }
        if (!build_verdict_.allowed) {
            // THE JUDGE'S OWN SENTENCE, from BuildTool's table — the same words
            // the ghost's red edges stand for. One verdict rendered twice.
            reason = std::string(localized(serialization::fnv1a64(build_verdict_.reason)));
            return false;
        }
        return true;
    };
    tw.plant_dab = [this](const PlantBrush& brush, glm::vec2 centre) {
        return plant_dab_here(brush, centre);
    };
    // ПОЛКА ГОТОВЫХ ПОСТРОЕК (20.08): список из assets/houses, постановка
    // под прицел с прилипанием к сетке постройки, снятие последней. Запись —
    // в секцию [house] сцены; дом поднимается тем же load_scene_houses, что
    // и на входе в мир, — второй дороги нет.
    tw.house_assets = [this]() {
        std::vector<std::string> out;
        std::error_code ec;
        for (const auto& it :
             std::filesystem::directory_iterator("assets/houses", ec)) {
            if (it.path().extension() == ".dfh") {
                out.push_back(it.path().stem().string());
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    };
    tw.place_house_at_aim = [this](const std::string& name, float yaw_deg) {
        const ToolAim aim = aim_this_frame();
        glm::vec3 pos = aim.point;
        if (house_.grid_on()) {
            pos = house_.snap_to_grid(pos);
        }
        world::ScenePlacedHouse H;
        H.file = "assets/houses/" + name + ".dfh";
        H.position = pos;
        H.yaw = yaw_deg * 0.017453292f;
        scene_doc_.houses.push_back(H);
        load_scene_houses();
        std::fprintf(stderr, "[постройка] полка: %s -> (%.1f %.1f %.1f) yaw %.0f°\n",
                     name.c_str(), static_cast<double>(pos.x),
                     static_cast<double>(pos.y), static_cast<double>(pos.z),
                     static_cast<double>(yaw_deg));
    };
    tw.unpack_house_at_aim = [this]() {
        if (scene_doc_.houses.empty()) {
            std::fprintf(stderr, "[постройка] распаковывать нечего\n");
            return;
        }
        const ToolAim aim = aim_this_frame();
        // Ближайшая по XZ к прицелу СРЕДИ ПОДНЯТЫХ (аудит #3: поиск по записям
        // сцены разъезжался с placed_houses_, когда чей-то файл не читался, и
        // распаковывался сосед); дальше 30 м — отказ вслух.
        std::size_t best = placed_houses_.size();
        float best_d = 30.0f;
        for (std::size_t i = 0; i < placed_houses_.size(); ++i) {
            const glm::vec3 c = placed_houses_[i].pos;
            const float d = glm::length(glm::vec2{c.x - aim.point.x, c.z - aim.point.z});
            if (d < best_d) {
                best_d = d;
                best = i;
            }
        }
        if (best >= placed_houses_.size()) {
            std::fprintf(stderr, "[постройка] под прицелом нет поднятой постройки "
                                 "(30 м)\n");
            return;
        }
        const PlacedHouse& ph = placed_houses_[best];
        const float c = std::cos(ph.yaw);
        const float sn = std::sin(ph.yaw);
        const auto res = house_.mutate("распаковал постройку", [&](world::HouseGraph& g) {
            return g.merge_from(ph.graph, [&](glm::vec3 l) {
                const glm::vec3 w = ph.pos
                                  + glm::vec3{l.x * c + l.z * sn, l.y,
                                              -l.x * sn + l.z * c};
                return house_.to_local(w);
            });
        });
        if (!res.ok) {
            std::fprintf(stderr, "[постройка] распаковка: %s\n", res.why.c_str());
            return;
        }
        scene_doc_.houses.erase(scene_doc_.houses.begin()
                                + static_cast<std::ptrdiff_t>(ph.scene_index));
        load_scene_houses();
        std::fprintf(stderr,
                     "[постройка] распакована постройка #%zu: правь стены и якоря\n",
                     best);
    };
    tw.save_session_house = [this](const std::string& name) {
        if (house_.graph().vertex_count() == 0) {
            std::fprintf(stderr, "[постройка] сохранять нечего — сессия пуста\n");
            return;
        }
        // НОРМИРОВКА К НУЛЮ: файл библиотеки локален — минимальный угол
        // постройки становится началом, и дом кладётся на любую карту.
        glm::vec3 lo{1e9f};
        for (const auto& v : house_.graph().vertices()) {
            lo = glm::min(lo, house_.graph().resolved_local(v.id));
        }
        world::HouseGraph out;
        const auto r = out.merge_from(house_.graph(),
                                      [&](glm::vec3 l) { return l - lo; });
        if (!r.ok) {
            std::fprintf(stderr, "[постройка] сохранение: %s\n", r.why.c_str());
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories("assets/houses", ec);
        const std::string path = "assets/houses/" + name + ".dfh";
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "[постройка] не открылся %s\n", path.c_str());
            return;
        }
        f << world::write_house(out);
        std::fprintf(stderr, "[постройка] сохранено: %s (вершин %zu)\n",
                     path.c_str(), out.vertex_count());
    };
    tw.apply_style_to_draft =
        [this](const std::vector<std::pair<std::string, std::string>>& kv) {
            if (house_line_tool_ != nullptr) {
                house_line_tool_->apply_style_to_draft(kv);
            }
            if (house_surface_tool_ != nullptr) {
                house_surface_tool_->apply_style_to_draft(kv);
            }
            std::fprintf(stderr, "[постройка] стиль лёг в заготовку (%zu пар)\n",
                         kv.size());
        };
    tw.remove_last_house = [this]() {
        if (scene_doc_.houses.empty()) {
            std::fprintf(stderr, "[постройка] полка: убирать нечего\n");
            return;
        }
        scene_doc_.houses.pop_back();
        load_scene_houses();
    };
    tw.material_swatch = [this](int surface, int tone, int px) {
        return house_material_swatch(surface, tone, px);
    };
    tw.wall_example = [this](int variant, int px) { return house_wall_example(variant, px); };
    tw.ground_height = [this](glm::vec2 xz) {
        // ЗАКОНЧЕННАЯ земля, правки рукой включительно: линия тропы обязана
        // лежать на той поверхности, на которую человек смотрит. Ноль вместо
        // «не знаю» утопил бы её на десятки метров — этот отказ уже был у
        // кольца кисти и пойман кадром сдачи.
        return chunks_.height_at(xz).value_or(0.0f);
    };
    tw.last_dab = [this](int& samples, float& worst_m) {
        samples = last_dab_samples_;
        worst_m = last_dab_worst_m_;
    };
    tw.relief_paths = [this]() -> const std::vector<world::ReliefPath>* {
        return &relief_.paths();
    };
    tw.commit_path = [this](std::size_t index, const world::ReliefPath* path) {
        return commit_relief_path(index, path);
    };
    tw.open_own_settings = [this]() {
        auto& box = editor_ui_.toolbox();
        if (box.active() != nullptr && box.settings_index() == NO_TOOL) {
            box.click_settings(box.active_index());
        }
    };
    tw.select_target = [this]() -> bool {
        selected_ = build_target_;
        props_.refusal.clear();
        if (selected_ >= scene_doc_.placements.size()) {
            props_.object.clear();
            return false;
        }
        const world::Placement& p = scene_doc_.placements[selected_];
        props_.object = p.object;
        props_.x = p.position.x;
        props_.y = p.position.y;
        props_.z = p.position.z;
        props_.yaw_deg = p.yaw * 180.0f / glm::pi<float>();
        props_.scale = p.scale;
        props_.group = p.group;
        props_.width_m = props_.depth_m = props_.height_m = 0.0f;
        BuildJudgeCtx mctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
        if (const render::ObjectExtent* e = build_extent(&mctx, p.object)) {
            props_.width_m = e->hi.x - e->lo.x;
            props_.depth_m = e->hi.y - e->lo.y;
            props_.height_m = e->top - e->bottom;
        }
        std::fprintf(stderr, "[выбор] %s (%.2f %.2f %.2f), поворот %.1f°\n",
                     p.object.c_str(), static_cast<double>(p.position.x),
                     static_cast<double>(p.position.y),
                     static_cast<double>(p.position.z),
                     static_cast<double>(p.yaw * 180.0f / glm::pi<float>()));
        return true;
    };

    // ДЕВЯТЬ ИНСТРУМЕНТОВ, И ПОРЯДОК ЗДЕСЬ — ПОРЯДОК НА ПОЛОСЕ И ПОРЯДОК
    // КЛАВИШ 1..9. Сам порядок назначен пользователем (19.08): «1 выбор,
    // 2 якоря, 3 прямые, 4 стены, 5 высота земли, 6 тропинка, 7 строительство
    // пропами, 8 рассада деревьев» — постройка съехала в начало, потому что ею
    // он пользуется чаще всего. Кисть поверхности в заказе не названа и стоит
    // девятой. Добавить десятый — написать класс и дописать строку сюда; ни
    // одного switch по дороге нет.
    EditorToolbox& box = editor_ui_.toolbox();
    house_.set_history(&history_);
    // 1 — ВЫБОР. Один инструмент выбирает объект сцены, якорь и прямую.
    {
        auto select = std::make_unique<SelectTool>(props_, std::move(ph));
        select->set_world(&tw);
        select->set_house(&house_);
        box.add(std::move(select));
    }
    // 2..4 — ПОСТРОЙКА: якоря, прямые, стены. Одна модель на троих; каждая
    // мутация пишет снимок внутри HouseSession::mutate, App про это не помнит.
    {
        auto vertex = std::make_unique<HouseVertexTool>(house_);
        vertex->set_world(&tw);
        box.add(std::move(vertex));
        auto line = std::make_unique<HouseLineTool>(house_);
        line->set_world(&tw);
        house_line_tool_ = line.get();
        box.add(std::move(line));
        auto surface = std::make_unique<HouseSurfaceTool>(house_);
        surface->set_world(&tw);
        house_surface_tool_ = surface.get();
        box.add(std::move(surface));
    }
    // 5 — ВЫСОТА ЗЕМЛИ. Мир нужен кисти ради читалки «Последний мазок».
    {
        auto height = std::make_unique<HeightBrushTool>();
        height->set_world(&tw);
        box.add(std::move(height));
    }
    // 6 — ТРОПА: у кисти центр и радиус, у тропы точки, порядок и два конца.
    {
        auto path = std::make_unique<PathTool>();
        path->set_world(&tw);
        box.add(std::move(path));
    }
    // 7 — СТРОИТЕЛЬСТВО ГОТОВЫМИ ДЕТАЛЯМИ.
    {
        auto place = std::make_unique<PlaceTool>(palette_, std::move(hooks));
        place->set_world(&tw);
        box.add(std::move(place));
    }
    // 8 — ПОСАДКА. Список пород и щелчок принадлежат одному хозяину.
    box.add(std::make_unique<PlantTool>([this]() -> const std::vector<std::string>& {
        if (plant_species_.empty()) {
            // ПОСАДКА ЧИТАЕТ ПОЛКУ ДЕРЕВЬЕВ, А НЕ ПОЛКИ КАРТЫ: на карте домов
            // полка манифеста равна assets/objects/parts;assets/objects/signs,
            // и список пород предлагал БРУСЬЯ. Дерево — не деталь дома.
            static constexpr const char* TREES = "assets/objects/trees";
            for (const BuildGroup& g : build_palette(TREES)) {
                for (const std::string& n : g.names) {
                    plant_species_.push_back(n);
                }
            }
            if (plant_species_.empty()) {
                std::fprintf(stderr,
                             "[посадка] полка деревьев «%s» пуста — сажать нечем\n",
                             TREES);
            }
        }
        return plant_species_;
    }));
    // 9 — КИСТЬ ПОВЕРХНОСТИ (в заказе не названа).
    {
        auto paint = std::make_unique<SurfacePaintTool>(editor_ui_);
        paint->set_world(&tw);
        box.add(std::move(paint));
    }
    // БИБЛИОТЕКА (UX-переделка 20.08): постройки + стили + сохранить +
    // распаковать. Последним в полосе — без цифры, выбирается кликом.
    {
        auto lib = std::make_unique<HouseLibraryTool>(house_);
        lib->set_world(&tw);
        box.add(std::move(lib));
    }
    props_wired_ = true;
}

} // namespace dfn::app
