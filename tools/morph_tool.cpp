/*
File: tools/morph_tool.cpp

Responsibility:
- ЧЕТЫРЕ ГЛАГОЛА ВОКРУГ СЕКЦИИ MORF, и все четыре — про ОДНО И ТО ЖЕ ТЕЛО:
    rest   — выдать питоновскому экспортёру РЕСТ-ПОЗУ скина и матрицу возврата
             на каждой вершине (без этого экспортёр лепил бы дельту на bind-позу,
             то есть на форму, которой в кадре никогда не бывает);
    attach — принять дельты В РЕСТ-ПРОСТРАНСТВЕ, перевести в bind и вписать
             секцию MORF в .dfo;
    bake   — применить веса ползунков и написать .dfo БЕЗ MORF (схема Skyrim:
             в мир уезжает выпеченное, редактор остаётся в редакторе);
    report — прибор: на КАЖДОЙ цели сколько вершин уехало, куда и докуда
             (полоса высот), и по каким полосам дельта НЕ имеет права течь.

ПОЧЕМУ ЭТО ОТДЕЛЬНЫЙ БИНАРНИК, А НЕ КЛЮЧ dfn_import_gltf. Импортёр читает
ЧУЖОЙ файл и пишет наш; здесь оба конца наши, и вход — не .glb, а уже
выпеченное тело с его канон-подгонкой. Сложи их в один — и цели пришлось бы
печь заново при каждой перепечке модели, хотя перепечка не меняет ни одной
вершины MakeHuman.

ПОЧЕМУ ДЕЛЬТА ПРИХОДИТ В РЕСТ-ПРОСТРАНСТВЕ, А ЛОЖИТСЯ В BIND. Наше тело
подогнано под канон ключом --fit-canon: суставы переехали, обратные привязки
остались, и произведение мировой матрицы сустава на обратную привязку у
HumanBase НЕ единичная матрица (замер: максимум расхождения 0.386). Значит
форма, которую видит глаз и мерит dfn_human_scale, — это скин, ПРОНЕСЁННЫЙ
скиннингом; хранимая вершина живёт в другом месте. Цель, слепленная по
силуэту, обязана прийти в том пространстве, где силуэт есть, а лечь — в том, где
лежат вершины. Перевод линеен: смещение переносится матрицей 3x3 вершинного
бленда, сдвиг сокращается.

Usage:
    dfn_morph rest   <body.dfo> --out <rest.bin>
    dfn_morph attach <body.dfo> --morf <targets.morf> [--out <body.dfo>]
    dfn_morph bake   <body.dfo> --out <baked.dfo> [--preset p.json] [--set n=v,..]
    dfn_morph report <body.dfo> [--threshold 0.01]

Dependencies:
- Uses: dfn_render (ObjectRegistry, MorphBlend), dfn_anim (rest palette), dfn_core.
- Used by: tools/make_body_targets.py (rest + attach), CMake target dfn_characters
  (attach), ctest (приёмка пресета и полос), рука.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ВСЯКИЙ ОТКАЗ ГРОМКИЙ И НЕНУЛЕВОЙ. «Цель готова» при нуле сдвинутых вершин —
  это ровно тот тихий брак, на котором записка ресёрчера поймала генератор
  ARKit-целей (52 цели, 0 сдвигов, файл вырос втрое).
*/

#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/render/sources/MorphBlend.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

/// 'DFRS' — рест-поза скина для экспортёра целей.
constexpr char REST_MAGIC[4] = {'D', 'F', 'R', 'S'};
/// 'DFMF' — переносной файл целей (вход attach). Пишет tools/make_body_targets.py.
constexpr char MORF_MAGIC[4] = {'D', 'F', 'M', 'F'};
constexpr std::uint32_t SIDECAR_VERSION = 1;

void put_u32(std::ofstream& f, std::uint32_t v) {
    unsigned char b[4] = {static_cast<unsigned char>(v & 0xFFu),
                          static_cast<unsigned char>((v >> 8) & 0xFFu),
                          static_cast<unsigned char>((v >> 16) & 0xFFu),
                          static_cast<unsigned char>((v >> 24) & 0xFFu)};
    f.write(reinterpret_cast<const char*>(b), 4);
}
void put_f32(std::ofstream& f, float v) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    put_u32(f, bits);
}

