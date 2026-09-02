/*
Module: engine/app
File: engine/app/sources/CharGenBody.cpp

Responsibility:
- Тело экрана создания персонажа: игровой персонаж фабрикой, бленд ползунков,
  масштаб роста, выпечка и пресет. Договор и все числа — в CharGenBody.h.

Dependencies:
- Uses: engine/app CharacterFactory / SkinnedCharacter, engine/anim (RestFit,
  BodyGaps, Hitbox), engine/render (ObjectRegistry, MorphBlend),
  engine/core serialization (fnv1a64).
- Used by: engine/app AppCharGen.cpp, tests/app/CharGenTests.cpp,
  tests/app/CharacterPathTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
*/

#include "engine/app/sources/CharGenBody.h"

#include "engine/app/sources/AppDoors.h"

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/Hitbox.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/serialization/sources/ContentHash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string_view>

namespace dfn::app {

namespace {

/// ПРИВОД ПОКОЯ — тот же BodyDrive, которым мир ведёт стоящего игрока: шаг
/// ноль, земля есть, оружие в ножнах. Тело на экране стоит так, как стоит в
/// мире, потому что стоит оно тем же кодом.
[[nodiscard]] anim::BodyDrive idle_drive() {
    anim::BodyDrive d;
    d.gait = anim::Gait::Walk;
    d.speed_mps = 0.0f;
    d.step_length_m = 0.0f;
    d.stride_phase = 0.0f;
    d.grounded = true;
    d.weapon_drawn = false;
    d.run_weight = 0.0f;
    return d;
}

void grow_bound(glm::vec3& lo, glm::vec3& hi, const std::vector<glm::vec3>& pts) {
    lo = glm::vec3{1e9f};
    hi = glm::vec3{-1e9f};
    for (const glm::vec3& p : pts) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    if (hi.x < lo.x) {
        lo = glm::vec3{0.0f};
        hi = glm::vec3{0.0f};
    }
}

} // namespace

std::filesystem::path chargen_source_body() {
    static const std::filesystem::path chosen = [] {
        const char* v = door_value("DFN_BODY_FILE");
        if (v != nullptr && v[0] != '\0') {
            std::fprintf(stderr, "[character] DFN_BODY_FILE: исходное тело экрана и "
                                 "мира — %s\n", v);
            return std::filesystem::path(v);
        }
        return std::filesystem::path(CHARGEN_SOURCE_BODY);
    }();
    return chosen;
}

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


std::uint64_t chargen_pose_hash(const SkinnedCharacter& body) {
    std::vector<glm::vec3> pos;
    body.rest_positions(pos);
    if (pos.empty()) {
        return 0;
    }
    // ДЕСЯТАЯ МИЛЛИМЕТРА: два пути (бленд в памяти и чтение выпечки с диска)
    // обязаны дать одну геометрию до float, но хэш по сырым битам поймал бы
    // и разницу в последнем разряде от порядка сложения; квант держит
    // утверждение «то же тело», а не «те же биты».
    std::string bytes;
    bytes.reserve(pos.size() * 12);
    for (const glm::vec3& p : pos) {
        for (int k = 0; k < 3; ++k) {
            const auto q = static_cast<std::int32_t>(std::lround(p[k] * 10000.0f));
            const char* c = reinterpret_cast<const char*>(&q);
            bytes.append(c, 4);
        }
    }
    return serialization::fnv1a64(bytes);
}

// --- ТЕЛО -------------------------------------------------------------------

bool CharGenBody::load(render::RenderSystem& render_system, platform::IRenderer& renderer,
                       platform::IPhysics* physics, const anim::Rig& rig,
                       const std::filesystem::path& path, bool legacy_rest) {
    auto object = render::read_object(path);
    if (!object || object->skin.empty()) {
        std::fprintf(stderr,
                     "[создание] тело не прочитано или в нём нет потока SKIN: %s\n",
                     path.string().c_str());
        return false;
    }
    source_ = std::move(*object);
    source_path_ = path;
    proportions_ = rig;
    legacy_rest_ = legacy_rest;
    weights_.resize_for(source_.morphs);
    height_m_ = CHARGEN_BODY_HEIGHT_M;
    if (source_.morphs.empty()) {
        // ГРОМКО, НО НЕ ОТКАЗ: тело без секции MORF показать МОЖНО (это
        // выпеченный персонаж), просто крутить у него нечего. Тихое согласие
        // здесь выглядело бы как «ползунки есть, но не работают».
        std::fprintf(stderr,
                     "[создание] у %s нет секции MORF: ползунков телосложения "
                     "не будет (тело уже выпечено?)\n",
                     path.string().c_str());
    }
    return settle(render_system, renderer, physics);
}

void CharGenBody::release(render::RenderSystem& render_system, platform::IRenderer& renderer,
                          platform::IPhysics* physics) {
    release_character(character_, bodies_, render_system, renderer, physics);
    source_ = render::RegistryObject{};
    blended_.clear();
    weights_ = render::MorphState{};
    height_m_ = CHARGEN_BODY_HEIGHT_M;
    lo_ = glm::vec3{0.0f};
    hi_ = glm::vec3{0.0f};
}

render::RegistryObject CharGenBody::baked_object() const {
    render::RegistryObject baked = source_;
    if (!source_.morphs.empty()) {
        std::vector<platform::SkinnedVertex> blended;
        render::blend_morphs(source_.skin.vertices, source_.morphs, weights_.weights,
                             source_.skin.indices, blended);
        baked.skin.vertices = std::move(blended);
    }
    // СЕКЦИЯ MORF СНИМАЕТСЯ — как Creation Kit пишет FaceGeom: мир грузит
    // обычного персонажа и про ползунки не знает вовсе.
    baked.morphs.clear();
    scale_registry_object(baked, chargen_height_scale(height_m_));
    baked.source += " chargen:baked";
    return baked;
}

bool CharGenBody::settle(render::RenderSystem& render_system, platform::IRenderer& renderer,
                         platform::IPhysics* physics) {
    if (source_.skin.empty()) {
        return false;
    }
    CharacterSpec spec;
    spec.proportions = &proportions_;
    spec.legacy_rest = legacy_rest_;
    spec.mesh_asset = CHARGEN_BODY_MESH_ID;
    spec.blade_asset = CHARGEN_BLADE_MESH_ID;
    spec.parts_mesh_first = CHARGEN_PARTS_MESH_ID_FIRST;
    spec.to_world = glm::mat4{1.0f};
    spec.make_capsule = true;
    spec.capsule_feet = glm::vec3{0.0f};
    if (!build_character_object(character_, bodies_, render_system, renderer, physics,
                                baked_object(), source_path_, spec)) {
        std::fprintf(stderr, "[создание] тело экрана не собралось фабрикой\n");
        return false;
    }
    settled_scale_ = chargen_height_scale(height_m_);
    measure_bounds();
    return true;
}

void CharGenBody::measure_bounds() {
    std::vector<glm::vec3> pos;
    character_.rest_positions(pos);
    grow_bound(lo_, hi_, pos);
}

bool CharGenBody::set_weight(std::size_t index, float value) {
    if (index >= source_.morphs.size() || index >= weights_.weights.size()) {
        return false;
    }
    const render::MorphTarget& target = source_.morphs[index];
    const float clamped = std::clamp(value, target.lo, target.hi);
    if (clamped == weights_.weights[index]) {
        return false;
    }
    weights_.weights[index] = clamped;
    return true;
}

bool CharGenBody::set_weight(std::string_view name, float value) {
    const int slot = render::morph_index(source_.morphs, name);
    return slot >= 0 && set_weight(static_cast<std::size_t>(slot), value);
}

void CharGenBody::reset() {
    weights_.resize_for(source_.morphs);
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

bool CharGenBody::apply(render::RenderSystem& render_system, platform::IRenderer& renderer) {
    if (!character_.ready()) {
        return false;
    }
    // БЫСТРАЯ ПОЛОВИНА: та же арифметика, что у baked_object(), но только
    // вершины — скелет от ползунков не меняется, а от роста меняется, и рост
    // здесь трогать нельзя: он ждёт settle(). Вершины масштабируются тем же
    // множителем, что и скелет тела при последнем settle() — иначе меш ушёл
    // бы от костей на разницу ростов до отпускания ручки.
    if (!source_.morphs.empty()) {
        render::blend_morphs(source_.skin.vertices, source_.morphs, weights_.weights,
                             source_.skin.indices, blended_);
    } else {
        blended_ = source_.skin.vertices;
    }
    // Масштаб роста, под которым собрано текущее тело, — тот, что стоял на
    // последнем settle(): переносы привязки в character_ уже умножены на
    // него, и вершины обязаны идти с тем же множителем.
    const float settled_scale = settled_scale_;
    if (std::fabs(settled_scale - 1.0f) > 1e-6f) {
        for (platform::SkinnedVertex& v : blended_) {
            v.position *= settled_scale;
        }
    }
    if (!character_.replace_vertices(render_system, renderer, blended_)) {
        return false;
    }
    measure_bounds();
    return true;
}

void CharGenBody::tick(float dt) {
    if (!character_.ready()) {
        return;
    }
    character_.advance(idle_drive(), glm::vec3{0.0f}, dt);
}

render::RenderSystem::SkinnedDraw CharGenBody::draw(float alpha, platform::IPhysics* physics,
                                                    const glm::mat4& to_world) {
    render::RenderSystem::SkinnedDraw d;
    if (!character_.ready()) {
        return d;
    }
    d = character_.build_draw(/*hide_head=*/false, alpha);
    // КОРОБКИ — ТОЙ ЖЕ МАТРИЦЕЙ, ЧТО МЕШ. В мире это draw.transform; на
    // экране предмет висит в осях камеры, и «мир» коробок — это та же
    // матрица, которой холст переводит его в мир.
    if (physics != nullptr && bodies_.hitboxes.live()) {
        bodies_.hitboxes.update(*physics, character_.hitbox_pose(), to_world);
    }
    return d;
}

anim::BodyGaps CharGenBody::screen_gaps() const {
    // ТОТ ЖЕ ПРИБОР, ЧТО У СМОТРОВОЙ И МИРА (CharacterFactory.h): второй
    // арифметики «где стоит сустав на экране» нет.
    return character_rest_gaps(character_);
}

// --- ПРЕСЕТ -----------------------------------------------------------------

CharGenPreset CharGenBody::preset(std::string name) const {
    CharGenPreset out;
    out.name = std::move(name);
    out.height_m = height_m_;
    out.sliders.reserve(source_.morphs.size());
    for (std::size_t i = 0; i < source_.morphs.size(); ++i) {
        out.sliders.emplace_back(source_.morphs[i].name, weights_.weights[i]);
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
        const int slot = render::morph_index(source_.morphs, name);
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
    if (source_.skin.empty()) {
        std::fprintf(stderr, "[создание] выпечка: тело не загружено\n");
        return false;
    }
    const render::RegistryObject baked = baked_object();
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
