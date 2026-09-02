/*
Module: engine/render
File: engine/render/sources/ObjectRegistry.h

Responsibility:
- The OBJECT REGISTRY's file format (.dfo) and its read/write API: a baked,
  named render object — mesh streams plus identity — written offline by a
  forge tool and only READ by the game (в1: nothing is generated in the frame).
  This is the third tool of the tooling pivot, after the map browser and the
  world baker.

Key items:
- RegistryObject: name/kind/source + the three mesh streams + content hash.
- MorphTarget / MorphDelta: один ползунок редактора персонажа — имя, полоса и
  разреженная дельта на вершинах SKIN (секция MORF).
- TextureRef: лист объекта по ССЫЛКЕ — роль, путь от корня репозитория,
  SHA-256 содержимого PNG, цветовое пространство, повтор (секция TEX).
- write_object() / read_object(): one object per .dfo file.
- object_content_hash(): the FROZEN fnv1a64 identity of the payload.

Dependencies:
- Uses: ProcFlora.h (MeshData streams), core serialization (BinaryWriter/
  BinaryReader, Fnv1a64).
- Used by: tools/forge_trees.cpp (writer), engine/app gallery loading (reader),
  tests/render/ObjectRegistryTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Rule 7 in full: explicit little-endian through BinaryWriter/Reader, unknown
  sections skipped, never a struct memcpy.
- THE CONTENT HASH IS THE OBJECT'S IDENTITY and fnv1a64 is frozen forever
  (user-ratified). Two objects with equal hashes are THE SAME OBJECT; a tool
  that re-bakes an unchanged tree must produce an unchanged hash, which is why
  the hash covers the PAYLOAD (streams, in order) and not the name — renaming
  a file must not re-version every reference to its content.
- A corrupt or truncated file fails SOFT (nullopt), never half-loads: an
  object missing its wood stream is not a lighter object, it is a different
  one wearing the same name.
*/

#pragma once

