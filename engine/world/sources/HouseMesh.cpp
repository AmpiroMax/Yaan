/*
Created: 18:08:2026 - 17:21:51
Last updated: 18:08:2026 - 22:20:15
Module: engine/world
File: engine/world/sources/HouseMesh.cpp

Responsibility:
- Реализация геометрии постройки. Устройство — docs/DESIGN_HOUSE_GRAPH.md §5.4.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НИ ОДНОГО ВЫЧИТАНИЯ РАДИУСОВ. Если однажды здесь появится «укоротим стену на
  радиус столба», значит правило §7.0 забыто: крепление идёт к ОСИ, нахлёст
  тел — норма. Цена ошибки не косметическая: подгонка к телу соседа делает
  смену радиуса пересчётом всей стены, а сегодня она не стоит ничего.
- ВСЁ ЗАМКНУТО (правило 52). У прямой есть торцы, у пластины — рант, у стены —
  торцы. Открытая оболочка сойдёт за тело ровно до первого взгляда сбоку и до
  первого выпуклого разложения в физике.
*/
/*
UPD:
- 18:08:2026 - 17:21:51: Создан вместе с заголовком.
- 18:08:2026 - 18:26:06: ДВА ШВА СОШЛИСЬ С МОДЕЛЬЮ. (1) Числа берутся из Element::params, а строка
  стиля осталась ЗАПАСНЫМ ходом. Расстыковка была настоящая и молчаливая:
  инструменты редактора пишут через set_param, то есть в поле, — и ни одно
  заданное человеком число не доехало бы до геометрии. Ни рукав построителя, ни
  рукав модели этого не видят: каждый прав в своей половине.
  (2) Цепочка или контур — решает Element::closed, а не «высота больше нуля».
  Прежнее правило было честно помечено временным; угадывание молча ломается на
  плоском поле, которому задали высоту, и на стене нулевой высоты.
- 18:08:2026 - 22:20:15: equidistant_point (МНК-центр окружности в плоскости контура) и surface_centre.
*/

#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <glm/geometric.hpp>

namespace dfn::world {

namespace {

constexpr float PI_F = 3.14159265358979323846f;
constexpr float DEG2RAD = PI_F / 180.0f;

bool parse_float_token(std::string_view s, float& out) {
    if (s.empty() || s.size() > 63) {
        return false;
    }
    char buf[64];
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    char* end = nullptr;
    const double v = std::strtod(buf, &end);
    if (end == buf || *end != '\0') {
        return false;
    }
    out = static_cast<float>(v);
    return true;
}

// -- линейная алгебра ------------------------------------------------------

/// Циклический метод Якоби для симметричной 3x3. Нужен ровно за одним: за
/// собственным вектором при НАИМЕНЬШЕМ собственном значении ковариации, то есть
/// за нормалью наилучшей по МНК плоскости.
void jacobi_eigen_3x3(float a[3][3], float evec[3][3], float eval[3]) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            evec[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    for (int sweep = 0; sweep < 32; ++sweep) {
        const float off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        if (off < 1e-20f) {
            break;
        }
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(a[p][q]) < 1e-20f) {
                    continue;
                }
                const float theta = (a[q][q] - a[p][p]) / (2.0f * a[p][q]);
                const float sign = theta >= 0.0f ? 1.0f : -1.0f;
                const float t = sign / (std::fabs(theta) + std::sqrt(theta * theta + 1.0f));
                const float c = 1.0f / std::sqrt(t * t + 1.0f);
                const float s = t * c;
                for (int k = 0; k < 3; ++k) {
                    const float akp = a[k][p];
                    const float akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const float apk = a[p][k];
                    const float aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; ++k) {
                    const float ekp = evec[k][p];
                    const float ekq = evec[k][q];
                    evec[k][p] = c * ekp - s * ekq;
                    evec[k][q] = s * ekp + c * ekq;
                }
            }
        }
    }
    for (int k = 0; k < 3; ++k) {
        eval[k] = a[k][k];
    }
}

/// Нормаль по Ньюэллу: площадно-взвешенная сумма по рёбрам. Она НЕ работает
/// плоскостью (для кривого контура МНК точнее), но она единственная знает
/// ПОРЯДОК ОБХОДА, и потому решает знак.
glm::vec3 newell_normal(std::span<const glm::vec3> pts) {
    glm::vec3 n{0.0f};
    const std::size_t count = pts.size();
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3& a = pts[i];
        const glm::vec3& b = pts[(i + 1) % count];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n * 0.5f;
}

float polygon_area_2d(std::span<const glm::vec2> poly) {
    float acc = 0.0f;
    const std::size_t count = poly.size();
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec2& a = poly[i];
        const glm::vec2& b = poly[(i + 1) % count];
        acc += a.x * b.y - b.x * a.y;
    }
    return acc * 0.5f;
}

float cross_2d(glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; }

bool point_in_triangle(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c) {
    const float d1 = cross_2d(b - a, p - a);
    const float d2 = cross_2d(c - b, p - b);
    const float d3 = cross_2d(a - c, p - c);
    const bool has_neg = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    const bool has_pos = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(has_neg && has_pos);
}

