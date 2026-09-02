/*
Module: tools
File: tools/check_face_scale.cpp

Responsibility:
- dfn_face_scale: СУДЬЯ ЛИЦА. Меряет одиннадцать мерок лица персонажа по
  МАСКАМ ОБЛАСТЕЙ (assets/characters/targets/face.masks — вершины .dfo, которые
  двигают цели MPFB2 глаза, углов глаз, век, углов рта, крыльев и кончика носа,
  основания носа, ушей) на рест-позированной коже и судит их: без baseline —
  по внешнему канону там, где он есть (межзрачковое и линия глаз), с baseline
  — ПОЛОСОЙ ВОКРУГ СВОЕЙ НЕЙТРАЛИ (docs/research/FACE_CANON.md §2: линейка со
  своим нулём — маска цели всегда шире анатомической точки, поэтому ширина рта
  по маске 87 мм против учебных 50, и судить её учебником значило бы отвергнуть
  собственную нейтраль).

Key items:
- main(): <file.dfo> --masks FILE [--baseline FILE] [--write-baseline FILE]
          [--tolerance 0.12] [--eyeline-tolerance 0.03] [--quiet].
- Мерки: IPD; ширина и высота глазной щели; ширина лица на уровне глаз (уши
  исключены по маске); высота головы; линия глаз; «пять глаз»; длина и вынос
  носа; ширина крыльев; ширина рта. Всё в метрах модели, кроме долей.
- Полоса по baseline: ±tolerance ОТНОСИТЕЛЬНО (умолчание 0.12 = ±2σ
  межзрачкового расстояния мужской выборки, 55.8…71.4 мм вокруг 63.6,
  Dodgson) на каждую длину; линия глаз — АБСОЛЮТНО ±0.03 (канон «глаза
  посередине головы», допуск наш, FACE_CANON §4). Обе ручки — ключи, но
  никогда не по умолчанию шире.

Dependencies:
- Uses: engine/render (.dfo reader), engine/anim (RestFit, SkinnedBody,
  BoneMap — та же рест-поза, что у dfn_human_scale и экрана создания),
  engine/core (Json).
- Used by: tools/check_morph_bands.py (крайние положения ручек лица,
  калибровка полос двоичным поиском), ctest (face_scale_*), отчёт волны.

Notes:
- ТА ЖЕ РЕСТ-ПОЗА, ЧТО У ТЕЛА (правило 47): мерить хранимые bind-вершины
  значило бы мерить форму, которой в кадре не бывает; кожа проносится тем же
  rest_rig_for + skinning_palette, что у dfn_human_scale.
- МАСКА ПРИХОДИТ ФАЙЛОМ, А НЕ ИЩЕТСЯ ПО ФОРМЕ (правило 47): «глаз» — это
  вершины, которые двигают цели глаза, установленные ОДИН РАЗ экспортёром, и
  каждая рука читается по тем же вершинам. Судья, ищущий глаз по впадине,
  терял бы его ровно на той ручке, которая впадину убирает.
- ЛИНИЯ ШЕИ — ТА ЖЕ, ЧТО У СУДЬИ ТЕЛА: сустав «neck» (Bone::Head ранга 1) на
  риге Rigify — верхняя шея, линия подбородка с точностью до процента.
- BASELINE — ФАЙЛ РЯДОМ С АССЕТОМ, а не секция в выпечке (довод у
  dfn_human_scale): записывается только явным --write-baseline.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Числа печатаются всегда, не только приговор; всякий отказ громкий и
  ненулевой. Строка мерки, которой нет в baseline, — ОТКАЗ, а не пропуск.
*/

#include "engine/anim/sources/BoneMap.h"
#include "engine/anim/sources/RestFit.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/serialization/sources/Json.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// ОДНА МЕРКА: имя, замер, внешний канон (0 — канона нет, печатается «—»),
/// абсолютная ли полоса (линия глаз) и полоса канона, если он есть.
struct Metric {
    const char* name;
    float measured = 0.0f;
    float canon_lo = 0.0f;
    float canon_hi = 0.0f;
    bool absolute = false;  ///< полоса по baseline — абсолютная, не относительная
    const char* unit = "m";
};

struct Masks {
    std::size_t verts = 0;
    std::map<std::string, std::vector<std::uint32_t>> regions;
};

