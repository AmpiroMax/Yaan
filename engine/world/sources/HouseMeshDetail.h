/*
Created: 21:08:2026 - 00:40:00
Last updated: 28:08:2026 - 18:31:38
Module: engine/world
File: engine/world/sources/HouseMeshDetail.h

Responsibility:
- ВНУТРЕННИЙ СЛОВАРЬ ПОСТРОИТЕЛЯ МЕША: MeshBuilder и руки, которые делят
  между собой модули-алгоритмы (по файлу на алгоритм, решение пользователя
  21.08). НЕ для потребителей мира: снаружи есть HouseMesh.h.

Key items:
- MeshBuilder: плоское затенение + под-части с материалом (кладка).
- push_prism / profile_ring / polygon_area_2d / clip_contour_to_rect /
  stable_reference_axis / course_jitter: общие руки модулей.
- Декларации построителей элементов: line/plate/chain/stairs/parquet/roof.

Dependencies:
- Uses: HouseMesh.h, HouseGraph.h.
- Used by: House*.cpp модули постройки; никем снаружи engine/world.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ДОБАВЛЯЕШЬ АЛГОРИТМ — НОВЫЙ ФАЙЛ, а не рост чужого; сюда — только то, что
  реально делят двое и больше.
*/
/*
UPD:
- 21:08:2026 - 00:40:00: Создан при разрезе HouseMesh.cpp на модули.
- 21:08:2026 - 01:50:00: MeshBuilder.collider — косметический слой без физического тела (плашка подтёка перегородила судью).
- 21:08:2026 - 14:35:00: MeshBuilder.collider_only - флаг проносится в
  MeshPart при flush_part (пандус лестниц: физика без картинки).
- 23:08:2026 - 05:20:00: push_prism получил uv_shift (сдвиг фазы плитки в метрах; ноль —
- 23:08:2026 - 18:09:35: MeshBuilder.glow — самосвечение элемента переносится в части.
  прежняя рамка бит-в-бит) — против «дощечек-клонов» (владелец 23.08).
- 28:08:2026 - 11:20:00: MeshBuilder.pane — оконный лист переносится в MeshPart
  при flush_part (свет из окна). Порядок «сначала set_material, потом флаг»
  назван в шапке поля: признак читается на закрытии ПРЕДЫДУЩЕЙ части.
- 28:08:2026 - 14:05:00: MeshBuilder.bevel_m — ФАСКА НА РЁБРА ПРИЗМЫ (дефект 3
  ТЗ материалов: «ни одной фаски во всём убранстве и домах, каждое ребро
  идеально острое»). Ноль — прежняя геометрия БИТ-В-БИТ по построению: при
  нулевой ширине push_prism идёт прежней веткой, а не «той же с нулевым
  сдвигом» (правило 47). Поле у ПОСТРОИТЕЛЯ, а не у призмы, по той же причине,
  что collider и glow: фаску задаёт сборка, а все девять модулей-алгоритмов
  зовут push_prism, не зная друг о друге.
- 28:08:2026 - 14:20:00: BevelHold — ширина фаски на время ОДНОЙ руки
  (ряды кладки и кровли). Возврат в деструкторе, потому что рука выходит из
  середины по нескольким веткам, а элемент после неё продолжает строиться.
- 28:08:2026 - 18:31:38: HouseLodCut и house_lod_simplify — объявление рядом с
  построителями (определение в HouseLod.cpp): дальние ступени пекутся ТЕМИ ЖЕ
  build_line/build_chain_surface/build_contour_surface из переписанного рецепта.
*/

#pragma once

#include "engine/world/sources/HouseMesh.h"

#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace dfn::world {

inline constexpr float HOUSE_PI_F = 3.14159265358979323846f;
inline constexpr float HOUSE_DEG2RAD = HOUSE_PI_F / 180.0f;

/// Плоское затенение: у каждой грани свои вершины. Дороже по памяти и честнее
/// по виду — сруб из восьмигранных брёвен обязан читаться гранями, а не
/// мыльным цилиндром, и это же снимает вопрос «чью нормаль усреднять на ребре».
struct MeshBuilder {
    HouseMesh* out = nullptr;

