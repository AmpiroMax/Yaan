/*
Created: 28:08:2026 - 11:14:50
Last updated: 28:08:2026 - 12:40:00
Module: engine/render
File: engine/render/sources/MaterialRegistry.cpp

Responsibility:
- Таблица именованных материалов и ПОЛНОЕ отображение пар (поверхность, тон)
  листа набора в материал. См. MaterialRegistry.h — там дизайн и причины.

Dependencies:
- Uses: MaterialRegistry.h, PartsAtlas.h. Ничего больше.
- Used by: RenderSystem, engine/app, tests/render/MaterialRegistryTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly; дизайн зоны — docs/design/MATERIALS.md.
- ДОПИСЫВАТЬ ТОЛЬКО В КОНЕЦ таблицы: порядковый — это идентичность (шапка .h).
*/
/*
UPD:
- 28:08:2026 - 11:14:50: Создан. Девять именованных материалов: восемь из
  списка владельца (oak-log, pine-board, brick-red, granite, plaster-white,
  thatch, linen, iron) плюс gold-leaf — тот самый случай, ради которого
  владелец назвал блик («для золота/металла уже есть нужда — герб»).
- 28:08:2026 - 12:40:00: ПЯТЬ ВЕЩЕСТВ ПИСЬМА (birch-bark, parchment, rag-paper,
  seal-wax, ink) — ТЗ владельца §7 требует их «в реестр с первого дня», и
  причина та же, что у золота: пока вещества нет, предмет им притворяется, а
  притворство потом стоит отмены. Шероховатость золота 0.60 вместо 0.22 —
  выведена развёрткой по кадрам, разбор у самого числа.
*/

#include "engine/render/sources/MaterialRegistry.h"

#include <array>