bool segments_cross(glm::vec2 p1, glm::vec2 p2, glm::vec2 q1, glm::vec2 q2) {
    const float d1 = cross_2d(p2 - p1, q1 - p1);
    const float d2 = cross_2d(p2 - p1, q2 - p1);
    const float d3 = cross_2d(q2 - q1, p1 - q1);
    const float d4 = cross_2d(q2 - q1, p2 - q1);
    return ((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f));
}

/// Ось, заведомо не параллельная данной. Выбор ДЕТЕРМИНИРОВАН и не зависит от
/// того, откуда смотрит камера: две сборки одного графа обязаны дать один меш.
glm::vec3 stable_reference_axis(glm::vec3 dir) {
    return std::fabs(dir.y) < 0.9f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
}

// -- сборка меша -----------------------------------------------------------

/// Плоское затенение: у каждой грани свои вершины. Дороже по памяти и честнее
/// по виду — сруб из восьмигранных брёвен обязан читаться гранями, а не
/// мыльным цилиндром, и это же снимает вопрос «чью нормаль усреднять на ребре».
struct MeshBuilder {
    HouseMesh* out = nullptr;

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

} // namespace

// ---------------------------------------------------------------------------
// Свойства
// ---------------------------------------------------------------------------

ElementParams parse_element_params(std::string_view style, std::vector<ParamIssue>* issues) {
    struct Slot {
        std::string_view key;
        float ElementParams::*field;
    };
    static const Slot slots[] = {
        {"radius", &ElementParams::radius},       {"length", &ElementParams::length},
        {"angle_x", &ElementParams::angle_x},     {"angle_y", &ElementParams::angle_y},
        {"angle_z", &ElementParams::angle_z},     {"thickness", &ElementParams::thickness},
        {"height", &ElementParams::height},       {"tex_deg", &ElementParams::tex_deg},
    };

    ElementParams p;
    std::size_t pos = 0;
    bool first = true;
    while (pos <= style.size()) {
        const std::size_t sep = style.find(';', pos);
        const std::string_view tok =
            style.substr(pos, sep == std::string_view::npos ? std::string_view::npos : sep - pos);
        pos = sep == std::string_view::npos ? style.size() + 1 : sep + 1;
        if (tok.empty()) {
            first = false;
            continue;
        }
        const std::size_t eq = tok.find('=');
        if (eq == std::string_view::npos) {
            // Голое слово имеет смысл ТОЛЬКО первым: это имя стиля. Второе
            // голое слово — почти наверняка забытый ключ, и молчать про него
            // значит применить не то, что просили.
            if (first) {
                p.name = std::string(tok);
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok), "свойство без имени ключа"});
            }
            first = false;
            continue;
        }
        first = false;
        const std::string_view key = tok.substr(0, eq);
        const std::string_view val = tok.substr(eq + 1);
        float number = 0.0f;
        const bool numeric = parse_float_token(val, number);

        if (key == "form") {
            if (val == "round") {
                p.form = LineForm::Round;
            } else if (val == "square") {
                p.form = LineForm::Square;
            } else if (val == "ngon") {
                p.form = LineForm::Ngon;
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok), "форма бывает round, square или ngon"});
            }
            continue;
        }
        if (key == "n" || key == "sides") {
            if (numeric) {
                p.sides = static_cast<int>(number);
                p.form = LineForm::Ngon;
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok), "число граней не число"});
            }
            continue;
        }
        bool matched = false;
        for (const Slot& s : slots) {
            if (key != s.key) {
                continue;
            }
            matched = true;
            if (numeric) {
                p.*(s.field) = number;
            } else if (issues != nullptr) {
                issues->push_back({std::string(tok), "значение не число"});
            }
            break;
        }
        if (!matched && issues != nullptr) {
            issues->push_back({std::string(tok), "неизвестное свойство"});
        }
    }
    return p;
}

int profile_sides(const ElementParams& p) {
    if (p.form == LineForm::Square) {
        return 4;
    }
    return p.sides >= 3 ? p.sides : HOUSE_ROUND_SIDES;
}

// ---------------------------------------------------------------------------
// Плоскость контура
// ---------------------------------------------------------------------------