struct Blob {
    std::vector<unsigned char> bytes;
    std::size_t at = 0;
    [[nodiscard]] bool ok() const { return at <= bytes.size(); }
    std::uint32_t u32() {
        if (at + 4 > bytes.size()) {
            at = bytes.size() + 1;
            return 0;
        }
        const std::uint32_t v = static_cast<std::uint32_t>(bytes[at])
                                | (static_cast<std::uint32_t>(bytes[at + 1]) << 8)
                                | (static_cast<std::uint32_t>(bytes[at + 2]) << 16)
                                | (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
        at += 4;
        return v;
    }
    float f32() {
        const std::uint32_t bits = u32();
        float v = 0.0f;
        std::memcpy(&v, &bits, 4);
        return v;
    }
    std::string str() {
        const std::uint32_t n = u32();
        if (at + n > bytes.size()) {
            at = bytes.size() + 1;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(bytes.data() + at), n);
        at += n;
        return s;
    }
};

[[nodiscard]] bool slurp(const std::filesystem::path& p, Blob& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        return false;
    }
    out.bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

/// ВЕРШИННЫЙ БЛЕНД РЕСТ-ПОЗЫ: V_i = Σ w_k · palette[joint_k]. Одна функция на
/// три глагола (правило 32): rest выдаёт её обратную, attach — применяет,
/// report — мерит через неё же. Две копии этой суммы разошлись бы ровно на
/// той модели, ради которой всё и затевалось.
[[nodiscard]] bool rest_blend(const dfn::render::RegistryObject& obj,
                              std::vector<glm::mat4>& out) {
    const dfn::anim::Rig rig =
        dfn::anim::Rig::build(dfn::anim::RigProportions::from_config());
    const dfn::anim::SkinnedRigBinding skinned =
        dfn::anim::bind_skinned_rig(rig, obj.skeleton);
    if (skinned.bound_count() == 0) {
        std::fprintf(stderr, "[morph] ни один сустав не связался с ригом — "
                             "рест-позы у этой модели нет\n");
        return false;
    }
    std::vector<glm::mat4> palette(obj.skeleton.size());
    dfn::anim::skinning_palette(rig, obj.skeleton, skinned, dfn::anim::LocalPose{},
                                palette);
    out.assign(obj.skin.vertices.size(), glm::mat4{1.0f});
    for (std::size_t i = 0; i < obj.skin.vertices.size(); ++i) {
        const dfn::platform::SkinnedVertex& v = obj.skin.vertices[i];
        glm::mat4 m{0.0f};
        float total = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const float w = v.weights[k];
            if (w <= 0.0f || v.joints[k] >= palette.size()) {
                continue;
            }
            m += palette[v.joints[k]] * w;
            total += w;
        }
        // ВЕРШИНА БЕЗ ВЕСОВ ОСТАЁТСЯ НА МЕСТЕ, а не улетает в начало координат:
        // единичная матрица — это честное «скиннинг её не трогает», а нулевая
        // была бы дырой в теле, которую видно только в кадре.
        out[i] = total > 1e-6f ? m : glm::mat4{1.0f};
    }
    return true;
}

[[nodiscard]] glm::vec3 apply(const glm::mat4& m, const glm::vec3& p) {
    return glm::vec3{m * glm::vec4{p, 1.0f}};
}

/// СУСТАВЫ В РЕСТ-ПОЗЕ. Тем же вызовом anim, что у dfn_human_scale (правило 32):
/// экспортёр целей сажает чужое тело на НАШУ фигуру по СУСТАВАМ, а не по
/// габаритной коробке, и вторая линейка здесь разошлась бы с судьёй ровно на
/// той подгонке под канон, ради которой всё и делается.
[[nodiscard]] bool rest_joints(const dfn::render::RegistryObject& obj,
                               std::vector<glm::vec3>& out) {
    const dfn::anim::Rig rig =
        dfn::anim::Rig::build(dfn::anim::RigProportions::from_config());
    const dfn::anim::SkinnedRigBinding skinned =
        dfn::anim::bind_skinned_rig(rig, obj.skeleton);
    std::vector<glm::mat4> model(obj.skeleton.size());
    dfn::anim::rest_model_matrices(rig, obj.skeleton, skinned, dfn::anim::LocalPose{},
                                   model);
    out.resize(model.size());
    for (std::size_t i = 0; i < model.size(); ++i) {
        out[i] = glm::vec3{model[i][3]};
    }
    return !out.empty();
}