[[nodiscard]] bool read_masks(const std::string& path, Masks& out, std::string& err) {
    std::ifstream f(path);
    if (!f) {
        err = "не открывается";
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream s(line);
        std::string head;
        s >> head;
        if (head == "verts") {
            s >> out.verts;
            continue;
        }
        if (head.empty() || head.back() != ':') {
            err = "строка не «область: номера»: " + line;
            return false;
        }
        head.pop_back();
        std::vector<std::uint32_t>& idx = out.regions[head];
        std::uint32_t v = 0;
        while (s >> v) {
            idx.push_back(v);
        }
        if (idx.empty()) {
            err = "область «" + head + "» пуста";
            return false;
        }
    }
    if (out.verts == 0 || out.regions.empty()) {
        err = "нет ни «verts», ни областей";
        return false;
    }
    return true;
}

struct Baseline {
    bool loaded = false;
    std::string body;
    float height = 0.0f;
    std::vector<std::pair<std::string, float>> rows;
    [[nodiscard]] const float* find(const char* name) const {
        for (const auto& r : rows) {
            if (r.first == name) {
                return &r.second;
            }
        }
        return nullptr;
    }
};

[[nodiscard]] bool write_baseline_file(const std::string& out, const std::string& source,
                                       const std::string& masks, const std::string& body,
                                       float height, const std::vector<Metric>& rows) {
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(6);
    o << "{\n";
    o << "  \"schema\": \"dfn.face-scale-baseline\",\n";
    o << "  \"version\": 1,\n";
    o << "  \"body\": \"" << body << "\",\n";
    o << "  \"source\": \"" << source << "\",\n";
    o << "  \"masks\": \"" << masks << "\",\n";
    o << "  \"decision\": \"FACE_CANON.md §2: полоса лицевой ручки центрируется на "
         "замере СВОЕЙ нейтрали, а не на числе из учебника\",\n";
    o << "  \"height_m\": " << height << ",\n";
    o << "  \"face\": {\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        o << "    \"" << rows[i].name << "\": " << rows[i].measured
          << (i + 1 == rows.size() ? "\n" : ",\n");
    }
    o << "  }\n}\n";
    std::ofstream f(out, std::ios::binary);
    if (!f) {
        return false;
    }
    const std::string text = o.str();
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(f);
}

[[nodiscard]] bool read_baseline_file(const std::string& in, Baseline& out,
                                      std::string& err) {
    std::ifstream f(in, std::ios::binary);
    if (!f) {
        err = "не открывается";
        return false;
    }
    std::ostringstream buf;
    buf << f.rdbuf();
    const dfn::serialization::JsonParseResult r =
        dfn::serialization::json_parse(buf.str());
    if (!r.ok) {
        err = "строка " + std::to_string(r.error.line) + ": " + r.error.message;
        return false;
    }
    if (r.root.get("schema").as_string() != "dfn.face-scale-baseline") {
        err = "не тот файл: schema != dfn.face-scale-baseline";
        return false;
    }
    out.body = std::string{r.root.get("body").as_string()};
    out.height = static_cast<float>(r.root.get("height_m").as_number());
    if (const dfn::serialization::JsonValue* t = r.root.find("face"); t != nullptr) {
        for (const dfn::serialization::JsonMember& m : t->members()) {
            out.rows.emplace_back(m.key, static_cast<float>(m.value.as_number()));
        }
    }
    if (out.rows.empty()) {
        err = "пустой: ни одной мерки";
        return false;
    }
    out.loaded = true;
    return true;
}

[[nodiscard]] glm::vec3 centroid(const std::vector<glm::vec3>& rest,
                                 const std::vector<std::uint32_t>& idx) {
    glm::vec3 sum{0.0f};
    for (const std::uint32_t i : idx) {
        sum += rest[i];
    }
    return idx.empty() ? sum : sum / static_cast<float>(idx.size());
}

[[nodiscard]] float extent_x(const std::vector<glm::vec3>& rest,
                             const std::vector<std::uint32_t>& idx) {
    float lo = 1e9f;
    float hi = -1e9f;
    for (const std::uint32_t i : idx) {
        lo = std::min(lo, rest[i].x);
        hi = std::max(hi, rest[i].x);
    }
    return idx.empty() ? 0.0f : hi - lo;
}

} // namespace