FittedPlane fit_contour_plane(std::span<const glm::vec3> pts) {
    FittedPlane out;
    if (pts.size() < 3) {
        return out;
    }
    glm::vec3 centroid{0.0f};
    for (const glm::vec3& p : pts) {
        centroid += p;
    }
    centroid /= static_cast<float>(pts.size());
    out.origin = centroid;

    float cov[3][3] = {};
    for (const glm::vec3& p : pts) {
        const glm::vec3 d = p - centroid;
        const float v[3] = {d.x, d.y, d.z};
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cov[i][j] += v[i] * v[j];
            }
        }
    }
    float evec[3][3] = {};
    float eval[3] = {};
    jacobi_eigen_3x3(cov, evec, eval);
    int smallest = 0;
    for (int k = 1; k < 3; ++k) {
        if (eval[k] < eval[smallest]) {
            smallest = k;
        }
    }
    glm::vec3 n{evec[0][smallest], evec[1][smallest], evec[2][smallest]};
    const float n_len = glm::length(n);
    if (n_len < HOUSE_GEOM_EPS) {
        return out;
    }
    n /= n_len;

    // ЗНАК БЕРЁТСЯ У ПОРЯДКА ОБХОДА. МНК знает плоскость, но не знает, где у неё
    // лицо: собственный вектор определён с точностью до знака. Правило правой
    // руки (§2.3) — единственный источник ответа, и он же тот, который дизайнер
    // видит на экране, обходя контур.
    const glm::vec3 nw = newell_normal(pts);
    if (glm::length(nw) >= HOUSE_GEOM_EPS && glm::dot(n, nw) < 0.0f) {
        n = -n;
    }
    // Ньюэлл нулевой — у обхода нет стороны: вершины на одной прямой или контур
    // сложен бантиком, где заметённая площадь взаимно уничтожается. Знак тогда
    // остаётся тот, что дал МНК, и это НЕ подмена ответа: плоскость дальше
    // нужна ровно за тем, чтобы спроецировать контур и НАЗВАТЬ беду. Ранний
    // выход отсюда стоил бы разбора: бантик уходил бы в «нулевую площадь», а
    // «нулевая площадь» и «сам себя пересёк» чинятся по-разному.
    out.normal = n;

    for (const glm::vec3& p : pts) {
        out.max_deviation = std::max(out.max_deviation, std::fabs(glm::dot(p - centroid, n)));
    }
    for (std::size_t i = 0; i < pts.size(); ++i) {
        for (std::size_t j = i + 1; j < pts.size(); ++j) {
            out.span = std::max(out.span, glm::length(pts[j] - pts[i]));
        }
    }
    out.flatness = out.span > HOUSE_GEOM_EPS ? out.max_deviation / out.span : 0.0f;
    // Площадь считается по ПРОЕКЦИИ на найденную плоскость: это та самая
    // площадь, которую потом получат треугольники, а значит проверять её надо
    // ту же, а не ньюэллову.
    glm::vec3 u{0.0f};
    glm::vec3 v{0.0f};
    const std::vector<glm::vec2> flat = project_contour(pts, out, u, v);
    out.area = std::fabs(polygon_area_2d(flat));
    out.degenerate = out.area < HOUSE_GEOM_EPS;
    return out;
}

std::vector<glm::vec2> project_contour(std::span<const glm::vec3> pts, const FittedPlane& plane,
                                       glm::vec3& u_out, glm::vec3& v_out) {
    std::vector<glm::vec2> flat;
    if (pts.size() < 2) {
        return flat;
    }
    // Ось u вдоль ПЕРВОГО РЕБРА КОНТУРА. Не вдоль мировой оси: тогда развёртка
    // одного и того же пола зависела бы от того, как дом повёрнут, и типовой
    // дом получал бы разную текстуру в разных местах карты.
    glm::vec3 hint = pts[1] - pts[0];
    hint -= plane.normal * glm::dot(plane.normal, hint);
    if (glm::length(hint) < HOUSE_GEOM_EPS) {
        hint = stable_reference_axis(plane.normal);
        hint -= plane.normal * glm::dot(plane.normal, hint);
    }
    u_out = glm::normalize(hint);
    v_out = glm::cross(plane.normal, u_out);
    flat.reserve(pts.size());
    for (const glm::vec3& p : pts) {
        const glm::vec3 d = p - plane.origin;
        flat.push_back({glm::dot(d, u_out), glm::dot(d, v_out)});
    }
    return flat;
}

std::vector<std::uint32_t> triangulate_contour(std::span<const glm::vec2> poly) {
    std::vector<std::uint32_t> tris;
    const std::size_t count = poly.size();
    if (count < 3) {
        return tris;
    }
    // Работаем против часовой стрелки. Если контур пришёл по часовой, обходим
    // его задом наперёд и НЕ ТРОГАЕМ выходные имена: наружу идут индексы
    // исходного контура, иначе вызывающий получит чужую нумерацию.
    const bool ccw = polygon_area_2d(poly) >= 0.0f;
    std::vector<std::uint32_t> ring;
    ring.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        ring.push_back(static_cast<std::uint32_t>(ccw ? i : count - 1 - i));
    }

    std::size_t guard = 0;
    const std::size_t guard_max = count * count + 16;
    while (ring.size() > 3) {
        bool clipped = false;
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const std::size_t ip = (i + ring.size() - 1) % ring.size();
            const std::size_t in = (i + 1) % ring.size();
            const glm::vec2 a = poly[ring[ip]];
            const glm::vec2 b = poly[ring[i]];
            const glm::vec2 c = poly[ring[in]];
            // Ухо обязано быть ВЫПУКЛЫМ углом...
            if (cross_2d(b - a, c - b) <= 0.0f) {
                continue;
            }
            // ...и ПУСТЫМ. Проверка на пустоту — ровно то, чем отсечение ушей
            // отличается от веера: без неё Г-образная комната получила бы
            // треугольник поперёк выемки, то есть пол там, где его нет.
            bool empty = true;
            for (std::size_t k = 0; k < ring.size() && empty; ++k) {
                if (k == i || k == ip || k == in) {
                    continue;
                }
                const std::size_t kp = (k + ring.size() - 1) % ring.size();
                const std::size_t kn = (k + 1) % ring.size();
                // Смотрим ТОЛЬКО на вогнутые вершины: выпуклая внутри уха
                // оказаться не может, а лишние проверки дают ложные отказы на
                // касающихся точках.
                if (cross_2d(poly[ring[k]] - poly[ring[kp]], poly[ring[kn]] - poly[ring[k]]) >
                    0.0f) {
                    continue;
                }
                if (point_in_triangle(poly[ring[k]], a, b, c)) {
                    empty = false;
                }
            }
            if (!empty) {
                continue;
            }
            tris.push_back(ring[ip]);
            tris.push_back(ring[i]);
            tris.push_back(ring[in]);
            ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped || ++guard > guard_max) {
            // Уши кончились раньше треугольников. Отдаём ПУСТО, а не половину:
            // половина триангуляции — это дыра в полу, которую никто не свяжет
            // с причиной.
            return {};
        }
    }
    tris.push_back(ring[0]);
    tris.push_back(ring[1]);
    tris.push_back(ring[2]);
    return tris;
}

