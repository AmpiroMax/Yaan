/*
Module: engine/render
File: engine/render/sources/MorphFollow.h

Responsibility:
- ПЕРЕНОС ДЕЛЬТ ТЕЛА НА ВЕРШИНЫ ЧАСТИ (волна «части следуют морфам»): часть
  (волосы, брови, ресницы, костюм) — свой меш на том же скелете, и цель MORF
  адресует только поток SKIN тела; здесь каждая вершина части пришита к
  ближайшему ТРЕУГОЛЬНИКУ кожи в рест-позе (барицентрика точки под ней плюс
  смещение в рамке треугольника), и куда бы треугольник ни уехал под морфом,
  вершина едет с ним. Отдельно — ЖЁСТКАЯ посадка (глаза, зубы, язык): часть
  едет целиком за центроидом маски вершин тела, не деформируясь.

Key items:
- FollowMap / build_follow_map(): карта «вершина части → k ближайших вершин
  тела (веса по расстоянию) + ближайший треугольник кожи и смещение в его
  рамке», строится ОДИН РАЗ при прикреплении.
- apply_follow(): рест части → вершины части над телом «сейчас»: точка на
  треугольнике по барицентрике + смещение в новой рамке; нормали пересчитаны
  той же разницей, что у blend_morphs. Без треугольника (вырожденная
  окрестность) — взвешенная сумма дельт k соседей.
- RigidFrame / rigid_frame() / apply_rigid(): центроид и радиус маски до и
  после морфа → сдвиг (и масштаб) части как целого.
- follow_gap_change(): прибор — насколько выросло (и сжалось) расстояние
  «вершина части ↔ кожа» против реста; follow_penetrations(): сколько вершин
  части ушло под кожу глубже порога; follow_vertex_gap_error() — то же до
  ближайшей вершины, справочно.

WHY A TRIANGLE FRAME AND NOT AN AVERAGE OF NEIGHBOUR DELTAS. Первая версия
переносила взвешенную сумму дельт четырёх ближайших вершин — и на складках
(подмышка костюма, прядь у уха) четыре соседа лежат по РАЗНЫЕ стороны складки
с разными дельтами: ухо уезжает на 14 мм, прядь над ним — на семь, ухо сквозь
волосы. Треугольник — одна сторона складки по построению, и это ровно то, как
mhclo-прокси MakeHuman сшиты с телом: точка на грани плюс смещение по нормали.

WHY NEAREST-SURFACE TRANSFER AND NOT A SECOND SET OF TARGETS. Целей MPFB для
волос, бровей и костюма нет и не будет: причёсок много, целей 53, и их
произведение — не файл, а фабрика. Перенос делает ЛЮБУЮ часть следующей за
ЛЮБОЙ целью без единой новой дельты в файле; цена — карта в памяти (56 байт на
вершину части) и одна рамка на вершину на движение ручки.

WHY THE MAP IS BUILT IN REST AND NEVER REBUILT. Соседство — свойство геометрии
покоя, а не текущего морфа: перестраивать его по сдвинутому телу значило бы,
что вершина пряди меняет опору, когда лоб растёт, и прядь дрожала бы на
ползунке. Карта — как веса скина: один раз при привязке.

WHY FAR VERTICES DAMP THE FRAME'S ROTATION. Конец длинной пряди в 10 см от
кожи повернулся бы на сантиметр от поворота грани на 6°; смещение длиннее
FOLLOW_FRAME_NEAR_M плавно переходит от «в рамке грани» к «параллельным
переносом» на FOLLOW_FRAME_FAR_M, и длина смещения не меняется никогда.

WHY THE EYE IS RIGID. Яблоко — сфера, и цель «глаз шире» двигает веки, а не
плющит зрачок; перенос сделал бы из сферы яйцо. Центроид маски глаза
(face.masks) до и после — вот куда яблоко едет; радиус маски — на сколько оно
растёт.

Dependencies:
- Uses: platform SkinnedVertex, MorphBlend (shift_normals_by_difference), glm.
- Used by: engine/app (CharacterParts), tests/app.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЭТО ЧИСТЫЕ ФУНКЦИИ. Ни окна, ни рендера, ни файла: одни входы — побитово
  один выход (тот же довод, что у MorphBlend.h).
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dfn::render {

/// СКОЛЬКО ОПОРНЫХ ВЕРШИН ТЕЛА У ВЕРШИНЫ ЧАСТИ. Четыре: их грани — окрестность,
/// в которой ищется ближайший треугольник; их дельты — запасной перенос.
inline constexpr std::uint32_t FOLLOW_K = 4;
/// СТЕПЕНЬ ОБРАТНОГО РАССТОЯНИЯ в весах запасного переноса: w = 1/d^p.
inline constexpr float FOLLOW_WEIGHT_POWER = 2.0f;
/// ПОВОРАЧИВАТЬ ЛИ СМЕЩЕНИЕ С РАМКОЙ ГРАНИ. Выключено по прибору: с поворотом
/// вершина костюма в 3 см от кожи на складке ягодиц уезжает от своей ближайшей
/// вершины тела на 15 мм (buttocks lo), без — на единицы; просвечивание
/// одинаковое. Смещение переносится ПАРАЛЛЕЛЬНО: p' = q' + (p − q).
inline constexpr bool FOLLOW_FRAME_ROTATES = false;
/// При повороте: смещение от кожи, до которого рамка грани поворачивает
/// вершину целиком, и от которого вершина едет параллельным переносом.
inline constexpr float FOLLOW_FRAME_NEAR_M = 0.02f;
inline constexpr float FOLLOW_FRAME_FAR_M = 0.10f;
/// «Нет треугольника» в SurfaceBind::tri.
inline constexpr std::uint32_t FOLLOW_NO_TRIANGLE = 0xFFFFFFFFu;

/// ОПОРА ОДНОЙ ВЕРШИНЫ ЧАСТИ: номера ближайших вершин тела (index[0] —
/// ближайшая) и веса (сумма 1; лишние — вес 0).
struct FollowBind {
    std::uint32_t index[FOLLOW_K] = {0, 0, 0, 0};
    float weight[FOLLOW_K] = {0.0f, 0.0f, 0.0f, 0.0f};
};

/// ШОВ ВЕРШИНЫ ЧАСТИ К КОЖЕ: треугольник тела (номер в списке индексов / 3),
/// барицентрика ближайшей точки на нём (a + u·(b−a) + v·(c−a)) и смещение
/// вершины от этой точки в рамке треугольника (t̂ = вдоль ab, n̂ = нормаль,
/// b̂ = n̂ × t̂), метры.
struct SurfaceBind {
    std::uint32_t tri = FOLLOW_NO_TRIANGLE;
    float u = 0.0f;
    float v = 0.0f;
    glm::vec3 local{0.0f};
};

/// Карта, параллельная вершинам части.
struct FollowMap {
    std::vector<FollowBind> binds;
    std::vector<SurfaceBind> surface;
    [[nodiscard]] bool empty() const { return binds.empty(); }
};

/// СТРОИТ КАРТУ по рест-позам: для каждой вершины `part` — FOLLOW_K ближайших
/// вершин `body` (веса 1/d^power; совпавшая вершина берёт всё) и ближайший
/// треугольник `body_indices` среди граней этих вершин. Сетка ячеек `cell_m`
/// — ускоритель, на результат не влияет (поиск точный). Пустые индексы —
/// карта без швов, перенос запасной.
void build_follow_map(std::span<const platform::SkinnedVertex> body,
                      std::span<const std::uint32_t> body_indices,
                      std::span<const platform::SkinnedVertex> part, FollowMap& out,
                      float power = FOLLOW_WEIGHT_POWER, float cell_m = 0.02f);

/// РЕСТ ЧАСТИ → ЧАСТЬ НАД ТЕЛОМ «СЕЙЧАС». `body_rest` и `body_now` — тело до и
/// после бленда (одна длина, те же `body_indices`); `part_rest` — часть, по
/// которой строилась карта; `indices` — треугольники части, только для
/// нормалей. При body_now == body_rest выход побитово равен part_rest.
void apply_follow(std::span<const platform::SkinnedVertex> body_rest,
                  std::span<const std::uint32_t> body_indices,
                  std::span<const platform::SkinnedVertex> body_now,
                  const FollowMap& map,
                  std::span<const platform::SkinnedVertex> part_rest,
                  std::span<const std::uint32_t> indices,
                  std::vector<platform::SkinnedVertex>& out);

/// ЦЕНТРОИД И СРЕДНЕКВАДРАТИЧНЫЙ РАДИУС МАСКИ вершин тела — рамка жёсткой
/// посадки. Пустая маска — рамка в нуле с радиусом 0 (и apply_rigid тогда
/// ничего не двигает).
struct RigidFrame {
    glm::vec3 centroid{0.0f};
    float radius = 0.0f;
};
[[nodiscard]] RigidFrame rigid_frame(std::span<const platform::SkinnedVertex> body,
                                     std::span<const std::uint32_t> mask);

/// ЧАСТЬ КАК ЦЕЛОЕ, НА МЕСТЕ: p' = now.centroid + s·(p − rest.centroid),
/// s = отношение радиусов при `scale` (иначе 1). `out` уже несёт рест части;
/// `subset` — какие её вершины принадлежат этой рамке (пусто — все: у глаз две
/// рамки на одном меше, у зубов одна). Нормали не трогаются (перенос и
/// равномерный масштаб их не меняют).
void apply_rigid(const RigidFrame& rest, const RigidFrame& now, bool scale,
                 std::span<const std::uint32_t> subset,
                 std::vector<platform::SkinnedVertex>& out);

/// ПРИБОР ПЕРЕНОСА: по каждой вершине части — расстояние до КОЖИ (ближайший
/// треугольник среди вееров её FOLLOW_K опорных вершин; набор кандидатов
/// зафиксирован рестом, правило 47) в ресте и сейчас. `grow_m` — наибольший
/// РОСТ расстояния (часть отстала от кожи: череп сквозь волосы), `shrink_m` —
/// наибольшее СЖАТИЕ (кожа подошла к части: ухо, прижатое ручкой, входит в
/// прядь над ним — это не отставание, а вторая поверхность). Приёмка — по
/// росту; сжатие и число вершин под кожей (follow_penetrations) — справочно.
void follow_gap_change(std::span<const platform::SkinnedVertex> body_rest,
                       std::span<const std::uint32_t> body_indices,
                       std::span<const platform::SkinnedVertex> body_now,
                       const FollowMap& map,
                       std::span<const platform::SkinnedVertex> part_rest,
                       std::span<const platform::SkinnedVertex> part_now, float& grow_m,
                       float& shrink_m);

/// ТОТ ЖЕ ПРИБОР ДО ВЕРШИНЫ: |Δ| расстояния до ближайшей вершины тела
/// (index[0]). Справочный: растяжение грани под смещённой вершиной он читает
/// как ошибку, а у пряди, пришитой у уха, это растяжение и есть (2.7 мм при
/// ear-wing) — потому приёмка идёт по расстоянию до кожи.
[[nodiscard]] float follow_vertex_gap_error(
    std::span<const platform::SkinnedVertex> body_rest,
    std::span<const platform::SkinnedVertex> body_now, const FollowMap& map,
    std::span<const platform::SkinnedVertex> part_rest,
    std::span<const platform::SkinnedVertex> part_now);

/// ПРИБОР ПРОСВЕЧИВАНИЯ: сколько вершин части лежит ПОД кожей глубже
/// `threshold_m` — знаковое расстояние до ближайшего треугольника среди
/// граней её FOLLOW_K опорных вершин (знак — по нормали грани). Зовётся для
/// реста и для «сейчас» одинаково; рост числа против реста и есть «тело
/// проступило сквозь костюм». Знаменатель — все вершины части.
[[nodiscard]] std::size_t follow_penetrations(std::span<const platform::SkinnedVertex> body,
                                              std::span<const std::uint32_t> body_indices,
                                              const FollowMap& map,
                                              std::span<const platform::SkinnedVertex> part,
                                              float threshold_m);

} // namespace dfn::render
