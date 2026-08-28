/*
Created: 28:08:2026 - 13:01:31
Last updated: 28:08:2026 - 13:01:31
Module: engine/app
File: engine/app/sources/PropPhysics.cpp

Responsibility:
- Реализация PropPhysics.h: таблица плотностей, чтение реестра физики, замер
  объёма/площади по треугольникам и вывод массы.

Dependencies:
- Uses: PropPhysics.h, стандартная библиотека, glm.
- Used by: AppProps.cpp, tests/app/PropPhysicsTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона big-grab владеет этим файлом.
- ТАБЛИЦА ПЛОТНОСТЕЙ ВРЕМЕННАЯ И ЗНАЕТ ОБ ЭТОМ. Она переезжает в ось 1
  материалов целиком; до тех пор её единственный потребитель — эта зона.
*/
/*
UPD:
- 28:08:2026 - 13:01:31: Создан вместе с заголовком.
*/

#include "engine/app/sources/PropPhysics.h"

#include "engine/core/materials/sources/PhysicsSubstance.h"

#include <algorithm>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>

namespace dfn::app {

float substance_density(std::string_view substance) {
    // ТАБЛИЦА ЖИВЁТ В ЯДРЕ, А НЕ ЗДЕСЬ (решение координатора 28.08): у
    // вещества пять полей, из которых плотность нужна этой зоне, трение и
    // упругость — физике, а тег поверхности — звуку. Три копии одного списка
    // разошлись бы в первую же неделю.
    const core::SubstanceId id = core::find_substance(substance);
    return id == core::SUBSTANCE_NONE ? -1.0f : core::substance(id).density_kg_m3;
}

std::map<std::string, PropRow> load_prop_table(const std::filesystem::path& path) {
    std::map<std::string, PropRow> table;
    std::ifstream in(path);
    if (!in) {
        return table; // нет файла — мир вчерашний: всё неподвижно
    }
    std::string line;
    while (std::getline(in, line)) {
        // Комментарий — от решётки до конца строки; она же служит заметкой.
        std::string note;
        if (const std::size_t hash = line.find('#'); hash != std::string::npos) {
            note = line.substr(hash + 1);
            line.resize(hash);
        }
        std::istringstream fields(line);
        std::string name;
        std::string cls;
        std::string substance;
        if (!(fields >> name >> cls >> substance)) {
            continue; // пустая строка или голый комментарий
        }
        PropRow row;
        row.cls = (cls == "loose") ? PropClass::Loose : PropClass::Fixed;
        row.substance = substance;
        // Четвёртое поле — толщина стенки В МИЛЛИМЕТРАХ (в файле удобнее
        // называть 8, а не 0.008) либо слово solid.
        std::string wall;
        if (fields >> wall && wall != "solid") {
            row.wall_m = std::strtof(wall.c_str(), nullptr) * 0.001f;
        }
        // Пятое поле — ЗАПОЛНЕНИЕ (доля нарисованного объёма). Пусто — единица.
        float fill = 1.0f;
        if (fields >> fill && fill > 0.0f && fill <= 1.0f) {
            row.fill = fill;
        }
        // Обрезать пробелы у заметки, чтобы отчёт печатал её как есть.
        const std::size_t first = note.find_first_not_of(" \t\r");
        const std::size_t last = note.find_last_not_of(" \t\r");
        row.note = first == std::string::npos ? std::string{}
                                              : note.substr(first, last - first + 1);
        table.emplace(std::move(name), std::move(row));
    }
    return table;
}

MeshBulk measure_bulk(std::span<const glm::vec3> positions,
                      std::span<const std::uint32_t> indices) {
    MeshBulk bulk;
    if (positions.empty() || indices.size() < 3) {
        return bulk;
    }
    bulk.lo = positions[0];
    bulk.hi = positions[0];
    for (const glm::vec3& p : positions) {
        bulk.lo = glm::min(bulk.lo, p);
        bulk.hi = glm::max(bulk.hi, p);
    }
    double volume6 = 0.0; // шестикратный знаковый объём
    double area2 = 0.0;   // удвоенная площадь
    const auto count = static_cast<std::uint32_t>(positions.size());
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const std::uint32_t ia = indices[i];
        const std::uint32_t ib = indices[i + 1];
        const std::uint32_t ic = indices[i + 2];
        if (ia >= count || ib >= count || ic >= count) {
            continue;
        }
        const glm::vec3& a = positions[ia];
        const glm::vec3& b = positions[ib];
        const glm::vec3& c = positions[ic];
        // ТЕОРЕМА О ДИВЕРГЕНЦИИ: сумма смешанных произведений вершин каждого
        // треугольника даёт шесть объёмов замкнутой оболочки. На НЕЗАМКНУТОЙ
        // она даёт число, которое ничего не значит, — и именно поэтому ниже
        // стоит сверка с габаритом, а не доверие этому числу.
        const glm::vec3 cross = glm::cross(b, c);
        volume6 += static_cast<double>(glm::dot(a, cross));
        area2 += static_cast<double>(glm::length(glm::cross(b - a, c - a)));
        ++bulk.triangles;
    }
    bulk.volume_m3 = static_cast<float>(std::abs(volume6) / 6.0);
    bulk.area_m2 = static_cast<float>(area2 * 0.5);
    const glm::vec3 span = bulk.hi - bulk.lo;
    bulk.bbox_m3 = std::max(span.x, 1e-4f) * std::max(span.y, 1e-4f)
                 * std::max(span.z, 1e-4f);
    return bulk;
}

