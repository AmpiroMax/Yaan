/*
Created: 23:08:2026 - 23:39:48
Last updated: 23:08:2026 - 23:39:48
Module: tools
File: tools/pack_cornhall.h

Responsibility:
- ПАКЕТ КОРНХОЛЛА (этап 2 пилота эпохи «12 городов», docs/plans/CITIES_12.md
  §4): шестнадцать рецептов cornhall-* — своя архитектура равнинного
  кирпичного города. Ступенчатый щипец, нависающие этажи, аркада рыночного
  ряда, ратуша с башней, ветряк-столбовка, пивоварня, постоялый двор с
  проездной аркой, голубятня, каланча, амбар на грибах, гончарный горн
  куполом, весовая палата.

Usage:
    dfn_houses --only 'cornhall-*'     испечь ровно этот пакет
    dfn_houses --list --only 'cornhall-*'

Key items:
- brick_wall / crow_gable / brick_arch / brick_dome / hip_cap: руки школы.
- cornhall_dwelling: жилое тело (два размера, черепица или солома, износ).
- pack_cornhall: секция реестра.

Dependencies:
- Uses: Forge, Aging, parquet_floor, keep_tier, vents_on — кухня кузницы
  tools/forge_houses.cpp, внутри анонимного пространства которой этот файл и
  разворачивается (см. НИЖЕ, почему именно так).
- Used by: tools/forge_houses.cpp (house_recipes -> pack_cornhall).

ПОЧЕМУ ЗАГОЛОВОК, А НЕ ВТОРАЯ ЕДИНИЦА ТРАНСЛЯЦИИ. Рука кузницы (Forge,
gable_roof, keep_tier, Aging) живёт в АНОНИМНОМ пространстве имён
forge_houses.cpp и другой единице трансляции недоступна. Из этого следуют
ровно три выхода, и два из них хуже третьего:
  1. вынести руку в общий заголовок — это правка 4318-строчного файла, за
     который сейчас держатся ещё несколько городов эпохи, ради удобства
     одного пакета; чужие рецепты пошли бы на переприёмку из-за моего;
  2. завести свою копию руки — ровно правило 39 (теневая копия цепочки
     становится дефектом в день, когда у оригинала появляется ветка);
  3. РАЗВЕРНУТЬ ПАКЕТ ВНУТРИ ТОЙ ЖЕ ЕДИНИЦЫ, а сам текст держать своим
     файлом — тогда в общем файле остаются ДВЕ СТРОКИ (включение и вызов),
     правило 21 (потолок 2500) не нарушается новым файлом, и город не правит
     городу.
Взят третий. Файл включается ОДИН РАЗ, из forge_houses.cpp, ниже кухни и выше
реестра; #pragma once защищает от второго включения, но включать его откуда-то
ещё нельзя по построению — вне того места он не соберётся.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПРАВКИ РЕЦЕПТОВ — ТОЛЬКО ЗДЕСЬ; .dfh перепекать, не править руками.
- ОБЩИЙ ПАКЕТ city-* НЕ ТРОГАТЬ. Всё, что нужно Корнхоллу сверх общего,
  заводится в этом файле; фундаменты, крыльца, бордюры, мощение и стены
  берутся из city-* как есть (CITIES_12: «ряд по высоте считается ОДНИМ
  семейством»).

О КИРПИЧЕ И О ТОМ, ПОЧЕМУ НОВОЙ КЛЕТКИ АТЛАСА ЗДЕСЬ НЕТ. Задание разрешало
одно касание движка — клетку Brick в PartsAtlas, «если отсутствует». Она НЕ
отсутствует, и это проверено по трём местам, а не по памяти:
  * колонка листа частей PartSurface::FiredClay (index 4) названа в
    PartsAtlas.h дословно «Обожжённая глина — кирпич и черепица», и её ряды
    тона дают ровно ТРИ ОТТЕНКА ОБЖИГА плюс выветренный: Light 0.48/0.26/0.18,
    Mid 0.42/0.24/0.17, Dark (пережог) 0.32/0.18/0.13, Weathered;
  * материал набора PartMaterial::Brick существует и отображён на эту колонку
    (PartForge.cpp, skin_of);
  * ШВЫ КЛАДКИ — ГЕОМЕТРИЯ, А НЕ КАРТИНКА: fill=2 (WallFill::Brick) гонит
    стену через build_courses — настоящие куски с настоящей тенью между
    рядами (HouseWalls.cpp). Шапка PartsAtlas.h запрещает рисовать перевязку
    на листе прямым текстом («a bond painted into a texture would be a second,
    flatter copy of something the kit already tells the truth about, and the
    two would disagree at every corner») — то есть новая клетка «кирпич со
    швами» была бы теневой копией по правилу 39, а не новой поверхностью.
Поэтому весь пакет пишет mat=4 + fill=2, движок не тронут ни одной строкой, и
кадр Вайтрана не может измениться по построению — ни один файл, из которого он
рисуется, не менялся. Расход правила 47 (доказать «новая клетка не меняет
старые») тут не нужен, потому что нет новой клетки.

ЧЕГО СЕГОДНЯ НЕ УМЕЕТ КОНВЕЙЕР — ОДНА НАЗВАННАЯ ДЫРА, а не список пожеланий.
«Кладка ЁЛОЧКОЙ в фахверковом заполнении» (§4.3) не строится: build_cladding
разводит fill=2 (кладка) и clad=1 (фахверк) по РАЗНЫМ ветвям и выходит из
первой же — панель фахверка не может быть выложена кирпичом. Заполнение здесь
читается ЦВЕТОМ (панель несёт mat=4 в светлом ряду обжига, каркас — брус
поверх), и это честная замена на дистанции улицы, но не ёлочка. Ёлочку даст
ровно одно из двух: (а) вариант brick_texel в листе частей — новая колонка,
которая по критерию самого листа своё место заслуживает (она отличается
РИТМОМ, а не цветом, как SawnBoard отличается от HewnTimber), но требует
согласованной правки ещё в трёх местах вне зоны кузни (модуль 9 в
AppHouse.cpp дважды, список материалов редактора, ряд проверок); (б) вид
кладки fill=9 «ёлочка» в build_courses — зона world. Оба — работа волны, а не
кузнеца; заявлено в отчёте пакета.
*/
/*
UPD:
- 23:08:2026 - 23:39:48: Создан — пакет Корнхолла, этап 2 (шестнадцать
  рецептов): жилые тела двух размеров с ступенчатым щипцом и их ветхие
  близнецы, соломенный вариант окраины, дом с нависающими этажами, рядная
  лавка с аркадой, ратуша с башней и часовой площадкой, ветряк-столбовка,
  пивоварня с сушильной трубой, постоялый двор с проездной аркой, голубятня,
  каланча, амбар на грибах, гончарный горн куполом, весовая палата.
*/

#pragma once

// ---------------------------------------------------------------------------
// ШКОЛА КОРНХОЛЛА — ОБЩИЕ РУКИ ПАКЕТА
//
// ЛОКАЛЬНЫЕ КООРДИНАТЫ ТЕ ЖЕ, ЧТО У ВСЕЙ ПОЛКИ: начало — северо-западный
// угол, дверь смотрит на +Z (юг). Для Корнхолла это не формальность:
// бургажная полоса ставит дом ТОРЦОМ к улице, значит уличный фасад — это
// ЩИПЦОВАЯ стена, конёк идёт вдоль Z, и ступенчатый щипец приходится ровно
// на ту стену, где стоит дверь. Отсюда gable_roof_z у всего жилья пакета,
// а не gable_roof.
// ---------------------------------------------------------------------------

/// УКЛОН ДВУСКАТА ШКОЛЫ — 40 градусов (§3 паспорта: «двускат умеренный 40»).
/// Подъём конька считается от ПОЛУПРОЛЁТА, а не назначается на глаз: иначе
/// два дома разной ширины встают на улице с разным наклоном крыш, и ряд
/// перестаёт читаться одной школой.
inline constexpr float CORNHALL_PITCH = 0.83910f; // tan(40 гр.)

/// ПЕРЕХЛЁСТ ТЕЛ ПО ВЫСОТЕ. Допуск касания судьи связности — 0.02 м
/// (HOUSE_CONTACT_TOL_M), и ярусы, поставленные ВСТЫК, расходятся островами
/// на первом же округлении. Все ступени, венцы и пояса пакета кладутся с
/// этим перехлёстом.
inline constexpr float CORNHALL_LAP = 0.06f;

/// КИРПИЧНАЯ СТЕНА — ОДНА РУКА НА ВЕСЬ ПАКЕТ (§3: кирпич из местной глины —
/// ГЛАВНЫЙ материал). mat=4 — колонка обожжённой глины листа частей;
/// fill=2 — кладка рядами НАСТОЯЩЕЙ геометрией, с тенью между рядами.
/// Порядок вершин задаёт лицо: a -> b по возрастанию X даёт лицо на +Z (юг),
/// обратный — на север. Кладка и рамы проёмов строятся на ЛИЦЕВОЙ стороне,
/// поэтому порядок здесь не украшение.
inline ElementId brick_wall(Forge& f, float y, float ax, float az, float bx, float bz,
                            float h, float th, const char* tone, const char* wear) {
    return f.wall(f.v(ax, y, az), f.v(bx, y, bz),
                  {{"height", f.num(h)},
                   {"thickness", f.num(th)},
                   {"mat", "4"},
                   {"tone", tone},
                   {"fill", "2"},
                   {"wear", wear}});
}

/// СТУПЕНЧАТЫЙ ЩИПЕЦ — ФИРМЕННЫЙ ПРИЗНАК ГОРОДА (§4.1: «7-11 ступеней, все
/// фасады на красной линии рынка и главной улицы»).
///
/// Кладка ступенями в плоскости z: блок k стоит на (карниз + k*подъём) и уже
/// нижнего на один вылет с каждой стороны. Блоков steps+1, а СИЛУЭТ читает
/// 2*steps+1 ступень — steps слева, steps справа и венчающий блок. Отсюда
/// таблица заказа: steps=3 -> 7 ступеней, 4 -> 9, 5 -> 11.
///
/// ЩИПЕЦ ВЫШЕ КРОВЛИ И ПОГЛОЩАЕТ ЕЁ КРОМКУ, А НЕ НАОБОРОТ. Двускат вылезает
/// за торец на свес o (0.35 у черепицы, 0.55 у соломы), и щипец, поставленный
/// в плоскости стены, был бы пробит скатом насквозь. Поэтому вызывающий
/// отодвигает плоскость щипца НАРУЖУ за кромку ската и берёт толщину, которая
/// эту кромку накрывает — ровно так стоит настоящий ворон-щипец, у которого
/// кровля кончается в кладке, а не свисает с торца.
inline void crow_gable(Forge& f, float x0, float x1, float z, bool face_south,
                       float y_eaves, float ridge_h, int steps, float th,
                       const char* tone, const char* wear) {
    const float hw = (x1 - x0) * 0.5f;
    const float run = hw / static_cast<float>(steps + 1);
    const float rise = ridge_h / static_cast<float>(steps + 1);
    for (int k = 0; k <= steps; ++k) {
        const float ax = x0 + run * static_cast<float>(k);
        const float bx = x1 - run * static_cast<float>(k);
        const float y = y_eaves + rise * static_cast<float>(k);
        if (face_south) {
            (void)brick_wall(f, y, ax, z, bx, z, rise + CORNHALL_LAP, th, tone, wear);
        } else {
            (void)brick_wall(f, y, bx, z, ax, z, rise + CORNHALL_LAP, th, tone, wear);
        }
    }
}

/// АРКА КИРПИЧНАЯ, ДЕСЯТЬ ХОРД (§4.6 и граница конвейера CITIES_12: «арки и
/// своды многоугольником; на пролёте <1 м арка читается гранёной — там
/// перемычка»). Полукруг радиуса span/2 от пяты y_spring в плоскости z,
/// набранный квадратными брусьями обожжённой глины: концы хорд совпадают,
/// поэтому кольцо связно телом, а не соседством.
///
/// ЗАЧЕМ ИМЕННО ДЕСЯТЬ. На пролёте 3 м хорда выходит 0.47 м, стрела сегмента
/// 1.9 см — это вдвое меньше допуска касания судьи и вчетверо меньше
/// собственной толщины архивольта, то есть гранёность прячется в самой
/// кладке. Пять хорд дали бы стрелу 7.6 см — уже видимый пятиугольник.
inline void brick_arch(Forge& f, float cx, float z, float span, float y_spring,
                       float ring_r, const char* tone, const char* wear) {
    constexpr int CHORDS = 10;
    constexpr float PI = 3.14159265358979f;
    const float r = span * 0.5f;
    for (int k = 0; k < CHORDS; ++k) {
        const float t0 = PI * static_cast<float>(k) / static_cast<float>(CHORDS);
        const float t1 = PI * static_cast<float>(k + 1) / static_cast<float>(CHORDS);
        (void)f.beam(f.v(cx - r * std::cos(t0), y_spring + r * std::sin(t0), z),
                     f.v(cx - r * std::cos(t1), y_spring + r * std::sin(t1), z),
                     {{"radius", f.num(ring_r)},
                      {"form", "square"},
                      {"mat", "4"},
                      {"tone", tone},
                      {"wear", wear}});
    }
}

/// КУПОЛ ВЕЕРОМ ПОВЕРХНОСТЕЙ — ТОЛЬКО КАК ДОМИНАНТА, ЛИМИТ 1-3 НА ГОРОД
/// (CITIES_12, границы конвейера: бюджет постройки 200-600 трис). В Корнхолле
/// купол носит ОДИН рецепт — гончарный горн у ворот (§4.18), и лимит держится
/// расстановкой, а не кузней.
///
/// Восьмигранник, три яруса по четверти окружности: радиусы r*cos(a), высоты
/// h*sin(a) при a = 0, 30, 60, 90 градусов. Два нижних яруса — по восемь
/// четырёхугольников, верхний — восемь треугольников к макушке: 24 контура,
/// около 290 треугольников в мешe. Все несут unsupported=1 — правило опоры
/// крыш смотрит на вершины, а у купола выше первого яруса стен нет и быть не
/// может (тот же выход, что у шатриков замка).
inline void brick_dome(Forge& f, float cx, float cz, float y0, float r0, float h,
                       const char* tone, const char* wear) {
    constexpr int SIDES = 8;
    constexpr float PI = 3.14159265358979f;
    constexpr int TIERS = 3;
    float rad[TIERS + 1];
    float lev[TIERS + 1];
    for (int i = 0; i <= TIERS; ++i) {
        const float a = (PI * 0.5f) * static_cast<float>(i) / static_cast<float>(TIERS);
        rad[i] = r0 * std::cos(a);
        lev[i] = y0 + h * std::sin(a);
    }
    const auto pt = [&](int tier, int j) {
        const float t = 2.0f * PI * static_cast<float>(j) / static_cast<float>(SIDES);
        return glm::vec3{cx + rad[tier] * std::cos(t), lev[tier],
                         cz + rad[tier] * std::sin(t)};
    };
    for (int i = 0; i < TIERS; ++i) {
        for (int j = 0; j < SIDES; ++j) {
            const glm::vec3 a = pt(i, j);
            const glm::vec3 b = pt(i, j + 1);
            ElementId id = dfn::world::NO_ELEMENT;
            if (i + 1 == TIERS) {
                id = f.contour({f.v(a.x, a.y, a.z), f.v(b.x, b.y, b.z),
                                f.v(cx, lev[TIERS], cz)},
                               {{"thickness", "0.16"}, {"mat", "4"}, {"tone", tone},
                                {"wear", wear}});
            } else {
                const glm::vec3 c = pt(i + 1, j + 1);
                const glm::vec3 d = pt(i + 1, j);
                id = f.contour({f.v(a.x, a.y, a.z), f.v(b.x, b.y, b.z),
                                f.v(c.x, c.y, c.z), f.v(d.x, d.y, d.z)},
                               {{"thickness", "0.16"}, {"mat", "4"}, {"tone", tone},
                                {"wear", wear}});
            }
            (void)f.g.set_param(id, "roof", "1");
            (void)f.g.set_param(id, "unsupported", "1");
        }
    }
}

