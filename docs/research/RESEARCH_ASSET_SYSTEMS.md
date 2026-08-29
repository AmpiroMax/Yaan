<!--
Module: docs
File: docs/research/RESEARCH_ASSET_SYSTEMS.md

Responsibility:
- Проектная разведка для РЕЕСТРА ОБЪЕКТОВ Daggerfall N: как Unity / Unreal 5 /
  Godot 4 / Bevy хранят единицу ассета, версии, LOD, анимацию, воду и уровни;
  что копировать, а что нет для пиксель-арт RPG с бинарным .dfw; рекомендация
  по нашей «единице реестра» (В5) и по хранению анимации с развилками для
  пользователя.

Key items:
- Сравнение 4 движков по 6 пунктам (таблица TL;DR + разделы), три подхода к
  анимации (скелет / VAT / shader-wind), техника flow-map воды, рекомендация
  под ContentHash/fnv1a64 и секционный .dfo-контейнер.

Dependencies:
- Uses: engine/core/serialization (ContentHash, BinaryReader/Writer),
  engine/world/sources/WorldFormat.h (.dfw контейнер), docs/design/RIVER_RESEARCH.md.
- Used by: решения по реестру объектов и формату .dfo, хранению анимации флоры/
  воды, конвейеру bake.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Разведка scouting-level: цитаты движковых доков — из официальных источников
  (список в конце), собраны под-агентами через WebSearch/WebFetch; точки
  неуверенности помечены [?]. Ничего не «по памяти» без пометки.
-->

# RESEARCH: как «взрослые» движки хранят объекты, LOD, анимацию и уровни

Проектная разведка для РЕЕСТРА ОБЪЕКТОВ Daggerfall N (собственный C++23 движок,
бинарный контейнер `.dfw`, offline-bake → игра только читает, конечный сюжетный
мир, пиксель-арт RPG). Цель: **не изобретать велосипед**, а взять готовые решения
Unity / Unreal Engine 5 / Godot 4 / Bevy и честно отделить «копировать» от «нет».

Статус: scouting-level. Синтез из официальных доков и разборов (Unity Manual,
Epic dev-docs, Godot docs, Bevy docs/release-notes, GDC/80.lv/SpeedTree/Valve).
Точки, где я не уверен (в основном Bevy — быстро меняется), помечены **[?]**.

Наши примитивы, к которым всё привязывается (уже в коде):
- `.dfw` — контейнер с magic + версией + тег-секциями (Rule 7: little-endian
  через BinaryWriter/Reader, неизвестные секции пропускаются, миграции с v1).
- `ContentHash` = FNV-1a 64, **алгоритм заморожен навсегда**, уже «keys asset
  lookups» и «render asset name hashes». Это наш готовый GUID.
- Модель Q13: генерация оффлайн, игра только читает.

---

## 0. TL;DR (одной таблицей)

| Тема | Unity | Unreal 5 | Godot 4 | Bevy | Что берём |
|------|-------|----------|---------|------|-----------|
| Единица | Prefab (.prefab) | Actor + StaticMesh (.uasset) | PackedScene (.tscn/.scn) | Scene (.scn.ron) + Handle | контейнер `.dfo`, адрес = ContentHash |
| Адрес | GUID (.meta) + fileID; Addressables address | object path + FGuid; soft path | res:// path + uid:// | AssetPath (path#label) + Handle/AssetId | content-hash (fnv1a64) |
| Шаблон/экземпляр | Prefab / PrefabInstance+m_Modifications | CDO/Class / Actor+transform | PackedScene / instance+overrides | Scene / spawned Entity+Transform | template в реестре, instance = (hash, transform, params) |
| Версия | нет пиннинга (VCS) | нет пиннинга (Perforce) | нет пиннинга (VCS) | нет | **hash = версия**; name→hash каталог |
| LOD | LODGroup + screen height | LOD array / **Nanite** | visibility_range + auto-LOD | слабо/самодельно **[?]** | несколько мешей в `.dfo`, выбор по экрану, дизеринг |
| Анимация | Animator+Clip / VAT / ShaderGraph wind | AnimBP / VAT / **WPO+Pivot Painter** | AnimationTree / shader wind | AnimationGraph / WGSL | 3 яруса: shader-wind / VAT / скелет |
| Вода | URP/HDRP Water, flow map | Water plugin (сплайны) | нет (шейдеры комьюнити) | нет | flow-map текстура + пена по глубине |
| Уровень | .unity (YAML, ссылки+трансформы) | .umap (Actors+ссылки) | .tscn (ext_resource+ноды) | .scn.ron (entities+components) | секция карты: список (hash, transform) |

---

## 1. Единица ассета и адресация (для В5)

Общий паттерн всех четырёх: **ассет-файл ≠ размещённый экземпляр**. Ассет —
это переиспользуемый шаблон (меш/материал/prefab); экземпляр — это ссылка на
шаблон + трансформ + небольшой набор оверрайдов. Уровень хранит экземпляры, а не
геометрию.

