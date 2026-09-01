/*
Module: engine/app
File: engine/app/sources/CharGenBody.cpp

Responsibility:
- Тело экрана создания персонажа на видеокарте, его бленд, выпечка и пресет.
  Договор и все числа — в CharGenBody.h.

Dependencies:
- Uses: engine/render (ObjectRegistry, MorphBlend), engine/platform IRenderer.
- Used by: engine/app AppCharGen.cpp, tests/app/CharGenTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
*/

#include "engine/app/sources/CharGenBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string_view>

namespace dfn::app {

namespace {

/// СКИННОВАННЫЕ ВЕРШИНЫ В ОБЫЧНЫЕ, ЧЕРЕЗ ПАЛИТРУ РЕСТ-ПОЗЫ. Линейный
/// блендскиннинг на четырёх слотах — та же формула, что у программы на
/// видеокарте, только один раз на движение ручки, а не каждый кадр.
///
/// НОРМАЛЬ ЕДЕТ ЛИНЕЙНОЙ ЧАСТЬЮ ТОЙ ЖЕ МАТРИЦЫ, без обратно-транспонированной:
/// в палитре рест-позы нет неоднородного масштаба (это переносы и повороты
/// суставов), а для таких матриц обратная транспонированная равна самой
/// линейной части. Нормализация после суммы обязательна — веса смешивают
/// повороты, и сумма единичной длины не имеет.
void skin_to_mesh(const std::vector<platform::SkinnedVertex>& src,
                  const std::vector<std::uint32_t>& indices,
                  const std::vector<glm::mat4>& palette, render::MeshData& dst) {
    dst.vertices.clear();
    dst.indices.clear();
    const std::uint32_t clay = render::pack(CHARGEN_CLAY);
    dst.vertices.reserve(src.size());
    for (const platform::SkinnedVertex& v : src) {
        if (palette.empty()) {
            dst.vertices.push_back({v.position, v.normal, v.uv, clay});
            continue;
        }
        glm::vec3 p{0.0f};
        glm::vec3 n{0.0f};
        float total = 0.0f;
        for (int i = 0; i < 4; ++i) {
            const float w = v.weights[i];
            if (w <= 0.0f) {
                continue;
            }
            const std::size_t j = v.joints[i];
            if (j >= palette.size()) {
                continue;
            }
            const glm::mat4& m = palette[j];
            p += w * glm::vec3(m * glm::vec4(v.position, 1.0f));
            n += w * (glm::mat3(m) * v.normal);
            total += w;
        }
        if (total <= 0.0f) {
            // ВЕРШИНА БЕЗ ЕДИНОГО ВЕСА — не ошибка формата, а вершина, которую
            // никто не гнёт: она остаётся там, где лежит.
            dst.vertices.push_back({v.position, v.normal, v.uv, clay});
            continue;
        }
        dst.vertices.push_back({p / total, glm::normalize(n), v.uv, clay});
    }
    dst.indices = indices;
}

/// РАВНОМЕРНЫЙ МАСШТАБ ВСЕГО, ЧТО ЗНАЕТ ПРО МЕТРЫ.
///
/// ПОЧЕМУ ЭТОГО ДОСТАТОЧНО И ПОЧЕМУ ЭТО НЕ ЛОМАЕТ СКИННИНГ. Модельная
/// матрица сустава — произведение локальных T·R·S по цепочке. Умножив КАЖДЫЙ
/// локальный перенос на k, получаем B' = S_k · B · S_k⁻¹ (сопряжение
/// однородным масштабом: перенос умножается на k, поворот и масштаб не
/// меняются, произведения телескопируются). Обратная привязка тогда
/// IB' = S_k · IB · S_k⁻¹, вершина p' = k·p, и скиннинг даёт ровно k·v.
/// То есть тело едет целиком, а ни одна ПРОПОРЦИЯ не трогается — почему
/// судья и пропускает оба конца полосы, в отличие от морфа.
///
/// ПЕРЕНОСЫ КЛИПОВ ТОЖЕ. Клип везёт абсолютные переносы суставов в метрах
/// модели; оставить их прежними значило бы, что на первом же кадре анимации
/// тело возвращается к старому росту рывком.
void scale_object(render::RegistryObject& obj, float k) {
    if (std::fabs(k - 1.0f) < 1e-6f) {
        return;
    }
    for (platform::SkinnedVertex& v : obj.skin.vertices) {
        v.position *= k;
    }
    for (skel::SkeletonJoint& j : obj.skeleton.joints) {
        j.bind_translation *= k;
        // IB' = S_k · IB · S_k⁻¹: строка переносов умножается на k, а
        // ЛИНЕЙНАЯ часть остаётся — сопряжение однородным масштабом её не
        // трогает. Пишется покомпонентно, чтобы это было видно, а не
        // выведено из двух умножений матриц.
        j.inverse_bind[3] = glm::vec4(glm::vec3(j.inverse_bind[3]) * k, 1.0f);
    }
    for (skel::AnimClip& clip : obj.clips) {
        for (skel::AnimChannel& ch : clip.channels) {
            if (ch.path != skel::AnimPath::Translation) {
                continue;
            }
            for (glm::vec4& value : ch.values) {
                value.x *= k;
                value.y *= k;
                value.z *= k;
            }
        }
    }
}

void grow_bound(glm::vec3& lo, glm::vec3& hi, const render::MeshData& mesh) {
    lo = glm::vec3{1e9f};
    hi = glm::vec3{-1e9f};
    for (const platform::Vertex& v : mesh.vertices) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    if (hi.x < lo.x) {
        lo = glm::vec3{0.0f};
        hi = glm::vec3{0.0f};
    }
}

} // namespace

std::uint64_t chargen_body_hash(const std::filesystem::path& path) {
    // ПОТОКОМ, А НЕ ЦЕЛИКОМ В ПАМЯТЬ: файл тела весит 7.6 МБ, и держать его
    // вторую копию рядом с уже прочитанной геометрией незачем. Хэш — тот же
    // fnv1a64, которым дерево считает всё остальное (правило 35).
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return 0;
    }
    std::uint64_t hash = 1469598103934665603ULL; // FNV offset basis
    std::array<char, 64 * 1024> buffer{};
    while (in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()))
           || in.gcount() > 0) {
        const std::size_t got = static_cast<std::size_t>(in.gcount());
        for (std::size_t i = 0; i < got; ++i) {
            hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(buffer[i]));
            hash *= 1099511628211ULL; // FNV prime
        }
    }
    return hash;
}