int main(int argc, char** argv) {
    std::string path;
    std::string masks_path;
    std::string baseline_path;
    std::string write_baseline;
    // ±2σ МЕЖЗРАЧКОВОГО РАССТОЯНИЯ мужской выборки (63.6 ± 3.9 мм, Dodgson):
    // 55.8…71.4 — это ±12 % вокруг середины. Одна полоса на все длины, потому
    // что у остальных мерок собственного канона нет (FACE_CANON §2), а IPD —
    // единственная, у которой он есть и он узок.
    float tolerance = 0.12f;
    // ЛИНИЯ ГЛАЗ — ДОЛЯ, и полоса к ней абсолютная: 0.50 ± 0.03.
    float eyeline_tolerance = 0.03f;
    bool quiet = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--masks" && i + 1 < argc) {
            masks_path = argv[++i];
        } else if (a == "--baseline" && i + 1 < argc) {
            baseline_path = argv[++i];
        } else if (a == "--write-baseline" && i + 1 < argc) {
            write_baseline = argv[++i];
        } else if (a == "--tolerance" && i + 1 < argc) {
            tolerance = std::strtof(argv[++i], nullptr);
        } else if (a == "--eyeline-tolerance" && i + 1 < argc) {
            eyeline_tolerance = std::strtof(argv[++i], nullptr);
        } else if (a == "--quiet") {
            quiet = true;
        } else if (path.empty()) {
            path = a;
        }
    }
    if (path.empty() || masks_path.empty()) {
        std::fprintf(stderr,
                     "dfn_face_scale <character.dfo> --masks face.masks [--baseline f.json] "
                     "[--write-baseline f.json] [--tolerance 0.12] "
                     "[--eyeline-tolerance 0.03] [--quiet]\n");
        return 2;
    }
    if (!baseline_path.empty() && !write_baseline.empty()) {
        std::fprintf(stderr, "[face] --baseline и --write-baseline вместе не имеют смысла\n");
        return 2;
    }
    const auto obj = dfn::render::read_object(path);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[face] cannot read \"%s\"\n", path.c_str());
        return 1;
    }
    if (obj->skeleton.empty() || obj->skin.vertices.empty()) {
        std::fprintf(stderr, "[face] \"%s\" is not a character (no SKEL/SKIN)\n", path.c_str());
        return 1;
    }
    Masks masks;
    std::string err;
    if (!read_masks(masks_path, masks, err)) {
        std::fprintf(stderr, "[face] маски \"%s\": %s\n", masks_path.c_str(), err.c_str());
        return 1;
    }
    if (masks.verts != obj->skin.vertices.size()) {
        // ТОТ ЖЕ СТОРОЖ, ЧТО У dfn_morph attach: маски адресуют вершины КОНКРЕТНОЙ
        // выпечки; на теле с другим числом вершин они мерили бы чужую геометрию.
        std::fprintf(stderr, "[face] маски сняты на %zu вершин, у тела %zu — ОТКАЗ\n",
                     masks.verts, obj->skin.vertices.size());
        return 1;
    }
    const char* NEEDED[] = {"eye-l", "eye-r", "eye-corner-inner-l", "eye-corner-outer-l",
                            "eye-corner-inner-r", "eye-corner-outer-r", "eyelid-l",
                            "eyelid-r", "mouth-angles", "nostrils", "nose-point",
                            "nose-base", "ears"};
    for (const char* n : NEEDED) {
        if (masks.regions.find(n) == masks.regions.end()) {
            std::fprintf(stderr, "[face] в масках нет области «%s»\n", n);
            return 1;
        }
        for (const std::uint32_t v : masks.regions[n]) {
            if (v >= obj->skin.vertices.size()) {
                std::fprintf(stderr, "[face] область «%s» адресует вершину %u\n", n, v);
                return 1;
            }
        }
    }
    const dfn::anim::SkeletonBinding bind = dfn::anim::bind_skeleton(obj->skeleton);
    if (bind.bound_count < dfn::anim::BONE_COUNT) {
        std::fprintf(stderr, "[face] \"%s\": only %u of %u rig bones bind by name\n",
                     path.c_str(), bind.bound_count, dfn::anim::BONE_COUNT);
        return 1;
    }

    // РЕСТ-ПОЗА — ТА ЖЕ, ЧТО У ТЕЛА И ЭКРАНА (RestFit.h).
    const dfn::anim::Rig rig = dfn::anim::rest_rig_for(obj->skeleton, obj->skin.vertices);
    const dfn::anim::SkinnedRigBinding skinned =
        dfn::anim::bind_skinned_rig(rig, obj->skeleton);
    std::vector<glm::mat4> palette(obj->skeleton.size());
    dfn::anim::skinning_palette(rig, obj->skeleton, skinned, dfn::anim::LocalPose{}, palette);
    std::vector<glm::vec3> rest(obj->skin.vertices.size());
    float lo_y = 0.0f;
    float hi_y = 0.0f;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        rest[i] = dfn::anim::cpu_skin_position(obj->skin.vertices[i], palette);
        lo_y = i == 0 ? rest[i].y : std::min(lo_y, rest[i].y);
        hi_y = i == 0 ? rest[i].y : std::max(hi_y, rest[i].y);
    }
    const float height = hi_y - lo_y;
    if (height < 1e-3f) {
        std::fprintf(stderr, "[face] \"%s\" has no height\n", path.c_str());
        return 1;
    }
    std::vector<glm::mat4> model(obj->skeleton.size());
    dfn::anim::rest_model_matrices(rig, obj->skeleton, skinned, dfn::anim::LocalPose{}, model);
    // ЛИНИЯ ШЕИ — как у dfn_human_scale: сустав «neck» (Bone::Head ранга 1).
    int32_t neck_joint = -1;
    for (std::size_t j = 0; j < obj->skeleton.size(); ++j) {
        const std::string& name = obj->skeleton.joints[j].name;
        if (dfn::anim::bone_from_joint_name(name) == dfn::anim::Bone::Head
            && dfn::anim::joint_name_rank(name) == 1) {
            neck_joint = static_cast<int32_t>(j);
            break;
        }
    }
    if (neck_joint < 0) {
        neck_joint = bind.joint[dfn::anim::bone_index(dfn::anim::Bone::Head)];
    }
    const float neck_y = model[static_cast<std::size_t>(neck_joint)][3].y;

    // --- МЕРКИ (FACE_CANON §1) ---------------------------------------------
    const auto& R = masks.regions;
    const glm::vec3 eye_l = centroid(rest, R.at("eye-l"));
    const glm::vec3 eye_r = centroid(rest, R.at("eye-r"));
    const float eye_y = 0.5f * (eye_l.y + eye_r.y);
    const float ipd = std::fabs(eye_r.x - eye_l.x);
    const auto slit_w = [&](const char* inner, const char* outer) {
        return glm::length(centroid(rest, R.at(outer)) - centroid(rest, R.at(inner)));
    };
    const float slit_width = 0.5f * (slit_w("eye-corner-inner-l", "eye-corner-outer-l")
                                     + slit_w("eye-corner-inner-r", "eye-corner-outer-r"));
    // ВЫСОТА ЩЕЛИ: размах по Y вершин века МЕЖДУ углами (по X).
    const auto slit_h = [&](const char* lid, const char* inner, const char* outer) {
        const float x0 = centroid(rest, R.at(inner)).x;
        const float x1 = centroid(rest, R.at(outer)).x;
        const float xa = std::min(x0, x1);
        const float xb = std::max(x0, x1);
        float lo = 1e9f;
        float hi = -1e9f;
        for (const std::uint32_t i : R.at(lid)) {
            if (rest[i].x >= xa && rest[i].x <= xb) {
                lo = std::min(lo, rest[i].y);
                hi = std::max(hi, rest[i].y);
            }
        }
        return hi > lo ? hi - lo : 0.0f;
    };
    const float slit_height = 0.5f * (slit_h("eyelid-l", "eye-corner-inner-l", "eye-corner-outer-l")
                                      + slit_h("eyelid-r", "eye-corner-inner-r", "eye-corner-outer-r"));
    // ШИРИНА ЛИЦА НА УРОВНЕ ГЛАЗ: пояс ±6 мм, уши исключены ПО МАСКЕ (не по
    // форме — правило 47).
    std::vector<bool> is_ear(rest.size(), false);
    for (const std::uint32_t i : R.at("ears")) {
        is_ear[i] = true;
    }
    float fx_lo = 1e9f;
    float fx_hi = -1e9f;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        if (is_ear[i] || std::fabs(rest[i].y - eye_y) > 0.006f) {
            continue;
        }
        fx_lo = std::min(fx_lo, rest[i].x);
        fx_hi = std::max(fx_hi, rest[i].x);
    }
    const float face_width = fx_hi > fx_lo ? fx_hi - fx_lo : 0.0f;
    const float head_height = hi_y - neck_y;
    const float eye_line = head_height > 1e-6f ? (hi_y - eye_y) / head_height : 0.0f;
    const float five_eyes = slit_width > 1e-6f ? face_width / slit_width : 0.0f;
    const glm::vec3 nose_base = centroid(rest, R.at("nose-base"));
    const glm::vec3 nose_point = centroid(rest, R.at("nose-point"));
    const float nose_length = eye_y - nose_base.y;
    const float nose_depth = std::fabs(nose_point.z - nose_base.z);
    const float nostrils = extent_x(rest, R.at("nostrils"));
    const float mouth = extent_x(rest, R.at("mouth-angles"));

    // ВНЕШНИЙ КАНОН — ТОЛЬКО ТАМ, ГДЕ ОН ЕСТЬ, и в масштабе 1.75 м (замер
    // FACE_CANON нормирован в рост 1.75).
    const float to175 = 1.75f / height;
    std::vector<Metric> rows{
        {"ipd", ipd, 0.0558f / to175, 0.0714f / to175, false, "m"},
        {"eye slit width", slit_width, 0.0f, 0.0f, false, "m"},
        {"eye slit height", slit_height, 0.0f, 0.0f, false, "m"},
        {"face width at eyes", face_width, 0.0f, 0.0f, false, "m"},
        {"head height", head_height, 0.0f, 0.0f, false, "m"},
        {"eye line", eye_line, 0.47f, 0.53f, true, "frac"},
        {"five eyes", five_eyes, 0.0f, 0.0f, false, "ratio"},
        {"nose length", nose_length, 0.0f, 0.0f, false, "m"},
        {"nose depth", nose_depth, 0.0f, 0.0f, false, "m"},
        {"nostril width", nostrils, 0.0f, 0.0f, false, "m"},
        {"mouth width", mouth, 0.0f, 0.0f, false, "m"},
    };

    if (!write_baseline.empty()) {
        if (!write_baseline_file(write_baseline, path, masks_path, obj->name, height, rows)) {
            std::fprintf(stderr, "[face] не пишется baseline \"%s\"\n", write_baseline.c_str());
            return 1;
        }
        std::printf("[face] baseline записан: %s — \"%s\", %.3f м, %zu мерок\n",
                    write_baseline.c_str(), obj->name.c_str(), static_cast<double>(height),
                    rows.size());
        return 0;
    }
    Baseline base;
    if (!baseline_path.empty() && !read_baseline_file(baseline_path, base, err)) {
        std::fprintf(stderr,
                     "[face] baseline \"%s\": %s\n        пересоздаётся ЯВНО: dfn_face_scale "
                     "%s --masks %s --write-baseline %s\n",
                     baseline_path.c_str(), err.c_str(), path.c_str(), masks_path.c_str(),
                     baseline_path.c_str());
        return 1;
    }
    const bool by_baseline = base.loaded;
    if (!quiet) {
        std::printf("[face] \"%s\": %s, рост %.3f м, голова %.1f мм, линия шеи %.3f м\n",
                    path.c_str(), obj->name.c_str(), static_cast<double>(height),
                    static_cast<double>(head_height * 1000.0f), static_cast<double>(neck_y));
        if (by_baseline) {
            std::printf("[face] ВЕРДИКТ ПО BASELINE \"%s\" (тело \"%s\"): длины ±%.0f%%, "
                        "линия глаз ±%.2f; канон печатается справочно\n",
                        baseline_path.c_str(), base.body.c_str(),
                        static_cast<double>(tolerance * 100.0f),
                        static_cast<double>(eyeline_tolerance));
            std::printf("        %-20s %10s %14s %10s %9s\n", "metric", "model",
                        "canon(1.75m)", "baseline", "d.base");
        } else {
            std::printf("        %-20s %10s %14s\n", "metric", "model", "canon(1.75m)");
        }
    }
    int failures = 0;
    for (const Metric& r : rows) {
        const bool has_canon = r.canon_hi > r.canon_lo;
        const bool canon_bad = has_canon && (r.measured < r.canon_lo || r.measured > r.canon_hi);
        char canon[40];
        if (has_canon) {
            if (r.absolute) {
                std::snprintf(canon, sizeof(canon), "%.2f..%.2f", static_cast<double>(r.canon_lo),
                              static_cast<double>(r.canon_hi));
            } else {
                std::snprintf(canon, sizeof(canon), "%.1f..%.1f mm",
                              static_cast<double>(r.canon_lo * to175 * 1000.0f),
                              static_cast<double>(r.canon_hi * to175 * 1000.0f));
            }
        } else {
            std::snprintf(canon, sizeof(canon), "%s", "—");
        }
        char model_s[24];
        if (r.absolute || r.unit[0] == 'r') {
            std::snprintf(model_s, sizeof(model_s), "%.3f", static_cast<double>(r.measured));
        } else {
            std::snprintf(model_s, sizeof(model_s), "%.1f mm",
                          static_cast<double>(r.measured * 1000.0f));
        }
        if (!by_baseline) {
            failures += canon_bad ? 1 : 0;
            if (!quiet) {
                std::printf("        %-20s %10s %14s %s\n", r.name, model_s, canon,
                            canon_bad ? "<-- OUT" : "");
            }
            continue;
        }
        const float* want = base.find(r.name);
        if (want == nullptr) {
            ++failures;
            if (!quiet) {
                std::printf("        %-20s %10s %14s %10s %9s <-- НЕТ В BASELINE\n", r.name,
                            model_s, canon, "-", "-");
            }
            continue;
        }
        float dev = 0.0f;
        bool bad = false;
        if (r.absolute) {
            dev = r.measured - *want;
            bad = std::fabs(dev) > eyeline_tolerance;
        } else {
            dev = *want > 1e-6f ? (r.measured - *want) / *want : 0.0f;
            bad = std::fabs(dev) > tolerance;
        }
        failures += bad ? 1 : 0;
        if (!quiet) {
            char base_s[24];
            char dev_s[24];
            if (r.absolute || r.unit[0] == 'r') {
                std::snprintf(base_s, sizeof(base_s), "%.3f", static_cast<double>(*want));
            } else {
                std::snprintf(base_s, sizeof(base_s), "%.1f mm",
                              static_cast<double>(*want * 1000.0f));
            }
            if (r.absolute) {
                std::snprintf(dev_s, sizeof(dev_s), "%+.3f", static_cast<double>(dev));
            } else {
                std::snprintf(dev_s, sizeof(dev_s), "%+.1f%%", static_cast<double>(dev * 100.0f));
            }
            std::printf("        %-20s %10s %14s %10s %9s %s\n", r.name, model_s, canon, base_s,
                        dev_s, bad ? "<-- OUT" : (canon_bad ? "(канон мимо)" : ""));
        }
    }
    if (failures > 0) {
        if (by_baseline) {
            std::fprintf(stderr,
                         "[face] %d мерок(и) вне BASELINE \"%s\" (длины ±%.0f%%, линия глаз "
                         "±%.2f) — ЛИЦО УШЛО ОТ НЕЙТРАЛИ\n",
                         failures, baseline_path.c_str(),
                         static_cast<double>(tolerance * 100.0f),
                         static_cast<double>(eyeline_tolerance));
        } else {
            std::fprintf(stderr, "[face] %d мерок(и) вне внешнего канона (IPD 55.8..71.4 мм, "
                                 "линия глаз 0.47..0.53)\n", failures);
        }
        return 1;
    }
    if (!quiet) {
        std::printf(by_baseline ? "[face] BASELINE: все %zu мерок внутри полосы\n"
                                : "[face] канон: обе канонические мерки внутри; остальные "
                                  "%zu печатаются без приговора\n",
                    by_baseline ? rows.size() : rows.size() - 2);
    }
    return 0;
}