bool contour_self_intersects(std::span<const glm::vec2> poly) {
    const std::size_t count = poly.size();
    if (count < 4) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            // Смежные рёбра делят вершину и «пересекаются» всегда — их пропуск
            // это определение, а не послабление.
            if ((j + 1) % count == i || (i + 1) % count == j) {
                continue;
            }
            if (segments_cross(poly[i], poly[(i + 1) % count], poly[j], poly[(j + 1) % count])) {
                return true;
            }
        }
    }
    return false;
}

glm::vec2 UvFrame::at(glm::vec3 p) const {
    const glm::vec3 d = p - origin;
    return {glm::dot(d, u), glm::dot(d, v)};
}

UvFrame make_uv_frame(glm::vec3 origin, glm::vec3 normal, glm::vec3 u_hint, float tex_deg) {
    UvFrame f;
    f.origin = origin;
    const float n_len = glm::length(normal);
    const glm::vec3 n = n_len > HOUSE_GEOM_EPS ? normal / n_len : glm::vec3{0.0f, 1.0f, 0.0f};
    glm::vec3 u0 = u_hint - n * glm::dot(n, u_hint);
    if (glm::length(u0) < HOUSE_GEOM_EPS) {
        u0 = stable_reference_axis(n);
        u0 -= n * glm::dot(n, u0);
    }
    u0 = glm::normalize(u0);
    const glm::vec3 v0 = glm::cross(n, u0);
    // ПОВОРОТ ВОКРУГ НОРМАЛИ. Оба орта остаются В ПЛОСКОСТИ грани и остаются
    // единичными, поэтому расстояние в развёртке равно расстоянию по
    // поверхности при ЛЮБОМ угле. Именно этим поворот вокруг нормали
    // отличается от поворота вокруг мировой вертикали с последующей проекцией:
    // тот на наклонной грани растягивает узор в 1/cos(наклон) раз.
    const float c = std::cos(tex_deg * DEG2RAD);
    const float s = std::sin(tex_deg * DEG2RAD);
    f.u = u0 * c + v0 * s;
    f.v = -u0 * s + v0 * c;
    return f;
}

// ---------------------------------------------------------------------------
// Тела
// ---------------------------------------------------------------------------