float chargen_height_scale(float height_m) {
    const float clamped = std::clamp(height_m, CHARGEN_HEIGHT_MIN_M,
                                     CHARGEN_HEIGHT_MAX_M);
    return clamped / CHARGEN_BODY_HEIGHT_M;
}

// --- ТЕЛО -------------------------------------------------------------------

bool CharGenBody::load(platform::IRenderer& renderer, const anim::Rig& rig,
                       const std::filesystem::path& path) {
    auto object = render::read_object(path);
    if (!object || object->skin.empty()) {
        std::fprintf(stderr,
                     "[создание] тело не прочитано или в нём нет потока SKIN: %s\n",
                     path.string().c_str());
        return false;
    }
    object_ = std::move(*object);
    rest_ = object_.skin.vertices;
    weights_.resize_for(object_.morphs);
    height_m_ = CHARGEN_BODY_HEIGHT_M;
    triangles_ = object_.skin.indices.size() / 3;
    if (object_.morphs.empty()) {
        // ГРОМКО, НО НЕ ОТКАЗ: тело без секции MORF показать МОЖНО (это
        // выпеченный персонаж), просто крутить у него нечего. Тихое согласие
        // здесь выглядело бы как «ползунки есть, но не работают».
        std::fprintf(stderr,
                     "[создание] у %s нет секции MORF: ползунков телосложения "
                     "не будет (тело уже выпечено?)\n",
                     path.string().c_str());
    }
    const platform::ProgramHandle program = renderer.load_program("prop");
    if (!program.valid()) {
        std::fprintf(stderr, "[создание] программа prop не загрузилась\n");
        return false;
    }
    program_ = program.id;
    // ПАЛИТРА РЕСТ-ПОЗЫ — ОДИН РАЗ. Скелет от ползунков не меняется (морф
    // двигает МЕШ, а не суставы — это и есть причина, по которой рост
    // ползунком невозможен), поэтому пересчитывать её на движение ручки
    // было бы работой, ответ которой известен заранее.
    rest_palette_.assign(object_.skeleton.size(), glm::mat4{1.0f});
    if (!object_.skeleton.empty()) {
        binding_ = anim::bind_skinned_rig(rig, object_.skeleton);
        if (binding_.bound_count() == 0) {
            // ГРОМКО, НО НЕ ОТКАЗ: тело покажется в СВОЕЙ привязке. Пустой
            // экран хуже T-позы, а молчание хуже обоих.
            std::fprintf(stderr,
                         "[создание] ни одно имя сустава не легло на риг: тело "
                         "показано в позе привязки\n");
        } else {
            anim::skinning_palette(rig, object_.skeleton, binding_,
                                   anim::LocalPose{}, rest_palette_);
        }
    }
    return upload(renderer);
}