**Unity.** «Объект» = **Prefab** — файл `.prefab` (YAML) с деревом GameObject +
Component. Адресуется **GUID** (32-hex в соседнем `.meta`) + **fileID/localID**
(локальный id объекта внутри файла). Ссылки в сценах/префабах = пара
`{fileID, guid}`. Поверх — **Addressables**: строковый адрес («address») и
`AssetReference` (внутри всё равно хранит GUID) для загрузки по имени/группе.
Полная ссылка = пара **{fileID, guid}** (guid = какой файл, fileID = какой объект
внутри). Экземпляр в сцене = **PrefabInstance** с `m_SourcePrefab` (ссылка на
префаб) + список **m_Modifications** = массив `{target, propertyPath, value,
objectReference}` (даже позиция инстанса — это override-запись `m_LocalPosition.x`);
плюс категории added/removed component и added GameObject. Оверрайды пишутся
**позиционно** против структуры префаба (перестановка элементов массива в базе
может «увести» оверрайд не на тот элемент). Ключевая мысль: **GUID стабилен и НЕ
зависит от пути** — файл можно переименовать/переместить, ссылки не рвутся.

**Unreal 5.** Базовый `UObject`; размещаемый — **Actor**; геометрия —
**StaticMesh/SkeletalMesh** внутри пакета **`.uasset`**. Адрес = **object path**
вида `/Game/Meshes/SM_Rock.SM_Rock` (package path + `.` + имя объекта) через
`FName`; в шапке пакета есть **FGuid**; всё индексируется **Asset Registry**.
Шаблон класса = **CDO (Class Default Object)** — эталонный экземпляр класса, от
которого наследуют дефолты. Правило сверки при загрузке: если свойство инстанса
всё ещё равно **старому** дефолту CDO — оно подтягивается к новому дефолту; если
отличается — считается намеренным оверрайдом и сохраняется (property-delta модель,
как у Unity). **Blueprint-класс** — редактируемый шаблон; Actor на уровне —
экземпляр с трансформом. Важное различие: **hard ref** (`TObjectPtr`,
грузит цель немедленно) vs **soft ref** (`TSoftObjectPtr`/`FSoftObjectPath`,
ссылка по пути, ленивая загрузка) — прямой аналог нашего «ссылаться по хешу, но
грузить по требованию». **Data Asset** + **Primary Asset Id** — реестр-подобный
слой для Asset Manager.

**Godot 4.** «Объект» строится из **Node**; переиспользуемый шаблон = **PackedScene**
(файл **`.tscn`** текстовый / **`.scn`** бинарный). Данные (меши, материалы) =
**Resource** (`.tres`/`.res`). Адрес = путь **`res://…`**; в Godot 4 добавили
**UID** — **`uid://…`** (через `ResourceUID`, кэш в `.uid`-файлах), чтобы ссылки
переживали перемещение файла (тот же мотив, что GUID у Unity). Экземпляр = сцена,
**инстансированная** в другую сцену: её override-ы (изменённые свойства узлов)
хранятся в родительском `.tscn`.

**Bevy.** ECS: «объекта» как монолита нет — сущность **Entity** = набор
**Component**-ов. Ассеты: ресурс **`Assets<T>`**, **`Handle<T>`** (strong/weak),
**`AssetId`** (Uuid или индекс), грузятся через `asset_server.load("path")`.
Адрес = **`AssetPath`** = путь + опциональный **label** для суб-ассета
(`"model.gltf#Mesh0/Primitive0"`). Шаблон-подобие — **`Scene`/`DynamicScene`**
(формат **`.scn.ron`**, сериализация компонентов через reflection); спавнится
через **`SceneRoot`/`DynamicSceneRoot`**. Полноценной prefab-системы исторически
нет; ведётся работа над **BSN (Bevy Scene Notation)** **[?]**. Экземпляр = сущность
с компонентом `Transform`.

**Копировать:** стабильный ID, отвязанный от пути (GUID/UID/FGuid) — у нас это
**уже есть** как `ContentHash`; ссылка «soft/по пути с ленивой загрузкой» (UE) —
т.е. карта ссылается на хеш, реальные треугольники грузятся по требованию;
экземпляр = ссылка + трансформ + минимум оверрайдов (все четыре так делают).
**Не копировать:** редакторную YAML/текст-сериализацию (Unity `.unity`, Godot
`.tscn`) — у нас бинарь и bake-only; богатую систему per-property override-ов
(`m_Modifications`) — для конечного мира достаточно нескольких параметров
экземпляра (см. §2). Addressables-как-отдельный-подсистемный-слой — избыточно,
хеш и есть адрес.

---

## 2. Составные объекты и версии (nested / variants / «latest»)