namespace {

/// Кольцо профиля вокруг оси. Точки идут против часовой стрелки в базисе (u,v).
std::vector<glm::vec3> profile_ring(glm::vec3 center, glm::vec3 axis_u, glm::vec3 axis_v,
                                    float face_radius, int sides) {
    std::vector<glm::vec3> ring;
    ring.reserve(static_cast<std::size_t>(sides));
    // radius — расстояние до ГРАНИ, значит до УГЛА дальше в 1/cos(pi/N) раз.
    // Одна формула на все формы: у круга это ровно вписанный радиус, у
    // квадрата — половина толщины бруса. Так «radius=0.12» значит «24 см
    // толщиной» независимо от формы, а не «то ли 24, то ли 17».
    const float corner = face_radius / std::cos(PI_F / static_cast<float>(sides));
    for (int k = 0; k < sides; ++k) {
        const float a = 2.0f * PI_F * (static_cast<float>(k) + 0.5f) / static_cast<float>(sides);
        ring.push_back(center + (axis_u * std::cos(a) + axis_v * std::sin(a)) * corner);
    }
    return ring;
}

/// Призма: нижнее кольцо loop (обходимое против часовой стрелки, если смотреть
/// со стороны +axis), верхнее — loop + axis. Треугольники кольца уже посчитаны
/// вызывающим и переиспользуются для крышки, днища и коллайдера.
void push_prism(MeshBuilder& mb, std::span<const glm::vec3> loop,
                std::span<const std::uint32_t> tris, glm::vec3 axis, float tex_deg,
                HouseMesh& mesh, ElementId owner) {
    const std::size_t count = loop.size();
    if (count < 3 || tris.size() < 3) {
        return;
    }
    const float axis_len = glm::length(axis);
    if (axis_len < HOUSE_GEOM_EPS) {
        return;
    }
    const glm::vec3 n = axis / axis_len;
    glm::vec3 edge_hint = loop[1] - loop[0];
    if (glm::length(edge_hint) < HOUSE_GEOM_EPS) {
        edge_hint = stable_reference_axis(n);
    }

    // Крышка (лицо, +n) и днище (-n). Днище — те же треугольники наоборот.
    const UvFrame top_uv = make_uv_frame(loop[0], n, edge_hint, tex_deg);
    const UvFrame bottom_uv = make_uv_frame(loop[0], -n, edge_hint, tex_deg);
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        mb.push_triangle(loop[tris[t]] + axis, loop[tris[t + 1]] + axis, loop[tris[t + 2]] + axis,
                         top_uv);
        mb.push_triangle(loop[tris[t + 2]], loop[tris[t + 1]], loop[tris[t]], bottom_uv);
    }
    // Рант. Каждое ребро контура даёт одну грань, наружу.
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3& a = loop[i];
        const glm::vec3& b = loop[(i + 1) % count];
        const glm::vec3 dir = b - a;
        if (glm::length(dir) < HOUSE_GEOM_EPS) {
            continue;
        }
        const glm::vec3 face_n = glm::normalize(glm::cross(dir, n));
        const UvFrame side_uv = make_uv_frame(a, face_n, dir, tex_deg);
        mb.push_quad(a, b, b + axis, a + axis, side_uv);
    }
    // Коллайдер: одна выпуклая призма на треугольник. Разбор идёт по ТЕМ ЖЕ
    // треугольникам, что и меш, поэтому Г-образная комната получает физику по
    // своей форме, а не по своей выпуклой оболочке. Отдельный алгоритм был бы
    // второй копией правды (правило 39) и разъехался бы с мешем в тот день,
    // когда кто-нибудь поправит триангуляцию.
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        ConvexPart part;
        part.element = owner;
        part.points = {loop[tris[t]],        loop[tris[t + 1]],        loop[tris[t + 2]],
                       loop[tris[t]] + axis, loop[tris[t + 1]] + axis, loop[tris[t + 2]] + axis};
        mesh.convex.push_back(std::move(part));
    }
}

void build_line(const HouseGraph& g, const Element& e, const ElementParams& p, MeshBuilder& mb,
                HouseMesh& mesh) {
    const glm::vec3 a = g.resolved_local(e.refs.front());
    glm::vec3 b{0.0f};
    if (e.refs.size() >= 2) {
        if (e.refs.size() > 2) {
            mesh.findings.push_back({e.id, MeshIssue::LineExtraRefs, 0.0f,
                                     "у прямой больше двух вершин: взяты первые две"});
        }
        b = g.resolved_local(e.refs[1]);
    } else {
        if (p.length <= HOUSE_GEOM_EPS) {
            mesh.findings.push_back({e.id, MeshIssue::LineNeedsLength, 0.0f,
                                     "прямая на одной вершине без length"});
            return;
        }
        // Направление: angle_x — отклонение от вертикали, angle_y — куда оно
        // направлено. Стойка по умолчанию СТОИТ (нулевые углы дают +Y), потому
        // что это самый частый случай, а не потому что +Y красивее.
        const float ax = p.angle_x * DEG2RAD;
        const float ay = p.angle_y * DEG2RAD;
        const glm::vec3 dir{std::sin(ay) * std::sin(ax), std::cos(ax), std::cos(ay) * std::sin(ax)};
        b = a + dir * p.length;
    }
    const glm::vec3 axis = b - a;
    const float len = glm::length(axis);
    if (len < HOUSE_GEOM_EPS) {
        mesh.findings.push_back({e.id, MeshIssue::Degenerate, len, "у прямой нулевая длина"});
        return;
    }
    const glm::vec3 w = axis / len;
    const glm::vec3 ref = stable_reference_axis(w);
    const glm::vec3 u0 = glm::normalize(glm::cross(ref, w));
    const glm::vec3 v0 = glm::cross(w, u0);
    // angle_z ВРАЩАЕТ ПРОФИЛЬ вокруг собственной оси. Для круглого бревна это
    // не меняет ничего, для квадратного бруса — меняет всё, и потому углов три,
    // а не два: направление стоит двух чисел, крен профиля — третьего.
    const float rz = p.angle_z * DEG2RAD;
    const glm::vec3 u = u0 * std::cos(rz) + v0 * std::sin(rz);
    const glm::vec3 v = -u0 * std::sin(rz) + v0 * std::cos(rz);

    const int sides = profile_sides(p);
    const float radius = std::max(p.radius, HOUSE_GEOM_EPS);
    const std::vector<glm::vec3> ring = profile_ring(a, u, v, radius, sides);
    std::vector<std::uint32_t> tris;
    tris.reserve(static_cast<std::size_t>(sides - 2) * 3);
    for (int k = 1; k + 1 < sides; ++k) {
        tris.push_back(0);
        tris.push_back(static_cast<std::uint32_t>(k));
        tris.push_back(static_cast<std::uint32_t>(k + 1));
    }
    // Кольцо обходится против часовой стрелки в базисе (u,v), а u x v == w,
    // значит веер смотрит по оси — ровно то, чего ждёт push_prism.
    push_prism(mb, ring, tris, axis, p.tex_deg, mesh, e.id);
}

