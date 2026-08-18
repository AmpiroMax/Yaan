/*
Created: 18:08:2026 - 18:04:34
Last updated: 18:08:2026 - 18:04:34
Module: engine/world
File: engine/world/sources/HouseStyle.cpp

Responsibility:
- Тела укладчика стены и читателя .dfstyle. Контракт и все обоснования чисел —
  в HouseStyle.h; здесь только то, как это считается.

Dependencies:
- Uses: HouseStyle.h, HouseMesh.h (ТОЛЬКО ради static_assert на радиус стойки),
  стандартная библиотека.
- Used by: рукав test_house_style, построитель меша.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПОРЯДОК РЕШЕНИЙ И ЕСТЬ ПРАВИЛО, и переставлять его нельзя: сначала проёмы
  (они не двигаются), потом обшивка по остатку, потом раскосы по свободным
  участкам. Уложить обшивку первой и подвинуть под неё окно — это ровно то
  поведение, от которого пользователь отказался.
- ЗДЕСЬ НЕТ НИ ОДНОЙ ВЕРШИНЫ. Всё, что возвращается, — прямоугольники и
  отрезки в координатах стены (правило 3: проверяется без окна).
*/
/*
UPD:
- 18:08:2026 - 18:04:34: Создан вместе с HouseStyle.h.
*/

#include "engine/world/sources/HouseStyle.h"

// ТОЛЬКО ради одной проверки: умолчание post_radius обязано БЫТЬ радиусом
// прямой, а не его копией. Копия — это вторая правда (правило 39), и она
// разъедется с оригиналом в день, когда кто-нибудь сделает стойки толще.
// Включение живёт в .cpp, а не в .h, нарочно: в заголовке оно замкнуло бы
// круг, когда HouseMesh начнёт строить геометрию по этой раскладке.
#include "engine/world/sources/HouseMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace dfn::world {
namespace {

constexpr float PI_F = 3.14159265358979323846f;

static_assert(HOUSE_BOARD_STEP_MAX == 2.0f * HOUSE_LINE_RADIUS_DEFAULT,
              "потолок шага обшивки ВЫВЕДЕН из радиуса угловой стойки: половина "
              "остатка обязана прятаться в её теле");
static_assert(HOUSE_BOARD_STEP_DEFAULT < HOUSE_BOARD_STEP_MAX,
              "умолчание шага обязано проходить собственный потолок");
static_assert(WallSpec{}.post_radius == HOUSE_LINE_RADIUS_DEFAULT,
              "умолчание радиуса стойки в WallSpec — это радиус прямой, а не "
              "его переписанная от руки копия");

/// Разбор числа целиком, без хвоста. "0.23abc" — это опечатка дизайнера, а не
/// 0.23: молча съеденный хвост превратил бы её в свойство, которого он не
/// задавал.
bool parse_float(const std::string& s, float& out) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(v)) {
        return false;
    }
    out = v;
    return true;
}

/// Вычесть отрезок [b0,b1] из набора непересекающихся отрезков, идущих по
/// возрастанию. Порядок сохраняется — на нём стоит детерминизм раскладки.
void subtract_span(std::vector<std::pair<float, float>>& spans, float b0, float b1) {
    std::vector<std::pair<float, float>> next;
    next.reserve(spans.size() + 1);
    for (const auto& s : spans) {
        if (b1 <= s.first || b0 >= s.second) {
            next.push_back(s);
            continue;
        }
        if (b0 > s.first) {
            next.emplace_back(s.first, b0);
        }
        if (b1 < s.second) {
            next.emplace_back(b1, s.second);
        }
    }
    spans = std::move(next);
}

std::string fmt(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return buf;
}

} // namespace

// ---------------------------------------------------------------------------
// Стиль: выведенные значения
// ---------------------------------------------------------------------------

float style_pier(const WallStyle& s) {
    return s.opening_pier ? *s.opening_pier : s.board_step;
}

float style_pitch(const WallStyle& s) {
    return s.opening_pitch ? *s.opening_pitch : (s.opening_w + style_pier(s));
}

float style_sill(const WallStyle& s) {
    // У ДВЕРИ ПОДОКОННИКА НЕТ ПО ОПРЕДЕЛЕНИЮ. Не «обычно ноль», а ноль: дверь с
    // подоконником — это окно, в которое пробуют войти.
    return s.opening == OpeningKind::Door ? 0.0f : s.opening_sill;
}

// ---------------------------------------------------------------------------
// Чтение .dfstyle
// ---------------------------------------------------------------------------