/// ШАТЁР НА КВАДРАТНОМ ЯРУСЕ: карнизная плита с напуском и четыре треугольника
/// к вершине. Рука башен пакета (ратуша, голубятня, каланча) — та же, что у
/// башенок замка, но своя: тамошняя сидит лямбдой внутри forge_keep и печёт
/// вместе с ней весь корпус.
///
/// БАЗА ШАТРА — ВЕРХ ПЛИТЫ ПЛЮС ПОЛТОЛЩИНЫ, А НЕ ВЕРХ ПЛИТЫ. Контур растёт в
/// обе стороны от своей высоты; шатёр, посаженный на y_top + толщину, повис бы
/// над площадкой (замок ловил это судьёй, зазор 0.047 м по нормали ската).
inline void hip_cap(Forge& f, float x0, float z0, float x1, float z1, float y_top,
                    float out, float rise, const char* mat, const char* tone,
                    const char* fill, const char* wear) {
    const float ax = x0 - out;
    const float az = z0 - out;
    const float bx = x1 + out;
    const float bz = z1 + out;
    {
        const ElementId slab =
            f.contour({f.v(ax, y_top, az), f.v(ax, y_top, bz), f.v(bx, y_top, bz),
                       f.v(bx, y_top, az)},
                      {{"thickness", "0.2"}, {"mat", "4"}, {"tone", tone},
                       {"wear", wear}});
        (void)f.g.set_param(slab, "unsupported", "1");
    }
    const float base = y_top + 0.1f;
    const glm::vec3 corner[4] = {
        {ax, base, az}, {bx, base, az}, {bx, base, bz}, {ax, base, bz}};
    const glm::vec3 apex{(x0 + x1) * 0.5f, base + rise, (z0 + z1) * 0.5f};
    for (int k = 0; k < 4; ++k) {
        const glm::vec3 c1 = corner[k];
        const glm::vec3 c2 = corner[(k + 1) % 4];
        const ElementId t =
            f.contour({f.v(c1.x, c1.y, c1.z), f.v(c2.x, c2.y, c2.z),
                       f.v(apex.x, apex.y, apex.z)},
                      {{"thickness", "0.14"}, {"mat", mat}, {"tone", tone},
                       {"wear", wear}});
        if (fill[0] != 0) {
            (void)f.g.set_param(t, "fill", fill);
        }
        (void)f.g.set_param(t, "roof", "1");
        (void)f.g.set_param(t, "unsupported", "1");
    }
}

/// ГОРИЗОНТАЛЬНАЯ ПЛИТА (настил яруса, пояс, площадка) — обход против часовой
/// сверху, лицо вверх. Своя короткая рука: parquet_floor кухни всегда кладёт
/// паркет досками (fill=5), а поясу и площадке нужен камень или кирпич.
inline ElementId slab_xz(Forge& f, float x0, float z0, float x1, float z1, float y,
                         float th, const char* mat, const char* tone,
                         const char* wear) {
    return f.contour({f.v(x0, y, z0), f.v(x0, y, z1), f.v(x1, y, z1), f.v(x1, y, z0)},
                     {{"thickness", f.num(th)}, {"mat", mat}, {"tone", tone},
                      {"wear", wear}});
}