void build_contour_surface(const Element& e, const ElementParams& p,
                           std::span<const glm::vec3> pts, MeshBuilder& mb, HouseMesh& mesh) {
    const FittedPlane plane = fit_contour_plane(pts);
    // САМОПЕРЕСЕЧЕНИЕ ПРОВЕРЯЕТСЯ ПЕРВЫМ, до отказа по вырождению, и порядок
    // здесь — часть ответа: у бантика заметённая площадь ровно ноль, поэтому
    // обратный порядок назвал бы его «контуром нулевой площади» и отправил
    // дизайнера искать совпавшие вершины, которых нет.
    glm::vec3 u{0.0f};
    glm::vec3 v{0.0f};
    const std::vector<glm::vec2> flat = project_contour(pts, plane, u, v);
    if (contour_self_intersects(flat)) {
        mesh.findings.push_back(
            {e.id, MeshIssue::ContourSelfIntersects, 0.0f, "контур самопересекается"});
    }
    if (plane.degenerate) {
        mesh.findings.push_back(
            {e.id, MeshIssue::Degenerate, plane.area, "у контура нулевая площадь"});
        return;
    }
    if (plane.flatness > HOUSE_CONTOUR_FLATNESS_MAX) {
        // ГОВОРИТСЯ, НО НЕ ЗАПРЕЩАЕТСЯ. Дизайнер посреди правки обязан видеть
        // то, что сделал; отказ строить превратил бы промах по якорю в
        // исчезнувший пол, а исчезнувший пол объясняет причину хуже, чем
        // сложенный пополам.
        mesh.findings.push_back({e.id, MeshIssue::ContourNonPlanar, plane.flatness,
                                 "контур слишком кривой для наклонной поверхности"});
    }
    const std::vector<std::uint32_t> tris = triangulate_contour(flat);
    if (tris.empty()) {
        mesh.findings.push_back({e.id, MeshIssue::TriangulationFailed, 0.0f,
                                 "уши кончились раньше треугольников"});
        return;
    }
    // ЛИЦО — по обходу, переключатель разворачивает (§2.3).
    const glm::vec3 n = e.facing_flipped ? -plane.normal : plane.normal;
    const float half = std::max(p.thickness, HOUSE_GEOM_EPS) * 0.5f;

    // ТОЛЩИНА СИММЕТРИЧНА: срединная плоскость проходит ровно по якорям, как
    // тело прямой сидит вокруг оси, а не сбоку от неё. Одно правило на два
    // вида, и разворот лица не двигает ни одной точки.
    std::vector<glm::vec3> loop;
    loop.reserve(pts.size());
    std::vector<std::uint32_t> use = tris;
    if (e.facing_flipped) {
        // Кольцо обязано обходиться против часовой стрелки со стороны +n. При
        // развороте лица переворачивается и порядок, и нумерация в тройках.
        for (std::size_t i = pts.size(); i-- > 0;) {
            loop.push_back(pts[i] - n * half);
        }
        const std::uint32_t last = static_cast<std::uint32_t>(pts.size() - 1);
        for (std::size_t t = 0; t + 2 < use.size(); t += 3) {
            const std::uint32_t x = last - use[t];
            const std::uint32_t y = last - use[t + 1];
            const std::uint32_t z = last - use[t + 2];
            use[t] = z;
            use[t + 1] = y;
            use[t + 2] = x;
        }
    } else {
        for (const glm::vec3& q : pts) {
            loop.push_back(q - n * half);
        }
    }
    push_prism(mb, loop, use, n * (half * 2.0f), p.tex_deg, mesh, e.id);
}

void build_chain_surface(const Element& e, const ElementParams& p, std::span<const glm::vec3> pts,
                         MeshBuilder& mb, HouseMesh& mesh) {
    if (p.height <= HOUSE_GEOM_EPS) {
        mesh.findings.push_back(
            {e.id, MeshIssue::ChainNeedsHeight, 0.0f, "цепочка без height: стены нет"});
        return;
    }
    const glm::vec3 up{0.0f, p.height, 0.0f};
    const float half = std::max(p.thickness, HOUSE_GEOM_EPS) * 0.5f;
    const std::uint32_t quad[6] = {0, 1, 2, 0, 2, 3};
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const glm::vec3& a = pts[i];
        const glm::vec3& b = pts[i + 1];
        const glm::vec3 d = b - a;
        if (std::sqrt(d.x * d.x + d.z * d.z) < HOUSE_GEOM_EPS) {
            mesh.findings.push_back({e.id, MeshIssue::ChainSegmentVertical, static_cast<float>(i),
                                     "отрезок цепочки вертикален: выдавливать некуда"});
            continue;
        }
        // ЛИЦО ОТРЕЗКА — по правилу правой руки от порядка вершин и вертикали.
        glm::vec3 face_n = glm::normalize(glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}));
        if (e.facing_flipped) {
            face_n = -face_n;
        }
        const glm::vec3 h = face_n * half;
        // КАЖДЫЙ ОТРЕЗОК — САМОСТОЯТЕЛЬНОЕ ТЕЛО, и на изломе они входят друг в
        // друга. Это не недоделанный ус, а прямое следствие §7.0: в углу стоит
        // столб, стены упираются в его ОСЬ, нахлёст — норма. Подгонка на ус
        // стоила бы пересчёта обеих стен при каждой правке соседа.
        const glm::vec3 loop[4] = {a - h, a + h, b + h, b - h};
        push_prism(mb, loop, quad, up, p.tex_deg, mesh, e.id);
    }
}