// --------------------------------------------------------------- глагол rest

int verb_rest(const dfn::render::RegistryObject& obj, const std::string& out_path) {
    std::vector<glm::mat4> vmat;
    if (!rest_blend(obj, vmat)) {
        return 1;
    }
    std::ofstream f(out_path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "[morph] не могу писать %s\n", out_path.c_str());
        return 1;
    }
    f.write(REST_MAGIC, 4);
    put_u32(f, SIDECAR_VERSION);
    put_u32(f, static_cast<std::uint32_t>(obj.skin.vertices.size()));
    put_u32(f, static_cast<std::uint32_t>(obj.skin.indices.size()));
    for (std::size_t i = 0; i < obj.skin.vertices.size(); ++i) {
        const glm::vec3 r = apply(vmat[i], obj.skin.vertices[i].position);
        put_f32(f, r.x);
        put_f32(f, r.y);
        put_f32(f, r.z);
        // ГЛАВНЫЙ СУСТАВ ВЕРШИНЫ. Это НЕ украшение отчёта: экспортёр целей ищет
        // соответствие нашей вершины на чужом теле, и «ближайшая точка» у
        // прижатой к боку кисти — ребро, а не кисть. Часть тела, названная
        // весами скина, — единственный признак, по которому кисть отличима от
        // ребра, когда они в сантиметре друг от друга.
        {
            std::uint32_t best = 0;
            float best_w = -1.0f;
            for (int k = 0; k < 4; ++k) {
                if (obj.skin.vertices[i].weights[k] > best_w) {
                    best_w = obj.skin.vertices[i].weights[k];
                    best = obj.skin.vertices[i].joints[k];
                }
            }
            put_u32(f, best);
        }
        // ОБРАТНАЯ ЛИНЕЙНАЯ ЧАСТЬ, а не сама матрица: экспортёру нужен ровно
        // перевод «смещение в рест-позе -> смещение в bind», и посчитать её
        // здесь один раз честнее, чем обращать 8546 матриц в питоне.
        const glm::mat3 inv = glm::inverse(glm::mat3{vmat[i]});
        for (int c = 0; c < 3; ++c) {
            for (int r2 = 0; r2 < 3; ++r2) {
                put_f32(f, inv[c][r2]);
            }
        }
    }
    for (const std::uint32_t i : obj.skin.indices) {
        put_u32(f, i);
    }
    std::vector<glm::vec3> joints;
    if (!rest_joints(obj, joints)) {
        return 1;
    }
    put_u32(f, static_cast<std::uint32_t>(joints.size()));
    for (std::size_t j = 0; j < joints.size(); ++j) {
        const std::string& name = obj.skeleton.joints[j].name;
        put_u32(f, static_cast<std::uint32_t>(name.size()));
        f.write(name.data(), static_cast<std::streamsize>(name.size()));
        put_f32(f, joints[j].x);
        put_f32(f, joints[j].y);
        put_f32(f, joints[j].z);
    }
    std::printf("[morph] rest: %zu вершин, %zu индексов, %zu суставов -> %s\n",
                obj.skin.vertices.size(), obj.skin.indices.size(), joints.size(),
                out_path.c_str());
    return 0;
}

// ------------------------------------------------------------- глагол attach