StyleIoResult parse_wall_styles(const std::string& text, std::vector<WallStyle>& out) {
    out.clear();
    std::istringstream in(text);
    std::string raw;
    int line_no = 0;
    std::size_t current = static_cast<std::size_t>(-1);

    while (std::getline(in, raw)) {
        ++line_no;
        // Примечание до конца строки. '#' в значении не бывает: значения — числа
        // и слова из одного алфавита, а разрешить его значило бы завести
        // экранирование ради случая, которого нет.
        const std::size_t hash = raw.find('#');
        if (hash != std::string::npos) {
            raw.erase(hash);
        }

        std::vector<std::string> toks;
        {
            std::istringstream ln(raw);
            std::string t;
            while (ln >> t) {
                toks.push_back(t);
            }
        }
        if (toks.empty()) {
            continue;
        }

        std::size_t first = 0;
        if (toks[0] == "style") {
            if (toks.size() < 2 || toks[1].find('=') != std::string::npos) {
                return {false, "у стиля нет имени", line_no};
            }
            for (const WallStyle& s : out) {
                if (s.name == toks[1]) {
                    return {false, "стиль с таким именем уже объявлен", line_no};
                }
            }
            WallStyle s;
            s.name = toks[1];
            out.push_back(std::move(s));
            current = out.size() - 1;
            // Хвост той же строки может нести пары сразу — так короче для
            // однострочного стиля, и это тот же лексер.
            first = 2;
        }

        for (std::size_t i = first; i < toks.size(); ++i) {
            const std::string& tok = toks[i];
            const std::size_t eq = tok.find('=');
            if (eq == std::string::npos) {
                // Голое слово посреди свойств — почти всегда опечатка в "style",
                // и она ОПАСНА: свойства следующего стиля молча легли бы в
                // предыдущий. Отказ, а не находка.
                return {false, "токен без знака равенства: '" + tok + "'", line_no};
            }
            if (current == static_cast<std::size_t>(-1)) {
                return {false, "свойство до первой строки style", line_no};
            }
            const std::string key = tok.substr(0, eq);
            const std::string val = tok.substr(eq + 1);
            WallStyle& s = out[current];

            float f = 0.0f;
            const bool numeric =
                key == "board_step" || key == "edge_margin" || key == "brace_deg" ||
                key == "opening_w" || key == "opening_h" || key == "opening_sill" ||
                key == "opening_pier" || key == "opening_pitch";
            if (numeric && !parse_float(val, f)) {
                return {false, "не число: '" + tok + "'", line_no};
            }

            if (key == "board_step") {
                s.board_step = f;
            } else if (key == "edge_margin") {
                s.edge_margin = f;
            } else if (key == "brace_deg") {
                // ГРАДУСЫ ТОЛЬКО ЗДЕСЬ (правило 14): файл — граница с человеком.
                s.brace_rad = f * PI_F / 180.0f;
            } else if (key == "opening") {
                if (val == "none") {
                    s.opening = OpeningKind::None;
                } else if (val == "window") {
                    s.opening = OpeningKind::Window;
                } else if (val == "door") {
                    // Вид ПРИНОСИТ СВОИ УМОЛЧАНИЯ. Дверь ростом с окно — не
                    // дверь, а требование HOUSES.md §6 («выше 1.8») выполняется
                    // тем, что оно применено, а не тем, что оно записано.
                    // Дизайнер перекроет их следующей парой, если захочет.
                    s.opening = OpeningKind::Door;
                    s.opening_w = HOUSE_DOOR_W_DEFAULT;
                    s.opening_h = HOUSE_DOOR_H_DEFAULT;
                } else {
                    return {false, "вид проёма не none/window/door: '" + val + "'", line_no};
                }
            } else if (key == "opening_w") {
                s.opening_w = f;
            } else if (key == "opening_h") {
                s.opening_h = f;
            } else if (key == "opening_sill") {
                s.opening_sill = f;
            } else if (key == "opening_pier") {
                s.opening_pier = f;
            } else if (key == "opening_pitch") {
                s.opening_pitch = f;
            } else {
                return {false, "неизвестный ключ: '" + tok + "'", line_no};
            }
        }
    }

    for (const WallStyle& s : out) {
        if (!(s.board_step > HOUSE_FIT_EPS)) {
            return {false, "у стиля '" + s.name + "' нулевой шаг обшивки", 0};
        }
        if (s.opening != OpeningKind::None && !(s.opening_w > HOUSE_FIT_EPS)) {
            return {false, "у стиля '" + s.name + "' нулевая ширина проёма", 0};
        }
    }
    return {};
}