    // -- ПОД-ЧАСТИ С МАТЕРИАЛОМ (кладка, 20.08). Стена собирается ИЗ КУСКОВ:
    // доски фахверка — брус, кирпичи — глина, блоки — камень, а элемент один.
    // Границы частей режутся сменой материала; думает о них только кладка,
    // остальной код зовёт begin/end и живёт как раньше.
    ElementId part_element = NO_ELEMENT;
    std::uint32_t part_begin = 0;
    int part_mat = -1;
    int part_tone = -1;
    /// КОСМЕТИКА БЕЗ КОЛЛАЙДЕРА (21.08): подтёки и трещины тоньше сантиметра
    /// физике не принадлежат — плашка подтёка в дверном проёме перегородила
    /// судью проходимости. Слой выключает на время своих плашек.
    bool collider = true;
    /// ФИЗИКА БЕЗ КАРТИНКИ: часть уходит в parts с пометкой collider_only.
    bool collider_only = false;
    /// САМОСВЕЧЕНИЕ ЭЛЕМЕНТА (glow=1): все его части эмиссивны.
    bool glow = false;
    /// ОСТЕКЛЕНИЕ: следующая часть — лист окна (MeshPart.pane).
    ///
    /// ПОРЯДОК ВЫСТАВЛЕНИЯ НЕ ПРОИЗВОЛЕН, и ошибка здесь молчалива. Признак
    /// читается в flush_part(), то есть в момент, когда закрывается ПРЕДЫДУЩАЯ
    /// часть: поднять флаг НАДО ПОСЛЕ set_material(8), которая эту предыдущую
    /// часть и закрывает, — иначе оконным листом окажется помечен простенок.
    bool pane = false;
    /// ШИРИНА ФАСКИ, МЕТРЫ (отступ грани от прежнего ребра). 0 — фаски нет и
    /// геометрия прежняя бит-в-бит. Читает push_prism, и только он: фаска —
    /// свойство ТЕЛА, а тела рождаются там.
    float bevel_m = 0.0f;

    void begin_element(ElementId id) {
        part_element = id;
        part_begin = static_cast<std::uint32_t>(out->indices.size());
        part_mat = -1;
        part_tone = -1;
    }
    void flush_part() {
        const std::uint32_t now = static_cast<std::uint32_t>(out->indices.size());
        if (part_element != NO_ELEMENT && now > part_begin) {
            out->parts.push_back({part_element, part_begin, now - part_begin, part_mat,
                                  part_tone, collider_only, glow, pane});
        }
        part_begin = now;
    }
    /// Дальше идёт геометрия ЭТОГО материала (-1 — материал элемента).
    void set_material(int mat, int tone) {
        if (mat == part_mat && tone == part_tone) {
            return;
        }
        flush_part();
        part_mat = mat;
        part_tone = tone;
    }
    void end_element() {
        flush_part();
        part_element = NO_ELEMENT;
    }

    void push_triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, const UvFrame& uv) {
        const glm::vec3 raw = glm::cross(b - a, c - a);
        const float len = glm::length(raw);
        if (len < HOUSE_GEOM_EPS) {
            return; // вырожденный треугольник в буфер не попадает
        }
        const glm::vec3 n = raw / len;
        const std::uint32_t base = static_cast<std::uint32_t>(out->vertices.size());
        out->vertices.push_back({a, n, uv.at(a)});
        out->vertices.push_back({b, n, uv.at(b)});
        out->vertices.push_back({c, n, uv.at(c)});
        out->indices.push_back(base);
        out->indices.push_back(base + 1);
        out->indices.push_back(base + 2);
    }

    /// Четырёхугольник, обходимый снаружи против часовой стрелки.
    void push_quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, const UvFrame& uv) {
        push_triangle(a, b, c, uv);
        push_triangle(a, c, d, uv);
    }
};



/// ШИРИНА ФАСКИ НА ВРЕМЯ ОДНОЙ РУКИ. Строитель один на весь дом, а рука ряда
/// кладки меняет его ширину только для СВОИХ кусков: без возврата следующий же
/// элемент дома остался бы без фаски, и беда была бы молчаливой — лишний
/// острый угол видно только на кадре с метра.
struct BevelHold {
    MeshBuilder& mb;
    float saved;
    BevelHold(MeshBuilder& b, float now) : mb(b), saved(b.bevel_m) { mb.bevel_m = now; }
    BevelHold(const BevelHold&) = delete;
    BevelHold& operator=(const BevelHold&) = delete;
    ~BevelHold() { mb.bevel_m = saved; }
};