int verb_attach(dfn::render::RegistryObject obj, const std::string& morf_path,
                const std::string& out_path) {
    Blob b;
    if (!slurp(morf_path, b)) {
        std::fprintf(stderr, "[morph] нет файла целей %s\n", morf_path.c_str());
        return 1;
    }
    if (b.bytes.size() < 16 || std::memcmp(b.bytes.data(), MORF_MAGIC, 4) != 0) {
        std::fprintf(stderr, "[morph] %s: не файл целей (магия)\n", morf_path.c_str());
        return 1;
    }
    b.at = 4;
    const std::uint32_t ver = b.u32();
    if (ver != SIDECAR_VERSION) {
        std::fprintf(stderr, "[morph] %s: версия %u, знаю %u\n", morf_path.c_str(),
                     ver, SIDECAR_VERSION);
        return 1;
    }
    const std::uint32_t verts = b.u32();
    if (verts != obj.skin.vertices.size()) {
        // ЭТО ГЛАВНЫЙ СТОРОЖ ВСЕГО КОНВЕЙЕРА. Цели напечены по КОНКРЕТНОЙ
        // выпечке тела; перепеки тело с другими ключами — и номера вершин
        // адресуют чужую геометрию. Молчаливое согласие здесь дало бы человека,
        // у которого ползунок «живот» дёргает ухо.
        std::fprintf(stderr,
                     "[morph] %s: цели напечены на %u вершин, у тела %zu -- ОТКАЗ. "
                     "Перепеки цели: tools/make_body_targets.py\n",
                     morf_path.c_str(), verts, obj.skin.vertices.size());
        return 1;
    }
    std::vector<glm::mat4> vmat;
    if (!rest_blend(obj, vmat)) {
        return 1;
    }
    const std::uint32_t count = b.u32();
    obj.morphs.clear();
    obj.morphs.reserve(count);
    std::size_t total_deltas = 0;
    for (std::uint32_t t = 0; t < count; ++t) {
        dfn::render::MorphTarget m;
        m.name = b.str();
        m.lo = b.f32();
        m.hi = b.f32();
        const std::uint32_t n = b.u32();
        m.deltas.reserve(n);
        for (std::uint32_t k = 0; k < n; ++k) {
            dfn::render::MorphDelta d;
            d.index = b.u32();
            const glm::vec3 rest_delta{b.f32(), b.f32(), b.f32()};
            if (!b.ok() || d.index >= obj.skin.vertices.size()) {
                std::fprintf(stderr, "[morph] %s: цель \"%s\" обрезана или адресует "
                                     "вершину %u -- ОТКАЗ\n",
                             morf_path.c_str(), m.name.c_str(), d.index);
                return 1;
            }
            // РЕСТ -> BIND. Сдвиг, а не точка: перенос сокращается, работает
            // одна линейная часть.
            d.offset = glm::mat3{vmat[d.index]} * rest_delta;
            m.deltas.push_back(d);
        }
        if (m.deltas.empty()) {
            std::fprintf(stderr,
                         "[morph] цель \"%s\" не сдвинула НИ ОДНОЙ вершины -- ОТКАЗ. "
                         "Пустая цель, объявленная готовой, — тихий брак (записка §2б)\n",
                         m.name.c_str());
            return 1;
        }
        if (!(m.lo < m.hi)) {
            std::fprintf(stderr, "[morph] цель \"%s\": полоса [%g, %g] пуста -- ОТКАЗ\n",
                         m.name.c_str(), static_cast<double>(m.lo),
                         static_cast<double>(m.hi));
            return 1;
        }
        total_deltas += m.deltas.size();
        obj.morphs.push_back(std::move(m));
    }
    if (!b.ok()) {
        std::fprintf(stderr, "[morph] %s: файл обрезан\n", morf_path.c_str());
        return 1;
    }
    // ПОРЯДОК ЦЕЛЕЙ — ЧАСТЬ ФОРМАТА (ObjectRegistry.h): сортируем ЗДЕСЬ, а не
    // надеемся на экспортёр. Сложение float не ассоциативно, и «пресет
    // воспроизводим байт-в-байт» держится ровно на этой строке.
    std::sort(obj.morphs.begin(), obj.morphs.end(),
              [](const dfn::render::MorphTarget& a, const dfn::render::MorphTarget& c) {
                  return a.name < c.name;
              });
    if (!dfn::render::write_object(obj, out_path)) {
        return 1;
    }
    std::printf("[morph] attach: %u целей, %zu дельт -> %s (hash %016llx)\n", count,
                total_deltas, out_path.c_str(),
                static_cast<unsigned long long>(dfn::render::object_content_hash(obj)));
    return 0;
}

// --------------------------------------------------------------- глагол bake