/// ЧИСЛА ЭЛЕМЕНТА: сперва СВОЁ ПОЛЕ, потом строка стиля.
///
/// Element::params появился 18.08, а построитель до сих пор читал числа из
/// строки стиля через точку с запятой. Расстыковка была настоящая и молчаливая:
/// инструменты редактора пишут через set_param, то есть в поле, — и ни одно из
/// заданных человеком чисел не доехало бы до геометрии. Ни рукав построителя,
/// ни рукав модели этого не видят: каждый прав в своей половине.
///
/// Строка остаётся ЗАПАСНЫМ ходом, а не равноправным: её понимают старые файлы
/// и старые рукава. Поле сильнее — если число задано и там, и там, побеждает
/// поле, потому что его задал редактор, а строку мог оставить кто угодно.
ElementParams element_params_of(const Element& e, std::vector<ParamIssue>* issues) {
    ElementParams p = parse_element_params(e.style, issues);
    for (const auto& kv : e.params) {
        // Тот же разбор, что и у строки: собираем «ключ=значение» и отдаём в
        // общий лексер, чтобы правило чтения числа было ОДНО (правило 32).
        const std::string one = kv.first + "=" + kv.second;
        const ElementParams got = parse_element_params(one, issues);
        if (kv.first == "radius") { p.radius = got.radius; }
        else if (kv.first == "length") { p.length = got.length; }
        else if (kv.first == "angle_x") { p.angle_x = got.angle_x; }
        else if (kv.first == "angle_y") { p.angle_y = got.angle_y; }
        else if (kv.first == "angle_z") { p.angle_z = got.angle_z; }
        else if (kv.first == "thickness") { p.thickness = got.thickness; }
        else if (kv.first == "height") { p.height = got.height; }
        else if (kv.first == "tex_deg") { p.tex_deg = got.tex_deg; }
        else if (kv.first == "form") { p.form = got.form; }
    }
    return p;
}

/// ЦЕПОЧКА ИЛИ КОНТУР — РЕШАЕТ ПРИЗНАК ЗАМКНУТОСТИ, а не высота.
///
/// Здесь стояло временное правило «высота больше нуля значит цепочка», честно
/// помеченное временным: признака замкнутости в Element тогда не было. Теперь
/// он есть (Element::closed, заведён 18.08 по этой же просьбе), и угадывание
/// снято. Разница не косметическая: угадывание молча ломается на плоском поле,
/// которому задали высоту, и на стене нулевой высоты — а это не выдуманные
/// случаи, а два первых, до которых дойдёт человек.
///
/// Двух вершин на контур не хватает по определению, поэтому они всегда цепочка.
bool is_chain_surface(const Element& e, const ElementParams&) {
    return e.refs.size() < 3 || !e.closed;
}

} // namespace

// ---------------------------------------------------------------------------
// Сборка
// ---------------------------------------------------------------------------

