/*
Created: 28:08:2026 - 18:30:43
Last updated: 28:08:2026 - 18:30:43
Module: engine/world
File: engine/world/sources/HouseLod.cpp

Responsibility:
- ДВЕ ПОЛОВИНЫ ОДНОЙ ЛЕСТНИЦЫ В ОДНОМ ФАЙЛЕ: выбор ступени по расстоянию
  (house_lod_for_distance) и то, ЧТО КАЖДАЯ СТУПЕНЬ СРЕЗАЕТ (house_lod_simplify
  — переписывает рецепт элемента перед сборкой).
- Половины лежат вместе намеренно: порог без списка срезаемого — число без
  смысла, список без порога — правка без адреса.

Key items:
- house_lod_for_distance, house_lod_bevel, house_lod_name, house_lod_simplify.

Dependencies:
- Uses: HouseLod.h, HouseMeshDetail.h (ElementParams, WallFill, fill_kind).
- Used by: HouseMesh.cpp (build_house_mesh), RenderSystem, приборы.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПОЛНАЯ СТУПЕНЬ НЕ ПРОХОДИТ ЧЕРЕЗ house_lod_simplify ВООБЩЕ: она обязана
  оставаться прежней БИТ-В-БИТ, а «то же самое, только через ветку» — это
  обещание, которое ломается первой же правкой ветки (правило 47).
- СРЕЗАННЫЙ РЯДНЫЙ СЛОЙ ОТДАЁТ СВОЙ МАТЕРИАЛ ПЛАСТИНЕ. Иначе дальний
  кирпичный дом становится штукатурным: кирпич живёт в кусках кладки, а
  пластина под ними носит материал элемента.
*/
/*
UPD:
- 28:08:2026 - 18:30:43: Создан — волна LOD построек (И13).
*/

#include "engine/world/sources/HouseLod.h"

#include "engine/world/sources/HouseMeshDetail.h"

#include <algorithm>