bool CharGenBody::upload(platform::IRenderer& renderer) {
    render::blend_morphs(rest_, object_.morphs, weights_.weights,
                         object_.skin.indices, blended_);
    render::MeshData mesh;
    skin_to_mesh(blended_, object_.skin.indices, rest_palette_, mesh);
    grow_bound(lo_, hi_, mesh);
    // ПАРА, А НЕ ПОСЛЕДОВАТЕЛЬНОСТЬ: новый создаётся ПЕРЕД уничтожением
    // старого, поэтому неудача заливки оставляет на экране прежнее тело, а не
    // дыру. Тот же порядок, что у replace_skinned_mesh шага 1.
    const platform::MeshHandle fresh = renderer.create_mesh(mesh.vertices, mesh.indices);
    if (!fresh.valid()) {
        std::fprintf(stderr, "[создание] меш тела не залился на видеокарту\n");
        return false;
    }
    ++uploads_;
    if (mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_});
        ++drops_;
    }
    mesh_ = fresh.id;
    return true;
}

void CharGenBody::release(platform::IRenderer& renderer) {
    if (mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_});
        ++drops_;
    }
    mesh_ = 0;
    program_ = 0;
    triangles_ = 0;
    object_ = render::RegistryObject{};
    rest_.clear();
    blended_.clear();
    rest_palette_.clear();
    binding_ = anim::SkinnedRigBinding{};
    weights_ = render::MorphState{};
    height_m_ = CHARGEN_BODY_HEIGHT_M;
}

bool CharGenBody::set_weight(std::size_t index, float value) {
    if (index >= object_.morphs.size() || index >= weights_.weights.size()) {
        return false;
    }
    const render::MorphTarget& target = object_.morphs[index];
    const float clamped = std::clamp(value, target.lo, target.hi);
    if (clamped == weights_.weights[index]) {
        return false;
    }
    weights_.weights[index] = clamped;
    return true;
}

bool CharGenBody::set_weight(std::string_view name, float value) {
    const int slot = render::morph_index(object_.morphs, name);
    return slot >= 0 && set_weight(static_cast<std::size_t>(slot), value);
}

void CharGenBody::reset() {
    weights_.resize_for(object_.morphs);
    height_m_ = CHARGEN_BODY_HEIGHT_M;
}

bool CharGenBody::set_height_m(float metres) {
    const float clamped = std::clamp(metres, CHARGEN_HEIGHT_MIN_M,
                                     CHARGEN_HEIGHT_MAX_M);
    if (clamped == height_m_) {
        return false;
    }
    height_m_ = clamped;
    return true;
}