**Unity.** **Nested Prefabs** (префаб внутри префаба) и **Prefab Variants**
(вариант = базовый префаб + слой override-ов, аналог наследования). Ссылка на
под-префаб хранится **по GUID** и **всегда резолвится в текущее содержимое
файла** — встроенного «пиннинга версии» НЕТ. «Конкретная версия против latest»
решается **системой контроля версий** (Git/Perforce), а не движком.

**Unreal.** **Blueprint inheritance** (родитель/потомок класса), компоненты
(inherited vs instanced). Ссылки — по object path / FGuid, тоже **всегда
текущий контент**, пиннинга нет, версии — на Perforce. `Data Asset` + Primary
Asset Id дают именованный слой, но не версионирование.

**Godot.** **Inherited scenes** (наследование сцены) + инстансирование дочерних
сцен, **editable children**, `local_to_scene`. Override-ы наследника лежат в его
`.tscn`. Версии — VCS, пиннинга нет.

**Bevy.** Композиция = сцена спавнит дочерние сущности; иерархия `ChildOf`/
`Children` (0.15). Вариантов/наследования prefab-ов в 0.14–0.16 нет — только
Scene/DynamicScene. Наследование сцен и field-patching принесла **BSN (Bevy
Scene Notation)**, но она **зашла лишь в 0.19 (июнь 2026)**, в разбираемую эпоху
это был proposal/эксперимент. Версий/пиннинга нет.

Вывод по индустрии: **никто не пиннит версию под-ассета внутри движка** — ссылка
= «последняя версия файла», а воспроизводимость обеспечивает система контроля
версий. Это удобно для живой разработки, но опасно для **конечного сюжетного
мира**: пересобрал дерево — все уровни молча получили новое.

**Копировать:** nested-композицию (объект из под-объектов) и наследование-через-
слой-оверрайдов как *концепцию сборки на этапе bake*. **Не копировать:** «ссылка
= всегда latest». У нас **content-hash уже даёт пиннинг бесплатно**: ссылка по
хешу = ссылка на *конкретный* байтовый контент (immutable). «latest» реализуем
отдельным слоем — **каталог имён**: стабильное имя (`"oak_giant"`) → текущий хеш;
уровень хранит РАЗРЕШЁННЫЙ хеш. Rebake меняет каталог, но уже испечённые уровни
держат свой хеш, пока их явно не перелинкуют. Это ровно модель Addressables
(address→GUID), только с иммутабельностью на стороне контента.

---

## 3. LOD (уровни детализации)

**Unity.** Компонент **LODGroup**: массив уровней LOD0/LOD1/…/(Culled), у каждого
свои Renderer-ы. Выбор — по **screen relative transition height** (доля высоты
экрана, которую занимает bounding sphere): падает ниже порога → следующий LOD.
Переход: `LODGroup.fadeMode` = **None** (поп), **Cross Fade** (через **дизеринг/
stipple** screen-door, фактор в `unity_LODFade.x`), либо **Speed Tree** —
принципиально иной способ: вершина хранит позицию в текущем И следующем LOD, и
геометрия **морфится интерполяцией позиций** (без наложения двух рендеров).
Импостор: **SpeedTree billboard** (последний LOD — 8-ракурсный billboard) или
плагин **Amplify Impostors** (octahedral). Импостор включается на самой дальней
дистанции, когда объект — несколько пикселей.

**Unreal 5.** Классика: **StaticMesh** хранит **массив LOD-ов** (можно
авто-сгенерить); выбор по **Screen Size** (проекционный размер сферы, 0..1);
переход — **Dithered LOD Transition** (временной дизеринг между уровнями).
**Nanite** заменяет дискретные LOD-ы **виртуализированной геометрией**: меш режется
на **кластеры (~128 треуг.)**, из них строится **иерархия LOD**, и на кадре
GPU выбирает срез иерархии по **screen-space error** каждого кластера —
непрерывный LOD без ручных ступеней и без «поп-ов». Традиционные LOD-ы всё ещё
нужны там, где Nanite (исторически) не годится: **skeletal, masked/translucent,
сильный WPO-ветер, тонкая листва** (постепенно снимается, но с оговорками).
Дальняя листва — **Imposter Baker** (октаэдральные импостеры).

**Godot 4.** Два механизма. (1) **Автоматический mesh LOD**: на импорте
генерятся упрощённые индекс-буферы, движок выбирает по экранной ошибке
(`mesh_lod_threshold`, `lod_bias`) — прозрачно, без ручных мешей. (2)
**Visibility Ranges** на `GeometryInstance3D`: `visibility_range_begin/end` +
`visibility_range_fade_mode` (**disabled / self / dependencies**) с гистерезис-
маржой для плавного fade и переключения на «дальнюю» версию/HLOD. Встроенных
импостеров нет.