namespace dfn::world {

HouseLod house_lod_for_distance(float distance_m, HouseLod current) {
    const auto level = [](HouseLod l) { return static_cast<int>(l); };
    // Самая грубая ступень, чей вход уже позади, — с краем, отодвинутым ОТ той
    // ступени, на которой предмет стоит сейчас. Та же форма, что у лестницы
    // флоры (ScatterBatcher::flora_lod_for_distance): один вид гистерезиса на
    // движок, потому что два разных читались бы как два разных дефекта.
    const float mid_in = level(current) >= level(HouseLod::Mid)
                             ? HOUSE_LOD_MID_IN_M - HOUSE_LOD_HYSTERESIS_M
                             : HOUSE_LOD_MID_IN_M + HOUSE_LOD_HYSTERESIS_M;
    const float far_in = level(current) >= level(HouseLod::Far)
                             ? HOUSE_LOD_FAR_IN_M - HOUSE_LOD_HYSTERESIS_M
                             : HOUSE_LOD_FAR_IN_M + HOUSE_LOD_HYSTERESIS_M;
    if (distance_m >= far_in) {
        return HouseLod::Far;
    }
    if (distance_m >= mid_in) {
        return HouseLod::Mid;
    }
    return HouseLod::Full;
}

float house_lod_bevel(HouseLod lod, float bevel_m) {
    // Фаска неразличима с 3.5 м (таблица в HouseLod.h), а стоит она городу
    // в полтора-два раза больше треугольников (замер волны материалов:
    // Вайтран 2.19 млн против 1.4 млн). Ноль сюда — не «узкая фаска», а
    // ПРЕЖНЯЯ ВЕТКА push_prism, то есть острое ребро без единого лишнего
    // треугольника.
    return lod == HouseLod::Full ? bevel_m : 0.0f;
}

const char* house_lod_name(HouseLod lod) {
    switch (lod) {
        case HouseLod::Full: return "полная";
        case HouseLod::Mid: return "средняя";
        case HouseLod::Far: return "дальняя";
    }
    return "?";
}

HouseLodCut house_lod_simplify(HouseLod lod, ElementParams& p) {
    HouseLodCut cut;
    if (lod == HouseLod::Full) {
        return cut; // см. AI Agents Notice: полная ступень сюда не заходит
    }

    // -- СРЕДНЯЯ СТУПЕНЬ (>= HOUSE_LOD_MID_IN_M = 28 м) --------------------
    // Срезается ровно то, чей шаг за 28 м не разрешается: фаска (3.5 м),
    // косметика износа (трещина 0.018 -> 6.3 м) и КИРПИЧНАЯ кладка
    // (шаг 0.075 -> 26.4 м). Фаску снимает house_lod_bevel у вызывающего:
    // она свойство построителя, а не рецепта.
    //
    // ЧЕГО СРЕДНЯЯ НЕ ТРОГАЕТ, И ЭТО ПРОВЕРЯЕМО: ставню (52.8 м), доску
    // обшивки (77.4), каменный блок (82.7), дранку (98.5), венец (103.8),
    // черепицу (105.6). Соблазн срезать их здесь же велик — они и есть
    // главный вес города, — но 28 м это ВИДНО, и лестница, режущая видимое,
    // перестаёт быть лестницей и становится дозой качества.
    p.wear = 0.0f;
    if (fill_kind(p) == WallFill::Brick) {
        p.fill = 0.0f;
        cut.plate_mat = 4; // глина кладки — на пластину (см. Notice)
    }
    if (lod == HouseLod::Mid) {
        return cut;
    }

    // -- ДАЛЬНЯЯ СТУПЕНЬ (>= HOUSE_LOD_FAR_IN_M = 110 м) -------------------
    // За 110 м не разрешается НИ ОДИН рядный слой, поэтому оболочка отдаёт их
    // средним тоном: стена — пластина, скат — плоскость. Проёмы остаются
    // (HOUSE_LOD_FAR_KEEPS_OPENINGS).
    //
    // ЭЛЕМЕНТ НЕ ВЫБРАСЫВАЕТСЯ НИ ОДИН, и это решение, а не недоделка.
    // Силуэт города на 500 м складывается ИМЕННО из мелких верхов — труб,
    // шпилей, слуховых окон, — а треугольники живут не в элементах, а в
    // рядных слоях: срезав слои, мы уже забрали почти весь вес, и отбор по
    // размеру отнял бы силуэт за остаток.
    switch (fill_kind(p)) {
        case WallFill::Block:
            p.fill = 0.0f;
            cut.plate_mat = 3; // камень блоков
            break;
        case WallFill::Logs:
            p.fill = 0.0f;
            cut.plate_mat = 0; // тёсаный брус венцов
            break;
        case WallFill::Parquet:
            p.fill = 0.0f;
            cut.plate_mat = 1; // пилёная доска настила
            break;
        case WallFill::Shingle:
            p.fill = 0.0f;
            cut.plate_mat = 1; // дранка
            break;
        case WallFill::Tile:
            p.fill = 0.0f;
            cut.plate_mat = 4; // черепица
            break;
        case WallFill::Brick:
        case WallFill::Plain:
            break;
        case WallFill::Stairs:
            // МАРШ ОСТАЁТСЯ МАРШЕМ: он замещает пластину целиком, и обнулить
            // ему заполнение значило бы получить наклонную плиту вместо
            // ступеней — то есть ДРУГОЙ силуэт, а не более дешёвый.
            // Дешевеет он зазорами: открытый марш с щелями и дрожью блоков
            // становится сплошным коробом.
            p.open = 0.0f;
            break;
    }
    // Накладная мелочь: обшивка (77.4 м), ставни/крыльцо/завалинка/торцы
    // (~53-100 м), потолочные балки (внутри дома, снаружи невидимы вовсе),
    // щит подступёнка.
    p.clad = 0.0f;
    p.shutters = 0.0f;
    p.porch = 0.0f;
    p.plinth = 0.0f;
    p.logends = 0.0f;
    p.beams = 0.0f;
    p.riser = 0.0f;
    if (!HOUSE_LOD_FAR_KEEPS_OPENINGS) {
        p.doors = 0.0f;
        p.windows = 0.0f;
    }
    return cut;
}

} // namespace dfn::world