const WallStyle* find_wall_style(std::span<const WallStyle> lib, const std::string& name) {
    for (const WallStyle& s : lib) {
        if (s.name == name) {
            return &s;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Раскладка
// ---------------------------------------------------------------------------

bool WallLayout::has(LayoutIssue i) const { return find(i) != nullptr; }

const LayoutFinding* WallLayout::find(LayoutIssue i) const {
    for (const LayoutFinding& f : findings) {
        if (f.issue == i) {
            return &f;
        }
    }
    return nullptr;
}

int wall_opening_capacity(float length, const WallStyle& style) {
    if (style.opening == OpeningKind::None) {
        return 0;
    }
    const float w = style.opening_w;
    const float pier = style_pier(style);
    const float pitch = style_pitch(style);
    if (!(w > HOUSE_FIT_EPS) || !(pitch > HOUSE_FIT_EPS)) {
        return 0;
    }
    // n проёмов ряда занимают (n-1)*pitch + w, и по простенку с каждой стороны.
    // Отсюда n <= 1 + (length - w - 2*pier) / pitch.
    float slack = length - w - 2.0f * pier;
    if (slack < -HOUSE_FIT_EPS) {
        return 0;
    }
    slack = std::max(slack, 0.0f);
    return 1 + static_cast<int>(std::floor((slack + HOUSE_FIT_EPS) / pitch));
}

WallLayout lay_out_wall(const WallSpec& spec, const WallStyle& style) {
    WallLayout out;
    out.openings_requested = std::max(0, spec.openings);

    if (!(spec.length > HOUSE_FIT_EPS) || !(spec.height > HOUSE_FIT_EPS)) {
        out.findings.push_back({LayoutIssue::WallDegenerate, 0.0f, out.openings_requested, 0,
                                "стена вырождена: длина " + fmt(spec.length) + " м, высота " +
                                    fmt(spec.height) + " м"});
        return out;
    }

    // -- 1. ПРОЁМЫ ПЕРВЫМИ ---------------------------------------------------
    // Они не двигаются ради обшивки и не меняют размер ради длины. Всё
    // остальное укладывается ВОКРУГ них.
    const float w = style.opening_w;
    const float sill = style_sill(style);
    const float pitch = style_pitch(style);
    bool too_tall = false;

    if (style.opening != OpeningKind::None && out.openings_requested > 0) {
        const float need_v = sill + style.opening_h;
        if (need_v > spec.height + HOUSE_FIT_EPS) {
            too_tall = true;
            out.findings.push_back(
                {LayoutIssue::OpeningTooTall, need_v, out.openings_requested, 0,
                 "проём просит " + fmt(need_v) + " м по высоте, стена " + fmt(spec.height) +
                     " м"});
        } else {
            out.openings_placed =
                std::min(out.openings_requested, wall_opening_capacity(spec.length, style));
        }
    }

    const int n = out.openings_placed;
    if (n > 0) {
        // ПО ЦЕНТРУ И СИММЕТРИЧНО: середина ряда совпадает с серединой стены.
        // При n == 1 это ровно центр, и не «примерно», а тем же выражением.
        const float c0 = spec.length * 0.5f - static_cast<float>(n - 1) * 0.5f * pitch;
        for (int i = 0; i < n; ++i) {
            const float c = c0 + static_cast<float>(i) * pitch;
            OpeningPlacement o;
            o.kind = style.opening;
            o.u0 = c - w * 0.5f;
            o.u1 = c + w * 0.5f;
            o.v0 = sill;
            o.v1 = sill + style.opening_h;
            out.openings.push_back(o);
        }
    }

    if (out.openings_placed < out.openings_requested) {
        // ГОВОРИТСЯ ВСЛУХ. Требование пользователя «максимум сколько влезло»
        // исполняется молча ровно так же, как исполняется опечатка в числе
        // проёмов, — и различить их потом нечем.
        const float need_u = static_cast<float>(out.openings_requested - 1) * pitch + w +
                             2.0f * style_pier(style);
        std::string why;
        if (style.opening == OpeningKind::None) {
            why = "у стиля '" + style.name + "' проёмов нет вовсе";
        } else if (too_tall) {
            why = "не проходит по ВЫСОТЕ";
        } else {
            why = "на " + std::to_string(out.openings_requested) + " нужно " + fmt(need_u) +
                  " м длины, есть " + fmt(spec.length) + " м";
        }
        out.findings.push_back({LayoutIssue::OpeningsDropped, need_u, out.openings_requested,
                                out.openings_placed,
                                "просили " + std::to_string(out.openings_requested) +
                                    ", влезло " + std::to_string(out.openings_placed) + ": " +
                                    why});
    }

    // -- 2. ОБШИВКА ----------------------------------------------------------
    const float margin = std::max(0.0f, style.edge_margin);
    const float span = spec.length - 2.0f * margin;
    const float step = style.board_step;
    if (step > HOUSE_FIT_EPS && span > HOUSE_FIT_EPS) {
        out.board_columns = static_cast<int>(std::floor((span + HOUSE_FIT_EPS) / step));
    }
    if (out.board_columns <= 0) {
        out.board_columns = 0;
        out.findings.push_back({LayoutIssue::NoBoardsFit, span, 0, 0,
                                "одеваемая длина " + fmt(span) + " м короче шага доски " +
                                    fmt(step) + " м"});
    }

    // ОСТАТОК ДЕЛИТСЯ ПОРОВНУ. Прижать ряд к левому торцу было бы дешевле на
    // одну строку и сделало бы стену несимметричной ради остатка — при том что
    // симметрия проёмов заявлена требованием.
    const float residue = span - static_cast<float>(out.board_columns) * step;
    const float start = margin + residue * 0.5f;
    out.end_bare = start;

    if (out.end_bare > spec.post_radius + HOUSE_FIT_EPS) {
        out.findings.push_back(
            {LayoutIssue::CladdingLeaksAtEnd, out.end_bare, 0, 0,
             "неодетая полоса у торца " + fmt(out.end_bare) + " м шире радиуса стойки " +
                 fmt(spec.post_radius) + " м — это сквозной просвет"});
    }

    for (int c = 0; c < out.board_columns; ++c) {
        const float cu0 = start + static_cast<float>(c) * step;
        const float cu1 = cu0 + step;
        std::vector<std::pair<float, float>> spans{{0.0f, spec.height}};
        for (const OpeningPlacement& o : out.openings) {
            // Касание торцами не считается пересечением: доска, вставшая ровно
            // в косяк, целая.
            if (o.u1 <= cu0 + HOUSE_FIT_EPS || o.u0 >= cu1 - HOUSE_FIT_EPS) {
                continue;
            }
            subtract_span(spans, o.v0, o.v1);
        }
        for (const auto& s : spans) {
            if (s.second - s.first > HOUSE_FIT_EPS) {
                out.boards.push_back({c, cu0, cu1, s.first, s.second});
            }
        }
    }

    // -- 3. РАСКОСЫ ----------------------------------------------------------
    // Свободные участки берутся ОТ ОСИ ДО ОСИ (0..length), а не от кромки
    // обшивки: раскос — каркас, он живёт между стойками, а зазор обшивки — про
    // наличник.
    if (style.brace_rad > HOUSE_FIT_EPS && style.brace_rad < PI_F * 0.5f - HOUSE_FIT_EPS) {
        std::vector<std::pair<float, float>> runs{{0.0f, spec.length}};
        for (const OpeningPlacement& o : out.openings) {
            subtract_span(runs, o.u0, o.u1);
        }
        const float nominal = spec.height / std::tan(style.brace_rad);
        for (const auto& r : runs) {
            const float len = r.second - r.first;
            // УЧАСТОК КОРОЧЕ ПОЛОВИНЫ НОМИНАЛЬНОГО ПРОЛЁТА РАСКОСА НЕ ПОЛУЧАЕТ.
            // Порог не назначен, а ВЫВЕДЕН из той же арифметики округления:
            // ниже него round() дал бы ноль пролётов, зажим вернул бы единицу, и
            // фактический угол ушёл бы вверх без предела — простенок в 23 см
            // получил бы «раскос» под 86°, то есть стойку под чужим именем.
            // Ровно на этом пороге полоса угла (см. HOUSE_BRACE_RAD_DEFAULT)
            // становится действительной для КАЖДОГО выданного раскоса, а не для
            // большинства.
            if (len < nominal * 0.5f - HOUSE_FIT_EPS) {
                continue;
            }
            // max() здесь — сторож, а не правило: при len >= nominal/2 округление
            // и так не может дать ноль.
            const int bays = std::max(
                1, static_cast<int>(std::lround(len / std::max(nominal, HOUSE_FIT_EPS))));
            const float bay = len / static_cast<float>(bays);
            for (int i = 0; i < bays; ++i) {
                const float x0 = r.first + static_cast<float>(i) * bay;
                const float x1 = x0 + bay;
                // НИЗКИЙ КОНЕЦ СНАРУЖИ: раскосы зеркальны относительно середины
                // стены. Средний пролёт при НЕЧЁТНОМ их числе сам себе зеркалом
                // быть не может и наклонён детерминированно влево-вниз.
                const bool low_left = (x0 + x1) * 0.5f <= spec.length * 0.5f;
                BracePlacement b;
                b.u_low = low_left ? x0 : x1;
                b.v_low = 0.0f;
                b.u_high = low_left ? x1 : x0;
                b.v_high = spec.height;
                b.angle_rad = std::atan2(spec.height, bay);
                b.length = std::sqrt(bay * bay + spec.height * spec.height);
                out.braces.push_back(b);
            }
        }
    }

    return out;
}

} // namespace dfn::world