bool CharGenBody::apply(platform::IRenderer& renderer) {
    if (program_ == 0) {
        return false;
    }
    return upload(renderer);
}

// --- ПРЕСЕТ -----------------------------------------------------------------

CharGenPreset CharGenBody::preset(std::string name) const {
    CharGenPreset out;
    out.name = std::move(name);
    out.height_m = height_m_;
    out.sliders.reserve(object_.morphs.size());
    for (std::size_t i = 0; i < object_.morphs.size(); ++i) {
        out.sliders.emplace_back(object_.morphs[i].name, weights_.weights[i]);
    }
    return out;
}

void CharGenBody::apply_preset(const CharGenPreset& preset) {
    reset();
    // НАРОД И ТИПАЖ ТЕЛО НЕ ТРОГАЮТ: они запись о том, откуда взялись числа, а
    // сами числа уже лежат в `sliders`. Восстанавливает их на экране тот, кто
    // знает список народов.
    (void)set_height_m(preset.height_m);
    for (const auto& [name, value] : preset.sliders) {
        const int slot = render::morph_index(object_.morphs, name);
        if (slot < 0) {
            std::fprintf(stderr,
                         "[создание] в пресете есть ползунок \"%s\", которого нет "
                         "у этого тела — пропущен\n",
                         name.c_str());
            continue;
        }
        (void)set_weight(static_cast<std::size_t>(slot), value);
    }
}

bool CharGenBody::bake(const std::filesystem::path& out) const {
    if (rest_.empty()) {
        std::fprintf(stderr, "[создание] печь нечего: тело не загружено\n");
        return false;
    }
    render::RegistryObject baked = object_;
    std::vector<platform::SkinnedVertex> blended;
    render::blend_morphs(rest_, object_.morphs, weights_.weights,
                         object_.skin.indices, blended);
    baked.skin.vertices = std::move(blended);
    // СЕКЦИЯ MORF СНИМАЕТСЯ — как Creation Kit пишет FaceGeom: мир грузит
    // обычного персонажа и про ползунки не знает вовсе.
    baked.morphs.clear();
    scale_object(baked, chargen_height_scale(height_m_));
    baked.source += " chargen:baked";
    std::error_code ec;
    std::filesystem::create_directories(out.parent_path(), ec);
    if (!render::write_object(baked, out)) {
        std::fprintf(stderr, "[создание] выпеченное тело не записалось: %s\n",
                     out.string().c_str());
        return false;
    }
    return true;
}

// --- ФАЙЛ ПРЕСЕТА -----------------------------------------------------------
//
// ПОРЯДОК ПОЛЕЙ — ЧАСТЬ ФОРМАТА, А НЕ ОФОРМЛЕНИЕ. `sliders` идёт ПОСЛЕДНИМ
// потому, что читатель шага 1 (tools/morph_tool.cpp: read_preset) ищет
// подстроку "sliders" и дальше разбирает пары «имя: число» до конца файла.
// Поставь рост или имя после него — и dfn_morph прочитал бы "height_m" как
// имя цели и отказал бы всей выпечке.