std::vector<ElementId> ordered_elements(const HouseGraph& g) {
    // У графа сегодня нет перебора элементов, зато есть перебор вершин через
    // components() и обратный ход incident(). Каждый элемент по построению
    // ссылается хотя бы на одну существующую вершину, поэтому такой обход
    // ничего не теряет; сортировка по имени делает порядок тем же, что в файле.
    std::vector<ElementId> ids;
    for (const std::vector<VertexId>& group : g.components()) {
        for (const VertexId v : group) {
            for (const ElementId e : g.incident(v)) {
                ids.push_back(e);
            }
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

const MeshPart* HouseMesh::part_of(ElementId id) const {
    for (const MeshPart& p : parts) {
        if (p.element == id) {
            return &p;
        }
    }
    return nullptr;
}

HouseMesh build_house_mesh(const HouseGraph& g) {
    HouseMesh mesh;
    MeshBuilder mb{&mesh};
    for (const ElementId id : ordered_elements(g)) {
        const Element* e = g.element(id);
        if (e == nullptr || e->refs.empty()) {
            continue;
        }
        std::vector<ParamIssue> issues;
        const ElementParams p = element_params_of(*e, &issues);
        for (const ParamIssue& is : issues) {
            mesh.findings.push_back({id, MeshIssue::UnknownParam, 0.0f, is.token + ": " + is.why});
        }
        const std::uint32_t begin = static_cast<std::uint32_t>(mesh.indices.size());
        if (e->kind == ElementKind::Line) {
            build_line(g, *e, p, mb, mesh);
        } else {
            std::vector<glm::vec3> pts;
            pts.reserve(e->refs.size());
            for (const VertexId r : e->refs) {
                pts.push_back(g.resolved_local(r));
            }
            if (is_chain_surface(*e, p)) {
                build_chain_surface(*e, p, pts, mb, mesh);
            } else {
                build_contour_surface(*e, p, pts, mb, mesh);
            }
        }
        const std::uint32_t count = static_cast<std::uint32_t>(mesh.indices.size()) - begin;
        if (count > 0) {
            mesh.parts.push_back({id, begin, count});
        }
    }
    return mesh;
}

/// ТОЧКА, РАВНОУДАЛЁННАЯ ОТ ВЕРШИН, в плоскости контура. Метод наименьших
/// квадратов по классическому раскрытию |p - c|² = r²: разность двух таких
/// уравнений линейна по c, и система нормальных уравнений решается прямо.
/// Возвращает false, когда система вырождена (все вершины на одной прямой).
static bool equidistant_point(const std::vector<glm::vec2>& flat, glm::vec2& out) {
    if (flat.size() < 3) {
        return false;
    }
    // Опорная вершина — последняя; вычитание её уравнения убирает |c|² и r².
    const glm::vec2 base = flat.back();
    double a11 = 0.0;
    double a12 = 0.0;
    double a22 = 0.0;
    double b1 = 0.0;
    double b2 = 0.0;
    for (std::size_t i = 0; i + 1 < flat.size(); ++i) {
        const double dx = 2.0 * (flat[i].x - base.x);
        const double dy = 2.0 * (flat[i].y - base.y);
        const double rhs = static_cast<double>(glm::dot(flat[i], flat[i]))
                         - static_cast<double>(glm::dot(base, base));
        a11 += dx * dx;
        a12 += dx * dy;
        a22 += dy * dy;
        b1 += dx * rhs;
        b2 += dy * rhs;
    }
    const double det = a11 * a22 - a12 * a12;
    if (std::abs(det) < 1e-9) {
        return false;
    }
    out = {static_cast<float>((b1 * a22 - b2 * a12) / det),
           static_cast<float>((a11 * b2 - a12 * b1) / det)};
    return true;
}

bool surface_centre(const HouseGraph& g, ElementId id, glm::vec3& out) {
    const Element* e = g.element(id);
    if (e == nullptr || e->kind != ElementKind::Surface || e->refs.empty()) {
        return false;
    }
    const ElementParams p = element_params_of(*e, nullptr);
    std::vector<glm::vec3> pts;
    pts.reserve(e->refs.size());
    for (const VertexId r : e->refs) {
        pts.push_back(g.resolved_local(r));
    }
    glm::vec3 mid{0.0f};
    for (const glm::vec3& v : pts) {
        mid += v;
    }
    mid /= static_cast<float>(pts.size());
    if (is_chain_surface(*e, p)) {
        // ПОЛОВИНА ВЫСОТЫ: полотно уходит вверх от этой ломаной, и его видное
        // место — посередине, а не на нижней кромке.
        out = mid + glm::vec3{0.0f, p.height * 0.5f, 0.0f};
        return true;
    }
    const FittedPlane plane = fit_contour_plane(pts);
    if (plane.degenerate) {
        out = mid;
        return true;
    }
    // В ПЛОСКОСТЬ КОНТУРА той же проекцией, что и триангуляция: второй способ
    // разложить контур по осям разошёлся бы с первым (правило 32).
    glm::vec3 ax{0.0f};
    glm::vec3 ay{0.0f};
    const std::vector<glm::vec2> flat = project_contour(pts, plane, ax, ay);
    glm::vec2 c{0.0f};
    if (!equidistant_point(flat, c)) {
        out = mid;
        return true;
    }
    out = plane.origin + ax * c.x + ay * c.y;
    return true;
}

bool surface_normal(const HouseGraph& g, ElementId id, glm::vec3& out) {
    const Element* e = g.element(id);
    if (e == nullptr || e->kind != ElementKind::Surface || e->refs.size() < 2) {
        return false;
    }
    const ElementParams p = element_params_of(*e, nullptr);
    std::vector<glm::vec3> pts;
    pts.reserve(e->refs.size());
    for (const VertexId r : e->refs) {
        pts.push_back(g.resolved_local(r));
    }
    if (is_chain_surface(*e, p)) {
        const glm::vec3 d = pts[1] - pts[0];
        if (std::sqrt(d.x * d.x + d.z * d.z) < HOUSE_GEOM_EPS) {
            return false;
        }
        out = glm::normalize(glm::cross(d, glm::vec3{0.0f, 1.0f, 0.0f}));
    } else {
        const FittedPlane plane = fit_contour_plane(pts);
        if (plane.degenerate) {
            return false;
        }
        out = plane.normal;
    }
    if (e->facing_flipped) {
        out = -out;
    }
    return true;
}

} // namespace dfn::world