#include "engine/core/skeleton/sources/Skeleton.h" // skel::Skeleton, skel::AnimClip
#include "engine/render/sources/ProcFlora.h" // MeshData

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::render {

/// ОДИН КУСОК ЗАПЕЧЁННОЙ ПОСТРОЙКИ ВНУТРИ ОБЪЕКТА: треугольники плюс НОМЕР
/// ПЛИТКИ листа набора, которой они кроются (колонка — поверхность, ряд —
/// тон/износ; PartsAtlas.h). Куски разделены ровно по плитке и ни по чему
/// больше: у кровати это брус царг, доска настила и доска изголовья — три
/// куска на весь предмет.
///
/// ПОЧЕМУ ЭТО НЕ ПЯТЫЙ MeshData, А СПИСОК. Потоки .dfo — это ПРОГРАММЫ
/// отрисовки (prop, foliage), и их конечное число. Плитка — это МАТЕРИАЛ, и
/// пар (поверхность, тон) тридцать шесть; складывать их в фиксированные поля
/// значило бы завести тридцать шесть потоков, из которых у любого предмета
/// заняты два-три.
struct HouseSubmesh {
    std::uint32_t surface = 0; ///< PartSurface ordinal (колонка листа)
    std::uint32_t tone = 0;    ///< PartTone ordinal (ряд листа)
    /// САМОСВЕТНЫЙ КУСОК (glow=1 рецепта): рисуется без освещения, как пламя
    /// очага. Флаг живёт у куска, а не у объекта: у жаровни светится огонь, а
    /// не её ножки.
    bool emissive = false;
    /// ИМЯ ВЕЩЕСТВА (.dfo v4, секция MTRL) — "oak-log", "brick-red". Пустое —
    /// «вещество не названо», и это ровно поведение v3: кусок кроется своей
    /// парой (surface, tone) и красится, как красился до материалов.
    ///
    /// ПОЧЕМУ ИМЯ, А НЕ НОМЕР ЗАПИСИ РЕЕСТРА. Номер — идентичность внутри
    /// процесса; на диске он был бы третьей копией порядка таблицы, и вставка
    /// вещества в её середину перекрасила бы всю полку молча. Имя переживает
    /// любую перекладку реестра И любую перекладку листа набора — то самое
    /// свойство, которого не хватало запечённым ординалам плитки.
    std::string material;
    /// МЕТРОВ ПОВЕРХНОСТИ НА ПОВТОР, если кусок хочет не тот шаг, что у
    /// вещества; 0 — штатный шаг вещества. Поле здесь потому, что до v4 шаг
    /// повтора был ЗАПЕЧЁН В UV на выпечке (PartSkin::span_m потребляется
    /// HewnBar.h) и в файл не доезжал вовсе: сменить тайлинг значило перепечь
    /// геометрию, и потому соломенная кровля повторялась с шагом дощатой
    /// стены и читалась ковром.
    float span_m = 0.0f;
    /// ИЗНОС КУСКА, если он не штатный износ вещества; -1 — штатный.
    float wear = -1.0f;
    MeshData mesh;
};

/// A SKINNED mesh stream (.dfo v5, character wave 30.08): the same triangles
/// every other stream carries, on the SkinnedVertex layout -- four palette
/// slots and four weights per vertex.
///
/// ITS OWN STRUCT AND ITS OWN SECTION, not a fifth MeshData, because the
/// VERTEX is a different shape. The existing streams are all
/// platform::Vertex and differ only in which PROGRAM draws them (the argument
/// written at HouseSubmesh); this one differs in what a vertex IS, which is
/// the one difference a stream cannot absorb.
struct SkinMesh {
    std::vector<platform::SkinnedVertex> vertices;
    std::vector<uint32_t> indices;

    [[nodiscard]] bool empty() const { return vertices.empty() || indices.empty(); }
};

/// ОДНА СДВИНУТАЯ ВЕРШИНА МОРФ-ЦЕЛИ (.dfo, секция MORF): её номер в потоке
/// SKIN и сдвиг в МЕТРАХ, в той же системе координат, в которой лежит сама
/// вершина, — то есть в рест-позе модели, ДО скиннинга.
///
/// РАЗРЕЖЕННОСТЬ — СВОЙСТВО ЯВЛЕНИЯ, А НЕ ЭКОНОМИЯ. Цель «живот» трогает
/// восьмую часть тела; плотный массив на 8546 вершин хранил бы нули там, где
/// цель по построению ничего не говорит, и стирал бы разницу между «эта
/// вершина не двигается» и «эта вершина двигается на ноль» — а именно на этой
/// разнице стоит приёмка «дельта не течёт из региона».
struct MorphDelta {
    std::uint32_t index = 0;
    glm::vec3 offset{0.0f};
};

/// ОДИН ПОЛЗУНОК РЕДАКТОРА ПЕРСОНАЖА: имя, полоса допустимых значений и
/// разреженная дельта на вершинах ЭТОГО скина.
///
/// ПОЛОСА ЛЕЖИТ В ФАЙЛЕ, А НЕ В КОДЕ, потому что она — свойство ЦЕЛИ, а не
/// интерфейса: «рост» ходит в узкой полосе канона пропорций, «живот» — от нуля
/// до единицы, и ползунок, который умеет вывести тело за канон, — это ползунок,
/// после которого судья dfn_human_scale обязан краснеть. Кто печёт цель, тот и
/// знает, докуда её можно тянуть.
///
/// НОРМАЛЕЙ ЗДЕСЬ НЕТ НАРОЧНО. Пересчёт нормалей по сдвинутой геометрии — та же
/// работа, что и сам бленд, и делается он раз на движение ползунка, а не раз в
/// кадр; хранить вторую дельту значило бы удвоить файл ради работы, которая уже
/// оплачена (docs/research/CHARACTER_EDITOR_TOOLS.md §3б).
struct MorphTarget {
    std::string name;   ///< "belly", "shoulders" — имя ползунка, оно же ключ пресета
    float lo = 0.0f;    ///< нижний конец полосы (значение ползунка, не метры)
    float hi = 1.0f;    ///< верхний конец полосы
    std::vector<MorphDelta> deltas;
};

/// ОДНА ТЕКСТУРА ОБЪЕКТА ПО ССЫЛКЕ (.dfo, секция TEX; волна «текстура на
/// скиннинге»). Пиксели в файл объекта НЕ входят: они лежат PNG-файлом в
/// дереве (assets/objects/characters/textures/<тело>/<роль>.png), а здесь —
/// роль, путь от корня репозитория, SHA-256 содержимого, цветовое
/// пространство и режим повтора.
///
/// ПОЧЕМУ ССЫЛКА, А НЕ ПИКСЕЛИ (решение координатора 02.09). Художник
/// итерирует кожу в PNG и смотрит результат, не перепекая тело; тот же PNG
/// разделяют несколько тел (народ — один альбедо на десятки лиц); и файл
/// объекта остаётся мегабайтами геометрии, а не десятками мегабайт пикселей.
///
/// SHA-256 — ЛИЧНОСТЬ ФАЙЛА, КОТОРЫЙ ВИДЕЛА ВЫПЕЧКА. Загрузчик пересчитывает
/// его над тем PNG, что нашёл, и несовпадение — отказ текстуры вслух (тело
/// рисуется палитрой), а не «почти та кожа»: та же дисциплина, что у хэша
/// содержимого самого объекта. Стандартный алгоритм, а не fnv — чтобы число
/// сходилось с SHA256SUMS автора и с `shasum -a 256` на глаз.
struct TextureRef {
    /// Роль листа: "albedo", "normal", "roughness"… Строка, а не перечисление:
    /// набор ролей растёт с материалами, а файл — нет.
    std::string role;
    /// Путь от корня репозитория, прямыми косыми: "assets/objects/characters/
    /// textures/HumanBase/albedo.png". Загрузчик решает его от текущего
    /// каталога (игра запускается из корня), затем от каталога .dfo вверх.
    std::string path;
    /// 64 шестнадцатеричных знака нижним регистром.
    std::string sha256;
    /// 0 — sRGB (альбедо), 1 — линейное (нормали, шероховатость).
    std::uint8_t colour_space = 0;
    /// 0 — повтор (repeat), 1 — зажим к краю (clamp).
    std::uint8_t wrap = 0;
};

inline constexpr std::uint8_t TEXTURE_COLOUR_SRGB = 0;
inline constexpr std::uint8_t TEXTURE_COLOUR_LINEAR = 1;
inline constexpr std::uint8_t TEXTURE_WRAP_REPEAT = 0;
inline constexpr std::uint8_t TEXTURE_WRAP_CLAMP = 1;

/// One baked object of the registry. The three streams mirror FloraMesh on
/// purpose: `wood` draws with the "prop" program, `cards` with "foliage" plus
/// the leaf atlas, `ground` draws with the wood and never reaches collision —
/// an object that needs different streams is a different KIND, not a fourth
/// vector on this one.
struct RegistryObject {
    std::string name;   ///< human handle, e.g. "oak-forge-v1-a" (not identity)
    std::string kind;   ///< "tree" today; the registry is not tree-shaped
    std::string source; ///< what produced it, e.g. "forge:oak seed=3" (provenance)
    MeshData wood;
    MeshData cards;
    MeshData ground;
    /// TEXTURED wood: trunk and heavy limbs with bark UVs into the leaf
    /// atlas' BarkPlate column. Drawn with the foliage program (albedo from
    /// the texture, real lighting), wind zeroed — a separate stream because
    /// the plain "prop" wood has no UVs and collision reads neither.
    MeshData bark;
    /// КУСКИ ЗАПЕЧЁННОЙ ПОСТРОЙКИ (v3). Пусто у всего, что испекли кузницы
    /// деревьев, набора и табличек, — и ровно поэтому их файлы не изменились
    /// ни на бит: пустой список в личность объекта не входит.
    ///
    /// НЕПУСТОЙ СПИСОК — ЭТО ДРУГОЙ СПОСОБ РИСОВАНИЯ, а не добавка к прежним.
    /// Объект с ним едет в потоки построек (те же плитки, тот же свет, тот же
    /// коллайдер), а не в россыпь сцены, и мешать одно с другим нельзя: одна
    /// поверхность, нарисованная дважды, воюет сама с собой за глубину.
    std::vector<HouseSubmesh> house;
    /// СКИНИРОВАННЫЙ ПЕРСОНАЖ (v5): вершины с весами, скелет и клипы. Пусты у
    /// всего, что испекли кузницы деревьев, набора, табличек и мебели, — и
    /// ровно поэтому их файлы не изменились ни на бит: пустой скин в личность
    /// объекта не входит (тот же довод, что у HOUS и MTRL).
    ///
    /// ТРИ ПОЛЯ, А НЕ ОДНО, потому что три вопроса раздельны: чем рисовать
    /// (skin), по какой иерархии гнуть (skeleton) и что играть (clips).
    /// Модель без клипов законна (её гнёт наша процедурная походка), скелет
    /// без меша — тоже (референс движения). Склеенные в один тип, они
    /// заставляли бы импортёр выдумывать недостающие два.
    SkinMesh skin;
    skel::Skeleton skeleton;
    std::vector<skel::AnimClip> clips;
    /// МОРФ-ЦЕЛИ ПЕРСОНАЖА (секция MORF). Пусты у всего, что не персонаж, и у
    /// персонажа, уже ВЫПЕЧЕННОГО с применёнными ползунками: пустой список в
    /// личность объекта не входит — четвёртый случай того же довода, что у
    /// HOUS, MTRL и SKIN.
    ///
    /// ПОРЯДОК СПИСКА — ЧАСТЬ ФОРМАТА. Бленд складывает дельты в этом порядке,
    /// и сложение float не ассоциативно: переставь цели местами — и тот же
    /// пресет даст другой файл выпечки, то есть приёмка «пресет воспроизводим
    /// байт-в-байт» проверяла бы удачу. Экспортёр пишет цели ОТСОРТИРОВАННЫМИ
    /// ПО ИМЕНИ, читатель порядок файла не трогает.
    std::vector<MorphTarget> morphs;
    /// ТЕКСТУРЫ ОБЪЕКТА ПО ССЫЛКЕ (секция TEX). Пусто у всего, что не носит
    /// листа, — и ровно поэтому их файлы не изменились ни на бит: пустой
    /// список в личность объекта не входит (пятый случай довода HOUS/MTRL/
    /// SKIN/MORF). Непустой — входит ЦЕЛИКОМ, включая sha: тело с другой
    /// кожей — другое тело.
    std::vector<TextureRef> textures;
    /// Первая ссылка названной роли или nullptr.
    [[nodiscard]] const TextureRef* texture(std::string_view role) const;
    /// fnv1a64 over the payload streams (see object_content_hash). Stored in
    /// the file AND recomputed on read; a mismatch is a refused file, because
    /// a registry whose identities cannot be trusted indexes nothing.
    uint64_t content_hash = 0;
};

/// The payload identity: every stream, vertices then indices, in file order.
/// Name/kind/source are NOT hashed — provenance may be re-worded without
/// changing what the object IS.
[[nodiscard]] uint64_t object_content_hash(const RegistryObject& obj);

/// Writes one object as a .dfo (atomic: temp + rename). Computes and stores
/// the content hash; returns false on IO failure or an empty object (an
/// object with no streams at all is a name pointing at nothing, refused).
[[nodiscard]] bool write_object(const RegistryObject& obj,
                                const std::filesystem::path& path);

/// Reads one .dfo. nullopt on bad magic, truncation, unknown newer version,
/// or a content hash that does not match the streams.
[[nodiscard]] std::optional<RegistryObject> read_object(const std::filesystem::path& path);

/// HOW BIG AN OBJECT IS, measured from its own meshes. Never typed in by hand:
/// a size written next to a part is the first thing to go stale when the part
/// is re-forged, and it goes stale silently.
struct ObjectExtent {
    float radius = 0.0f;  ///< horizontal reach from the origin
    float bottom = 0.0f;  ///< lowest vertex, relative to the origin
    float top = 0.0f;     ///< highest — what another part rests on
    /// SOLID GEOMETRY TALLER THAN A STEP. Not "has a wood stream": flora gives
    /// a grass tuft a few-centimetre root nub in that stream so the placer
    /// renders it at all, which once made every blade of grass an obstacle and
    /// buried a report under forty thousand meadow findings. What a walker
    /// steps over without noticing is not in his way.
    bool solid = false;
    glm::vec2 lo{0.0f};   ///< footprint in xz about the origin, ALL streams
    glm::vec2 hi{0.0f};
    glm::vec2 slo{0.0f};  ///< the same for the SOLID streams only (trunks, not
    glm::vec2 shi{0.0f};  ///< crowns: two birches may share their canopies)
};

/// ONE MEASUREMENT FOR EVERYONE (Rule 32). The scene judge, the tools and the
/// editor's build ghost all ask here. A second copy of this scan would drift,
/// and the drift would show as a ghost whose green outline is the wrong size —
/// which reads as the rules being wrong rather than the ruler.
[[nodiscard]] ObjectExtent measure_object(const RegistryObject& obj);

} // namespace dfn::render