bool write_chargen_preset(const std::filesystem::path& out,
                          const CharGenPreset& preset) {
    std::error_code ec;
    std::filesystem::create_directories(out.parent_path(), ec);
    std::ofstream f(out, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "[создание] пресет не записался: %s\n",
                     out.string().c_str());
        return false;
    }
    f << "{\n";
    f << "  \"version\": 2,\n";
    f << "  \"name\": \"";
    for (const char c : preset.name) {
        if (c == '"' || c == '\\') {
            f << '\\';
        }
        f << c;
    }
    f << "\",\n";
    // НАРОД И ТИПАЖ — ПОСЛЕ ИМЕНИ И ДО ЧИСЕЛ, потому что читаются они глазами
    // чаще, чем одиннадцать весов.
    f << "  \"people\": \"" << preset.people << "\",\n";
    f << "  \"archetype\": \"" << preset.archetype << "\",\n";
    char buf[64] = {};
    std::snprintf(buf, sizeof(buf), "%.6f", static_cast<double>(preset.height_m));
    f << "  \"height_m\": " << buf << ",\n";
    f << "  \"sliders\": {\n";
    for (std::size_t i = 0; i < preset.sliders.size(); ++i) {
        std::snprintf(buf, sizeof(buf), "%.6f",
                      static_cast<double>(preset.sliders[i].second));
        f << "    \"" << preset.sliders[i].first << "\": " << buf
          << (i + 1 < preset.sliders.size() ? "," : "") << "\n";
    }
    f << "  }\n}\n";
    return static_cast<bool>(f);
}

bool read_chargen_preset(const std::filesystem::path& in, CharGenPreset& out) {
    std::ifstream f(in, std::ios::binary);
    if (!f) {
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    out = CharGenPreset{};

    // РАЗБОР ТОГО, ЧТО МЫ САМИ НАПИСАЛИ, а не общий JSON: дерево не несёт
    // парсера, а заводить его ради трёх полей значило бы принести в зону
    // зависимость, которую никто не заказывал. Тот же выбор, что у шага 1
    // (tools/morph_tool.cpp), и по той же причине.
    const auto field = [&text](std::string_view key) -> std::size_t {
        const std::string needle = "\"" + std::string(key) + "\"";
        const std::size_t at = text.find(needle);
        return at == std::string::npos ? std::string::npos : at + needle.size();
    };

    if (const std::size_t at = field("name"); at != std::string::npos) {
        const std::size_t open = text.find('"', at);
        if (open != std::string::npos) {
            for (std::size_t i = open + 1; i < text.size(); ++i) {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    out.name.push_back(text[++i]);
                    continue;
                }
                if (text[i] == '"') {
                    break;
                }
                out.name.push_back(text[i]);
            }
        }
    }
    // ПРОСТАЯ СТРОКА БЕЗ ЭКРАНИРОВАНИЯ: id народа и типажа — это латинские
    // слова из имени файла, а не то, что вводит игрок.
    const auto plain = [&](std::string_view key, std::string& into) {
        const std::size_t at = field(key);
        if (at == std::string::npos) {
            return;
        }
        const std::size_t open = text.find('"', at);
        const std::size_t close = open == std::string::npos
                                      ? std::string::npos
                                      : text.find('"', open + 1);
        if (close != std::string::npos) {
            into = text.substr(open + 1, close - open - 1);
        }
    };
    plain("people", out.people);
    plain("archetype", out.archetype);
    out.height_m = CHARGEN_BODY_HEIGHT_M;
    if (const std::size_t at = field("height_m"); at != std::string::npos) {
        const std::size_t colon = text.find(':', at);
        if (colon != std::string::npos) {
            out.height_m = std::strtof(text.c_str() + colon + 1, nullptr);
        }
    }
    const std::size_t sliders = field("sliders");
    if (sliders == std::string::npos) {
        return true; // пресет без ползунков законен: это чистая нейтраль
    }
    std::size_t i = text.find('{', sliders);
    if (i == std::string::npos) {
        return true;
    }
    while (true) {
        const std::size_t open = text.find('"', i);
        if (open == std::string::npos) {
            break;
        }
        const std::size_t close = text.find('"', open + 1);
        if (close == std::string::npos) {
            break;
        }
        const std::size_t colon = text.find(':', close);
        if (colon == std::string::npos) {
            break;
        }
        char* end = nullptr;
        const float value = std::strtof(text.c_str() + colon + 1, &end);
        if (end == text.c_str() + colon + 1) {
            break;
        }
        out.sliders.emplace_back(text.substr(open + 1, close - open - 1), value);
        i = static_cast<std::size_t>(end - text.c_str());
    }
    return true;
}

} // namespace dfn::app