/// ЧИСЛА ПРЕСЕТА, И ТОЛЬКО ОНИ. Не разбор JSON вообще, а разбор ОДНОГО объекта
/// «имя: число» — ровно то, что пишет панель редактора. Общий парсер здесь был
/// бы третьей зависимостью ради двадцати строк, а неполный общий парсер —
/// худшим из двух миров: он молча читает не то, что ему дали.
[[nodiscard]] bool read_preset(const std::filesystem::path& p,
                               std::map<std::string, float>& out) {
    std::ifstream f(p);
    if (!f) {
        std::fprintf(stderr, "[morph] нет пресета %s\n", p.string().c_str());
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    const std::size_t body = text.find("\"sliders\"");
    if (body == std::string::npos) {
        std::fprintf(stderr, "[morph] %s: нет объекта \"sliders\"\n", p.string().c_str());
        return false;
    }
    std::size_t i = text.find('{', body);
    if (i == std::string::npos) {
        return false;
    }
    ++i;
    while (i < text.size()) {
        while (i < text.size() && text[i] != '"' && text[i] != '}') {
            ++i;
        }
        if (i >= text.size() || text[i] == '}') {
            break;
        }
        const std::size_t ks = ++i;
        while (i < text.size() && text[i] != '"') {
            ++i;
        }
        const std::string key = text.substr(ks, i - ks);
        i = text.find(':', i);
        if (i == std::string::npos) {
            return false;
        }
        ++i;
        char* end = nullptr;
        const float v = std::strtof(text.c_str() + i, &end);
        if (end == text.c_str() + i) {
            std::fprintf(stderr, "[morph] %s: у ключа \"%s\" не число\n",
                         p.string().c_str(), key.c_str());
            return false;
        }
        out[key] = v;
        i = static_cast<std::size_t>(end - text.c_str());
    }
    return true;
}

int verb_bake(dfn::render::RegistryObject obj, const std::string& out_path,
              const std::map<std::string, float>& wanted) {
    if (obj.morphs.empty()) {
        std::fprintf(stderr, "[morph] у тела нет секции MORF — печь нечего\n");
        return 1;
    }
    std::vector<float> weights(obj.morphs.size(), 0.0f);
    for (const auto& [name, value] : wanted) {
        const int idx = dfn::render::morph_index(obj.morphs, name);
        if (idx < 0) {
            std::fprintf(stderr, "[morph] пресет называет ползунок \"%s\", которого у "
                                 "тела нет -- ОТКАЗ (правдоподобная выпечка не та же, "
                                 "что заказанная)\n",
                         name.c_str());
            return 1;
        }
        const dfn::render::MorphTarget& t = obj.morphs[static_cast<std::size_t>(idx)];
        if (value < t.lo || value > t.hi) {
            std::fprintf(stderr,
                         "[morph] \"%s\" = %g вне полосы [%g, %g] -- ОТКАЗ\n",
                         name.c_str(), static_cast<double>(value),
                         static_cast<double>(t.lo), static_cast<double>(t.hi));
            return 1;
        }
        weights[static_cast<std::size_t>(idx)] = value;
    }
    std::vector<dfn::platform::SkinnedVertex> blended;
    dfn::render::blend_morphs(obj.skin.vertices, obj.morphs, weights, obj.skin.indices,
                              blended);
    obj.skin.vertices = std::move(blended);
    // СЕКЦИЯ СНИМАЕТСЯ. Это не экономия байт, а утверждение: выпеченное тело —
    // ОБЫЧНЫЙ персонаж, и мир, который его грузит, про ползунки не знает
    // (Creation Kit и FaceGeom, записка §1.3).
    obj.morphs.clear();
    obj.source += " morph:baked";
    if (!dfn::render::write_object(obj, out_path)) {
        return 1;
    }
    std::printf("[morph] bake -> %s (hash %016llx)\n", out_path.c_str(),
                static_cast<unsigned long long>(dfn::render::object_content_hash(obj)));
    return 0;
}

// ------------------------------------------------------------- глагол report

int verb_report(const dfn::render::RegistryObject& obj, float threshold) {
    if (obj.morphs.empty()) {
        std::fprintf(stderr, "[morph] у тела нет секции MORF\n");
        return 1;
    }
    std::vector<glm::mat4> vmat;
    if (!rest_blend(obj, vmat)) {
        return 1;
    }
    // МЕРЯЕМ В РЕСТ-ПОЗЕ, потому что «стопа не шевелится от живота» — это
    // утверждение о ФИГУРЕ, а не о хранимых вершинах.
    std::vector<dfn::platform::SkinnedVertex> rest = obj.skin.vertices;
    float lo = 0.0f;
    float hi = 0.0f;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        rest[i].position = apply(vmat[i], obj.skin.vertices[i].position);
        lo = i == 0 ? rest[i].position.y : std::min(lo, rest[i].position.y);
        hi = i == 0 ? rest[i].position.y : std::max(hi, rest[i].position.y);
    }
    const float height = std::max(1e-4f, hi - lo);
    std::printf("[morph] тело %.3f м, вершин %zu, целей %zu, порог %.0f мм\n",
                static_cast<double>(height), rest.size(), obj.morphs.size(),
                static_cast<double>(threshold * 1000.0f));
    // ХОД МЕРЯЕТСЯ НА ЕДИНИЧНОМ ВЕСЕ, а не на конце полосы, и это не мелочь:
    // потребитель (tools/check_morph_bands.py) умножает на значение ползунка
    // сам, и если бы полоса уже была учтена здесь, она вошла бы в число ДВАЖДЫ —
    // «мёртвый ползунок» объявлялся бы живым ровно в квадрате своей полосы.
    std::printf("  %-14s %5s %8s %8s %8s %8s\n", "цель", "верш", "мм/ед", "низ,H",
                "верх,H", "полоса");
    for (const dfn::render::MorphTarget& m : obj.morphs) {
        dfn::render::MorphTarget in_rest = m;
        for (std::size_t k = 0; k < in_rest.deltas.size(); ++k) {
            in_rest.deltas[k].offset =
                glm::mat3{vmat[in_rest.deltas[k].index]} * m.deltas[k].offset;
        }
        const dfn::render::MorphSpread s =
            dfn::render::morph_spread(rest, in_rest, 1.0f, threshold);
        std::printf("  %-14s %5zu %8.1f %8.3f %8.3f [%g, %g]\n", m.name.c_str(),
                    s.moved, static_cast<double>(s.worst_m * 1000.0f),
                    static_cast<double>((s.lowest_y - lo) / height),
                    static_cast<double>((s.highest_y - lo) / height),
                    static_cast<double>(m.lo), static_cast<double>(m.hi));
    }
    return 0;
}

