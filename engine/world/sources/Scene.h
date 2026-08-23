/*
Created: 15:08:2026 - 16:24:04
Last updated: 23:08:2026 - 22:10:00
Module: engine/world
File: engine/world/sources/Scene.h

Responsibility:
- THE COMPOSITION FILE (.scene) and the RULES that judge it: what stands where
  on a map, as data both a human and an agent edit, plus the machine-checkable
  invariants a composed world must satisfy. The fourth tool of the pivot,
  after the map browser, the world baker and the object registry.

Key items:
- Placement / SceneDoc: one object of the registry, placed.
- read_scene / write_scene: the text format (in git, diffable, mergeable).
- SceneRule / check_scene(): the rules, and the report they produce.

Dependencies:
- Uses: engine/core/math, std. NOT engine/render: a scene is data about WHAT
  stands where, and it must be checkable in a tool with no window.
- Used by: tools/check_scene.cpp, the app's gallery/composition loading, the
  editor's placement UI (later), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- THE RULES EXIST BECAUSE MEMORY DOES NOT. The user's own words when he asked
  for this tool: «это поможет избегать ошибок по типу висящих в воздухе
  тропинок». Every rule here was bought by a real defect in this repository —
  an object hovering over ground it was placed on, a tree planted past the map
  edge, exhibits standing inside each other. A rule with no defect behind it
  does not belong in this file; a defect met twice does.
- A CHECK REPORTS, IT DOES NOT REPAIR. Silent repair is how a broken scene
  becomes a scene nobody knows is broken: the report names the placement, the
  rule and the number, and the caller decides. `fix_scene` exists separately
  and is explicit.
- The reader is TOTAL: an unknown key is skipped, not fatal (the format will
  grow), but a MALFORMED number is an error with a line, because "0 by
  accident" is the failure mode this project keeps paying for.
*/
/*
UPD:
- 15:08:2026 - 16:24:04: Создан по заданию пользователя: «надо сделать
  приложение, где агенты смогут объекты и пространство оформлять по внутренним
  правилам... чтобы инструментом могли как агенты, так и человек пользоваться».
- 16:08:2026 - 21:08:52: ГРУППЫ И ОПОРА — по заданию пользователя про инструмент, где агенты
  СОБИРАЮТ ДОМА из готовых деталей. Placement::group («farmhouse») меняет два
  правила: члены одной постройки могут пересекаться (это стык, а не дефект) и
  могут стоять ДРУГ НА ДРУГЕ, а не только на земле — без второго дома нельзя
  проверить в принципе: каждая балка выше подошвы читалась бы как висящая, и
  отчёт стал бы шумом. SceneWorld дорос двумя необязательными крюками (правило
  26, только добавления): object_top — насколько деталь возвышается над своим
  началом (то, на что встаёт следующая), object_box — СЛЕД детали как
  прямоугольник. Второй куплен ошибкой: начало строительной детали лежит у её
  КРАЯ, поэтому круг радиуса от начала у фронтона 4 м даёт 4.5 м вокруг угла, и
  забор «стоял внутри» дома через три метра пустой травы. И асимметричный
  допуск bury_tolerance_m: постройка ВРЕЗАЕТСЯ в склон подошвой (так кладут
  камень), а висит в воздухе — всегда ошибка; одинокому дереву поблажки нет.
- 16:08:2026 - 22:40:23: split_shelves() — разбор списка полок реестра. ОДНО определение: три
  инструмента уже разбирали его своей копией, а список полок, означающий в игре
  и в её судье разное, — это судья другого мира (правило 35).
- 16:08:2026 - 22:45:34: check_panel_solid/SolidReport — прибор сплошной ПАНЕЛИ (сборки
  пользователя: «из мелких деталей собирать большие, чтобы меньше дырок»).
  Голая геометрия на входе — заголовок остаётся без engine/render; одна
  функция, двое зовущих: судья (--solid) и пекарь сборок (--require-solid).
- 17:08:2026 - 03:09:30: СПАВН В КОМПОЗИЦИИ (spawn / spawn_yaw, необязательные). Запрос зоны
  flora: пользователь перенёс точку входа на полянку в середину каменной тропы
  лицом к дубу. Это принадлежит КОМПОЗИЦИИ, а не стенду: «встань здесь и смотри
  туда» — утверждение о том, что ПОСТРОЕНО, а стенд знает только середину
  своего чанка. Тем же ключом потом встанет спавн у двери внутри дома.
- 17:08:2026 - 10:53:33: СЕКЦИЯ [light] — лампы композиции (позиция пламени, цвет, радиус,
  просьба о тени, заметка). Свет НЕ объект: объект это то, во что можно
  упереться, а лампа нет; и один столб горит ночью и не горит днём, поэтому
  композитор обязан двигать пламя отдельно от столба. Ламп может быть сколько
  угодно — файл говорит, что СУЩЕСТВУЕТ, рендер решает, что ГОРИТ.
- 17:08:2026 - 11:35:28: СЕКЦИЯ [pad] — та самая правка карты высот, которую просил пользователь
  («редактировать масштаб и карту высот»). Площадка это УТВЕРЖДЕНИЕ, а не мазок
  кистью: «здесь земля такой высоты, растушёвка столько метров», — поэтому её
  можно двигать, перечитывать и судить, чего нарисованное поле высот не умеет.
- 17:08:2026 - 12:33:08: ПРАВИЛА РАЗМЕЩЕНИЯ по заданию пользователя («запретим ставить деревья
  на любые тропы... как например нельзя дерево в доме ставить, дом поверх
  дерева ставить»): OffPath и OutsideBuildings. Второе — ОДНО правило с двух
  концов: дерево в доме и дом поверх дерева это одно и то же пересечение,
  увиденное с разных сторон, и два правила разошлись бы в первом же спорном
  случае. Плюс два крюка, которые делают их не шумными: object_solid (препятствие
  — это твёрдая геометрия ВЫШЕ ШАГА игрока; трава и цветы им не являются) и
  object_box_solid (пересечение меряется по СТВОЛАМ, а не по кронам: две берёзы
  в двух метрах — это лес, а не дефект).
- 17:08:2026 - 12:49:26: ПРАВИЛА СОЕДИНИТЕЛЕЙ (зона домов, HOUSES.md §5; правка чужого
  файла — исключение правила 26, ТОЛЬКО ДОБАВЛЕНИЯ): JointSeat (торец панели
  внутри стойки, ловит «забыл стойку» и «стойка тонка для угла»), JointAngle
  (угол панели кратен шагу грани стойки, допуск ВЫВЕДЕН из ширины панели:
  atan(((w_f - T)/2) / r_in)); joint_seat_margin_m в SceneLimits. Панель и
  стойка узнаются по ИМЕНИ реестра (wall-*, joint-*-dNN-nX-*) — имя несёт
  рабочие свойства по правилу самого набора.
- 17:08:2026 - 13:14:56: секция [river] — точки «x z отметка_воды», ширина, глубина, берег.
- 17:08:2026 - 16:53:03: ЧЕТЫРЕ ЗНАЧЕНИЯ SceneRule заказа 17.08 (зона домов, HOUSES.md §8):
  WallTwoJoints, JointCapacity, DeckOnJoints, RoofSeat. Только ДОБАВЛЕНИЕ
  (правило 26): существующие значения не сдвинуты, чужие сцены читаются как
  читались. Сами правила живут в SceneHouseRules.cpp — Scene.cpp перерос
  правило 21 (1052 строки) ещё до того, как заказ удвоил число соединительных
  правил, и поэтому вынос сделан ПЕРЕД тем, как писать новое.
- 17:08:2026 - 17:28:41: StairSeat/StairHeadroom и МЕРКИ ГЕРОЯ в SceneLimits (HOUSES.md
  §9, заказ пользователя про лестницы и проём). Мерки живут в лимитах, а не
  литералами в правиле: «пройдёт ли игрок» обязано быть перенастраиваемым
  вместе с игроком, иначе в день, когда герой подрастёт, дом останется с
  шишкой, а зелёный тест — зелёным. Снова только добавления (правило 26).
- 17:08:2026 - 19:05:00: КЛЮЧ `relief` — имя сиделки .relief рядом со сценой (зона кистей
  рельефа, заказ 17.08; добро лида получено). Только имя, а не сами сэмплы:
  мазок это десятки тысяч чисел, и внутри .scene они убили бы единственное
  свойство, ради которого он текст — что его читает человек и что его diff
  что-то значит. Одна строка сохраняет оба: композиция остаётся читаемой,
  лепка лежит в git рядом. Ключ необязательный, его отсутствие — прежнее
  поведение до последнего бита.
- 20:08:2026 - 15:30:00: Секция [house]: готовая постройка (.dfh) + место — регистрация домов кузницы.
- 22:08:2026 - 16:20:00: SceneAir / [air] — туман композиции: спан под масштаб карты
  (город 256 м против лесного километра констант). Необязателен.
- 22:08:2026 - 20:10:00: SceneAir.cloud_cover — стартовая облачность карты (приёмка круга 2:
  под вечной палубой 0.45 тень дома на земле неотличима от облачного пятна).
- 22:08:2026 - 21:00:00: SceneLight.interior — интерьерный свет гейтится небесной видимостью приёмника (очаг не светит улице сквозь кладку).
- 23:08:2026 - 01:40:00: SceneLight.room_center/room_half — коробка комнаты у [light].
- 22:08:2026 - 22:51:38: SceneLight.softness — мягкость источника, ключ softness у [light].
- 23:08:2026 - 01:17:49: SceneLight.flicker — мерцание живого огня, ключ flicker у [light].
- 23:08:2026 - 22:10:00: И15 волна А, шаг 1 — ТРИ ДОБАВЛЕНИЯ ФОРМАТА под интерьеры-локации
  (docs/plans/INTERIORS_I15.md): секция [portal] (ScenePortal: at/radius_m/to/
  to_spawn), секция [spawn] с ИМЕНЕМ (SceneSpawn — их может быть несколько,
  дверь адресует ту, что названа), ключ interior= у [house]. Только
  добавления (правило 26): ни одно существующее поле не сдвинуто, и сцена без
  этих секций читается и пишется бит-в-бит как раньше. Разбор секций переведён
  с семи параллельных булей на ОДНО перечисление Section — с восьмой и девятой
  секцией «сбросить чужие були в каждой ветке» перестало быть выполнимым
  вручную, а забытый сброс не роняет чтение, а молча кладёт ключ в чужую
  секцию (самый дорогой из возможных отказов этого файла).
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace dfn::world {

/// One placed object: a name in the object registry plus where it stands.
/// Positions are WORLD METRES and heights are ABSOLUTE — not offsets from a
/// ground the file cannot see. A scene that stored "0.2 above the ground"
/// would be a scene whose meaning changed when the terrain did.
struct Placement {
    std::string object;      ///< registry object name, e.g. "oak-forge-a"
    glm::vec3 position{0.0f};
    float yaw = 0.0f;        ///< radians
    float scale = 1.0f;
    /// Free-text note from whoever placed it — the human's «зачем оно тут».
    /// Carried through read/write untouched, so a tool never eats a comment.
    std::string note;
    /// WHAT THIS PART BELONGS TO, e.g. "farmhouse". Empty = it stands alone.
    /// A group is one built thing, and it changes two rules:
    ///   - members may INTERSECT each other (a beam sits in a notch; a rafter
    ///     passes through a wall plate — that is carpentry, not a defect);
    ///   - a member may rest on ANOTHER MEMBER instead of on the terrain, so
    ///     the second storey of a house is not reported as hovering.
    /// Without groups the checker could only ever judge things standing on
    /// open ground, which is trees and nothing else.
    std::string group;
};

/// A LAMP THE COMPOSITION HANGS. Not an object — an object is a thing you can
/// walk into, and a light is not. The lamp POST is a Placement like any other;
/// this is the flame on it, and the two are separate rows on purpose: the same
/// post carries a lit lamp at night and an unlit one by day, and a composer
/// must be able to move one without the other.
///
/// A scene may declare far more lamps than the renderer can light at once
/// (eight in a frame, two of them casting). That is deliberate and not a
/// budget to police here: the file says what EXISTS, the renderer decides what
/// is LIT, and it picks the nearest by distance with a fade at the edge.
struct SceneLight {
    glm::vec3 position{0.0f};   ///< world metres, the flame itself
    glm::vec3 color{1.0f, 0.85f, 0.55f}; ///< linear; the default is a flame
    float radius_m = 6.0f;      ///< 0 = off, and an off lamp is not an error
    bool casts_shadow = false;  ///< honoured for the two nearest that ask
    /// ИНТЕРЬЕРНЫЙ свет гейтится небесной видимостью приёмника (22.08): очаг
    /// в доме не светит улице сквозь кладку — теневой слот есть только у двух
    /// ближайших, остальные шесть иначе жгут occl = 1.0 через любую стену.
    /// Уличные жаровни и фонари флаг НЕ ставят — им гейт погасил бы улицу.
    bool interior = false;
    /// КОРОБКА КОМНАТЫ (план, метры): интерьерный свет не выходит за неё.
    /// Нулевые полуразмеры — коробки нет (прежний гейт по AO приёмника).
    glm::vec2 room_center{0.0f};
    glm::vec2 room_half{0.0f};
    // Мягкость источника 0..1 (ключ softness): 0 — резкий факельный профиль,
    // 1 — свет огибает форму и затухает полого (фонарь со «стеклом»).
    float softness = 0.0f;
    // Мерцание живого огня 0..1 (ключ flicker): модуляция яркости на CPU,
    // детерминированная от времени сцены и позиции. 0 — ровный свет.
    // В КОНЦЕ структуры сознательно: агрегатные инициализаторы не ломаются.
    float flicker = 0.0f;
    std::string note;
};

/// A FLAT THE COMPOSITION CUTS INTO THE GROUND — the terrace of a town, the
/// pad under a house, the shelf a market square stands on. Rectangular when
/// half_extents is set, circular on `radius` otherwise.
///
/// This is the "edit the heightmap" half of the tool the user asked for. It is
/// a STATEMENT, not a brush stroke: a pad says "here the ground is this high,
/// blending back over this many metres", so it can be moved, re-read and
/// judged — which a painted heightfield could not be.
struct ScenePad {
    glm::vec2 center{0.0f};
    glm::vec2 half_extents{0.0f}; ///< rectangle; zero = use radius
    float radius = 0.0f;
    float blend = 8.0f;           ///< metres to fade back into the natural ground
    float height = 0.0f;          ///< absolute metres
    std::string note;
};

/// A WATERCOURSE THE COMPOSITION AUTHORS. Points are "x z water_height" — the
/// third number is the SURFACE OF THE WATER at that station, because a river's
/// fall is a design decision (where the rapids are, how deep the town's canal
/// sits below its quay) and not something to be derived from the ground it has
/// not been cut into yet.
///
/// One statement produces both the channel CUT and the WATER standing in it.
/// Authored as terrain alone it would be a dry ditch; as water alone, a sheet
/// lying on a hillside.
/// ГОТОВАЯ ПОСТРОЙКА НА КАРТЕ (20.08: «дома зарегистрировать, чтобы можно
/// было их как готовые постройки ставить»). Ссылка на .dfh из библиотеки
/// assets/houses + место. Файл, а не вкопированный граф: постройка правится
/// в одном месте, и diff сцены остаётся про КОМПОЗИЦИЮ, а не про стены.
struct ScenePlacedHouse {
    std::string file;      ///< путь к .dfh от корня репозитория
    glm::vec3 position{0.0f};
    float yaw = 0.0f;      ///< радианы вокруг вертикали, 0 — как в файле
    /// ЛОКАЦИЯ ВНУТРИ ЭТОЙ ПОСТРОЙКИ (ключ interior, И15): путь к .scene
    /// интерьера от корня репозитория. Пусто — оболочка без входа, и это
    /// ЗАКОННОЕ умолчание, а не недоделка: в Skyrim вход имеет постройка с
    /// жильцом или назначением, остальные — дверь-декорация (решение
    /// владельца 24.08). Ключ живёт у РАЗМЕЩЕНИЯ, а не у .dfh: два дома из
    /// одного чертежа стоят в разных местах города и внутри у них разное.
    std::string interior;
    std::string note;
};

/// НАЗВАННАЯ ТОЧКА ВХОДА ([spawn], И15). Заголовочные spawn/spawn_yaw
/// отвечают на вопрос «где игрок, когда карта открыта», и у них ровно один
/// ответ. У локации вопросов столько, сколько у неё дверей: вошедший с рынка
/// обязан оказаться у ТОЙ двери, в которую вошёл, а не у первой попавшейся.
///
/// ИМЯ, А НЕ НОМЕР. Портал адресует точку строкой (to_spawn = door-north),
/// потому что номер меняется при вставке двери в середину файла, и сохранение
/// игрока, хранящее номер, после правки дома высаживает его в кладовке.
/// Заголовочный spawn остаётся как был — это точка по умолчанию.
struct SceneSpawn {
    std::string name;         ///< "door", "door-north"; пустое имя = безымянная
    glm::vec3 position{0.0f}; ///< метры сцены (у интерьера — свои от нуля)
    float yaw = 0.0f;         ///< радианы; 0 смотрит на север
    std::string note;
};

/// ПЕРЕХОД В ДРУГУЮ ЛОКАЦИЮ ([portal], И15) — дверь дома, устье штольни,
/// люк подклета. Утверждение о СВЯЗИ, а не о геометрии: створка двери уже
/// стоит в сцене как деталь постройки, и портал ничего не рисует.
///
/// `to = ^back` — ОБРАТНАЯ ДВЕРЬ: вернуться туда, откуда вошли, а не в
/// названный файл. Интерьер не знает и не должен знать, из какого города в
/// него вошли: одна и та же изба живёт в трёх сценах, и записанный в неё путь
/// наружу был бы ложью в двух случаях из трёх. Куда именно возвращаться,
/// помнит СОСТОЯНИЕ перехода (точка активации), а не файл.
struct ScenePortal {
    glm::vec3 at{0.0f};    ///< точка активации, метры сцены
    float radius_m = 1.0f; ///< с какого расстояния дверь берётся рукой
    std::string to;        ///< путь к .scene от корня, либо "^back"
    std::string to_spawn;  ///< имя [spawn] в целевой сцене; пусто — её spawn
    std::string note;
};

/// Ответ на "^back" одним местом: обратная ли это дверь.
[[nodiscard]] bool portal_is_back(const ScenePortal& p);

struct SceneRiver {
    std::vector<glm::vec3> points; ///< x, z, water surface height (m)
    float width_m = 6.0f;
    float depth_m = 1.0f;
    float bank_m = 6.0f;
    std::string note;
};

/// THE COMPOSITION'S AIR ([air], 22.08). A 256 m walled city needs its fog to
/// span ITS scale, not the forest kilometre the look-dev constants were tuned
/// for: at start 300 m the whole map sits in clear air, the world's edge reads
/// as a table edge, and (since the soft-shadow cascade shrank to 160 m) the
/// band 160..300 m shows unshadowed, unfogged ground. Optional — a scene
/// without it keeps the global constants, which is the control arm.
struct SceneAir {
    bool set = false;
    float fog_start_m = 0.0f;
    float fog_end_m = 0.0f;
    /// ДЕФОЛТНАЯ ОБЛАЧНОСТЬ КАРТЫ, 0..1; < 0 — не задана (глобальный дефолт).
    /// Приёмка 22.08 [8]/[N5]: под вечной палубой 0.45 тень дома на земле
    /// неотличима от облачного пятна, и «теней нет» читалось как дефект
    /// карты теней. Эталонные кадры города сняты при прямом солнце — карта
    /// вправе заявить свой свет. Значение — стартовое состояние погоды, а не
    /// вечный зажим: будущее расписание погоды пишет поверх, DFN_CLOUD тоже.
    float cloud_cover = -1.0f;
};

/// One composed scene: the placements of one map.
struct SceneDoc {
    std::string map;         ///< "category/stem" this scene composes
    /// World extent in metres, for the bounds rule. 0 = unknown (the checker
    /// then says so instead of passing the rule silently).
    float world_span_m = 0.0f;
    /// WHERE THE PLAYER STANDS when this map opens, and which way he looks.
    /// Optional: without it the stand's own spawn is used, exactly as before.
    /// It belongs to the COMPOSITION and not to the stand, because "stand here
    /// and look at that" is a statement about what was BUILT — the middle of a
    /// stone path facing the great oak, or just inside a house's door. A stand
    /// only knows where the middle of its chunk is.
    bool has_spawn = false;
    glm::vec3 spawn{0.0f};
    float spawn_yaw = 0.0f;   ///< radians; 0 looks north (forward = {sin,0,-cos})
    std::vector<Placement> placements;
    std::vector<ScenePlacedHouse> houses;
    /// НАЗВАННЫЕ точки входа (И15). Пусто на каждой карте, у которой одна
    /// дверь и она же начало — то есть на всех сегодняшних.
    std::vector<SceneSpawn> spawns;
    /// ПЕРЕХОДЫ (И15). Пусто = карта без порталов, и мир без порталов обязан
    /// остаться бит-в-бит прежним (правило 47: доза 0 — контрольная рука).
    std::vector<ScenePortal> portals;
    std::vector<SceneLight> lights;
    std::vector<ScenePad> pads;
    std::vector<SceneRiver> rivers;
    SceneAir air;
    /// THE HAND-PAINTED GROUND, by filename — the .relief sidecar next to this
    /// file (world::ReliefLayer). Empty on every map nobody sculpted, and then
    /// the world is exactly what the generator and the pads make it.
    ///
    /// A NAME HERE AND THE SAMPLES OVER THERE, on purpose. A brush stroke is
    /// tens of thousands of numbers; pasted into this file they would destroy
    /// the one property that makes a .scene worth being text — that a human
    /// reads it and a diff means something. One key keeps both: the composition
    /// stays legible, and the sculpt stays in git beside it.
    ///
    /// The name is a FILENAME, not a path: a scene that could point outside its
    /// own directory would be a scene that breaks when the map is moved.
    std::string relief;
};

/// Which rule a finding broke. Named, not numbered: a report a human reads.
enum class SceneRule : uint8_t {
    OnGround,      ///< the object neither hovers nor is buried
    InsideBounds,  ///< the whole object stays inside the map
    NoOverlap,     ///< two objects do not stand inside each other
    KnownObject,   ///< the registry has an object by this name
    /// NOTHING STANDS ON A PATH. A road with a tree growing out of it is not a
    /// road, and the path is now the ground's own property — so an object over
    /// it is not merely ugly, it contradicts what the ground says it is.
    OffPath,
    /// NOTHING STANDS INSIDE A BUILDING THAT IS NOT PART OF IT — and the rule
    /// reads both ways, which is why it is one rule and not two: a tree in a
    /// house and a house on a tree are the same overlap seen from two ends.
    /// The user named both (17.08): «нельзя дерево в доме ставить / дом поверх
    /// дерева ставить».
    OutsideBuildings,
    /// ТОРЕЦ ПАНЕЛИ ЖИВЁТ ВНУТРИ СТОЙКИ (HOUSES.md §3/§5). A wall panel's end
    /// never touches another panel: both vertical edges of the end face must
    /// lie inside its joint post's cylinder, r_in minus a margin. Catches both
    /// «забыл стойку» (no joint anywhere near the end) and «стойка тонка для
    /// этого угла». Panels and joints are recognised by their registry NAMES
    /// (wall-*, joint-*-dNN-nX-*): the name carries the working properties by
    /// the kit's own rule, so the judge reads the same contract the composer
    /// does.
    JointSeat,
    /// УГОЛ ПАНЕЛИ КРАТЕН ШАГУ СТОЙКИ (HOUSES.md §4/§5). A faceted joint
    /// hands out exactly N directions (360/N apart, measured against the
    /// POST'S OWN yaw, never the world axes — a square post turned 30 deg
    /// offers turned facets). The tolerance is DERIVED, not designated: the
    /// angle error at which the panel's exit band rides past the facet's
    /// arris, atan(((w_f - T)/2) / r_in). A facet narrower than the panel is
    /// itself a finding — that post cannot carry that panel at ANY angle.
    JointAngle,
    /// МОДУЛЬ СТЕНЫ ВИСИТ НА ДВУХ ШАРНИРАХ (HOUSES.md §8). The user, 17.08:
    /// «каждый модуль стены должен быть присоединён к ДВУМ шарнирам-столбам,
    /// обязательно». Both ends inside ONE post satisfies JointSeat twice over
    /// and is still not a wall: it has an angle but no span.
    WallTwoJoints,
    /// СКОЛЬКО У ШАРНИРА ГРАНЕЙ — СТОЛЬКО ПАНЕЛЕЙ ОН НЕСЁТ (§8). No budget was
    /// chosen here: a panel seats FLUSH ON A FACET, so a facet already
    /// carrying one has nothing left for the next. Round joints have no limit.
    JointCapacity,
    /// ПОЛ И ПОТОЛОК НЕ ВИСЯТ В ПРОСТРАНСТВЕ (§8): a deck is let into 2 to 4
    /// horizontal joints — two at the least, always; four at the most, which
    /// is a rectangle framed on every side.
    DeckOnJoints,
    /// КРЫША ДЕРЖИТСЯ ЗА ГОРИЗОНТАЛЬНЫЕ ШАРНИРЫ (§8). A sloped panel whose
    /// lower edge is parallel to its upper spans TWO of them (that is what
    /// makes arches and canopies, not only triangles); a triangular one hangs
    /// by its APEX on one. The exception is the КОЗЫРЁК — an eaves edge out
    /// past the building's own posts, over open ground, seats on nothing.
    RoofSeat,
    /// ЛЕСТНИЦА — ТРЕТИЙ КЛИЕНТ ГОРИЗОНТАЛЬНЫХ ШАРНИРОВ (HOUSES.md §9,
    /// пользователь 17.08: «надо лестницы крепить к пол-потолок»). Низ марша
    /// садится на шарнир нижнего уровня, верх — на шарнир верхнего.
    StairSeat,
    /// НАД ЛЕСТНИЦЕЙ ДОЛЖНА БЫТЬ ДЫРКА, ЧЕРЕЗ КОТОРУЮ ПРОЙДЁТ ИГРОК (§9). И
    /// это правило МЕРИТ, а не считает: капсула игрока ставится НА КАЖДУЮ
    /// СТУПЕНЬ и спрашивает, что она задевает. Выведенная формула длины проёма
    /// — калькулятор для генератора; при расхождении прав ЭТО правило, потому
    /// что оно про игрока, а формула про её собственные допущения.
    StairHeadroom,
};

/// One violation. Carries the NUMBER, not just a verdict — "hovers" is an
/// opinion, "hovers by 0.42 m" is a measurement somebody can act on.
struct SceneFinding {
    SceneRule rule = SceneRule::OnGround;
    std::size_t placement_index = 0;
    std::string object;
    float amount_m = 0.0f;   ///< how far past the rule (metres), signed
    std::string detail;
};

/// What the checker needs to know about the world it is judging. Supplied by
/// the caller so this header depends on no generator: a tool passes worldgen's
/// sampler, a test passes a flat plane, and both exercise the same rules.
struct SceneWorld {
    /// Ground height at a world x/z, metres. Required.
    float (*ground_at)(void* ctx, glm::vec2 world_xz) = nullptr;
    /// Footprint radius of a registry object, metres, and its lowest point
    /// relative to its origin (negative for roots that dive). Required: the
    /// rules measure the OBJECT, never a guessed size.
    bool (*object_extent)(void* ctx, const std::string& name, float& radius_m,
                          float& bottom_m) = nullptr;
    /// How tall the object is above its own origin, metres. OPTIONAL: when it
    /// is null nothing can rest on anything and the ground rule measures
    /// against the terrain alone, exactly as before groups existed. Supplied
    /// separately rather than as a fourth out-parameter of object_extent so
    /// that every caller written against the old shape keeps compiling
    /// (Rule 26: contracts grow, they do not change).
    bool (*object_top)(void* ctx, const std::string& name, float& top_m) = nullptr;
    /// The object's FOOTPRINT as a box in its own local space, relative to its
    /// origin. OPTIONAL: without it the checker falls back to the radius
    /// circle above, which is right for a tree — round, centred on its trunk —
    /// and badly wrong for a building part, whose origin is at one END. A 4 m
    /// gable measured as a circle claims a 4.5 m radius about its corner and
    /// then "stands inside" everything in the yard.
    bool (*object_box)(void* ctx, const std::string& name, glm::vec2& min_xz,
                       glm::vec2& max_xz) = nullptr;
    /// Metres from the OUTER EDGE of the worn path surface, outward; negative
    /// ON the trodden surface. OPTIONAL — a world with no paths supplies none,
    /// and then the path rule simply never fires.
    ///
    /// It asks the SAME field the ground was worn by. Asking a second source
    /// would let the judge forbid building where the ground shows no path, and
    /// permit it where the ground shows one.
    bool (*path_clearance)(void* ctx, glm::vec2 world_xz, float& metres) = nullptr;
    /// Is this object SOLID — does it have geometry a body is built from?
    /// OPTIONAL; without it everything counts as solid, which is what the rule
    /// assumed before the question could be asked.
    ///
    /// It is the criterion the GAME already uses: an object with no solid
    /// stream gets no collision body and the player walks through it. Grass,
    /// flowers and mushrooms are such objects, and two of them sharing a
    /// patch of ground is a meadow, not a defect — which is why the overlap
    /// rule must not fire on them. Deciding this by a LIST OF SPECIES would
    /// have been a second definition of "solid" that drifts from the one the
    /// player's knees already know.
    bool (*object_solid)(void* ctx, const std::string& name) = nullptr;
    /// The footprint of the object's SOLID part only — its trunk and its
    /// walls, not its crown. OPTIONAL; without it the whole footprint is used,
    /// as before.
    ///
    /// The overlap rule needs this and the support rule must NOT have it. Two
    /// birches two metres apart have mingling crowns and separate trunks: that
    /// is a wood, and measuring their crowns called it a defect forty thousand
    /// times on one map. A beam resting on a post, on the other hand, rests on
    /// whatever part of it is under it. Same objects, two questions, two
    /// footprints.
    bool (*object_box_solid)(void* ctx, const std::string& name, glm::vec2& min_xz,
                             glm::vec2& max_xz) = nullptr;
    void* ctx = nullptr;
};

/// Tolerances. Defaults are the numbers today's defects were measured at; a
/// caller may tighten them, and the checker reports which value it used.
struct SceneLimits {
    float ground_tolerance_m = 0.05f; ///< hover/bury beyond this is a finding
    /// How far a GROUP MEMBER may be dug into the terrain before it counts.
    /// Asymmetric with the hover tolerance on purpose: a building sits on a
    /// slope by burying the uphill side of its footing course — that is how
    /// masonry works — while a part hovering by the same amount is always a
    /// mistake. Loose objects (no group) get no such licence: a tree buried
    /// half a metre is a defect, not a design.
    float bury_tolerance_m = 0.5f;
    float edge_margin_m = 2.0f;       ///< keep this much of the map beyond it
    float overlap_slack_m = 0.5f;     ///< crowns may mingle by this much
    /// How far an object must keep from the worn edge of a path. Not zero: a
    /// trunk exactly at the edge still drops its crown and its roots over the
    /// tread, and a walker still has to step around it.
    float path_clearance_m = 0.5f;
    /// How far a loose object must keep out of a building's footprint. Small
    /// on purpose — a barrel against a wall is a barrel against a wall, not a
    /// defect; what this rule is for is a tree in the middle of a room.
    float building_slack_m = 0.25f;
    /// How far inside the joint's inscribed radius a panel end's corner must
    /// stay (HOUSES.md §5: dist <= r - 0.02). The 0.02 is float safety made
    /// visible: a corner ON the joint's surface is a seam that flickers.
    float joint_seat_margin_m = 0.02f;
    /// МЕРКИ ГЕРОЯ, которыми судья мерит проём над лестницей (HOUSES.md §9).
    /// Копии docs/NUMBERS.md, и они живут ЗДЕСЬ, а не литералами в правиле:
    /// правило про «пройдёт ли игрок» обязано быть перенастраиваемым вместе с
    /// игроком, иначе в день, когда герой подрастёт, дом останется с шишкой, а
    /// зелёный тест — зелёным.
    float player_capsule_height_m = 1.8f;
    float player_capsule_radius_m = 0.35f;
};

/// Reads a .scene file. Returns false and fills `error` (with the line number)
/// on a malformed number or a missing required key.
[[nodiscard]] bool read_scene(const std::filesystem::path& path, SceneDoc& out,
                              std::string& error);

/// Writes it back, atomically, in a stable order — so a diff shows what a
/// human changed and not what a container reordered.
[[nodiscard]] bool write_scene(const SceneDoc& doc, const std::filesystem::path& path);

/// Judges the scene. Returns every finding; an empty result is a clean scene.
[[nodiscard]] std::vector<SceneFinding> check_scene(const SceneDoc& doc,
                                                    const SceneWorld& world,
                                                    const SceneLimits& limits = {});

/// Sits every hovering or buried placement back on the ground and returns how
/// many it moved. SEPARATE from check_scene and explicit on purpose: a checker
/// that repairs is a checker whose report nobody reads.
[[nodiscard]] std::size_t fix_scene_ground(SceneDoc& doc, const SceneWorld& world,
                                           const SceneLimits& limits = {});

/// Splits a shelf list ("a;b;c") into its directories, trimmed, empties
/// dropped. ONE definition because three tools already need it — the app, the
/// checker and the assembler — and a shelf list that means different things in
/// the game and in its judge is a judge of a different world (Rule 35).
[[nodiscard]] std::vector<std::string> split_shelves(const std::string& list);

/// Human-readable one-liner for a finding (the tool's output, and the text an
/// agent pastes into a map chat).
[[nodiscard]] std::string describe(const SceneFinding& finding);

/// Report of the panel-solidity grid (check_panel_solid). Carries the COUNT
/// and the ADDRESS, not a verdict: «дыра на x=1.93 y=0.46» is actionable,
/// «панель дырявая» is an opinion (the gable lesson, in the instrument).
struct SolidReport {
    int rays_cast = 0;
    int rays_through = 0;        ///< 0 = solid
    uint8_t normal_axis = 0;     ///< 0=x 1=y 2=z: the thinnest bbox axis
    /// World-space point where the FIRST through-ray crossed the panel slab's
    /// mid-plane — where to look for the hole.
    glm::vec3 first_hole{0.0f};
};

/// THE PANEL INSTRUMENT. A flat assembly (wall panel, floor deck) has no
/// interior to stand a probe in, so the sealed-hull fan cannot judge it; what
/// a panel promises is NO DAYLIGHT STRAIGHT THROUGH. Casts a grid of parallel
/// rays across the triangles' THINNEST bounding-box axis (the panel's normal,
/// found from geometry, never from the property under test), one ray per
/// `step_m`; the default 0.01 m is set by the narrowest real hole this zone
/// has shipped — a 0.017 m board gap — which a coarser grid would certify as
/// solid. `rim_m` trims the border, where the panel's edge is the frame's
/// business and a boundary ray measures floating point, not wood.
///
/// Takes BARE world-space triangles (positions + indices) so this header
/// stays free of engine/render: the scene judge and the assembly baker each
/// build the soup from the shelves they already read, and both call THIS
/// function — one instrument, two callers, no second opinion.
[[nodiscard]] SolidReport check_panel_solid(const std::vector<glm::vec3>& positions,
                                            const std::vector<uint32_t>& indices,
                                            float step_m = 0.01f,
                                            float rim_m = 0.02f);

} // namespace dfn::world