**Bevy.** Автогенерации классических дискретных LOD-ов нет (issue #6868). Есть
**`VisibilityRange`** (0.14, «HLOD») — рендер только в диапазоне дистанций, в
margin-полосах **кроссфейд дизерингом**, culling рано в пайплайне, per-view; но
**упрощённые меши подаёшь сам**. Плюс экспериментальные **meshlets** (0.14, за
cargo-фичей `meshlet`) — Nanite-подобная виртуальная геометрия. Наш кейс ближе к
«сделать самим, как Unity LODGroup».

**Копировать:** несколько мешей-уровней прямо внутри ассета (Unity/UE — массив
LOD в файле) → у нас секция `LODS` в `.dfo` (N мешей + пороги); выбор по
**экранному размеру** (доля высоты экрана / screen-space error), НЕ по чистой
дистанции (устойчивее к FOV); переход **дизерингом/stipple** (дёшево, без
альфа-сортировки, идеологически подходит пиксель-арту — можно даже честный
ordered-dither под стиль); **billboard-импостор как последний LOD** для деревьев
и дальней флоры, включать когда объект ≲ несколько десятков пикселей.
**Не копировать:** Nanite (нам не нужен — низкополи пиксель-арт, конечный мир;
это решение проблемы, которой у нас нет); авто-генерацию LOD на «импорте в
рантайме» — у нас всё печётся оффлайн, LOD-ы генерит bake-инструмент.

---

## 4. ХРАНЕНИЕ АНИМАЦИИ (главный раздел)

Три ортогональных подхода. В реальных движках они **сосуществуют**: один объект
может нести и скелет, и шейдерный ветер одновременно.

### 4.1 Скелетная (кости + ключевые кадры) — структурное движение

Данные: иерархия костей + **треки ключевых кадров** (позиция/поворот/масштаб на
кость во времени) + скиннинг-веса на вершинах. Обмен — glTF/FBX.

- **Unity:** **AnimationClip** (ассеты) проигрываются **Animator**-ом по графу
  **AnimatorController** (**state machine**: состояния, переходы, параметры) с
  **blend tree** (1D/2D смешивание, напр. idle→walk→run по скорости). **Avatar**
  даёт ретаргет гуманоида. Рендер — `SkinnedMeshRenderer`.
- **Unreal:** **Skeleton** + **Skeletal Mesh** + **Animation Sequence**;
  логика — **Animation Blueprint** (**AnimGraph** со **state machine** и **Blend
  Space**). Условия «какая анимация играет» — переменные из геймплея в AnimBP.
- **Godot:** **Animation** (ресурс) + **AnimationPlayer**; графа —
  **AnimationTree** (**StateMachine**, **BlendSpace1D/2D**, **BlendTree**).
  Скелет — `Skeleton3D`.
- **Bevy:** **AnimationClip** + **AnimationPlayer**; с **0.14** проигрывание
  **только через `AnimationGraph`** (clip = узел графа, играешь индекс узла; блендинг
  снизу вверх по графу). Скелет и джойнты из glTF (`SkinnedMesh`), морф-цели из
  glTF (`MorphWeights`).

Условия проигрывания везде решаются **графом состояний / blend-деревом**, куда
геймплей подаёт параметры (скорость, «в бою», «ранен»). Это стандарт для
персонажей/существ/дверей/механизмов.

Плюсы: маленький объём, произвольное смешивание и переходы, реакция на геймплей.
Минусы: нужен скелет+скиннинг, CPU/compute на позы, сложнее пайплайн.

### 4.2 Вершинная / морф — VAT и blend shapes

**Vertex Animation Texture (VAT):** позиции (и нормали) **каждой вершины на
каждом кадре** запекаются в **текстуру**: тексель `(x = vertexID, y = frame)`
хранит смещение вершины; в **вершинном шейдере** сэмплируем строку под текущее
время → готовая поза без костей (нужен стабильный per-vertex UV/ID как индекс).
Стандартный инструмент выпечки — **Houdini Labs Vertex Animation Textures ROP**,
4 режима: **Soft** (постоянная топология — ткань/флаги/сложная листва), **Rigid**
(разрушение — на кусок хранится pivot+кватернион, вершины привязаны к piece-ID),
**Fluid** (топология меняется покадрово — нужна lookup-таблица), **Sprite**
(камера-facing карточки/партиклы). Точность: 8-бит даёт бандинг, продакшн —
**16-бит half EXR** или 32-бит float; позиции ремапятся в нормализованный диапазон
через **встроенную таблицу bounds (min/max)**: `pos = lerp(min, max, sample)`;
нормали можно паковать в альфу position-map (экономия текстуры).

- **Unity:** VAT печётся в Houdini, играется кастомным vertex-шейдером
  (Shader Graph). **Blend shapes / morph targets** — через `SkinnedMeshRenderer`
  (`SetBlendShapeWeight`), для мимики/фаз.
- **Unreal:** VAT через material (сэмпл position-map в WPO); **Morph Targets** в
  Skeletal Mesh.
- **Godot:** VAT — кастомный spatial-шейдер; **blend shapes** — `MeshInstance3D.
  blend_shapes` / морфы меша.
- **Bevy:** **morph targets из glTF поддержаны**; VAT — кастомный WGSL-материал **[?]**.

Плюсы VAT: **нет костей, всё на GPU**, дёшев per-draw, отлично для сложного
вершинного движения — **ткань/флаги/знамёна, разрушения, всплески жидкости,
сложная листва, поверхность водопада**. Минусы: **фиксированный луп** (нет
блендинга/реакции), **текстура растёт с числом вершин × кадров** (память),
не гнётся под геймплей.

### 4.3 Процедурная в шейдере (wind sway) — окружающая качка

**Ничего не хранится покадрово** — только **параметры** (жёсткость, амплитуда,
частота, фаза). Движение синтезируется в **вершинном шейдере** из глобального
вектора ветра + шум-порывов. Иерархическая модель (эталон — **SpeedTree wind**):
**main bend** (ствол) → **branch sway** (ветки) → **leaf flutter** (листья),
слои синусов/шума; **фаза сдвинута per-instance** хешем от мировой позиции, чтобы
поле не колыхалось синхронно.

- **Unreal:** **World Position Offset (WPO)** в материале — канонический способ;
  **Pivot Painter 2.0** запекает **pivot-позицию (16-бит EXR) + ось вращения
  (8-бит TGA) + Element-ID в доп. UV-канал**, и ветер в WPO вращает каждую ветку
  вокруг её собственного пивота — иерархия до **4 уровней / ~30k элементов**;
  готовая **SimpleGrassWind** material function; **SpeedTree** wind. Инстансинг — **Foliage / HISM** + ветер в WPO.
- **Unity:** ветер в Shader Graph / **SpeedTree wind**; трава — `DrawMeshInstanced`
  / terrain details, качка в vertex-шейдере.
- **Godot:** ветер в **spatial-шейдере** (сдвиг `VERTEX`); трава —
  **MultiMeshInstance3D** + ветер в шейдере.
- **Bevy:** кастомный WGSL vertex-shader; трава — GPU-инстансинг, крейты типа
  `warbler_grass` **[?]**.

**Трава/листва в шипнутых открытых играх (важно):**
- **Ghost of Tsushima** (GDC 2021): каждый клинок — **кубическая кривая Безье
  (3 контрольные точки)**, клинки **генерятся compute-шейдером на GPU** в буфер
  (не хранятся мешами), рисуются indirect-инстансингом. Ветер = **скроллящийся
  2D Perlin-шум** сэмплится по world-position клинка + локальная синусоида с
  **фазой из хеша** `Hash21(worldPos)` (та же точка → тот же клинок). **Ноль
  CPU-симуляции на клинок.** Интерактивная «примятость» — через displacement-буфер
  с позициями персонажей, читается в шейдере. LOD — падение числа вершин с
  дистанцией + прореживание плотности.
- **Horizon Zero Dawn / Forbidden West** (Guerrilla): **GPU-driven** размещение
  и рендер растительности, процедурный ветер тем же слоистым способом.
- Общий приём для травы: хранится **один blade-меш**, поле = density/height-map,
  **фаза/вариация — хеш от world-position**, нулевой per-blade CPU.

Плюсы: **нулевые данные анимации**, масштабируется на миллионы инстансов, идеален
для «окружающей» качки. Минусы: только «болтанка», не структурное движение,
нет точного контроля позы.

### 4.4 Кто что использует (сводка) и КАК несёт анимацию

| Что анимируем | Правильный подход | Как хранится |
|---|---|---|
| Персонаж/существо/NPC | скелет + граф состояний | кости+треки, blend tree |
| Дверь/рычаг/механизм | скелет или простой трек | 1-2 трека трансформа |
| Знамя/плащ/ткань/цепь | VAT (луп) | position/normal-текстура |
| Водопад-поверхность, всплеск | VAT (fluid-режим) | position-текстура |
| Дерево/куст/трава — качка | **процедурный shader-wind** | только параметры |
| Дерево — детальная иерархия веток | Pivot-Painter-стиль | pivot в vertex-атрибутах |
| Мимика/фазы | blend shapes / morph | морф-цели меша |

**Копировать нам:** трёхъярусную схему как явный контракт формата `.dfo`:
1. **Shader-wind по умолчанию** для всей флоры (трава/листья/кусты/деревья-качка)
   — храним ТОЛЬКО параметры ветра в «рецепте» объекта (stiffness, amplitude,
   phase-seed), глобальный вектор ветра — uniform, фаза per-instance из world-pos.
   Ноль байт анимационных данных, идеально ложится на bake-only и пиксель-арт.
2. **VAT** для сложного лупового вершинного движения, которое шейдер не выразит
   (знамёна, ткань, поверхность водопада, всплески) — секция `VANM`: position(+normal)-
   текстура, bounds-таблица, fps/кадры. Печём в оффлайне (наш конёк).
3. **Скелет** только для артикулированных существ/NPC/механизмов — секция `SKEL`
   (иерархия костей) + `ACLP` (треки ключевых кадров) + мелкий граф состояний.
Каждый ярус несёт объект **сам** (секции внутри `.dfo`); «какая анимация при
каких условиях» — маленький **state-machine/рецепт** в объекте, параметры от
геймплея (как AnimBP/AnimationTree, но урезанный).

**Не копировать:** тяжёлые редакторные графы (полный AnimatorController/AnimGraph
с визуальным редактором) — нам хватит компактного data-driven FSM; ретаргет-
аватары (один арт-стиль, ретаргет не нужен); Nanite-связанные ограничения WPO.

**Развилки для пользователя (анимация):**
- **A. Персонажи:** (a) **скелет 3D** (гибко, но пайплайн) vs (b) **спрайт/
  billboard-флипбук** — аутентично оригинальному Daggerfall, дёшево, ложится в
  пиксель-арт vs (c) **VAT-запечённые циклы** (GPU, без костей, но луп и память).
- **B. Деревья:** (a) **чистый shader-wind** (дёшево, качка «целиком») vs
  (b) **Pivot-Painter-иерархия** (ветки крутятся вокруг своих осей — нужно печь
  pivot в вершинные атрибуты) vs (c) **VAT** (лучшая картинка, самая память).
- **C. Точность VAT:** 8-бит+remap (мало памяти, риск дрожи) vs 16-бит EXR.

---

## 5. Вода — реки и водопады

Ключевой приём везде один: **flow map** — текстура, где **RG-каналы кодируют 2D-
вектор течения** (направление+сила). В фрагментном шейдере flow-вектор (распакованный `rg*2-1`)
**искажает UV/скроллит нормали**, вода «обтекает» камни. Без коррекции
`uv += flow*time` бесконечно растягивает текстуру → **cyclic flow-map blend**
(Vlachos, Valve, SIGGRAPH 2010): два сэмпла с фазами, сдвинутыми на 0.5
(`phase1 = phase0+0.5`, обе wrap %1), кросс-фейд треугольной волной
`blend = 2*abs(phase0-0.5)` — пока одна сбрасывается, вторая ведёт. Плюс
per-pixel `cycleOffset` из шум-карты, чтобы соседние тексели сбрасывались не
синхронно (иначе видна «пульсация»). Всё в фрагментном шейдере.

- **Unity:** URP/HDRP **Water** система; исторически Standard Assets water с flow.
  Flow-map (RG), скролл нормалей, пена.
- **Unreal 5:** **Water plugin** — **сплайновые water bodies** (river/lake/ocean);
  река = сплайн, вдоль него **flow**; океан — **Gerstner waves**; пена на порогах/
  берегах. Данные потока — из сплайна/покрашенной карты.
- **Godot 4:** встроенной воды **нет**; комьюнити-шейдеры с flow-map, скроллом
  нормалей, пеной.
- **Bevy:** встроенной воды нет; кастомные шейдеры/крейты **[?]**.

Реки: течение вдоль **сплайна/покрашенного поля**, скролл нормалей, скорость =
магнитуда flow. Водопады: **панорамирование UV вниз** + партиклы + пена у
основания. **Пена**: (1) **intersection foam** у берегов/камней — по **разнице
глубины** (depth буфер: где вода близко к твёрдому — белим); (2) **white-water**
у порогов — где **flow быстрый** или у краёв сплайна.

**Где данные потока «запекаются»:** во **flow-map текстуру** — красится в
редакторе или **генерится из сплайна реки / уклона террейна** оффлайн.

**Копировать:** flow-map (RG) как **запечённую секцию воды в карте/чанке**
(генерим из русла реки на bake-этапе — у нас уже есть RIVER_RESEARCH и террейн);
cyclic-blend против шва; пену по **разнице глубины** (intersection foam:
`SceneDepth − surfaceDepth < порог` → пена; **требует, чтобы рендер отдавал depth
в водный проход** — одна эта фича открывает и береговую пену, и пену вокруг камней
без запекания геометрии; проверить у render); водопад = панорама UV вниз + пена у
основания. **Не копировать:** Gerstner-океан и
тяжёлые симуляции (нам нужны реки/водопады конечного мира, не динамический океан);
UE Water plugin как рантайм-сплайн-систему — у нас сплайн живёт только в bake,
в игру попадает готовая flow-map.

---

## 6. Запечённые уровни vs генерация

Единый паттерн: **файл уровня хранит не геометрию, а список ссылок на ассеты +
трансформы** (+ параметры экземпляров, свет, навмеш). Меши лежат **отдельными**
ассетами и переиспользуются.

- **Unity:** сцена **`.unity`** (YAML) — GameObject-ы/PrefabInstance-ы, ссылки
  `{fileID, guid}` + Transform; меши — отдельные ассеты. Static batching,
  запечённые lightmaps лежат рядом.
- **Unreal:** уровень **`.umap`** — **Actors с трансформами**, ссылающиеся на
  StaticMesh **по пути**; сам меш — отдельный `.uasset`. **World Partition /
  Level Streaming** существуют, но суть та же: уровень = файл ссылок+трансформов.
- **Godot:** **`.tscn`** — дерево узлов с **`[ext_resource]`** (ссылки на сцены/
  меши по path/uid) + трансформы; меши отдельно (или изредка embedded).
- **Bevy:** **`.scn.ron`** — сущности и их компоненты через reflection, включая
  `Transform` и `Handle`-ссылки на ассеты по пути; glTF как источник сцены.

Для **конечного сюжетного мира** нам интересен именно «уровень как файл ссылок» —
и это ровно то, что уже делает **`.dfw`**: INFO + per-chunk CHNK/ENTS секции.

**Копировать:** уровень = **массив (object_hash, transform, per-instance params)**
на чанк; геометрия — в реестре объектов, не в уровне (дедуп, переиспользование);
запечённый свет/навмеш как отдельные секции чанка. Стриминг оставить как есть
(per-chunk секции уже streaming-friendly), но **без World-Partition-сложности** —
мир конечный. **Не копировать:** динамический world-partition/процедурную
подгрузку регионов на лету и рантайм-инстансинг из редактора — у нас всё
предопределено на bake.

---

## 7. РЕКОМЕНДАЦИЯ по НАШЕЙ единице реестра (В5)

**Единица = запечённый объект-контейнер `.dfo`** (Daggerfall N Object), той же
дисциплины, что `.dfw` (magic + версия + тег-секции, Rule 7, миграции с v1):

```
.dfo  (object template в реестре)
  INFO  — идентичность: content-hash, kind, bounds, флаги
  LODS  — N мешей-уровней + пороги screen-size + режим перехода (dither/xfade)
  RCPE  — «рецепт»: материалы, параметры shader-wind, пиновка вариаций
  анимация (0..N из):
    SKEL/ACLP  — скелет + треки (артикулированные существа)
    VANM       — vertex-animation-texture (луповое вершинное движение)
    (wind — данных нет, только параметры в RCPE)
  IMPS  — опц. billboard-импостор (дальний LOD)
```

**Адрес = `ContentHash` (fnv1a64), который уже «keys asset lookups».** Это наш
GUID: отвязан от пути (как Unity GUID / Godot UID / UE FGuid) **и** иммутабелен по
контенту (даёт версионный пиннинг бесплатно, чего у них нет). Дедуп — естественный
(одинаковый контент → один хеш).

**Шаблон vs экземпляр** (как у всех четырёх): реестр хранит **шаблон** (`.dfo`);
**карта хранит экземпляры** = `(object_hash, transform, small params)`, где params
= variant-индекс, tint, wind-phase-seed, occlusion-флаги. Богатый `m_Modifications`
Unity нам не нужен — для конечного мира хватает горсти полей.

**Версии / «latest»** (развилка!): вводим **каталог имён** (name → current hash),
как address→GUID в Addressables:
- **Развилка В5-версии A (пиннинг):** карта хранит **точный content-hash**. Мир
  полностью воспроизводим и иммутабелен; rebake объекта НЕ трогает старые уровни,
  пока не перелинкуешь. Идеально под «конечный сюжетный мир». *(рекомендую по
  умолчанию)*
- **Развилка В5-версии B (latest):** карта хранит **стабильное имя**, хеш
  резолвится на загрузке/линковке. Быстрее итерации, но риск «молчаливого дрейфа»
  мира — против нашей философии Q13.
Компромисс: имена в редакторе (B) для удобства, но **bake карты фиксирует hash**
(A) — на диск уходит пиннинг, в редакторе работаешь по имени.

**Развилка В5-гранулярности:** объект = (a) **один меш+рецепт** (просто, много
записей реестра) vs (b) **составной префаб** (дерево под-объектов со своими
хешами, nested как Unity/Godot — гибко, но нужен сборщик на bake). Рекомендую
**(b) с иммутабельными под-хешами**: композиция на bake, в игру уходит плоский
список инстансов.

---

## 8. Итоговый чек-лист «копировать / не копировать»

**Копировать:**
- Стабильный content-hash как адрес (уже есть) + soft-ссылка (ленивая загрузка по
  хешу) + name→hash каталог для «latest».
- Экземпляр = ссылка + трансформ + минимум параметров; геометрия только в реестре.
- LOD: массив мешей в объекте, выбор по screen-size, переход дизерингом,
  billboard-импостор как дальний уровень.
- Анимация: 3 яруса (shader-wind по умолчанию / VAT для лупов / скелет для
  существ), объект несёт свои секции, мини-FSM в рецепте.
- Вода: запечённая flow-map (RG) из русла реки, cyclic-blend, пена по глубине.
- Уровень = список (hash, transform) на чанк, свет/навмеш отдельными секциями.

**Не копировать:**
- Текстовую/YAML редакторную сериализацию (у нас бинарь bake-only).
- Nanite и рантайм-авто-LOD (низкополи, конечный мир — проблемы нет).
- «Ссылка = всегда latest» без пиннинга (у нас hash = версия).
- Полновесные визуальные граф-редакторы анимации, ретаргет-аватары.
- Gerstner-океан / рантайм water-plugin со сплайнами; World Partition-динамику.

---

## Источники (проверены под-агентами при разведке)

**Unity.** AssetMetadata/GUID: docs.unity3d.com/6000.3/Documentation/Manual/AssetMetadata.html;
GUID-механика: boristhebrave.com/2020/02/05/messing-with-unitys-guids/;
PrefabInstanceOverrides, NestedPrefabs, PrefabVariants (docs.unity3d.com/Manual/…);
Addressables asset-reference-intro (…/com.unity.addressables@2.0/…);
class-LODGroup + lod-transitions-lod-group; LODFadeMode.SpeedTree;
class-AnimationClip / class-AnimatorController / class-BlendTree / Retargeting;
GPUInstancing; SpeedTree8 Shader Graph sub-graph; HDRP Water: unity.com/blog/…/new-hdrp-water-system…,
…/high-definition@15.0/manual/WaterSystem-currentmap.html; FormatDescription (.unity YAML), Lightmappers.
Amplify Impostors: wiki.amplify.pt / 80.lv/articles/new-optimization-solution-amplify-impostors.

**Unreal 5** (dev.epicgames.com/documentation): objects-in-unreal-engine,
working-with-assets, referencing-assets (soft/hard), asset-registry, data-assets,
asset-management; creating-and-using-lods, optimizing-lod-screen-size-per-platform,
nanite-virtualized-geometry + nanite-technical-details + nanite-foliage;
impostor-baker-plugin + shaderbits.com/blog/octahedral-impostors;
skeletal-mesh-animation-system, animation-blueprints, world-position-offset-material-functions,
pivot-painter-tool-2.0; VAT: sidefx.com/tutorials/vertex-animation-textures-for-unreal,
80.lv/articles/testing-out-houdini-s-vertex-animation-textures-in-ue5;
instanced-static-mesh-component; water-system / water-body-actors / water-meshing-system;
world-partition (+ OFPA / Data Layers).

**Godot 4** (docs.godotengine.org/en/stable): tutorials/scripting/resources,
classes/class_resourceuid, getting_started/…/instancing, classes/class_packedscene
(GenEditState/editable_instances), tutorials/3d/visibility_ranges + mesh_lod,
tutorials/animation/animation_tree, classes/class_meshinstance3d (blend shapes),
classes/class_multimesh + tutorials/3d/using_multi_mesh_instance,
shaders/shader_reference/spatial_shader (VERTEX/TIME/INSTANCE_CUSTOM),
engine_details/file_formats/tscn, tutorials/io/background_loading; вода — github.com/Arnklit/Waterways,
godotshaders.com.

**Bevy** (0.14–0.16; docs.rs/bevy, bevy.org/news): AssetServer/Handle/AssetId/AssetPath,
gltf labels (bevy-cheatbook.github.io/3d/gltf.html), DynamicScene/SceneRoot (bevy 0.15),
BSN discussion #14437 / PR #23413 / bevy.org/news/bevy-0-19, VisibilityRange
(docs.rs/…/VisibilityRange + PR #16468) + meshlets (jms55.github.io/posts/2024-06-09-…),
AnimationGraph (bevy.org/learn/migration-guides/0-13-to-0-14, examples/animation/morph-targets),
grass: warbler_grass / bevy_procedural_grass; вода: crates.io/crates/bevy_water.

**Техники.** VAT: sidefx.com/docs/houdini/nodes/out/labs--vertex_animation_textures-3.0.html,
github.com/keijiro/HdrpVatExample. Ветер: GPU Gems 3 ch.6 (procedural wind) + ch.4
(SpeedTree), docs.speedtree.com/…/advancewind; Ghost of Tsushima — GDC Vault
1027033 (Procedural Grass) + «Simulating Wind», tigerabrodi.blog/grass-in-ghost-of-tsushima;
Horizon — GDC Vault 1025530 (Between Tech and Art). Вода: Vlachos «Water Flow in
Portal 2» (SIGGRAPH 2010 PDF), mtnphil.wordpress.com/2012/08/25/water-flow-shader/,
cyanilux.com (waterfall + shoreline breakdowns), roystan.net/articles/toon-water.