// ---------------------------------------------------------------------------
// ЖИЛОЕ ТЕЛО ГОРОДА — ОДНА РУКА, ЧЕТЫРЕ ВЫПЕЧКИ
//
// Размер, этажность, число ступеней щипца и кровля — ПАРАМЕТРЫ, а не четыре
// тела: тот же довод, по которому Aging остался параметром (копия тела
// расходится с оригиналом на первой же правке формы). Отсюда малый дом,
// большой дом, их ветхие близнецы и соломенный вариант окраины печатаются
// ОДНИМ кодом.
//
// ГРАДИЕНТ КРОВЛИ — ПАСПОРТНЫЙ ПРИЗНАК, А НЕ УКРАШЕНИЕ (§3: черепица в центре
// по пожарному регламенту, солома у стены и в предместьях). Он и есть то, чем
// окраина Корнхолла отличается от его же рынка при одном наборе форм.
// ---------------------------------------------------------------------------
inline void cornhall_dwelling(Aging age, float W, float D, int floors, int steps,
                              bool thatch, bool hoist) {
    Forge f;
    const float PL = 0.45f;                 // кирпичный цоколь (§3)
    const float STOREY[3] = {3.0f, 2.8f, 2.6f};
    float eaves = PL;
    for (int i = 0; i < floors; ++i) {
        eaves += STOREY[i];
    }
    const float RIDGE_H = W * 0.5f * CORNHALL_PITCH;
    // Щипец выносится за кромку ската и накрывает её: свес соломы длиннее.
    const float GO = thatch ? 0.52f : 0.32f;
    const float GTH = thatch ? 0.72f : 0.52f;
    const char* const roof_mat = thatch ? "6" : "4";
    const char* const roof_tone = thatch ? "1" : "0";
    const char* const roof_fill = thatch ? "" : "8";

    // ---------- цоколь: сплошная кирпичная лента под всеми стенами ----------
    (void)brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, PL, 0.42f, "2", age.w(0.62f));
    (void)brick_wall(f, 0.0f, W, D, W, 0.0f, PL, 0.42f, "2", age.w(0.62f));
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, PL, 0.42f, "2", age.w(0.62f));
    (void)brick_wall(f, 0.0f, 0.0f, D, W, D, PL, 0.42f, "2", age.w(0.62f));
    parquet_floor(f, 0.0f, 0.0f, W, D, PL + 0.06f);
    f.frame_posts(0.0f, 0.0f, W, D, PL, eaves, "0");

    // ---------- первый этаж: кирпич с окнами, дверь в щипцовом фасаде ------
    const float Y1 = PL;
    const float H1 = STOREY[0];
    const auto low = [&](float ax, float az, float bx, float bz, const char* win,
                         bool door) {
        const ElementId id =
            brick_wall(f, Y1, ax, az, bx, bz, H1, 0.38f, "1", age.w(0.46f));
        if (door) {
            (void)f.g.set_param(id, "doors", "1");
            (void)f.g.set_param(id, "porch", "1");
        } else {
            (void)f.g.set_param(id, "windows", win);
            (void)f.g.set_param(id, "plinth", age.plinth());
        }
    };
    low(W, 0.0f, 0.0f, 0.0f, "2", false);  // север — глухой торец с окнами
    low(W, D, W, 0.0f, "3", false);        // восток — длинная боковая
    low(0.0f, 0.0f, 0.0f, D, "3", false);  // запад
    // ЮЖНЫЙ ФАСАД ИЗ ТРЁХ ПРОЛЁТОВ: раскладка знает один вид проёма на стену
    // (дверь берёт верх над окнами), и окна получают свои пролёты по бокам.
    const float d0 = W * 0.34f;
    const float d1 = W * 0.66f;
    low(0.0f, D, d0, D, "1", false);
    low(d0, D, d1, D, "", true);
    low(d1, D, W, D, "1", false);
    f.door_leaf(W * 0.5f, Y1, D);

    // ---------- верхние этажи: фахверк с кирпичным заполнением -------------
    // ЗАПОЛНЕНИЕ ЧИТАЕТСЯ ЦВЕТОМ, А НЕ ПЕРЕВЯЗКОЙ: панель несёт mat=4 в
    // СВЕТЛОМ ряду обжига, каркас (доски и раскосы) кладётся поверх обшивкой.
    // Ёлочки конвейер не умеет — названо в шапке файла.
    float y = PL + H1;
    for (int s = 1; s < floors; ++s) {
        const float h = STOREY[s];
        const auto up = [&](float ax, float az, float bx, float bz, const char* win) {
            const ElementId id =
                f.wall(f.v(ax, y, az), f.v(bx, y, bz),
                       {{"height", f.num(h)}, {"thickness", "0.3"}, {"mat", "4"},
                        {"tone", "0"}, {"clad", "1"}, {"windows", win},
                        {"shutters", age.shutters()}, {"wear", age.w(0.32f)}});
            (void)id;
        };
        up(W, 0.0f, 0.0f, 0.0f, "2");
        up(W, D, W, 0.0f, "3");
        up(0.0f, 0.0f, 0.0f, D, "3");
        up(0.0f, D, W, D, "2");
        // Настил яруса с балками — он же связывает четыре стены между собой.
        {
            const ElementId fl = slab_xz(f, 0.0f, 0.0f, W, D, y, 0.12f, "1", "1",
                                         age.w(0.3f));
            (void)f.g.set_param(fl, "beams", "1");
        }
        // Марш вдоль восточной стены: подъём h на ход 1.5*h (уклон 33.7 гр.).
        {
            const float run = h * 1.5f;
            (void)f.contour({f.v(W - 1.7f, y - h, D - 0.6f), f.v(W - 0.3f, y - h, D - 0.6f),
                             f.v(W - 0.3f, y, D - 0.6f - run),
                             f.v(W - 1.7f, y, D - 0.6f - run)},
                            {{"thickness", "0.1"}, {"fill", "6"}, {"open", "1"},
                             {"mat", "1"}, {"tone", "1"}, {"wear", age.w(0.3f)}});
        }
        y += h;
    }

    // ---------- кровля и два ступенчатых щипца -----------------------------
    f.gable_roof_z(0.0f, 0.0f, W, D, eaves, RIDGE_H, roof_mat, roof_tone, roof_fill,
                   age.w(0.36f));
    crow_gable(f, 0.0f, W, D + GO, /*face_south=*/true, eaves, RIDGE_H, steps, GTH,
               "1", age.w(0.46f));
    crow_gable(f, 0.0f, W, -GO, /*face_south=*/false, eaves, RIDGE_H, steps, GTH,
               "1", age.w(0.5f));

    // ---------- ПОДЪЁМНАЯ БАЛКА С БЛОКОМ В ЩИПЦЕ (§4.4) --------------------
    // «Все купеческие дома, под коньком»: брус, торчащий из щипца на 0.9 м, и
    // короткий подвес блока. Признак торгового дома, и с улицы он читается
    // раньше вывески — потому его несёт только большой дом и дом с выносом.
    if (hoist) {
        const float hy = eaves + RIDGE_H * 0.78f;
        (void)f.beam(f.v(W * 0.5f, hy, D + GO - 0.1f), f.v(W * 0.5f, hy, D + GO + 0.9f),
                     {{"radius", "0.09"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", age.w(0.4f)}});
        (void)f.beam(f.v(W * 0.5f, hy - 0.36f, D + GO + 0.78f),
                     f.v(W * 0.5f, hy - 0.02f, D + GO + 0.78f),
                     {{"radius", "0.07"}, {"mat", "0"}, {"tone", "2"},
                      {"wear", age.w(0.4f)}});
    }

    // ---------- крыльцо: площадка на уровне порога и две ступени -----------
    (void)slab_xz(f, W * 0.5f - 1.1f, D, W * 0.5f + 1.1f, D + 1.0f, Y1 - 0.06f, 0.12f,
                  "3", "1", age.w(0.5f));
    (void)f.contour({f.v(W * 0.5f - 0.8f, 0.0f, D + 1.7f),
                     f.v(W * 0.5f + 0.8f, 0.0f, D + 1.7f),
                     f.v(W * 0.5f + 0.8f, Y1 - 0.06f, D + 1.0f),
                     f.v(W * 0.5f - 0.8f, Y1 - 0.06f, D + 1.0f)},
                    {{"thickness", "0.1"}, {"fill", "6"}, {"mat", "3"}, {"tone", "1"},
                     {"wear", age.w(0.5f)}});

    // ---------- ТРУБА В СЕВЕРНОМ ЩИПЦЕ -------------------------------------
    // Очаг у глухой торцовой стены, труба идёт ПО ЩИПЦУ — так стоит стояк у
    // всякого дома, чей уличный торец занят дверью и лавкой. Кровлю рецепт не
    // трогает: труба стоит в плоскости щипца и им же держится.
    if (vents_on()) {
        f.chimney(W * 0.5f, -GO, eaves - 0.6f, eaves + RIDGE_H + 1.5f, 0.34f, true,
                  "4", "2", age.w(0.55f));
    }
    f.save(age.file);
}

// ---------------------------------------------------------------------------
// ДОМ С НАВИСАЮЩИМИ ЭТАЖАМИ (§4.2: «рыночный фронт; вынос 0.6-0.9 м на этаж,
// улица сужается кверху»).
//
// ВЫНОС 0.7 М НА ЭТАЖ, И ЭТО ЧИСЛО ЗАЖАТО С ДВУХ СТОРОН. Сверху — паспорт
// (0.6-0.9); снизу — граница конвейера CITIES_12: «под консолью 1.8 м для
// капсулы игрока». Первый этаж 3.0 м, значит низ консоли на 2.6 — запас
// восемьдесят сантиметров, и вынос можно было бы взять больше; но улица при
// щели фасадов 0.0-0.3 м (техблок) шириной 6 м, и два ряда по 0.9 на трёх
// этажах съели бы 5.4 м из шести. 0.7 оставляет вверху 1.8 м неба — ровно ту
// щель, которой готический рыночный переулок и читается.
// ---------------------------------------------------------------------------
inline void forge_cornhall_jetty(const char* file) {
    Forge f;
    const float W = 7.0f;
    const float D = 9.0f;
    const float JUT = 0.7f;
    const float PL = 0.45f;
    const float H1 = 3.0f;
    const float H2 = 2.8f;
    const float H3 = 2.6f;
    const float Y2 = PL + H1;
    const float Y3 = Y2 + H2;
    const float EAVES = Y3 + H3;
    const float D2 = D + JUT;          // южная кромка второго этажа
    const float D3 = D + JUT * 2.0f;   // и третьего
    const float RIDGE_H = W * 0.5f * CORNHALL_PITCH;

    (void)brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, PL, 0.42f, "2", "0.6");
    (void)brick_wall(f, 0.0f, W, D, W, 0.0f, PL, 0.42f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, PL, 0.42f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, D, W, D, PL, 0.42f, "2", "0.6");
    parquet_floor(f, 0.0f, 0.0f, W, D, PL + 0.06f);

    // Первый этаж — кирпич, широкая лавочная витрина не строится (раскладка
    // знает один вид проёма на стену), поэтому фасад тот же трёхпролётный.
    const auto low = [&](float ax, float az, float bx, float bz, const char* win,
                         bool door) {
        const ElementId id = brick_wall(f, PL, ax, az, bx, bz, H1, 0.38f, "1", "0.45");
        if (door) {
            (void)f.g.set_param(id, "doors", "1");
            (void)f.g.set_param(id, "porch", "1");
        } else {
            (void)f.g.set_param(id, "windows", win);
        }
    };
    low(W, 0.0f, 0.0f, 0.0f, "2", false);
    low(W, D, W, 0.0f, "3", false);
    low(0.0f, 0.0f, 0.0f, D, "3", false);
    low(0.0f, D, W * 0.34f, D, "1", false);
    low(W * 0.34f, D, W * 0.66f, D, "", true);
    low(W * 0.66f, D, W, D, "1", false);
    f.door_leaf(W * 0.5f, PL, D);

    // ---------- ярусы с выносом --------------------------------------------
    const auto jetty_tier = [&](float y, float h, float zs, const char* wear) {
        const auto up = [&](float ax, float az, float bx, float bz, const char* win) {
            (void)f.wall(f.v(ax, y, az), f.v(bx, y, bz),
                         {{"height", f.num(h)}, {"thickness", "0.3"}, {"mat", "4"},
                          {"tone", "0"}, {"clad", "1"}, {"windows", win},
                          {"shutters", "1"}, {"wear", wear}});
        };
        up(W, 0.0f, 0.0f, 0.0f, "2");
        up(W, zs, W, 0.0f, "3");
        up(0.0f, 0.0f, 0.0f, zs, "3");
        up(0.0f, zs, W, zs, "3");
        // Настил ЯРУСА ПО НОВОМУ ПЯТНУ: он и есть то, чем вынос держится —
        // консоли под ним лишь показывают, как это сделано.
        const ElementId fl = slab_xz(f, 0.0f, 0.0f, W, zs, y, 0.14f, "1", "1", wear);
        (void)f.g.set_param(fl, "beams", "1");
    };
    jetty_tier(Y2, H2, D2, "0.34");
    jetty_tier(Y3, H3, D3, "0.3");

    // КОНСОЛЬНЫЕ БАЛКИ. Наклонный подкос от стены нижнего яруса к кромке
    // верхнего настила: пять на этаж, по шагу простенков.
    const auto brackets = [&](float y, float z_from, float z_to) {
        for (int k = 0; k < 5; ++k) {
            const float x = 0.6f + (W - 1.2f) * static_cast<float>(k) / 4.0f;
            (void)f.beam(f.v(x, y - 1.05f, z_from - 0.12f), f.v(x, y - 0.02f, z_to - 0.1f),
                         {{"radius", "0.11"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.4"}});
        }
        // Лежень по кромке — в него врубаются концы балок настила.
        (void)f.beam(f.v(-0.05f, y - 0.16f, z_to - 0.1f), f.v(W + 0.05f, y - 0.16f, z_to - 0.1f),
                     {{"radius", "0.12"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.4"}});
    };
    brackets(Y2, D, D2);
    brackets(Y3, D2, D3);

    // Марш первого и второго этажа вдоль восточной стены.
    for (const auto& st : {std::pair<float, float>{Y2, H1},
                           std::pair<float, float>{Y3, H2}}) {
        const float y = st.first;
        const float h = st.second;
        (void)f.contour({f.v(W - 1.7f, y - h, D - 0.6f), f.v(W - 0.3f, y - h, D - 0.6f),
                         f.v(W - 0.3f, y, D - 0.6f - h * 1.5f),
                         f.v(W - 1.7f, y, D - 0.6f - h * 1.5f)},
                        {{"thickness", "0.1"}, {"fill", "6"}, {"open", "1"},
                         {"mat", "1"}, {"tone", "1"}, {"wear", "0.32"}});
    }

    // Кровля кроет ШИРОЧАЙШЕЕ пятно — третий ярус; северный торец остаётся на
    // месте, южный уехал на 1.4, и конёк вместе с ним.
    f.gable_roof_z(0.0f, 0.0f, W, D3, EAVES, RIDGE_H, "4", "0", "8", "0.36");
    crow_gable(f, 0.0f, W, D3 + 0.32f, true, EAVES, RIDGE_H, 4, 0.52f, "1", "0.45");
    crow_gable(f, 0.0f, W, -0.32f, false, EAVES, RIDGE_H, 4, 0.52f, "1", "0.5");
    // Подъёмная балка — дом рыночного фронта, значит купеческий (§4.4).
    {
        const float hy = EAVES + RIDGE_H * 0.78f;
        (void)f.beam(f.v(W * 0.5f, hy, D3 + 0.22f), f.v(W * 0.5f, hy, D3 + 1.2f),
                     {{"radius", "0.09"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.4"}});
        (void)f.beam(f.v(W * 0.5f, hy - 0.38f, D3 + 1.08f),
                     f.v(W * 0.5f, hy - 0.02f, D3 + 1.08f),
                     {{"radius", "0.07"}, {"mat", "0"}, {"tone", "2"}, {"wear", "0.4"}});
    }
    if (vents_on()) {
        f.chimney(W * 0.5f, -0.32f, EAVES - 0.6f, EAVES + RIDGE_H + 1.5f, 0.34f, true,
                  "4", "2", "0.55");
    }
    f.save(file);
}

// ---------------------------------------------------------------------------
// РЯДНАЯ ЛАВКА С АРКАДОЙ (§4.6: «крытый рыночный ряд — аркада на кирпичных
// столбах, две длинные стороны площади; пролёт 3 м»).
//
// ЕДИНСТВЕННЫЙ ЖИЛОЙ РЕЦЕПТ ПАКЕТА БЕЗ СТУПЕНЧАТОГО ЩИПЦА, И ЭТО НЕ ПРОПУСК.
// Ряд стоит ДЛИННОЙ стороной к площади, значит конёк идёт вдоль улицы, а
// щипцовые торцы приходятся на брандмауэры между соседями — их не видно
// никогда. Подпись этого рецепта — аркада, и вешать на него ещё и щипец
// значило бы поставить признак туда, где его никто не прочтёт.
//
// ПРОХОДИМОСТЬ ГАЛЕРЕИ ЗАМЕРЕНА, А НЕ ОБЕЩАНА: пята арок на 2.6 м, значит
// самое низкое место прохода — 2.6 (у столба), глубина галереи 2.6 м, чистый
// пролёт между столбами 3.0. Капсула игрока проходит везде.
// ---------------------------------------------------------------------------
inline void forge_cornhall_arcade(const char* file) {
    Forge f;
    const float PIER = 0.6f;      // кирпичный столб в плане
    const float BAY = 3.0f;       // чистый пролёт (§4.6)
    const int BAYS = 3;
    const float W = static_cast<float>(BAYS) * BAY + static_cast<float>(BAYS + 1) * PIER;
    const float D = 7.0f;         // тело лавки
    const float GAL = 2.6f;       // глубина галереи
    const float DG = D + GAL;     // южная кромка аркады
    const float SPRING = 2.6f;    // пята арок
    const float LINTEL = 4.0f;    // низ пояса-перемычки над арками
    const float Y2 = 4.5f;        // пол второго этажа = верх аркады
    const float H2 = 2.9f;
    const float EAVES = Y2 + H2;
    const float RIDGE_H = (D + GAL) * 0.5f * CORNHALL_PITCH;

    // ---------- тело лавки: кирпичный первый этаж --------------------------
    (void)brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, 0.45f, 0.42f, "2", "0.6");
    (void)brick_wall(f, 0.0f, W, D, W, 0.0f, 0.45f, 0.42f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, 0.45f, 0.42f, "2", "0.6");
    parquet_floor(f, 0.0f, 0.0f, W, D, 0.08f);
    {
        const ElementId n = brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, Y2, 0.4f, "1", "0.45");
        (void)f.g.set_param(n, "windows", "5");
        const ElementId e = brick_wall(f, 0.0f, W, D, W, 0.0f, Y2, 0.4f, "1", "0.45");
        (void)f.g.set_param(e, "windows", "2");
        const ElementId w = brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, Y2, 0.4f, "1", "0.45");
        (void)f.g.set_param(w, "windows", "2");
    }
    // Задняя стенка галереи — фасад лавок: дверь в среднем пролёте, окна по
    // краям. Три пролёта по числу арок, чтобы проём попадал в свою арку.
    for (int k = 0; k < BAYS; ++k) {
        const float x0 = PIER + static_cast<float>(k) * (BAY + PIER);
        const float x1 = x0 + BAY;
        const ElementId a = brick_wall(f, 0.0f, x0, D, x1, D, Y2, 0.36f, "1", "0.42");
        if (k == 1) {
            (void)f.g.set_param(a, "doors", "1");
        } else {
            (void)f.g.set_param(a, "windows", "2");
        }
        // Кусок стены НАПРОТИВ столба — иначе фасад лавок дырявый в простенках.
        (void)brick_wall(f, 0.0f, x0 - PIER, D, x0, D, Y2, 0.36f, "1", "0.42");
    }
    (void)brick_wall(f, 0.0f, W - PIER, D, W, D, Y2, 0.36f, "1", "0.42");
    f.door_leaf(PIER + (BAY + PIER) + BAY * 0.5f, 0.0f, D);

    // ---------- аркада: столбы, арки, пояс ---------------------------------
    for (int k = 0; k <= BAYS; ++k) {
        const float x0 = static_cast<float>(k) * (BAY + PIER);
        (void)brick_wall(f, 0.0f, x0, DG - PIER * 0.5f, x0 + PIER, DG - PIER * 0.5f,
                         LINTEL + 0.6f, PIER, "1", "0.5");
    }
    for (int k = 0; k < BAYS; ++k) {
        const float cx = PIER + static_cast<float>(k) * (BAY + PIER) + BAY * 0.5f;
        brick_arch(f, cx, DG - PIER * 0.5f, BAY, SPRING, 0.24f, "1", "0.45");
    }
    // ПОЯС НАД АРКАМИ. Он же — то, чем аркада становится ОДНИМ телом: пояс
    // накрывает и макушки арок (4.1), и верх всех столбов, перехлёстывая обе.
    (void)brick_wall(f, LINTEL, 0.0f, DG - PIER * 0.5f, W, DG - PIER * 0.5f, 0.6f, 0.5f,
                     "1", "0.45");
    // Мостовая галереи и её порог.
    (void)slab_xz(f, 0.0f, D, W, DG, 0.06f, 0.14f, "3", "1", "0.55");

    // ---------- второй этаж поверх галереи ---------------------------------
    {
        const ElementId fl = slab_xz(f, 0.0f, 0.0f, W, DG, Y2, 0.16f, "1", "1", "0.35");
        (void)f.g.set_param(fl, "beams", "1");
    }
    const auto up = [&](float ax, float az, float bx, float bz, const char* win) {
        (void)f.wall(f.v(ax, Y2, az), f.v(bx, Y2, bz),
                     {{"height", f.num(H2)}, {"thickness", "0.3"}, {"mat", "4"},
                      {"tone", "0"}, {"clad", "1"}, {"windows", win},
                      {"shutters", "1"}, {"wear", "0.32"}});
    };
    up(W, 0.0f, 0.0f, 0.0f, "6");
    up(W, DG, W, 0.0f, "3");
    up(0.0f, 0.0f, 0.0f, DG, "3");
    up(0.0f, DG, W, DG, "6");
    // Марш из галереи на второй этаж — в северо-восточном углу лавки.
    (void)f.contour({f.v(W - 1.8f, 0.08f, D - 0.5f), f.v(W - 0.4f, 0.08f, D - 0.5f),
                     f.v(W - 0.4f, Y2, D - 0.5f - Y2 * 1.5f),
                     f.v(W - 1.8f, Y2, D - 0.5f - Y2 * 1.5f)},
                    {{"thickness", "0.1"}, {"fill", "6"}, {"open", "1"}, {"mat", "1"},
                     {"tone", "1"}, {"wear", "0.32"}});
    f.gable_roof(0.0f, 0.0f, W, DG, EAVES, RIDGE_H, "4", "0", "8", "0.36");
    if (vents_on()) {
        f.chimney(2.4f, 1.2f, EAVES - 0.5f, EAVES + RIDGE_H + 1.2f, 0.32f, true, "4",
                  "2", "0.55");
        f.chimney(W - 2.4f, 1.2f, EAVES - 0.5f, EAVES + RIDGE_H + 1.2f, 0.32f, true,
                  "4", "2", "0.55");
    }
    f.save(file);
}

// ---------------------------------------------------------------------------
// РАТУША С БАШНЕЙ, ЧАСАМИ И ОТКРЫТОЙ ЛОДЖИЕЙ (§4.8: «площадь; доминанта №2»).
//
// ДОМИНАНТА ЧИТАЕТСЯ СТУПЕНЯМИ, А НЕ ОДНОЙ ВЫСОТОЙ (CITY_DESIGN_GUIDE §8, тот
// же довод, по которому замок Вайтрана переписывали ярусами). Здесь ступеней
// четыре: зал под черепицей (конёк 12.5), ствол башни (14.4), часовой ярус с
// вылетной площадкой (18.0) и шатёр (22.6). Самый высокий ЖИЛОЙ рецепт пакета
// — большой дом с коньком 12.4; башня выше него ровно вдвое, что и требует
// гайд от доминанты (в 2-3 раза выше второго объекта).
//
// ЛОДЖИЯ — ТА ЖЕ АРКАДА, ЧТО У РЫНОЧНОГО РЯДА, И ЭТО НАМЕРЕННО. Город, у
// которого ратуша и торговый ряд говорят одним приёмом, читается ОДНОЙ школой;
// разные арки в ста метрах друг от друга читались бы двумя разными городами.
// ---------------------------------------------------------------------------
inline void forge_cornhall_townhall(const char* file) {
    Forge f;
    const float W = 15.0f;      // зал по фасаду площади
    const float D = 9.0f;       // глубина зала
    const float PIER = 0.7f;
    const float BAY = 3.0f;
    const float LOG_Z = D;      // лоджия утоплена В зал, а не пристроена
    const float SPRING = 2.8f;
    const float Y2 = 4.8f;      // пол верхнего зала
    const float H2 = 3.6f;
    const float EAVES = Y2 + H2;
    const float RIDGE_H = D * 0.5f * CORNHALL_PITCH;
    // Башня: западный торец, вынесена вперёд на площадь.
    const float TX0 = 0.0f;
    const float TX1 = 4.6f;
    const float TZ0 = 2.2f;
    const float TZ1 = 6.8f;
    const float T_SHAFT = 14.4f;   // верх ствола
    const float T_CLOCK = 18.0f;   // верх часового яруса

    // ---------- зал: цоколь, стены, лоджия ---------------------------------
    // ЦОКОЛЬ НЕ ИДЁТ ПОПЕРЁК ЛОДЖИИ (та же находка, что у проездной арки
    // постоялого двора): лента 0.6 м под аркадой — порог втрое выше того, по
    // которому раскладка сама бракует мощение, и в открытую лоджию нельзя
    // было бы войти. Западная треть фасада ленту несёт: там дверь, и под
    // дверь раскладка ставит общее крыльцо city-stoop.
    (void)brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, 0.6f, 0.5f, "2", "0.6");
    (void)brick_wall(f, 0.0f, W, D, W, 0.0f, 0.6f, 0.5f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, 0.6f, 0.5f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, D, W - (3.0f * BAY + 4.0f * PIER), D, 0.6f,
                     0.5f, "2", "0.6");
    parquet_floor(f, 0.0f, 0.0f, W, D, 0.1f);
    {
        const ElementId n = brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, Y2, 0.45f, "1", "0.4");
        (void)f.g.set_param(n, "windows", "5");
        const ElementId e = brick_wall(f, 0.0f, W, D, W, 0.0f, Y2, 0.45f, "1", "0.4");
        (void)f.g.set_param(e, "windows", "3");
        const ElementId w = brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, Y2, 0.45f, "1", "0.4");
        (void)f.g.set_param(w, "windows", "3");
    }
    // ЛОДЖИЯ: три арки в южном фасаде, и за ними — глухая стена зала, которая
    // отступает на глубину лоджии. Столбы стоят в плоскости фасада.
    //
    // НАЧАЛО ЛОДЖИИ ВЫВОДИТСЯ ИЗ ЕЁ ДЛИНЫ, А НЕ НАЗНАЧАЕТСЯ. Три пролёта по
    // 3.0 и четыре столба по 0.7 — это 11.8 м; при назначенном начале 5.0
    // последний столб вставал на x=16.8 при зале шириной 15, то есть на
    // 1.8 м ЗА восточной стеной (нашлось по колонке манифеста: g-габарит
    // растянулся до 16.8 и увёл ось двери в -X). Восточный конец лоджии
    // теперь совпадает с углом зала по построению.
    const float LOG_LEN = 3.0f * BAY + 4.0f * PIER;
    const float LOG_X0 = W - LOG_LEN;
    for (int k = 0; k <= 3; ++k) {
        const float x0 = LOG_X0 + static_cast<float>(k) * (BAY + PIER);
        (void)brick_wall(f, 0.0f, x0, LOG_Z - PIER * 0.5f, x0 + PIER,
                         LOG_Z - PIER * 0.5f, 4.2f, PIER, "1", "0.45");
    }
    for (int k = 0; k < 3; ++k) {
        const float cx = LOG_X0 + PIER + static_cast<float>(k) * (BAY + PIER) + BAY * 0.5f;
        brick_arch(f, cx, LOG_Z - PIER * 0.5f, BAY, SPRING, 0.26f, "1", "0.42");
    }
    // Пояс над арками — им лоджия становится одним телом со столбами.
    (void)brick_wall(f, 4.1f, LOG_X0, LOG_Z - PIER * 0.5f, W, LOG_Z - PIER * 0.5f, 0.7f,
                     0.55f, "1", "0.42");
    // Западная треть фасада — глухая стена с окнами конторы.
    {
        const ElementId s = brick_wall(f, 0.0f, 0.0f, D, LOG_X0, D, Y2, 0.45f, "1", "0.4");
        (void)f.g.set_param(s, "windows", "2");
    }
    // ВХОД — В ЗАДНЕЙ СТЕНЕ ЛОДЖИИ, ПОСЕРЕДИНЕ. В ратушу входят ЧЕРЕЗ лоджию,
    // а не мимо неё, и дверь, стоявшая в западной трети фасада, ломала ещё и
    // посадку: манифест выводит ось двери из смещения точки от центра
    // g-габарита, и дверь на x=2.5 при зале 17 м читалась как «дверь смотрит
    // на запад». Здесь дверная точка лежит близко к оси зала, и ось выходит
    // +Z — та, которой дом и повёрнут к площади.
    const float ENTRY = (LOG_X0 + W) * 0.5f;
    {
        const ElementId l = brick_wall(f, 0.0f, LOG_X0, D - 2.6f, ENTRY - 2.1f, D - 2.6f,
                                       Y2, 0.4f, "1", "0.4");
        (void)f.g.set_param(l, "windows", "2");
        const ElementId m = brick_wall(f, 0.0f, ENTRY - 2.1f, D - 2.6f, ENTRY + 2.1f,
                                       D - 2.6f, Y2, 0.4f, "1", "0.4");
        (void)f.g.set_param(m, "doors", "1");
        (void)f.g.set_param(m, "porch", "1");
        const ElementId r = brick_wall(f, 0.0f, ENTRY + 2.1f, D - 2.6f, W, D - 2.6f, Y2,
                                       0.4f, "1", "0.4");
        (void)f.g.set_param(r, "windows", "2");
    }
    f.door_leaf(ENTRY, 0.0f, D - 2.6f);
    (void)slab_xz(f, LOG_X0, D - 2.6f, W, D, 0.08f, 0.16f, "3", "1", "0.5");

    // ---------- верхний зал и кровля ---------------------------------------
    {
        const ElementId fl = slab_xz(f, 0.0f, 0.0f, W, D, Y2, 0.18f, "1", "1", "0.3");
        (void)f.g.set_param(fl, "beams", "1");
    }
    const auto up = [&](float ax, float az, float bx, float bz, const char* win) {
        (void)f.wall(f.v(ax, Y2, az), f.v(bx, Y2, bz),
                     {{"height", f.num(H2)}, {"thickness", "0.34"}, {"mat", "4"},
                      {"tone", "0"}, {"clad", "1"}, {"windows", win},
                      {"shutters", "1"}, {"wear", "0.3"}});
    };
    up(W, 0.0f, 0.0f, 0.0f, "6");
    up(W, D, W, 0.0f, "3");
    up(0.0f, 0.0f, 0.0f, D, "3");
    up(0.0f, D, W, D, "6");
    (void)f.contour({f.v(1.0f, 0.1f, D - 0.8f), f.v(2.6f, 0.1f, D - 0.8f),
                     f.v(2.6f, Y2, D - 0.8f - Y2 * 1.5f),
                     f.v(1.0f, Y2, D - 0.8f - Y2 * 1.5f)},
                    {{"thickness", "0.1"}, {"fill", "6"}, {"open", "1"}, {"mat", "1"},
                     {"tone", "1"}, {"wear", "0.3"}});
    f.gable_roof(0.0f, 0.0f, W, D, EAVES, RIDGE_H, "4", "0", "8", "0.34");

    // ---------- БАШНЯ: ствол, часовой ярус, шатёр --------------------------
    // Ствол идёт ОТ ЗЕМЛИ сквозь зал: башня, начатая с карниза, читается
    // надстройкой, а не башней (и висела бы островом до первого этажа).
    {
        float y = 0.0f;
        const float TIER[4] = {3.8f, 3.6f, 3.6f, 3.4f};
        for (int t = 0; t < 4; ++t) {
            const char* win = t == 0 ? "" : "1";
            const auto q = [&](float ax, float az, float bx, float bz) {
                const ElementId id =
                    brick_wall(f, y, ax, az, bx, bz, TIER[t], 0.55f, "1", "0.42");
                if (win[0] != 0) {
                    (void)f.g.set_param(id, "windows", win);
                }
            };
            q(TX1, TZ0, TX0, TZ0);  // север
            q(TX1, TZ1, TX1, TZ0);  // восток
            q(TX0, TZ0, TX0, TZ1);  // запад
            q(TX0, TZ1, TX1, TZ1);  // юг
            y += TIER[t];
        }
    }
    // ЧАСОВАЯ ПЛОЩАДКА: плита с напуском 0.45 на кронштейнах — тот самый
    // вылет, по которому башня ратуши узнаётся с другого конца площади.
    for (const float bx : {TX0 + 0.7f, TX0 + 2.3f, TX1 - 0.7f}) {
        for (const float bz : {TZ0, TZ1}) {
            const float dz = bz < (TZ0 + TZ1) * 0.5f ? -0.42f : 0.42f;
            (void)f.beam(f.v(bx, T_SHAFT - 1.2f, bz), f.v(bx, T_SHAFT - 0.12f, bz + dz),
                         {{"radius", "0.11"}, {"form", "square"}, {"mat", "3"},
                          {"tone", "1"}, {"wear", "0.5"}});
        }
    }
    {
        const ElementId g = slab_xz(f, TX0 - 0.45f, TZ0 - 0.45f, TX1 + 0.45f,
                                    TZ1 + 0.45f, T_SHAFT, 0.24f, "3", "1", "0.5");
        (void)f.g.set_param(g, "unsupported", "1");
    }
    // Часовой ярус со звонными проёмами и ЦИФЕРБЛАТОМ на южной грани.
    {
        const auto q = [&](float ax, float az, float bx, float bz, const char* win) {
            const ElementId id =
                brick_wall(f, T_SHAFT, ax, az, bx, bz, T_CLOCK - T_SHAFT, 0.5f, "1",
                           "0.42");
            (void)f.g.set_param(id, "windows", win);
        };
        q(TX1, TZ0, TX0, TZ0, "1");
        q(TX1, TZ1, TX1, TZ0, "1");
        q(TX0, TZ0, TX0, TZ1, "1");
        q(TX0, TZ1, TX1, TZ1, "1");
    }
    {
        // ЦИФЕРБЛАТ — восьмиугольник штукатурки в плоскости южной грани, две
        // стрелки поверх. Восемь граней, а не круг: кривых у графа нет, а на
        // радиусе 1.05 м восьмиугольник с двадцати метров — круг.
        const float cx = (TX0 + TX1) * 0.5f;
        const float cy = (T_SHAFT + T_CLOCK) * 0.5f + 0.2f;
        const float cz = TZ1 + 0.28f;
        const float r = 1.05f;
        std::vector<VertexId> face;
        for (int k = 0; k < 8; ++k) {
            const float t = 6.28318531f * static_cast<float>(k) / 8.0f;
            face.push_back(f.v(cx + r * std::cos(t), cy + r * std::sin(t), cz));
        }
        const ElementId dial = f.contour(std::move(face),
                                         {{"thickness", "0.1"}, {"mat", "5"},
                                          {"tone", "0"}, {"wear", "0.3"}});
        (void)f.g.set_param(dial, "unsupported", "1");
        (void)f.beam(f.v(cx, cy, cz + 0.08f), f.v(cx + 0.36f, cy + 0.62f, cz + 0.08f),
                     {{"radius", "0.05"}, {"form", "plank"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.3"}});
        (void)f.beam(f.v(cx, cy, cz + 0.08f), f.v(cx - 0.7f, cy - 0.22f, cz + 0.08f),
                     {{"radius", "0.04"}, {"form", "plank"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.3"}});
    }
    hip_cap(f, TX0, TZ0, TX1, TZ1, T_CLOCK, 0.4f, 4.6f, "4", "0", "8", "0.34");
    // Флюгер-шпиль на макушке (§5: флаги гильдий на ратуше — древко под них).
    (void)f.beam(f.v((TX0 + TX1) * 0.5f, T_CLOCK + 0.1f + 4.6f,
                     (TZ0 + TZ1) * 0.5f),
                 f.v((TX0 + TX1) * 0.5f, T_CLOCK + 0.1f + 6.4f, (TZ0 + TZ1) * 0.5f),
                 {{"radius", "0.07"}, {"mat", "3"}, {"tone", "2"}, {"wear", "0.4"}});
    if (vents_on()) {
        f.chimney(W - 2.2f, 1.4f, EAVES - 0.5f, EAVES + RIDGE_H + 1.4f, 0.36f, true,
                  "4", "2", "0.55");
    }
    f.save(file);
}

// ---------------------------------------------------------------------------
// ВЕТРЯНАЯ МЕЛЬНИЦА-СТОЛБОВКА (§4.9: «вал за стеной, три штуки»; §8: «три
// ветряка на валу — образ города с тракта за километр»).
//
// СТОЛБОВКА — ЭТО НЕ БАШНЯ С КРЫЛЬЯМИ, А АМБАР НА СТОЛБЕ. Весь корпус
// поворачивается на одном дубовом столбе, и низ столба закрыт кирпичным
// подклетом; хвостовая лестница спускается с задней стены на землю и служит
// рычагом, которым корпус разворачивают по ветру. Отсюда три обязательных
// признака силуэта, и все три здесь есть: корпус ВЫШЕ земли и ничем не
// подпёрт по бокам, лестница идёт ОТ КОРПУСА в землю (а не наоборот), крылья
// сидят на переднем щипце, а не на боку.
//
// КРЫЛЬЯ — РЕШЁТКА, А НЕ ЛОПАСТЬ. Маховое крыло английской мельницы это
// маховик (whip), поперечины (bars) и обрешётка; сплошная доска на нём
// появляется только под парусиной, которую снимают. Решётка и дешевле: четыре
// крыла по восемь тел против четырёх плоскостей, зато на просвет она читается
// мельницей с любого ракурса, а плоскость с торца исчезает.
// ---------------------------------------------------------------------------
inline void forge_cornhall_windmill(const char* file) {
    Forge f;
    const float BW = 5.6f;      // кирпичный подклет в плане
    const float BH = 2.3f;      // его высота
    const float CX = BW * 0.5f;
    const float BX0 = 0.8f;     // корпус: x 0.8..4.8
    const float BX1 = 4.8f;
    const float BZ0 = 0.2f;     // корпус: z 0.2..5.6, сноса нет — вал вдоль Z
    const float BZ1 = 5.6f;
    const float BODY_Y = 3.0f;  // пол корпуса
    const float BODY_H = 3.3f;  // его стены
    const float EAVES = BODY_Y + BODY_H;
    const float RIDGE_H = (BX1 - BX0) * 0.5f * CORNHALL_PITCH;
    const float POST_TOP = 5.4f;

    // ---------- подклет: кирпичное кольцо, в котором сидит основание -------
    (void)brick_wall(f, 0.0f, BW, 0.0f, 0.0f, 0.0f, BH, 0.45f, "1", "0.55");
    (void)brick_wall(f, 0.0f, BW, BW, BW, 0.0f, BH, 0.45f, "1", "0.55");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, BW, BH, 0.45f, "1", "0.55");
    (void)brick_wall(f, 0.0f, 0.0f, BW, BW, BW, BH, 0.45f, "1", "0.55");
    {
        const ElementId cap = slab_xz(f, -0.3f, -0.3f, BW + 0.3f, BW + 0.3f, BH, 0.22f,
                                      "4", "1", "0.55");
        (void)f.g.set_param(cap, "unsupported", "1");
    }
    // СТОЛБ — от земли сквозь подклет в корпус. Он и есть то, чем мельница
    // стоит; всё остальное на нём висит.
    (void)f.beam(f.v(CX, 0.0f, BW * 0.5f), f.v(CX, POST_TOP, BW * 0.5f),
                 {{"radius", "0.42"}, {"form", "square"}, {"mat", "0"}, {"tone", "2"},
                  {"wear", "0.5"}});
    // Подкосы-четвертины от столба к углам подклета: без них столб читается
    // воткнутой палкой.
    for (const auto& c : {std::pair<float, float>{0.7f, 0.7f},
                          {BW - 0.7f, 0.7f}, {0.7f, BW - 0.7f},
                          {BW - 0.7f, BW - 0.7f}}) {
        (void)f.beam(f.v(c.first, 0.35f, c.second), f.v(CX, POST_TOP - 1.5f, BW * 0.5f),
                     {{"radius", "0.13"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.5"}});
    }

    // ---------- корпус: обшитая тёсом коробка на столбе ---------------------
    {
        const ElementId fl = slab_xz(f, BX0, BZ0, BX1, BZ1, BODY_Y, 0.2f, "0", "2", "0.5");
        (void)f.g.set_param(fl, "beams", "1");
        (void)f.g.set_param(fl, "unsupported", "1");
    }
    {
        const auto q = [&](float ax, float az, float bx, float bz, const char* win,
                           bool door) {
            const ElementId id =
                f.wall(f.v(ax, BODY_Y, az), f.v(bx, BODY_Y, bz),
                       {{"height", f.num(BODY_H)}, {"thickness", "0.26"},
                        {"mat", "1"}, {"tone", "2"}, {"clad", "1"}, {"wear", "0.55"}});
            if (door) {
                (void)f.g.set_param(id, "doors", "1");
            } else if (win[0] != 0) {
                (void)f.g.set_param(id, "windows", win);
            }
        };
        q(BX1, BZ0, BX0, BZ0, "", false);   // север — сюда садится вал крыльев
        q(BX1, BZ1, BX1, BZ0, "1", false);  // восток
        q(BX0, BZ0, BX0, BZ1, "1", false);  // запад
        q(BX0, BZ1, BX1, BZ1, "", true);    // юг — дверь на хвостовую лестницу
    }
    f.door_leaf((BX0 + BX1) * 0.5f, BODY_Y, BZ1);
    f.gable_roof_z(BX0, BZ0, BX1, BZ1, EAVES, RIDGE_H, "1", "2", "7", "0.55");

    // ---------- ХВОСТОВАЯ ЛЕСТНИЦА: она же рычаг разворота ------------------
    (void)f.contour({f.v((BX0 + BX1) * 0.5f - 0.75f, 0.0f, BZ1 + 4.6f),
                     f.v((BX0 + BX1) * 0.5f + 0.75f, 0.0f, BZ1 + 4.6f),
                     f.v((BX0 + BX1) * 0.5f + 0.75f, BODY_Y, BZ1 + 0.05f),
                     f.v((BX0 + BX1) * 0.5f - 0.75f, BODY_Y, BZ1 + 0.05f)},
                    {{"thickness", "0.12"}, {"fill", "6"}, {"open", "1"}, {"mat", "1"},
                     {"tone", "2"}, {"wear", "0.55"}});
    // Хвостовой брус-водило, за который мельницу поворачивают. ВЕРХНИЙ КОНЕЦ
    // ЗАВЕДЁН ПОД НАСТИЛ КОРПУСА, а не приставлен к его южной стене: судья
    // намерил зазор 0.110 м до створки — брус, кончавшийся у обшивки, висел
    // отдельным островом, потому что торец наклонного бруса срезан наискось и
    // до плоскости стены не достаёт.
    (void)f.beam(f.v((BX0 + BX1) * 0.5f, 0.9f, BZ1 + 4.5f),
                 f.v((BX0 + BX1) * 0.5f, BODY_Y + 0.1f, BZ1 - 0.3f),
                 {{"radius", "0.13"}, {"form", "square"}, {"mat", "0"}, {"tone", "2"},
                  {"wear", "0.55"}});

    // ---------- ВАЛ И ЧЕТЫРЕ КРЫЛА -----------------------------------------
    const float HX = (BX0 + BX1) * 0.5f;
    const float HY = EAVES + RIDGE_H * 0.42f;  // ось чуть ниже конька
    const float HZ = BZ0 - 1.5f;               // ступица вынесена за щипец
    (void)f.beam(f.v(HX, HY - 0.45f, BZ0 + 1.6f), f.v(HX, HY, HZ),
                 {{"radius", "0.24"}, {"form", "square"}, {"mat", "0"}, {"tone", "2"},
                  {"wear", "0.55"}});
    constexpr float SAIL_R = 5.0f;   // размах 10 м — мельница видна с тракта
    constexpr float SAIL_IN = 1.0f;  // от ступицы до начала обрешётки
    for (int s = 0; s < 4; ++s) {
        const float a = 0.78539816f + 1.57079633f * static_cast<float>(s);
        const float ux = std::cos(a);
        const float uy = std::sin(a);
        // Маховик: главный брус от ступицы к концу крыла.
        (void)f.beam(f.v(HX, HY, HZ),
                     f.v(HX + ux * SAIL_R, HY + uy * SAIL_R, HZ - 0.18f),
                     {{"radius", "0.11"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.55"}});
        // Передняя кромка — второй брус, отнесённый поперёк маха: крыло
        // получает ШИРИНУ, без которой оно палка.
        const float px = -uy;
        const float py = ux;
        (void)f.beam(f.v(HX + px * 0.55f + ux * SAIL_IN,
                         HY + py * 0.55f + uy * SAIL_IN, HZ - 0.34f),
                     f.v(HX + px * 0.55f + ux * SAIL_R, HY + py * 0.55f + uy * SAIL_R,
                         HZ - 0.34f),
                     {{"radius", "0.07"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.55"}});
        // Обрешётка: шесть поперечин от маховика к передней кромке.
        for (int k = 0; k < 6; ++k) {
            const float t = SAIL_IN + (SAIL_R - SAIL_IN - 0.3f)
                          * static_cast<float>(k) / 5.0f;
            (void)f.beam(f.v(HX + ux * t - px * 0.18f, HY + uy * t - py * 0.18f,
                             HZ - 0.24f),
                         f.v(HX + ux * t + px * 0.62f, HY + uy * t + py * 0.62f,
                             HZ - 0.3f),
                         {{"radius", "0.05"}, {"form", "plank"}, {"mat", "1"},
                          {"tone", "2"}, {"wear", "0.6"}});
        }
    }
    f.save(file);
}

// ---------------------------------------------------------------------------
// ПИВОВАРНЯ С СОЛОДОВНЕЙ И ВЫСОКОЙ СУШИЛЬНОЙ ПЕЧЬЮ (§4.10: «квартал у воды;
// труба — доминанта КВАРТАЛА»; §4.11 — сушильня хмеля с вытяжным колпаком).
//
// ТРУБА ДЕРЖИТ КВАРТАЛ, НО НЕ СПОРИТ С РАТУШЕЙ. Сушильный ствол выведен на
// 13.6 м против 22.6 у башни ратуши: доминанта квартала обязана быть выше
// своих соседей (жильё 8.7-12.4) и ниже городской — иначе на силуэте города
// два главных предмета, то есть ни одного.
//
// КОЛПАК-ФЛЮГЕР ПОВОРАЧИВАЕТСЯ ПО ВЕТРУ, И ЭТО ВИДНО СТОЯЩИМ: движущихся
// частей у конвейера нет (CITIES_12: «движущихся платформ нет»), но КОСОЙ
// колпак с хвостовиком читается поворотным, а прямой — нет. Поэтому конус
// поставлен с наклоном и с длинным хвостовиком-крылом.
// ---------------------------------------------------------------------------
inline void forge_cornhall_brewery(const char* file) {
    Forge f;
    const float W = 11.0f;      // варня
    const float D = 8.0f;
    const float Y2 = 4.2f;      // пол верхнего яруса (солодовня)
    const float H2 = 3.2f;
    const float EAVES = Y2 + H2;
    const float RIDGE_H = D * 0.5f * CORNHALL_PITCH;
    // Сушильный ствол — пристроен к восточному торцу, стоит своими стенами.
    const float KX0 = W - 0.3f;
    const float KX1 = W + 2.5f;
    const float KZ0 = 1.6f;
    const float KZ1 = 4.4f;
    const float K_TOP = 11.2f;

    (void)brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, 0.5f, 0.48f, "2", "0.62");
    (void)brick_wall(f, 0.0f, W, D, W, 0.0f, 0.5f, 0.48f, "2", "0.62");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, 0.5f, 0.48f, "2", "0.62");
    (void)brick_wall(f, 0.0f, 0.0f, D, W, D, 0.5f, 0.48f, "2", "0.62");
    parquet_floor(f, 0.0f, 0.0f, W, D, 0.1f);
    {
        const auto low = [&](float ax, float az, float bx, float bz, const char* win,
                             bool door) {
            const ElementId id = brick_wall(f, 0.0f, ax, az, bx, bz, Y2, 0.42f, "1",
                                            "0.5");
            if (door) {
                (void)f.g.set_param(id, "doors", "1");
                (void)f.g.set_param(id, "porch", "1");
            } else {
                (void)f.g.set_param(id, "windows", win);
            }
        };
        low(W, 0.0f, 0.0f, 0.0f, "4", false);
        low(W, D, W, 0.0f, "2", false);
        low(0.0f, 0.0f, 0.0f, D, "3", false);
        low(0.0f, D, W * 0.35f, D, "1", false);
        low(W * 0.35f, D, W * 0.65f, D, "", true);
        low(W * 0.65f, D, W, D, "1", false);
    }
    f.door_leaf(W * 0.5f, 0.0f, D);
    {
        const ElementId fl = slab_xz(f, 0.0f, 0.0f, W, D, Y2, 0.16f, "1", "1", "0.4");
        (void)f.g.set_param(fl, "beams", "1");
    }
    // Верхний ярус — солодовня: фахверк, окон много и они узкие (солод сушат
    // сквозняком), ставни настежь.
    {
        const auto up = [&](float ax, float az, float bx, float bz, const char* win) {
            (void)f.wall(f.v(ax, Y2, az), f.v(bx, Y2, bz),
                         {{"height", f.num(H2)}, {"thickness", "0.3"}, {"mat", "4"},
                          {"tone", "0"}, {"clad", "1"}, {"windows", win},
                          {"shutters", "1"}, {"wear", "0.38"}});
        };
        up(W, 0.0f, 0.0f, 0.0f, "5");
        up(W, D, W, 0.0f, "3");
        up(0.0f, 0.0f, 0.0f, D, "3");
        up(0.0f, D, W, D, "5");
    }
    (void)f.contour({f.v(0.5f, 0.1f, D - 0.8f), f.v(1.9f, 0.1f, D - 0.8f),
                     f.v(1.9f, Y2, D - 0.8f - Y2 * 1.5f),
                     f.v(0.5f, Y2, D - 0.8f - Y2 * 1.5f)},
                    {{"thickness", "0.1"}, {"fill", "6"}, {"open", "1"}, {"mat", "1"},
                     {"tone", "1"}, {"wear", "0.4"}});
    f.gable_roof(0.0f, 0.0f, W, D, EAVES, RIDGE_H, "4", "0", "8", "0.4");

    // ---------- СУШИЛЬНЫЙ СТВОЛ --------------------------------------------
    // Четыре стены от земли: пристройка, а не труба на кровле. Кладка тёмная —
    // сажа на кирпиче (§6 паспорта просит эту клетку отдельно; тёмный ряд
    // обжига её и даёт, пока своей нет).
    {
        float y = 0.0f;
        const float TIER[3] = {3.9f, 3.7f, 3.6f};
        for (int t = 0; t < 3; ++t) {
            const auto q = [&](float ax, float az, float bx, float bz, const char* win) {
                const ElementId id =
                    brick_wall(f, y, ax, az, bx, bz, TIER[t], 0.42f, t == 2 ? "2" : "1",
                               t == 2 ? "0.66" : "0.55");
                if (win[0] != 0) {
                    (void)f.g.set_param(id, "windows", win);
                }
            };
            q(KX1, KZ0, KX0, KZ0, t == 0 ? "" : "1");
            q(KX1, KZ1, KX1, KZ0, t == 0 ? "" : "1");
            q(KX0, KZ0, KX0, KZ1, "");
            q(KX0, KZ1, KX1, KZ1, t == 0 ? "" : "1");
            y += TIER[t];
        }
    }
    {
        const ElementId cor = slab_xz(f, KX0 - 0.3f, KZ0 - 0.3f, KX1 + 0.3f, KZ1 + 0.3f,
                                      K_TOP, 0.22f, "4", "2", "0.66");
        (void)f.g.set_param(cor, "unsupported", "1");
    }
    // КОЛПАК: четыре косых треугольника к вершине, смещённой на юг, плюс
    // хвостовик-крыло. Смещённая вершина и есть «поворачивается по ветру».
    {
        const float base = K_TOP + 0.11f;
        const glm::vec3 c[4] = {{KX0 - 0.3f, base, KZ0 - 0.3f},
                                {KX1 + 0.3f, base, KZ0 - 0.3f},
                                {KX1 + 0.3f, base, KZ1 + 0.3f},
                                {KX0 - 0.3f, base, KZ1 + 0.3f}};
        const glm::vec3 apex{(KX0 + KX1) * 0.5f, base + 2.3f, KZ0 + 0.4f};
        for (int k = 0; k < 4; ++k) {
            const glm::vec3 a = c[k];
            const glm::vec3 b = c[(k + 1) % 4];
            const ElementId t = f.contour({f.v(a.x, a.y, a.z), f.v(b.x, b.y, b.z),
                                           f.v(apex.x, apex.y, apex.z)},
                                          {{"thickness", "0.13"}, {"mat", "1"},
                                           {"tone", "2"}, {"wear", "0.62"}});
            (void)f.g.set_param(t, "roof", "1");
            (void)f.g.set_param(t, "unsupported", "1");
        }
        // Хвостовик-крыло: доска на кронштейне, за которую колпак и ловит ветер.
        (void)f.beam(f.v(apex.x, apex.y - 0.25f, apex.z),
                     f.v(apex.x, apex.y - 0.9f, KZ1 + 2.2f),
                     {{"radius", "0.09"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.6"}});
        const ElementId vane =
            f.contour({f.v(apex.x - 0.05f, apex.y - 1.35f, KZ1 + 0.9f),
                       f.v(apex.x - 0.05f, apex.y - 0.55f, KZ1 + 2.3f),
                       f.v(apex.x - 0.05f, apex.y - 1.55f, KZ1 + 2.3f)},
                      {{"thickness", "0.07"}, {"mat", "1"}, {"tone", "2"},
                       {"wear", "0.6"}});
        (void)f.g.set_param(vane, "unsupported", "1");
    }
    // БОЧЕК ЗДЕСЬ НЕТ, И ЭТО РЕШЕНИЕ, А НЕ ЗАБЫВЧИВОСТЬ. Три бочки у западной
    // стены стояли в первой выпечке и получили от судьи три отказа по
    // связности: тело, отставленное от стены на 0.29 м, — остров, а бочка,
    // вплотную вросшая в кладку, читается пузырём на фасаде. Пивные бочки в
    // три яруса значатся в §5 паспорта ВНЕШНИМ ДЕКОРОМ, то есть их ставит
    // расстановка отдельными предметами, у которых своё право стоять на земле
    // рядом. Постройка не обязана носить на себе двор.
    f.save(file);
}

// ---------------------------------------------------------------------------
// ПОСТОЯЛЫЙ ДВОР С ПРОЕЗДНОЙ АРКОЙ И ГАЛЕРЕЕЙ (§4.12: «у каждых ворот, три
// штуки»).
//
// АРКА — СКВОЗНАЯ, И ЭТО ЕДИНСТВЕННОЕ, ЧТО ДЕЛАЕТ ЕЁ ПРОЕЗДНОЙ. Две арки в
// торцах и тоннель между ними: гружёная телега въезжает с улицы и выезжает во
// двор. Габарит проезда 3.2 x 3.9 взят по телеге с сеном, а не по капсуле
// игрока; при 3.2 арка ещё уверенно набирается десятью хордами (стрела 2 см).
//
// ГАЛЕРЕЯ ПО ДВОРУ — НА СЕВЕРНОМ ФАСАДЕ, ГДЕ ДВОР И ЕСТЬ. Уличный торец
// закрыт наглухо и несёт ступенчатый щипец; вся жизнь двора — с изнанки. Это
// тот же приём, которым Корнхолл отличается от портового города: снаружи
// плотная красная стена, внутри деревянный ярус.
// ---------------------------------------------------------------------------
inline void forge_cornhall_inn(const char* file) {
    Forge f;
    const float W = 12.0f;
    const float D = 15.0f;
    const float PL = 0.5f;
    const float H1 = 4.4f;      // проезд просит 3.9 в свету
    const float H2 = 3.0f;
    const float H3 = 2.8f;
    const float Y2 = PL + H1;
    const float Y3 = Y2 + H2;
    const float EAVES = Y3 + H3;
    const float RIDGE_H = W * 0.5f * CORNHALL_PITCH;
    const float GX0 = 7.0f;     // проезд: x 7.0..10.2
    const float GX1 = 10.2f;
    const float GCX = (GX0 + GX1) * 0.5f;
    const float SPRING = 2.3f;  // пята: макушка 3.9

    // ЦОКОЛЬ РАЗОРВАН НА УСТЬЯХ ПРОЕЗДА, И ЭТО НЕ КОСМЕТИКА. Сплошная лента
    // 0.5 м поперёк арки — порог, через который телега не переедет, а стоящий
    // рядом city-stoop проездную арку не лечит: крыльцо ставят под дверь, не
    // под ворота. Приёмочный кадр торца показал ровно это (низ арки был
    // засыпан), и разрыв ленты — единственная честная правка: у настоящего
    // постоялого двора мостовая проезда и есть продолжение улицы.
    (void)brick_wall(f, 0.0f, GX0, 0.0f, 0.0f, 0.0f, PL, 0.48f, "2", "0.6");
    (void)brick_wall(f, 0.0f, W, 0.0f, GX1, 0.0f, PL, 0.48f, "2", "0.6");
    (void)brick_wall(f, 0.0f, W, D, W, 0.0f, PL, 0.48f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, PL, 0.48f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, D, GX0, D, PL, 0.48f, "2", "0.6");
    (void)brick_wall(f, 0.0f, GX1, D, W, D, PL, 0.48f, "2", "0.6");
    parquet_floor(f, 0.0f, 0.0f, GX0, D, PL + 0.06f);

    // ---------- первый этаж: зал на западе, проезд на востоке --------------
    {
        const auto low = [&](float ax, float az, float bx, float bz, const char* win,
                             bool door) {
            const ElementId id = brick_wall(f, PL, ax, az, bx, bz, H1, 0.44f, "1", "0.48");
            if (door) {
                (void)f.g.set_param(id, "doors", "1");
                (void)f.g.set_param(id, "porch", "1");
            } else if (win[0] != 0) {
                (void)f.g.set_param(id, "windows", win);
            }
        };
        // Северный (дворовый) торец: слева зал, справа устье проезда.
        low(GX0, 0.0f, 0.0f, 0.0f, "3", false);
        low(W, 0.0f, GX1, 0.0f, "", false);
        // Южный (уличный) торец: дверь трактира слева, устье проезда справа.
        low(0.0f, D, GX0 * 0.45f, D, "1", false);
        low(GX0 * 0.45f, D, GX0, D, "", true);
        low(GX1, D, W, D, "", false);
        low(W, D, W, 0.0f, "5", false);   // восточная длинная
        low(0.0f, 0.0f, 0.0f, D, "5", false);  // западная длинная
    }
    // ЩЁКИ ТОННЕЛЯ ИДУТ ОТ ЗЕМЛИ, А НЕ ОТ ПОЛА ТРАКТИРА. Ленты под проездом
    // нет (см. выше), и щека, начатая на 0.5, висела бы над мостовой щелью в
    // полметра: пол зала выше улицы, а проезд — нет.
    for (const auto& cheek : {std::pair<float, float>{GX0, 1.0f},
                              std::pair<float, float>{GX1, -1.0f}}) {
        const float x = cheek.first;
        const bool north_first = cheek.second > 0.0f;
        (void)brick_wall(f, 0.0f, x, north_first ? 0.0f : D, x, north_first ? D : 0.0f,
                         PL + H1, 0.44f, "1", "0.5");
    }
    // ПЕРЕМЫЧКА НАД АРКОЙ до низа перекрытия: макушка арки на 4.4, пол второго
    // этажа на 4.9, и без этого пояса над воротами оставалась щель в полметра.
    (void)brick_wall(f, 4.3f, GX0, D, GX1, D, PL + H1 - 4.3f, 0.44f, "1", "0.48");
    (void)brick_wall(f, 4.3f, GX1, 0.0f, GX0, 0.0f, PL + H1 - 4.3f, 0.44f, "1", "0.48");
    f.door_leaf((GX0 * 0.45f + GX0) * 0.5f, PL, D);
    // Арки в обоих торцах проезда и настил над ним.
    brick_arch(f, GCX, D - 0.22f, GX1 - GX0, PL + SPRING, 0.28f, "1", "0.45");
    brick_arch(f, GCX, 0.22f, GX1 - GX0, PL + SPRING, 0.28f, "1", "0.45");
    // МОСТОВАЯ ПРОЕЗДА — ЗАПОДЛИЦО С УЛИЦЕЙ (верх 0.13 при пороге ступени
    // 0.20), а не на уровне пола трактира.
    (void)slab_xz(f, GX0, -0.6f, GX1, D + 0.6f, 0.06f, 0.14f, "3", "1", "0.6");

    // ---------- два жилых яруса --------------------------------------------
    for (const auto& t : {std::pair<float, float>{Y2, H2},
                          std::pair<float, float>{Y3, H3}}) {
        const float y = t.first;
        const float h = t.second;
        {
            const ElementId fl = slab_xz(f, 0.0f, 0.0f, W, D, y, 0.16f, "1", "1", "0.36");
            (void)f.g.set_param(fl, "beams", "1");
        }
        const auto up = [&](float ax, float az, float bx, float bz, const char* win) {
            (void)f.wall(f.v(ax, y, az), f.v(bx, y, bz),
                         {{"height", f.num(h)}, {"thickness", "0.3"}, {"mat", "4"},
                          {"tone", "0"}, {"clad", "1"}, {"windows", win},
                          {"shutters", "1"}, {"wear", "0.34"}});
        };
        up(W, 0.0f, 0.0f, 0.0f, "4");
        up(W, D, W, 0.0f, "6");
        up(0.0f, 0.0f, 0.0f, D, "6");
        up(0.0f, D, W, D, "4");
    }
    // ---------- ГАЛЕРЕЯ ПО ДВОРОВОМУ (СЕВЕРНОМУ) ФАСАДУ ---------------------
    // Настил, стойки до него и перила: ярус, с которого попадают в комнаты.
    {
        const float GZ = -1.5f;
        const ElementId deck = slab_xz(f, 0.0f, GZ, W, 0.05f, Y2, 0.14f, "1", "1", "0.42");
        (void)f.g.set_param(deck, "beams", "1");
        for (int k = 0; k <= 5; ++k) {
            const float x = 0.4f + (W - 0.8f) * static_cast<float>(k) / 5.0f;
            (void)f.beam(f.v(x, PL, GZ + 0.3f), f.v(x, Y2, GZ + 0.3f),
                         {{"radius", "0.12"}, {"mat", "0"}, {"tone", "2"},
                          {"wear", "0.5"}});
            // Стойка перил над настилом.
            (void)f.beam(f.v(x, Y2, GZ + 0.3f), f.v(x, Y2 + 1.05f, GZ + 0.3f),
                         {{"radius", "0.07"}, {"form", "square"}, {"mat", "1"},
                          {"tone", "1"}, {"wear", "0.45"}});
        }
        (void)f.beam(f.v(-0.1f, Y2 + 1.0f, GZ + 0.3f), f.v(W + 0.1f, Y2 + 1.0f, GZ + 0.3f),
                     {{"radius", "0.07"}, {"form", "plank"}, {"mat", "1"}, {"tone", "1"},
                      {"wear", "0.45"}});
        (void)f.beam(f.v(-0.1f, Y2 + 0.5f, GZ + 0.3f), f.v(W + 0.1f, Y2 + 0.5f, GZ + 0.3f),
                     {{"radius", "0.05"}, {"form", "plank"}, {"mat", "1"}, {"tone", "1"},
                      {"wear", "0.45"}});
        // Марш со двора на галерею.
        (void)f.contour({f.v(0.6f, 0.0f, GZ - 4.4f), f.v(2.2f, 0.0f, GZ - 4.4f),
                         f.v(2.2f, Y2, GZ + 0.1f), f.v(0.6f, Y2, GZ + 0.1f)},
                        {{"thickness", "0.12"}, {"fill", "6"}, {"open", "1"},
                         {"mat", "1"}, {"tone", "1"}, {"wear", "0.45"}});
        // Навес галереи — свес нижнего ската главной кровли сюда не достаёт.
        for (int k = 0; k <= 5; ++k) {
            const float x = 0.4f + (W - 0.8f) * static_cast<float>(k) / 5.0f;
            (void)f.beam(f.v(x, Y3 - 0.1f, GZ + 0.3f), f.v(x, Y3 + 0.1f, 0.05f),
                         {{"radius", "0.08"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.45"}});
        }
        const ElementId hood =
            f.contour({f.v(W, Y3 + 0.35f, 0.1f), f.v(0.0f, Y3 + 0.35f, 0.1f),
                       f.v(0.0f, Y3 - 0.05f, GZ + 0.2f), f.v(W, Y3 - 0.05f, GZ + 0.2f)},
                      {{"thickness", "0.14"}, {"mat", "4"}, {"tone", "0"},
                       {"fill", "8"}, {"wear", "0.4"}});
        (void)f.g.set_param(hood, "roof", "1");
        (void)f.g.set_param(hood, "unsupported", "1");
    }
    f.gable_roof_z(0.0f, 0.0f, W, D, EAVES, RIDGE_H, "4", "0", "8", "0.36");
    crow_gable(f, 0.0f, W, D + 0.32f, true, EAVES, RIDGE_H, 4, 0.54f, "1", "0.46");
    crow_gable(f, 0.0f, W, -0.32f, false, EAVES, RIDGE_H, 4, 0.54f, "1", "0.5");
    if (vents_on()) {
        f.chimney(3.0f, -0.32f, EAVES - 0.6f, EAVES + RIDGE_H + 1.6f, 0.4f, true, "4",
                  "2", "0.55");
        f.chimney(3.0f, D + 0.32f, EAVES - 0.6f, EAVES + RIDGE_H + 1.6f, 0.36f, true,
                  "4", "2", "0.55");
    }
    f.save(file);
}

// ---------------------------------------------------------------------------
// КИРПИЧНАЯ ГОЛУБЯТНЯ-БАШНЯ С ЛЕТКАМИ ЯРУСАМИ (§4.15: «усадебные дворы,
// четыре штуки»; подпись города №5).
//
// ЛЕТОК — ЭТО ПОЛОЧКА, А НЕ ДЫРКА. Отверстие 12 см конвейер не строит:
// проём в стене начинается с дверного и оконного габарита, всё мельче живёт
// текстурой, которой у нас нет. Но голубятню глаз узнаёт не по дыркам, а по
// ГОРИЗОНТАЛЬНЫМ ПРИСАДНЫМ ДОСКАМ через равные ярусы и по стае над ними —
// и доски строятся честно. Три яруса, шаг 1.6 м, вылет 0.28.
// ---------------------------------------------------------------------------
inline void forge_cornhall_dovecote(const char* file) {
    Forge f;
    const float S = 3.0f;
    const float TOP = 7.4f;
    // Ярусы по 2.5, чтобы окно (просит 2.10) влезало в верхний.
    {
        float y = 0.0f;
        const float TIER[3] = {2.5f, 2.5f, 2.4f};
        for (int t = 0; t < 3; ++t) {
            const auto q = [&](float ax, float az, float bx, float bz, bool win) {
                const ElementId id =
                    brick_wall(f, y, ax, az, bx, bz, TIER[t], 0.36f, "1", "0.5");
                if (win) {
                    (void)f.g.set_param(id, "windows", "1");
                }
                if (t == 0) {
                    (void)f.g.set_param(id, "plinth", "1");
                }
            };
            q(S, 0.0f, 0.0f, 0.0f, t == 2);
            q(S, S, S, 0.0f, t == 2);
            q(0.0f, 0.0f, 0.0f, S, t == 2);
            if (t == 0) {
                const ElementId d =
                    brick_wall(f, y, 0.0f, S, S, S, TIER[t], 0.36f, "1", "0.5");
                (void)f.g.set_param(d, "doors", "1");
                (void)f.g.set_param(d, "porch", "1");
            } else {
                q(0.0f, S, S, S, t == 2);
            }
            y += TIER[t];
        }
    }
    f.door_leaf(S * 0.5f, 0.0f, S);
    // ПРИСАДНЫЕ ДОСКИ: три яруса на южной и восточной гранях, с подкосами.
    for (int k = 0; k < 3; ++k) {
        const float y = 2.6f + 1.6f * static_cast<float>(k);
        (void)f.beam(f.v(-0.1f, y, S + 0.24f), f.v(S + 0.1f, y, S + 0.24f),
                     {{"radius", "0.08"}, {"form", "plank"}, {"mat", "1"}, {"tone", "1"},
                      {"wear", "0.5"}});
        (void)f.beam(f.v(S + 0.24f, y, -0.1f), f.v(S + 0.24f, y, S + 0.1f),
                     {{"radius", "0.08"}, {"form", "plank"}, {"mat", "1"}, {"tone", "1"},
                      {"wear", "0.5"}});
        for (const float x : {0.5f, S - 0.5f}) {
            (void)f.beam(f.v(x, y - 0.02f, S + 0.2f), f.v(x, y - 0.5f, S + 0.02f),
                         {{"radius", "0.05"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.5"}});
            (void)f.beam(f.v(S + 0.2f, y - 0.02f, x), f.v(S + 0.02f, y - 0.5f, x),
                         {{"radius", "0.05"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.5"}});
        }
    }
    hip_cap(f, 0.0f, 0.0f, S, S, TOP, 0.34f, 1.9f, "4", "0", "8", "0.4");
    // ФОНАРИК НА МАКУШКЕ: сюда голуби и влетают. Четыре стойки, кольцо и
    // маленький шатёр — тот же приём, что у шатра, но вчетверо мельче.
    {
        const float ly = TOP + 0.1f + 1.9f;
        const float r = 0.52f;
        const float cx = S * 0.5f;
        for (const auto& c : {std::pair<float, float>{cx - r, cx - r},
                              {cx + r, cx - r}, {cx + r, cx + r}, {cx - r, cx + r}}) {
            // НИЗ СТОЙКИ ПРОБИВАЕТ СКАТ, А НЕ САДИТСЯ НА ЕГО МАКУШКУ. Вершина
            // шатра — ТОЧКА, и стойка в 0.52 м от неё встаёт над поверхностью
            // ската на 0.19 м (судья намерил 0.046 по нормали и отказал).
            // Отметка ly-0.9 уводит пятку под скат при любом его подъёме.
            (void)f.beam(f.v(c.first, ly - 0.9f, c.second),
                         f.v(c.first, ly + 0.95f, c.second),
                         {{"radius", "0.07"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.45"}});
        }
        hip_cap(f, cx - r, cx - r, cx + r, cx + r, ly + 0.95f, 0.24f, 0.7f, "4", "0",
                "8", "0.4");
    }
    f.save(file);
}

// ---------------------------------------------------------------------------
// ПОЖАРНАЯ КАЛАНЧА С КОЛОКОЛОМ (§4.17: «при ратуше»).
//
// КАЛАНЧА — ЭТО НЕ БАШНЯ, А ОБХОД НАД САРАЕМ. Внизу депо с широкими воротами
// (труба, багры, бочка водовоза), наверху ОТКРЫТАЯ площадка кругом, откуда
// смотрят, и колокол, которым будят. Открытая площадка и есть признак:
// закрытый ярус читался бы голубятней, а звонница без обхода — храмом.
//
// ВЫСОТА 17.2 ВЗЯТА ПО РАБОТЕ, А НЕ ПО КРАСОТЕ: смотреть надо ПОВЕРХ конька
// рыночного ряда (11.4) и поверх щипцов главной улицы (12.4), иначе с обхода
// видно соседний двор, а не квартал за ним.
// ---------------------------------------------------------------------------
inline void forge_cornhall_firetower(const char* file) {
    Forge f;
    const float BW = 6.4f;      // депо в плане
    const float BD = 5.0f;
    const float BH = 3.9f;      // ворота просят высоту
    const float TX0 = 1.7f;     // ствол внутри депо
    const float TX1 = 4.7f;
    const float TZ0 = 1.0f;
    const float TZ1 = 4.0f;
    const float T_TOP = 12.4f;  // верх ствола = пол обхода
    const float B_TOP = 15.6f;  // верх звонного яруса

    // ---------- депо -------------------------------------------------------
    // ЦОКОЛЬ РАЗОРВАН ПОД ВОРОТАМИ ДЕПО: из каланчи выезжает бочка водовоза,
    // и лента 0.45 м поперёк проёма 3.4 м — то же самое, что запертые ворота
    // (та же находка, что у проездной арки постоялого двора).
    (void)brick_wall(f, 0.0f, BW, 0.0f, 0.0f, 0.0f, 0.45f, 0.44f, "2", "0.6");
    (void)brick_wall(f, 0.0f, BW, BD, BW, 0.0f, 0.45f, 0.44f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, BD, 0.45f, 0.44f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, BD, 1.5f, BD, 0.45f, 0.44f, "2", "0.6");
    (void)brick_wall(f, 0.0f, BW - 1.5f, BD, BW, BD, 0.45f, 0.44f, "2", "0.6");
    parquet_floor(f, 0.0f, 0.0f, BW, BD, 0.09f);
    {
        const ElementId n = brick_wall(f, 0.0f, BW, 0.0f, 0.0f, 0.0f, BH, 0.4f, "1", "0.5");
        (void)f.g.set_param(n, "windows", "2");
        const ElementId e = brick_wall(f, 0.0f, BW, BD, BW, 0.0f, BH, 0.4f, "1", "0.5");
        (void)f.g.set_param(e, "windows", "2");
        const ElementId w = brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, BD, BH, 0.4f, "1", "0.5");
        (void)f.g.set_param(w, "windows", "2");
        // Южный фасад — ВОРОТА 3.4 м, набранные простенками и перемычкой:
        // раскладка знает только створку 1.0 м, и широкий проём собирается
        // стенами (тот же приём, что у амбара Вайтрана).
        (void)brick_wall(f, 0.0f, 0.0f, BD, 1.5f, BD, BH, 0.4f, "1", "0.5");
        (void)brick_wall(f, 0.0f, BW - 1.5f, BD, BW, BD, BH, 0.4f, "1", "0.5");
        (void)brick_wall(f, 3.3f, 1.5f, BD, BW - 1.5f, BD, BH - 3.3f, 0.4f, "1", "0.5");
    }
    {
        const ElementId fl = slab_xz(f, 0.0f, 0.0f, BW, BD, BH, 0.16f, "1", "1", "0.42");
        (void)f.g.set_param(fl, "beams", "1");
    }
    // Односкатный колпак депо вокруг ствола — иначе ствол растёт из плиты.
    {
        const ElementId r =
            f.contour({f.v(BW + 0.4f, BH + 0.9f, -0.4f), f.v(-0.4f, BH + 0.9f, -0.4f),
                       f.v(-0.4f, BH + 0.2f, BD + 0.6f), f.v(BW + 0.4f, BH + 0.2f, BD + 0.6f)},
                      {{"thickness", "0.15"}, {"mat", "4"}, {"tone", "0"},
                       {"fill", "8"}, {"wear", "0.4"}});
        (void)f.g.set_param(r, "roof", "1");
        (void)f.g.set_param(r, "unsupported", "1");
    }

    // ---------- ствол от земли сквозь депо ---------------------------------
    {
        float y = 0.0f;
        const float TIER[4] = {3.2f, 3.1f, 3.1f, 3.0f};
        for (int t = 0; t < 4; ++t) {
            const auto q = [&](float ax, float az, float bx, float bz) {
                const ElementId id =
                    brick_wall(f, y, ax, az, bx, bz, TIER[t], 0.45f, "1", "0.46");
                if (t > 0) {
                    (void)f.g.set_param(id, "windows", "1");
                }
            };
            q(TX1, TZ0, TX0, TZ0);
            q(TX1, TZ1, TX1, TZ0);
            q(TX0, TZ0, TX0, TZ1);
            q(TX0, TZ1, TX1, TZ1);
            y += TIER[t];
        }
    }
    // ---------- ОБХОД: плита с напуском, стойки и перила --------------------
    for (const float bx : {TX0 + 0.5f, (TX0 + TX1) * 0.5f, TX1 - 0.5f}) {
        for (const float bz : {TZ0, TZ1}) {
            const float dz = bz < (TZ0 + TZ1) * 0.5f ? -0.62f : 0.62f;
            (void)f.beam(f.v(bx, T_TOP - 1.3f, bz), f.v(bx, T_TOP - 0.14f, bz + dz),
                         {{"radius", "0.1"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.5"}});
        }
    }
    {
        const ElementId deck = slab_xz(f, TX0 - 0.65f, TZ0 - 0.65f, TX1 + 0.65f,
                                       TZ1 + 0.65f, T_TOP, 0.18f, "1", "1", "0.45");
        (void)f.g.set_param(deck, "unsupported", "1");
    }
    {
        const float px[2] = {TX0 - 0.6f, TX1 + 0.6f};
        const float pz[2] = {TZ0 - 0.6f, TZ1 + 0.6f};
        for (const float x : px) {
            for (const float z : pz) {
                (void)f.beam(f.v(x, T_TOP, z), f.v(x, T_TOP + 1.1f, z),
                             {{"radius", "0.08"}, {"form", "square"}, {"mat", "0"},
                              {"tone", "2"}, {"wear", "0.45"}});
            }
        }
        for (const float z : pz) {
            (void)f.beam(f.v(px[0], T_TOP + 1.05f, z), f.v(px[1], T_TOP + 1.05f, z),
                         {{"radius", "0.06"}, {"form", "plank"}, {"mat", "1"},
                          {"tone", "1"}, {"wear", "0.45"}});
        }
        for (const float x : px) {
            (void)f.beam(f.v(x, T_TOP + 1.05f, pz[0]), f.v(x, T_TOP + 1.05f, pz[1]),
                         {{"radius", "0.06"}, {"form", "plank"}, {"mat", "1"},
                          {"tone", "1"}, {"wear", "0.45"}});
        }
    }
    // ---------- ЗВОННЫЙ ЯРУС: четыре стойки, арочки, колокол ---------------
    {
        const float px[2] = {TX0, TX1};
        const float pz[2] = {TZ0, TZ1};
        for (const float x : px) {
            for (const float z : pz) {
                (void)f.beam(f.v(x, T_TOP, z), f.v(x, B_TOP, z),
                             {{"radius", "0.19"}, {"form", "square"}, {"mat", "0"},
                              {"tone", "2"}, {"wear", "0.45"}});
            }
        }
        for (const float z : pz) {
            brick_arch(f, (TX0 + TX1) * 0.5f, z, TX1 - TX0 - 0.4f, B_TOP - 2.0f, 0.14f,
                       "1", "0.45");
            (void)f.beam(f.v(px[0], B_TOP - 0.12f, z), f.v(px[1], B_TOP - 0.12f, z),
                         {{"radius", "0.13"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.45"}});
        }
        for (const float x : px) {
            (void)f.beam(f.v(x, B_TOP - 0.12f, pz[0]), f.v(x, B_TOP - 0.12f, pz[1]),
                         {{"radius", "0.13"}, {"form", "square"}, {"mat", "0"},
                          {"tone", "2"}, {"wear", "0.45"}});
        }
        // КОЛОКОЛ на ярме: десятигранное тело под перекладиной.
        const float cx = (TX0 + TX1) * 0.5f;
        const float cz = (TZ0 + TZ1) * 0.5f;
        // СРЕДНЯЯ ПЕРЕКЛАДИНА — ТА, НА КОТОРОЙ КОЛОКОЛ И ВИСИТ. Обвязка по
        // периметру проходит по КРАЯМ яруса, через середину не идёт ни одна
        // балка, и колокол с ярмом висели островом (судья: зазор 0.060 м).
        (void)f.beam(f.v(TX0, B_TOP - 0.12f, cz), f.v(TX1, B_TOP - 0.12f, cz),
                     {{"radius", "0.13"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.45"}});
        (void)f.beam(f.v(cx, B_TOP - 0.55f, cz), f.v(cx, B_TOP - 0.16f, cz),
                     {{"radius", "0.11"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.45"}});
        (void)f.beam(f.v(cx, B_TOP - 1.5f, cz), f.v(cx, B_TOP - 0.5f, cz),
                     {{"radius", "0.44"}, {"sides", "10"}, {"mat", "3"}, {"tone", "2"},
                      {"wear", "0.35"}});
    }
    hip_cap(f, TX0, TZ0, TX1, TZ1, B_TOP, 0.5f, 2.4f, "4", "0", "8", "0.38");
    // Марш с земли на первый ярус ствола — иначе на каланчу не подняться.
    (void)f.contour({f.v(BW + 0.5f, 0.0f, 1.2f), f.v(BW + 0.5f, 0.0f, 2.8f),
                     f.v(BW - 0.9f, BH, 2.8f), f.v(BW - 0.9f, BH, 1.2f)},
                    {{"thickness", "0.12"}, {"fill", "6"}, {"open", "1"}, {"mat", "1"},
                     {"tone", "1"}, {"wear", "0.45"}});
    f.save(file);
}

// ---------------------------------------------------------------------------
// ЗЕРНОВОЙ АМБАР НА КАМЕННЫХ ГРИБАХ-ОПОРАХ (§4.5: «задворки купеческих усадеб
// и предместья»).
//
// ГРИБ РАБОТАЕТ ШЛЯПКОЙ, И ПОТОМУ ОН ГРИБ. Каменная ножка поднимает пол над
// землёй, а широкая плоская шляпка не даёт крысе перелезть — форма целиком
// функциональна, и в силуэте она читается сразу. Восемь опор: шаг 3.0 по
// длине и 2.6 по ширине держит балку сечением, которое строится.
//
// СОЛОМА И ТЁС — ПРЕДМЕСТЬЕ. Тот же дом в центре был бы кирпичным под
// черепицей; амбар стоит за красной линией, где пожарный регламент кончается
// (§3 паспорта: «черепица в центре, солома у стены и в предместьях»).
// ---------------------------------------------------------------------------
inline void forge_cornhall_granary(const char* file) {
    Forge f;
    const float W = 7.2f;
    const float D = 5.6f;
    const float LEG = 0.92f;      // низ пола над землёй
    const float FLOOR = LEG + 0.1f;
    const float H = 2.9f;
    const float EAVES = FLOOR + H;
    const float RIDGE_H = W * 0.5f * CORNHALL_PITCH;

    // ---------- восемь грибов ----------------------------------------------
    for (int i = 0; i < 4; ++i) {
        for (int k = 0; k < 2; ++k) {
            const float x = 0.6f + (W - 1.2f) * static_cast<float>(i) / 3.0f;
            const float z = 0.6f + (D - 1.2f) * static_cast<float>(k);
            (void)f.beam(f.v(x, 0.0f, z), f.v(x, LEG - 0.26f, z),
                         {{"radius", "0.24"}, {"sides", "8"}, {"mat", "3"},
                          {"tone", "1"}, {"wear", "0.55"}});
            (void)f.beam(f.v(x, LEG - 0.3f, z), f.v(x, LEG + 0.02f, z),
                         {{"radius", "0.46"}, {"sides", "8"}, {"mat", "3"},
                          {"tone", "1"}, {"wear", "0.55"}});
        }
    }
    // Лежни по грибам и настил.
    for (int k = 0; k < 2; ++k) {
        const float z = 0.6f + (D - 1.2f) * static_cast<float>(k);
        (void)f.beam(f.v(0.2f, LEG + 0.1f, z), f.v(W - 0.2f, LEG + 0.1f, z),
                     {{"radius", "0.16"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.5"}});
    }
    {
        const ElementId fl = slab_xz(f, 0.0f, 0.0f, W, D, FLOOR, 0.16f, "1", "1", "0.5");
        (void)f.g.set_param(fl, "beams", "1");
        (void)f.g.set_param(fl, "unsupported", "1");
    }
    // ---------- обшитое тесом тело -----------------------------------------
    {
        const auto q = [&](float ax, float az, float bx, float bz, bool door) {
            const ElementId id =
                f.wall(f.v(ax, FLOOR, az), f.v(bx, FLOOR, bz),
                       {{"height", f.num(H)}, {"thickness", "0.24"}, {"mat", "1"},
                        {"tone", "1"}, {"clad", "1"}, {"wear", "0.55"}});
            if (door) {
                (void)f.g.set_param(id, "doors", "1");
                (void)f.g.set_param(id, "porch", "1");
            }
        };
        q(W, 0.0f, 0.0f, 0.0f, false);
        q(W, D, W, 0.0f, false);
        q(0.0f, 0.0f, 0.0f, D, false);
        q(0.0f, D, W, D, true);
    }
    f.door_leaf(W * 0.5f, FLOOR, D);
    f.gable_roof_z(0.0f, 0.0f, W, D, EAVES, RIDGE_H, "6", "1", "", "0.5");
    // Приставной марш к двери: он ТРОГАЕТ порог, и это вынужденно — судья
    // связности не держит острова, а настоящий амбарный трап отставляли от
    // стены именно затем, чтобы крыса не перешла. Компромисс назван вслух.
    (void)f.contour({f.v(W * 0.5f - 0.7f, 0.0f, D + 2.0f),
                     f.v(W * 0.5f + 0.7f, 0.0f, D + 2.0f),
                     f.v(W * 0.5f + 0.7f, FLOOR, D + 0.05f),
                     f.v(W * 0.5f - 0.7f, FLOOR, D + 0.05f)},
                    {{"thickness", "0.1"}, {"fill", "6"}, {"open", "1"}, {"mat", "1"},
                     {"tone", "1"}, {"wear", "0.55"}});
    f.save(file);
}

// ---------------------------------------------------------------------------
// ГОНЧАРНЫЙ ГОРН КУПОЛОМ (§4.18: «у ворот — огненное ремесло, снаружи; рядом
// поле битой посуды»).
//
// ЕДИНСТВЕННЫЙ КУПОЛ ГОРОДА. Граница конвейера прямо ограничивает купол
// доминантой в 1-3 штуки на город при бюджете 200-600 трис; здесь купол
// стоит на самом МЕЛКОМ рецепте пакета, и это не парадокс: горн у ворот
// виден с тракта раньше стен, и он единственный, кому изогнутый силуэт нужен
// по делу (свод печи и в жизни купольный, потому что иначе он не держит жар).
//
// ОГОНЬ ЗДЕСЬ «ГОРИТ», А НЕ «СВЕТИТ» (правило О паспорта: у гончарных горнов
// 3 светят, 4 горят). Топочная арка — вход тому и другому; светящуюся
// геометрию в неё поставит расстановка, кузнице тут делать нечего.
// ---------------------------------------------------------------------------
inline void forge_cornhall_kiln(const char* file) {
    Forge f;
    const float S = 3.9f;         // топочный куб в плане
    const float DRUM = 2.2f;      // верх барабана = пята купола
    const float CX = S * 0.5f;
    // Барабан: четыре стены пережжённого кирпича на подошве.
    (void)brick_wall(f, 0.0f, S, 0.0f, 0.0f, 0.0f, DRUM, 0.5f, "2", "0.7");
    (void)brick_wall(f, 0.0f, S, S, S, 0.0f, DRUM, 0.5f, "2", "0.7");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, S, DRUM, 0.5f, "2", "0.7");
    // Южная грань: две щеки и перемычка — топочный проём 1.3 м.
    (void)brick_wall(f, 0.0f, 0.0f, S, 1.3f, S, DRUM, 0.5f, "2", "0.7");
    (void)brick_wall(f, 0.0f, S - 1.3f, S, S, S, DRUM, 0.5f, "2", "0.7");
    (void)brick_wall(f, 1.55f, 1.3f, S, S - 1.3f, S, DRUM - 1.55f, 0.5f, "2", "0.7");
    brick_arch(f, CX, S + 0.18f, 1.3f, 0.9f, 0.2f, "2", "0.7");
    // Подошва — рабочая площадка вокруг горна.
    (void)slab_xz(f, -1.2f, -1.2f, S + 1.2f, S + 1.2f, 0.05f, 0.16f, "4", "1", "0.68");
    // КУПОЛ: восьмигранник радиуса 2.0 — чуть шире барабана, свес читается
    // напуском кладки, как у настоящего свода.
    brick_dome(f, CX, S * 0.5f, DRUM, 2.0f, 2.5f, "2", "0.72");
    // Устье наверху: короткий кирпичный воротник.
    (void)f.beam(f.v(CX, DRUM + 2.4f, S * 0.5f), f.v(CX, DRUM + 3.1f, S * 0.5f),
                 {{"radius", "0.36"}, {"sides", "8"}, {"mat", "4"}, {"tone", "2"},
                  {"wear", "0.75"}});
    // Обвязка стяжными обручами: два пояса брусьев вокруг барабана — свод
    // распирает стены, и обруч это то, чем гончар с этим борется.
    for (const float y : {0.9f, 1.8f}) {
        (void)f.beam(f.v(-0.12f, y, -0.12f), f.v(S + 0.12f, y, -0.12f),
                     {{"radius", "0.07"}, {"form", "square"}, {"mat", "3"},
                      {"tone", "2"}, {"wear", "0.6"}});
        (void)f.beam(f.v(S + 0.12f, y, -0.12f), f.v(S + 0.12f, y, S + 0.12f),
                     {{"radius", "0.07"}, {"form", "square"}, {"mat", "3"},
                      {"tone", "2"}, {"wear", "0.6"}});
        (void)f.beam(f.v(S + 0.12f, y, S + 0.12f), f.v(-0.12f, y, S + 0.12f),
                     {{"radius", "0.07"}, {"form", "square"}, {"mat", "3"},
                      {"tone", "2"}, {"wear", "0.6"}});
        (void)f.beam(f.v(-0.12f, y, S + 0.12f), f.v(-0.12f, y, -0.12f),
                     {{"radius", "0.07"}, {"form", "square"}, {"mat", "3"},
                      {"tone", "2"}, {"wear", "0.6"}});
    }
    f.save(file);
}

// ---------------------------------------------------------------------------
// ВЕСОВАЯ-МЕРНАЯ ПАЛАТА С ЭТАЛОНАМИ НА ФАСАДЕ (§4.7: «торец площади»).
//
// ГОРОДСКАЯ МЕРА ВЫВЕШЕНА НА СТЕНЕ, И ЭТО НЕ ДЕКОР, А ЮРИДИЧЕСКИЙ ДОКУМЕНТ:
// прут длиной в локоть города, вделанный в кладку, — то, чем спор о мере
// разрешался на месте. Четыре прута разной длины на южном фасаде читаются
// именно так и стоят три бруса.
//
// Палата — самый маленький рецепт со ступенчатым щипцом: тем и доказывается,
// что признак школы держится на любом размере, а не только на богатом доме.
// ---------------------------------------------------------------------------
inline void forge_cornhall_weighhouse(const char* file) {
    Forge f;
    const float W = 8.0f;
    const float D = 6.5f;
    const float PIER = 0.6f;
    const float BAY = 2.5f;
    const float H1 = 4.6f;
    const float RIDGE_H = W * 0.5f * CORNHALL_PITCH;
    const float PORCH = 2.2f;   // глубина открытого приёмного крыльца
    const float DP = D + PORCH;

    (void)brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, 0.5f, 0.46f, "2", "0.6");
    (void)brick_wall(f, 0.0f, W, D, W, 0.0f, 0.5f, 0.46f, "2", "0.6");
    (void)brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, 0.5f, 0.46f, "2", "0.6");
    parquet_floor(f, 0.0f, 0.0f, W, D, 0.1f);
    {
        const ElementId n = brick_wall(f, 0.0f, W, 0.0f, 0.0f, 0.0f, H1, 0.44f, "1", "0.45");
        (void)f.g.set_param(n, "windows", "3");
        const ElementId e = brick_wall(f, 0.0f, W, D, W, 0.0f, H1, 0.44f, "1", "0.45");
        (void)f.g.set_param(e, "windows", "2");
        const ElementId w = brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, D, H1, 0.44f, "1", "0.45");
        (void)f.g.set_param(w, "windows", "2");
        // Южный фасад глухой с окнами: он весь уходит под приёмное крыльцо.
        const ElementId s1 = brick_wall(f, 0.0f, 0.0f, D, W, D, H1, 0.44f, "1", "0.45");
        (void)f.g.set_param(s1, "windows", "3");
    }
    // ДВЕРЬ КОНТОРЫ — В ЗАПАДНОЙ СТЕНЕ, И ОСЬ ДВЕРИ ТЕПЕРЬ ЧЕСТНАЯ. Дверь,
    // втиснутая в угол южного фасада, и стояла плохо (её закрывал столб
    // крыльца), и врала посадке: манифест выводит ось из смещения дверной
    // точки от центра g-габарита, и точка (1.0, 6.5) при пятне 9.1 x 9.4
    // читалась как «дверь на запад» — то есть верно по букве и неверно по
    // делу. Здесь дверь ДЕЙСТВИТЕЛЬНО западная, а весь южный торец отдан
    // приёмному крыльцу, как и положено палате на торце площади.
    {
        const ElementId a = brick_wall(f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, H1, 0.44f, "1",
                                       "0.45");
        (void)f.g.set_param(a, "windows", "1");
        const ElementId m = brick_wall(f, 0.0f, 0.0f, 2.0f, 0.0f, 4.5f, H1, 0.44f, "1",
                                       "0.45");
        (void)f.g.set_param(m, "doors", "1");
        (void)f.g.set_param(m, "porch", "1");
        const ElementId c = brick_wall(f, 0.0f, 0.0f, 4.5f, 0.0f, D, H1, 0.44f, "1",
                                       "0.45");
        (void)f.g.set_param(c, "windows", "1");
    }
    f.door_leaf_x(0.0f, 0.0f, 3.25f);
    // ПРИЁМНОЕ КРЫЛЬЦО: три столба и две арки — сюда заводят телегу под весы.
    // НАЧАЛО ВЫВОДИТСЯ ИЗ ДЛИНЫ (та же находка, что у лоджии ратуши): два
    // пролёта по 2.5 и три столба по 0.6 — 6.8 м, и назначенное начало 1.4
    // выносило последний столб на 8.2 при палате шириной 8.0.
    const float POR_LEN = 2.0f * BAY + 3.0f * PIER;
    const float POR_X0 = (W - POR_LEN) * 0.5f;
    for (int k = 0; k <= 2; ++k) {
        const float x0 = POR_X0 + static_cast<float>(k) * (BAY + PIER);
        (void)brick_wall(f, 0.0f, x0, DP - PIER * 0.5f, x0 + PIER, DP - PIER * 0.5f,
                         4.0f, PIER, "1", "0.48");
    }
    for (int k = 0; k < 2; ++k) {
        const float cx = POR_X0 + PIER + static_cast<float>(k) * (BAY + PIER)
                       + BAY * 0.5f;
        brick_arch(f, cx, DP - PIER * 0.5f, BAY, 2.5f, 0.22f, "1", "0.45");
    }
    (void)brick_wall(f, 3.85f, POR_X0, DP - PIER * 0.5f, POR_X0 + POR_LEN,
                     DP - PIER * 0.5f, 0.55f, 0.46f, "1", "0.45");
    (void)slab_xz(f, POR_X0, D, POR_X0 + POR_LEN, DP, 0.06f, 0.14f, "3", "1", "0.55");
    // Кровля крыльца — односкат от главной стены к поясу арок.
    {
        const ElementId r =
            f.contour({f.v(POR_X0 + POR_LEN, 5.0f, D - 0.2f),
                       f.v(POR_X0 - 0.2f, 5.0f, D - 0.2f),
                       f.v(POR_X0 - 0.2f, 4.35f, DP + 0.3f),
                       f.v(POR_X0 + POR_LEN, 4.35f, DP + 0.3f)},
                      {{"thickness", "0.14"}, {"mat", "4"}, {"tone", "0"},
                       {"fill", "8"}, {"wear", "0.4"}});
        (void)f.g.set_param(r, "roof", "1");
        (void)f.g.set_param(r, "unsupported", "1");
    }
    // КОРОМЫСЛО ВЕСОВ под крыльцом: стойка, коромысло и две подвески.
    {
        const float cx = POR_X0 + PIER + BAY + PIER * 0.5f;
        const float cz = DP - PIER * 0.5f - 0.9f;
        (void)f.beam(f.v(cx, 0.13f, cz), f.v(cx, 2.9f, cz),
                     {{"radius", "0.11"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.45"}});
        (void)f.beam(f.v(cx - 1.5f, 2.85f, cz), f.v(cx + 1.5f, 2.85f, cz),
                     {{"radius", "0.08"}, {"form", "square"}, {"mat", "0"},
                      {"tone", "2"}, {"wear", "0.45"}});
        for (const float dx : {-1.4f, 1.4f}) {
            (void)f.beam(f.v(cx + dx, 1.65f, cz), f.v(cx + dx, 2.83f, cz),
                         {{"radius", "0.04"}, {"mat", "3"}, {"tone", "2"},
                          {"wear", "0.4"}});
            (void)f.beam(f.v(cx + dx, 1.5f, cz), f.v(cx + dx, 1.68f, cz),
                         {{"radius", "0.44"}, {"sides", "8"}, {"mat", "3"},
                          {"tone", "1"}, {"wear", "0.45"}});
        }
    }
    // ЭТАЛОНЫ НА ФАСАДЕ: четыре прута разной длины, вделанные в кладку.
    for (int k = 0; k < 4; ++k) {
        const float len = 0.55f + 0.35f * static_cast<float>(k);
        const float y = 1.5f + 0.55f * static_cast<float>(k);
        (void)f.beam(f.v(W - 1.6f - len, y, D + 0.24f), f.v(W - 1.6f, y, D + 0.24f),
                     {{"radius", "0.055"}, {"form", "square"}, {"mat", "3"},
                      {"tone", "2"}, {"wear", "0.4"}});
    }
    f.gable_roof_z(0.0f, 0.0f, W, D, H1, RIDGE_H, "4", "0", "8", "0.36");
    crow_gable(f, 0.0f, W, D + 0.32f, true, H1, RIDGE_H, 3, 0.52f, "1", "0.45");
    crow_gable(f, 0.0f, W, -0.32f, false, H1, RIDGE_H, 3, 0.52f, "1", "0.48");
    f.save(file);
}

// ---------------------------------------------------------------------------
// СЕКЦИЯ РЕЕСТРА
//
// ИМЕНА С ПРЕФИКСОМ ГОРОДА — ЭТО И ЕСТЬ ВЕСЬ ПРОТОКОЛ СОСЕДСТВА (recipe_book.h):
// файл выводится из имени, семейство манифеста — префикс до дефиса, а
// --only 'cornhall-*' печёт ровно этот пакет и НЕ ТРОГАЕТ ни одного чужого
// файла на полке. Общий city-* здесь не упоминается ни разу.
// ---------------------------------------------------------------------------
inline void pack_cornhall(std::vector<Recipe>& b) {
    // ЖИЛЬЁ: одно тело, четыре выпечки. Ухоженный и запущенный близнец —
    // тот же довод, что у пакета Вайтрана: улица из одинаково потрёпанных
    // домов читается одной текстурой, и разброс износа между соседями и есть
    // то, из чего глаз собирает возраст города. Полоса ветхого 0.65..0.8
    // взята оттуда же — порог 0.7 уводит деталь в ВЫВЕТРЕННЫЙ ряд атласа.
    struct Dwelling {
        const char* name;
        float w;
        float d;
        int floors;
        int steps;   // силуэт читает 2*steps+1 ступень
        bool thatch;
        bool hoist;
        bool aged;
    };
    for (const Dwelling& d : {Dwelling{"cornhall-house-s", 6.5f, 8.0f, 2, 3, false, false, true},
                              Dwelling{"cornhall-house-l", 8.0f, 10.0f, 3, 5, false, true, true},
                              Dwelling{"cornhall-house-s-thatch", 6.5f, 8.0f, 2, 3, true, false, false}}) {
        const Dwelling v = d;
        add_recipe(b, "cornhall", d.name, [v](const char* f) {
            Aging a;
            a.file = f;
            cornhall_dwelling(std::move(a), v.w, v.d, v.floors, v.steps, v.thatch,
                              v.hoist);
        });
        if (!d.aged) {
            continue;
        }
        add_recipe(b, "cornhall", std::string(d.name) + "-old", [v](const char* f) {
            Aging a;
            a.file = f;
            a.shift = 0.28f;
            a.lo = 0.65f;
            a.hi = 0.8f;
            a.kept = false;
            cornhall_dwelling(std::move(a), v.w, v.d, v.floors, v.steps, v.thatch,
                              v.hoist);
        });
    }
    add_recipe(b, "cornhall", "cornhall-house-jetty",
               [](const char* f) { forge_cornhall_jetty(f); });
    add_recipe(b, "cornhall", "cornhall-arcade",
               [](const char* f) { forge_cornhall_arcade(f); });
    add_recipe(b, "cornhall", "cornhall-townhall",
               [](const char* f) { forge_cornhall_townhall(f); });
    add_recipe(b, "cornhall", "cornhall-windmill",
               [](const char* f) { forge_cornhall_windmill(f); });
    add_recipe(b, "cornhall", "cornhall-brewery",
               [](const char* f) { forge_cornhall_brewery(f); });
    add_recipe(b, "cornhall", "cornhall-inn",
               [](const char* f) { forge_cornhall_inn(f); });
    add_recipe(b, "cornhall", "cornhall-dovecote",
               [](const char* f) { forge_cornhall_dovecote(f); });
    add_recipe(b, "cornhall", "cornhall-firetower",
               [](const char* f) { forge_cornhall_firetower(f); });
    add_recipe(b, "cornhall", "cornhall-granary",
               [](const char* f) { forge_cornhall_granary(f); });
    add_recipe(b, "cornhall", "cornhall-kiln",
               [](const char* f) { forge_cornhall_kiln(f); });
    add_recipe(b, "cornhall", "cornhall-weighhouse",
               [](const char* f) { forge_cornhall_weighhouse(f); });
}