namespace dfn::render {
namespace {

// ЗОЛОТО ГЕРБА, снятое С САМОГО ГЕРБА, а не выдуманное. Доминирующий цвет
// вершин assets/objects/heraldry/oak-seal-relief.dfo — 0xFF3899C7, то есть
// rgb(199,153,56) из 72039 вершин поля. Материал обязан начинаться с того
// цвета, который уже принят владельцем: волна про БЛИК, а не про перекраску.
inline constexpr glm::vec3 GOLD_LEAF_TINT{199.0f / 255.0f, 153.0f / 255.0f,
                                          56.0f / 255.0f};

// ПОРЯДОК ЗАПИСЕЙ — ИДЕНТИЧНОСТЬ. Индекс в этом массиве и есть MaterialId.
constexpr std::array<Material, 15> TABLE{{
    // 0 — MATERIAL_NONE: «материала не названо», плоский цвет вершины.
    // ПУСТОЕ ИМЯ СОЗНАТЕЛЬНО: material_by_name("") обязан не находить ничего,
    // иначе опечатка в рецепте молча превратилась бы в «как раньше».
    Material{},

    // --- 1..6: вещества, у которых ЕСТЬ зерно, то есть плитка листа набора --
    Material{.name = "oak-log",
             .tiled = true,
             .surface = PartSurface::HewnTimber,
             .tone = PartTone::Dark,
             .tile_span_m = PARTS_TILE_SPAN_M,
             .roughness = 1.0f,
             .wear = 0.55f},
    Material{.name = "pine-board",
             .tiled = true,
             .surface = PartSurface::SawnBoard,
             .tone = PartTone::Mid,
             .tile_span_m = PARTS_TILE_SPAN_M,
             .roughness = 1.0f,
             .wear = 0.35f},
    Material{.name = "brick-red",
             .tiled = true,
             .surface = PartSurface::FiredClay,
             .tone = PartTone::Mid,
             .tile_span_m = PARTS_TILE_SPAN_M,
             .roughness = 1.0f,
             .wear = 0.40f},
    Material{.name = "granite",
             .tiled = true,
             .surface = PartSurface::Stone,
             .tone = PartTone::Weathered,
             .tile_span_m = PARTS_TILE_SPAN_M,
             // Гранит — НЕ ламберт: у полевого шпата и слюды есть собранный
             // блик, и именно он отличает камень от крашеной штукатурки в
             // косом солнце. 0.86 — «почти матовый», не зеркало.
             .roughness = 0.86f,
             .wear = 0.70f},
    Material{.name = "plaster-white",
             .tiled = true,
             .surface = PartSurface::Plaster,
             .tone = PartTone::Light,
             .tile_span_m = PARTS_TILE_SPAN_M,
             .roughness = 1.0f,
             .wear = 0.30f},
    Material{.name = "thatch",
             .tiled = true,
             .surface = PartSurface::Thatch,
             .tone = PartTone::Mid,
             // СОЛОМА КРУПНЕЕ ДОСКИ: стебель — это ~0.5 м пучка, и метровый
             // повтор доски на кровле читается как ковёр. Первое поле,
             // ради которого tile_span_m обязан доехать до .dfo: сегодня оно
             // живёт в PartSkin у кузницы и до файла не доходит вовсе.
             .tile_span_m = 0.55f,
             .roughness = 1.0f,
             // Новой соломенной кровли не бывает: её кладут пучками и она
             // седеет с первой зимы. Норма вещества, а не выбор рецепта.
             .wear = 0.75f},

    // --- 7..9: вещества БЕЗ зерна на нашем масштабе -------------------------
    // У них плитки нет не потому, что до неё не дошли руки: у полотна и у
    // полированного металла на 4 мм/тексель нечего показывать, кроме того,
    // как они отражают. Плоское альбедо плюс блик — это ПОЛНОЕ описание.
    Material{.name = "linen",
             .tint = {0.86f, 0.83f, 0.74f},
             // Ткань матовее всего, что у нас есть, и это не «умолчание»:
             // 1.0 здесь — измеряемое утверждение о полотне.
             .roughness = 1.0f,
             .wear = 0.40f},
    Material{.name = "iron",
             .tint = {0.32f, 0.33f, 0.35f},
             .roughness = 0.55f,
             .metalness = 1.0f,
             .wear = 0.60f},
    Material{.name = "gold-leaf",
             .tint = GOLD_LEAF_TINT,
             // 0.60, И ЭТО ЧИСЛО ВЫВЕДЕНО РАЗВЁРТКОЙ ПО КАДРАМ, А НЕ ВКУСОМ.
             // Первая догадка была 0.22 («золото почти зеркало») и оказалась
             // неверной ровно так же, как первая догадка про полосу фейда
             // листвы: физически правдоподобно, невидимо на кадре. Развёртка
             // на гербе (доля кадра, где прибавка >= 4/255, то есть примерно
             // ступень нашей палитры):
             //     0.22 -> 0.094 %   блика не видно вовсе
             //     0.40 -> 1.066 %   читается
             //     0.60 -> 7.004 %   ветви несут бегущую золочёную кромку
             // ПИК ПРИБАВКИ ПРИ ЭТОМ ОДИН И ТОТ ЖЕ (89.4 / 89.8 / 89.8) —
             // прямое подтверждение, что нормировка по пику работает как
             // задумано: шероховатость двигает ПЛОЩАДЬ, а не яркость.
             //
             // ПОЧЕМУ НЕ ФИЗИЧЕСКИ ПРАВИЛЬНЫЕ 0.2. Наши источники — ТОЧКИ, а
             // настоящее золочёное изделие бликует под ПРОТЯЖЁННЫМ светом
             // (небо, окно, свеча в полуметре). Точечный источник — дельта,
             // и лепесток обязан быть шире ровно настолько, насколько
             // источник не точка. 0.60 — это не «наше золото шершавое», это
             // поправка на то, чем мы заменяем протяжённый свет.
             .roughness = 0.60f,
             .metalness = 1.0f,
             // Золото герба не ветшает — оно затем и золото.
             .wear = 0.0f},

    // --- 10..14: ВЕЩЕСТВА ПИСЬМА (ТЗ владельца §7, «с первого дня») ---------
    // Книги, записки, журналы, дневники заказаны отдельным видом предмета, и
    // сама читаемость — не наша зона. НАШЕ здесь ровно одно: пять веществ,
    // которых нет ни в одной из девяти колонок листа. Они заведены сейчас, а
    // не «когда дойдёт очередь», по той же причине, по которой заведён
    // gold-leaf: пока вещества нет, предмет обязан им ПРИТВОРЯТЬСЯ, а
    // притворство закрепляется в принятых витринах и потом стоит отмены
    // (девять таких обходов уже лежат на полке мебели, §1.4-бис документа).
    //
    // У всех пяти НЕТ ПЛИТКИ, и это не недоделка: на 4 мм/тексель у листа
    // пергамента нет зерна, которое лист набора мог бы показать. Плоское
    // альбедо плюс то, как они отражают, — полное описание. Береста —
    // единственная с намёком на блеск: берёста лоснится.
    Material{.name = "birch-bark", // берёста: письмо простонародья севера и центра
             .tint = {0.78f, 0.66f, 0.48f},
             .roughness = 0.72f,
             .wear = 0.45f},
    Material{.name = "parchment", // пергамент: учёная и церковная книга, грамоты
             .tint = {0.84f, 0.79f, 0.66f},
             .roughness = 0.90f,
             .wear = 0.35f},
    Material{.name = "rag-paper", // тряпичная бумага: столица, печатные листки
             .tint = {0.88f, 0.86f, 0.80f},
             .roughness = 0.95f,
             .wear = 0.25f},
    Material{.name = "seal-wax", // воск печати (у бедных — хлебный мякиш)
             .tint = {0.55f, 0.13f, 0.11f},
             // Воск — не металл, но собирает блик: печать блестит, и это то,
             // чем она отличается от нарисованного кружка.
             .roughness = 0.45f,
             .wear = 0.10f},
    Material{.name = "ink", // чернила: письмо пером после грамоты
             .tint = {0.09f, 0.08f, 0.11f},
             .roughness = 0.80f,
             .wear = 0.20f},
}};

/// ИМЕНОВАННЫЕ МАТЕРИАЛЫ, У КОТОРЫХ ЕСТЬ ПЛИТКА, — по паре (поверхность, тон).
/// Только они: у linen/iron/gold-leaf плитки нет, и приписать их какой-нибудь
/// паре значило бы соврать, что золото — это тон штукатурки.
[[nodiscard]] constexpr MaterialId tiled_named_of(PartSurface surface,
                                                  PartTone tone) {
    for (std::size_t i = 1; i < TABLE.size(); ++i) {
        const Material& m = TABLE[i];
        if (m.tiled && m.surface == surface && m.tone == tone) {
            return static_cast<MaterialId>(i);
        }
    }
    return MATERIAL_NONE;
}

} // namespace

std::span<const Material> materials() { return {TABLE.data(), TABLE.size()}; }

const Material& material(MaterialId id) {
    // Файл из будущего рисуется как «материала не названо», а не роняет игру:
    // read_object уже отказывает битым файлам, и второй способ умереть на том
    // же пути ничего не защищает.
    return id < TABLE.size() ? TABLE[id] : TABLE[MATERIAL_NONE];
}

MaterialId material_by_name(std::string_view name) {
    if (name.empty()) {
        return MATERIAL_NONE;
    }
    for (std::size_t i = 1; i < TABLE.size(); ++i) {
        if (TABLE[i].name == name) {
            return static_cast<MaterialId>(i);
        }
    }
    return MATERIAL_NONE;
}

MaterialId named_material_of(PartSurface surface, PartTone tone) {
    return tiled_named_of(surface, tone);
}

Material material_of(PartSurface surface, PartTone tone) {
    if (const MaterialId id = tiled_named_of(surface, tone); id != MATERIAL_NONE) {
        return TABLE[id];
    }
    // ВЫВЕДЕННЫЙ МАТЕРИАЛ безымянной пары: та же плитка, тот же тон, умолчания
    // по всему остальному. Умолчания — это в точности сегодняшний ламберт, и
    // потому кадр не меняется ни на бит (проверяется в MaterialRegistryTests,
    // а не обещается).
    //
    // ИЗНОС ВЫВОДИТСЯ ИЗ РЯДА, а не берётся штатным 0.5: ряд Weathered — это и
    // есть «выветренное» (PartsAtlas.h: тон и износ здесь ОДНА ось), и
    // материал, который об этом молчит, потерял бы единственное, что пара
    // про износ сообщала.
    Material m{};
    m.name = {}; // безымянный по построению: имя есть не у всякого вещества
    m.tiled = true;
    m.surface = surface;
    m.tone = tone;
    m.tile_span_m = PARTS_TILE_SPAN_M;
    switch (tone) {
    case PartTone::Light:
        m.wear = 0.10f;
        break;
    case PartTone::Mid:
        m.wear = 0.35f;
        break;
    case PartTone::Dark:
        m.wear = 0.50f;
        break;
    case PartTone::Weathered:
        m.wear = 0.85f;
        break;
    }
    return m;
}

} // namespace dfn::render