/// Площадь 2D-контура со знаком (CCW > 0).
[[nodiscard]] float polygon_area_2d(std::span<const glm::vec2> poly);

/// Ось, заведомо не параллельная данной; выбор детерминирован.
[[nodiscard]] glm::vec3 stable_reference_axis(glm::vec3 dir);

/// Клип невыпуклого контура прямоугольником (Сазерленд–Ходжман).
[[nodiscard]] std::vector<glm::vec2> clip_contour_to_rect(
    std::span<const glm::vec2> poly, glm::vec2 lo, glm::vec2 hi);

/// Кольцо профиля вокруг оси; CCW в базисе (u,v).
[[nodiscard]] std::vector<glm::vec3> profile_ring(glm::vec3 center, glm::vec3 axis_u,
                                                  glm::vec3 axis_v, float radius,
                                                  int sides);

/// Призма по кольцу: крышка, днище, бока и выпуклый кусок в коллайдер.
/// `uv_shift` — сдвиг фазы текстуры в МЕТРАХ по осям рамки (u, v). Ноль —
/// прежняя рамка бит-в-бит. Заведён против «дощечек-клонов» (владелец 23.08):
/// рамка uv центрируется на грани, и каждая доска пола сэмплила плитку с
/// ОДНОЙ фазы — пол читался копиями одной дощечки.
void push_prism(MeshBuilder& mb, std::span<const glm::vec3> loop,
                std::span<const std::uint32_t> tris, glm::vec3 axis, float tex_deg,
                HouseMesh& mesh, ElementId owner, glm::vec2 uv_shift = {});

/// Детерминированная дрожь куска (ряд, колонка) -> 0..1.
[[nodiscard]] float course_jitter(int row, int col);

/// ЧТО СТУПЕНЬ СРЕЗАЛА У ЭТОГО ЭЛЕМЕНТА (И13). Материал возвращается наружу,
/// потому что срезанный рядный слой обязан отдать свой материал ПЛАСТИНЕ:
/// кирпич живёт в кусках кладки, а пластина под ними носит материал элемента,
/// и без передачи дальний кирпичный дом стал бы штукатурным.
struct HouseLodCut {
    int plate_mat = -1;  ///< -1 — материал элемента, как было
    int plate_tone = -1;
};

/// ПЕРЕПИСЫВАЕТ РЕЦЕПТ ЭЛЕМЕНТА ПОД СТУПЕНЬ. Определена в HouseLod.cpp рядом
/// с порогами: список срезаемого и расстояние, на котором его срезают, — одно
/// решение, и жить они обязаны на одном экране.
[[nodiscard]] HouseLodCut house_lod_simplify(HouseLod lod, ElementParams& p);

// -- построители элементов (по файлу на алгоритм) ---------------------------
void build_line(const HouseGraph& g, const Element& e, const ElementParams& p,
                MeshBuilder& mb, HouseMesh& mesh);
void build_contour_surface(const Element& e, const ElementParams& p,
                           std::span<const glm::vec3> pts, MeshBuilder& mb,
                           HouseMesh& mesh);
void build_chain_surface(const Element& e, const ElementParams& p,
                         std::span<const glm::vec3> pts, MeshBuilder& mb,
                         HouseMesh& mesh);
void build_stairs(const Element& e, const ElementParams& p, glm::vec3 a, glm::vec3 b,
                  float half_w, MeshBuilder& mb, HouseMesh& mesh);
void build_stairs_contour(const Element& e, const ElementParams& p,
                          std::span<const glm::vec3> pts, MeshBuilder& mb,
                          HouseMesh& mesh);
void build_parquet(const Element& e, const ElementParams& p, const FittedPlane& plane,
                   std::span<const glm::vec2> flat, const glm::vec3& ax,
                   const glm::vec3& ay, MeshBuilder& mb, HouseMesh& mesh);
void build_roof_courses(const Element& e, const ElementParams& p,
                        const FittedPlane& plane, std::span<const glm::vec2> flat,
                        const glm::vec3& ax, const glm::vec3& ay, MeshBuilder& mb,
                        HouseMesh& mesh);

} // namespace dfn::world