PropMass prop_mass(const MeshBulk& bulk, const PropRow& row, std::string_view name) {
    PropMass result;
    const float density = substance_density(row.substance);
    if (density <= 0.0f) {
        result.finding = "вещество '" + row.substance + "' не в таблице плотностей";
        result.mass_kg = PROP_MASS_MIN_KG;
        result.fell_back = true;
        std::fprintf(stderr, "[предмет] %.*s: %s\n", static_cast<int>(name.size()),
                     name.data(), result.finding.c_str());
        return result;
    }

    // ШАГ 1 — ГОДЕН ЛИ ЗНАКОВЫЙ ОБЪЁМ. Отношение к габариту отвечает на этот
    // вопрос дешевле, чем перепись рёбер: у сшитой оболочки оно лежит в
    // разумной полосе по построению, у несшитой — куда угодно.
    const float ratio = bulk.bbox_m3 > 0.0f ? bulk.volume_m3 / bulk.bbox_m3 : 0.0f;
    float solid_volume = bulk.volume_m3;
    if (ratio < PROP_VOLUME_RATIO_MIN || ratio > PROP_VOLUME_RATIO_MAX) {
        solid_volume = bulk.bbox_m3 * PROP_FALLBACK_FILL;
        result.fell_back = true;
        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      "оболочка не сшита (объём/габарит = %.3f вне полосы %.2f..%.2f) "
                      "— масса по габариту с заполнением %.2f",
                      static_cast<double>(ratio),
                      static_cast<double>(PROP_VOLUME_RATIO_MIN),
                      static_cast<double>(PROP_VOLUME_RATIO_MAX),
                      static_cast<double>(PROP_FALLBACK_FILL));
        result.finding = buf;
        std::fprintf(stderr, "[предмет] %.*s: %s\n", static_cast<int>(name.size()),
                     name.data(), buf);
    }

    // ШАГ 2 — ОБОЛОЧКА ИЛИ БРУСОК. У полой вещи веществом занята СТЕНКА:
    // площадь поверхности на её толщину. Меньшее из двух берётся не из
    // осторожности, а потому что стенка толще половины предмета — это уже
    // брусок, и считать её отдельно значило бы удвоить его вес.
    // ЗАПОЛНЕНИЕ ПРИМЕНЯЕТСЯ К СПЛОШНОМУ ОБЪЁМУ И ТОЛЬКО К НЕМУ: у оболочки
    // долю вещества уже назвала толщина стенки.
    float used = solid_volume * row.fill;
    if (row.wall_m > 0.0f) {
        // ПЛОЩАДЬ ДЕЛИТСЯ ПОПОЛАМ, и это не запас, а ГЕОМЕТРИЯ НАШИХ МЕШЕЙ
        // (правило 52: «у предмета мира нет плоских частей: всё объёмное и
        // замкнутое»). Всякий кусок предмета — ЗАМКНУТОЕ тело, то есть у
        // доски посчитаны обе стороны: и лицо, и изнанка. Объём тонкой
        // пластины равен «площадь ОДНОЙ стороны на толщину», значит из полной
        // площади замкнутого куска надо взять половину. Без этого множителя
        // сундук выходил 83 кг вместо 42 — вдвое, ровно вдвое, и это было
        // видно по числу, а не по рассуждению (замер полки 28.08).
        const float shell = 0.5f * bulk.area_m2 * row.wall_m;
        used = std::min(shell, solid_volume);
        result.shell = true;
    }
    result.used_volume_m3 = used;
    result.mass_kg = std::clamp(used * density, PROP_MASS_MIN_KG, PROP_MASS_MAX_KG);
    return result;
}

std::vector<glm::vec3> hull_points(std::span<const glm::vec3> positions,
                                   std::size_t max_points) {
    std::vector<glm::vec3> points;
    if (positions.empty() || max_points < 4) {
        return points;
    }
    if (positions.size() <= max_points) {
        points.assign(positions.begin(), positions.end());
        return points;
    }
    const std::size_t stride = (positions.size() + max_points - 1) / max_points;
    points.reserve(max_points + 8);
    for (std::size_t i = 0; i < positions.size(); i += stride) {
        points.push_back(positions[i]);
    }
    // ГАБАРИТНЫЕ КРАЙНОСТИ ДОКЛАДЫВАЮТСЯ ВСЕГДА: равномерный шаг может не взять
    // ни одной вершины самого верха, и тогда прореженная оболочка окажется
    // НИЖЕ нарисованного предмета — то есть кружка стояла бы, утопая в столе.
    glm::vec3 lo = positions[0];
    glm::vec3 hi = positions[0];
    for (const glm::vec3& p : positions) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    for (const glm::vec3& p : positions) {
        if (p.x == lo.x || p.x == hi.x || p.y == lo.y || p.y == hi.y || p.z == lo.z
            || p.z == hi.z) {
            points.push_back(p);
            if (points.size() >= max_points + 8) {
                break;
            }
        }
    }
    return points;
}

} // namespace dfn::app