void usage() {
    std::fprintf(stderr,
                 "dfn_morph rest   <body.dfo> --out <rest.bin>\n"
                 "dfn_morph attach <body.dfo> --morf <targets.morf> [--out <body.dfo>]\n"
                 "dfn_morph bake   <body.dfo> --out <baked.dfo> "
                 "[--preset <p.json>] [--set name=v,...]\n"
                 "dfn_morph report <body.dfo> [--threshold 0.01]\n");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 2;
    }
    const std::string verb = argv[1];
    const std::string body = argv[2];
    std::string out;
    std::string morf;
    std::string preset;
    std::string sets;
    float threshold = 0.01f;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out" && i + 1 < argc) {
            out = argv[++i];
        } else if (a == "--morf" && i + 1 < argc) {
            morf = argv[++i];
        } else if (a == "--preset" && i + 1 < argc) {
            preset = argv[++i];
        } else if (a == "--set" && i + 1 < argc) {
            sets = argv[++i];
        } else if (a == "--threshold" && i + 1 < argc) {
            threshold = std::strtof(argv[++i], nullptr);
        } else {
            std::fprintf(stderr, "[morph] неизвестный ключ %s\n", a.c_str());
            return 2;
        }
    }
    const auto obj = dfn::render::read_object(body);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[morph] не читается %s\n", body.c_str());
        return 1;
    }
    if (obj->skin.vertices.empty() || obj->skeleton.empty()) {
        std::fprintf(stderr, "[morph] %s — не персонаж (нет SKIN/SKEL)\n", body.c_str());
        return 1;
    }
    if (verb == "rest") {
        if (out.empty()) {
            usage();
            return 2;
        }
        return verb_rest(*obj, out);
    }
    if (verb == "attach") {
        if (morf.empty()) {
            usage();
            return 2;
        }
        return verb_attach(*obj, morf, out.empty() ? body : out);
    }
    if (verb == "bake") {
        if (out.empty()) {
            usage();
            return 2;
        }
        std::map<std::string, float> wanted;
        if (!preset.empty() && !read_preset(preset, wanted)) {
            return 1;
        }
        std::size_t i = 0;
        while (i < sets.size()) {
            const std::size_t comma = std::min(sets.find(',', i), sets.size());
            const std::size_t eq = sets.find('=', i);
            if (eq == std::string::npos || eq > comma) {
                std::fprintf(stderr, "[morph] --set: «%s» не имя=число\n",
                             sets.substr(i, comma - i).c_str());
                return 2;
            }
            wanted[sets.substr(i, eq - i)] =
                std::strtof(sets.c_str() + eq + 1, nullptr);
            i = comma + 1;
        }
        return verb_bake(*obj, out, wanted);
    }
    if (verb == "report") {
        return verb_report(*obj, threshold);
    }
    usage();
    return 2;
}
