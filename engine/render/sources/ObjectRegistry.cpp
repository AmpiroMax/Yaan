/*
Created: 14:08:2026 - 23:36:19
Last updated: 27:08:2026 - 15:10:00
Module: engine/render
File: engine/render/sources/ObjectRegistry.cpp

Responsibility:
- Implements the .dfo container: write_object / read_object /
  object_content_hash over the Rule 7 section discipline.

Dependencies:
- Uses: ObjectRegistry.h, core serialization.
- Used by: dfn_render target, tools/forge_trees.cpp, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Every multi-byte value goes through explicit little-endian calls; vertices
  are written FIELD BY FIELD because platform::Vertex's layout is a compiler's
  choice, not a format.
- The hash walks the streams in the same order the file stores them; changing
  either order is a format break that silently re-versions every object.
*/
/*
UPD:
- 14:08:2026 - 23:36:19: Created with ObjectRegistry.h.
- 15:08:2026 - 01:46:53: формат v2 — секция BARK; хэш файла v1 сверяется
  правилом v1 (версия — обещание о том, как ЧИТАТЬ, включая как сверять).

- 17:08:2026 - 14:29:43: write_object отказывался писать объект, у которого ЕДИНСТВЕННЫЙ
  непустой поток — bark: проверка «нет потоков» осталась от формата v1 и
  перечисляла три потока, хотя сама функция двумя строками ниже пишет
  четвёртый. Всплыло, когда строительный набор стал текстурным целиком.
  Правило не изменилось — оно теперь называет все потоки, которые стережёт.
- 17:08:2026 - 18:16:18: measure_object() перенесена сюда из tools/check_scene.cpp без изменений.
- 27:08:2026 - 15:10:00: ФОРМАТ v3 — СЕКЦИЯ HOUS (куски постройки с номером
  плитки). Хэш ЛИЧНОСТИ считает её, ТОЛЬКО ЕСЛИ ОНА НЕПУСТА: иначе секция,
  добавленная сегодня, переверсировала бы 2400+ файлов полок, ни один из
  которых не изменился. Правило «версия — обещание о том, как читать» здесь
  прочитано второй раз: v1 сверяется без bark, v2/v3 — с bark, и v3 добавляет
  постройку только там, где она есть.
*/

#include "engine/render/sources/ObjectRegistry.h"

#include "engine/core/serialization/sources/BinaryReader.h"
#include "engine/core/serialization/sources/BinaryWriter.h"
#include "engine/core/serialization/sources/ContentHash.h"

#include <cmath>
#include <bit>
#include <cstdio>

namespace dfn::render {
namespace {

/// 'DFNO' — Daggerfall N object file (.dfo).
inline constexpr uint32_t OBJECT_MAGIC = serialization::make_tag('D', 'F', 'N', 'O');
inline constexpr uint32_t OBJECT_FORMAT_VERSION = 3; // v3: + HOUS submeshes

namespace section {
inline constexpr serialization::SectionTag INFO = serialization::make_tag('I', 'N', 'F', 'O');
inline constexpr serialization::SectionTag WOOD = serialization::make_tag('W', 'O', 'O', 'D');
inline constexpr serialization::SectionTag CARD = serialization::make_tag('C', 'A', 'R', 'D');
inline constexpr serialization::SectionTag GRND = serialization::make_tag('G', 'R', 'N', 'D');
inline constexpr serialization::SectionTag BARK = serialization::make_tag('B', 'A', 'R', 'K');
/// Куски постройки: (surface, tone, emissive) + поток. Одна секция на ВЕСЬ
/// список, а не по секции на кусок: секция — это раздел формата, а не запись,
/// и объект из трёх кусков не имеет права заводить три раздела.
inline constexpr serialization::SectionTag HOUS = serialization::make_tag('H', 'O', 'U', 'S');
} // namespace section

inline constexpr uint16_t SECTION_VERSION = 1;

/// Far above any real object (the whole forest chunk is ~160k triangles) and
/// far below "the machine dies allocating" — the same stance as the world
/// format's bound, for the same reason.
inline constexpr uint64_t MAX_ELEMENTS = 16ull * 1024ull * 1024ull;

/// ТЕЛО ПОТОКА БЕЗ СЕКЦИИ. Отдельно от write_stream, потому что у кусков
/// постройки секция ОДНА на весь список (section::HOUS): раздел формата — это
/// раздел, а не запись, и объект из трёх кусков не заводит три раздела.
void write_stream_body(serialization::BinaryWriter& w, const MeshData& mesh) {
    w.write_u32(static_cast<uint32_t>(mesh.vertices.size()));
    for (const platform::Vertex& v : mesh.vertices) {
        w.write_f32(v.position.x);
        w.write_f32(v.position.y);
        w.write_f32(v.position.z);
        w.write_f32(v.normal.x);
        w.write_f32(v.normal.y);
        w.write_f32(v.normal.z);
        w.write_f32(v.uv.x);
        w.write_f32(v.uv.y);
        w.write_u32(v.color_rgba);
    }
    w.write_u32(static_cast<uint32_t>(mesh.indices.size()));
    for (const uint32_t i : mesh.indices) {
        w.write_u32(i);
    }
}

void write_stream(serialization::BinaryWriter& w, serialization::SectionTag tag,
                  const MeshData& mesh) {
    w.begin_section(tag, SECTION_VERSION);
    write_stream_body(w, mesh);
    w.end_section();
}

[[nodiscard]] bool read_stream(serialization::BinaryReader& r, MeshData& mesh) {
    const uint32_t vertex_count = r.read_u32();
    if (static_cast<uint64_t>(vertex_count) > MAX_ELEMENTS) {
        return false;
    }
    mesh.vertices.resize(vertex_count);
    for (platform::Vertex& v : mesh.vertices) {
        v.position.x = r.read_f32();
        v.position.y = r.read_f32();
        v.position.z = r.read_f32();
        v.normal.x = r.read_f32();
        v.normal.y = r.read_f32();
        v.normal.z = r.read_f32();
        v.uv.x = r.read_f32();
        v.uv.y = r.read_f32();
        v.color_rgba = r.read_u32();
    }
    const uint32_t index_count = r.read_u32();
    if (static_cast<uint64_t>(index_count) > MAX_ELEMENTS) {
        return false;
    }
    mesh.indices.resize(index_count);
    for (uint32_t& i : mesh.indices) {
        i = r.read_u32();
    }
    return r.ok();
}

void hash_stream(serialization::Fnv1a64& h, const MeshData& mesh) {
    h.update_u64(mesh.vertices.size());
    for (const platform::Vertex& v : mesh.vertices) {
        // Float BITS, not values: the identity must be exactly the bytes the
        // file stores, or two byte-identical files could hash apart on a
        // platform with different float formatting rules.
        h.update_u64(std::bit_cast<uint32_t>(v.position.x));
        h.update_u64(std::bit_cast<uint32_t>(v.position.y));
        h.update_u64(std::bit_cast<uint32_t>(v.position.z));
        h.update_u64(std::bit_cast<uint32_t>(v.normal.x));
        h.update_u64(std::bit_cast<uint32_t>(v.normal.y));
        h.update_u64(std::bit_cast<uint32_t>(v.normal.z));
        h.update_u64(std::bit_cast<uint32_t>(v.uv.x));
        h.update_u64(std::bit_cast<uint32_t>(v.uv.y));
        h.update_u64(v.color_rgba);
    }
    h.update_u64(mesh.indices.size());
    for (const uint32_t i : mesh.indices) {
        h.update_u64(i);
    }
}

} // namespace

uint64_t object_content_hash(const RegistryObject& obj) {
    serialization::Fnv1a64 h;
    hash_stream(h, obj.wood);
    hash_stream(h, obj.cards);
    hash_stream(h, obj.ground);
    hash_stream(h, obj.bark);
    // КУСКИ ПОСТРОЙКИ ВХОДЯТ В ЛИЧНОСТЬ, ТОЛЬКО ЕСЛИ ОНИ ЕСТЬ. Хэш — это
    // личность содержимого, и «у объекта нет постройки» обязано хэшироваться
    // ровно так же, как хэшировалось до появления секции: иначе один
    // добавленный раздел молча переверсировал бы все 2400+ файлов полок,
    // заставив перепечь деревья, набор и таблички, ни один из которых не
    // изменился ни на вершину. Это тот же довод, по которому имя и
    // происхождение в хэш не входят.
    if (!obj.house.empty()) {
        h.update_u64(obj.house.size());
        for (const HouseSubmesh& s : obj.house) {
            h.update_u64(s.surface);
            h.update_u64(s.tone);
            h.update_u64(s.emissive ? 1u : 0u);
            hash_stream(h, s.mesh);
        }
    }
    return h.digest();
}

/// The v1 identity: computed WITHOUT the bark stream, exactly as every v1
/// file stored it. A version is a promise about how to READ, and that includes
/// how to verify (Rule 7's migration clause applied to the hash).
[[nodiscard]] static uint64_t object_content_hash_v1(const RegistryObject& obj) {
    serialization::Fnv1a64 h;
    hash_stream(h, obj.wood);
    hash_stream(h, obj.cards);
    hash_stream(h, obj.ground);
    return h.digest();
}

bool write_object(const RegistryObject& obj, const std::filesystem::path& path) {
    // THE BARK STREAM COUNTS TOO. This test was written for the v1 format and
    // kept its three streams when v2 added a fourth that this very function
    // writes two lines below — so an object whose geometry is ENTIRELY
    // textured (every part of the building kit, since it gained its own atlas
    // sheet) was refused as "a name pointing at nothing". The rule is
    // unchanged; it now names every stream it guards.
    if (obj.wood.vertices.empty() && obj.cards.vertices.empty()
        && obj.ground.vertices.empty() && obj.bark.vertices.empty()
        && obj.house.empty()) {
        std::fprintf(stderr, "[dfo] \"%s\": refusing to write an object with no "
                             "streams -- a name pointing at nothing\n",
                     obj.name.c_str());
        return false;
    }
    serialization::BinaryWriter w;
    w.begin_file(OBJECT_MAGIC, OBJECT_FORMAT_VERSION);
    w.begin_section(section::INFO, SECTION_VERSION);
    w.write_string(obj.name);
    w.write_string(obj.kind);
    w.write_string(obj.source);
    w.write_u64(object_content_hash(obj));
    w.end_section();
    write_stream(w, section::WOOD, obj.wood);
    write_stream(w, section::CARD, obj.cards);
    write_stream(w, section::GRND, obj.ground);
    write_stream(w, section::BARK, obj.bark);
    // СЕКЦИЯ ПИШЕТСЯ, ТОЛЬКО ЕСЛИ ЕСТЬ ЧТО ПИСАТЬ. Пустая секция — это лишние
    // байты в каждом из 2400+ файлов полок и повод для читателя думать, что
    // объект «какой-то другой»; а перепечь их сегодняшняя правка не должна.
    if (!obj.house.empty()) {
        w.begin_section(section::HOUS, SECTION_VERSION);
        w.write_u32(static_cast<uint32_t>(obj.house.size()));
        for (const HouseSubmesh& s : obj.house) {
            w.write_u32(s.surface);
            w.write_u32(s.tone);
            w.write_u32(s.emissive ? 1u : 0u);
            write_stream_body(w, s.mesh);
        }
        w.end_section();
    }
    if (!w.ok()) {
        return false;
    }
    return w.save_to_file(path);
}

std::optional<RegistryObject> read_object(const std::filesystem::path& path) {
    serialization::BinaryReader r;
    if (!r.open_file(path, OBJECT_MAGIC)) {
        return std::nullopt;
    }
    if (r.container_version() > OBJECT_FORMAT_VERSION) {
        return std::nullopt; // newer than this build understands: refused
    }
    RegistryObject obj;
    uint64_t stored_hash = 0;
    bool streams_ok = true;
    while (const auto s = r.next_section()) {
        if (s->tag == section::INFO) {
            obj.name = r.read_string();
            obj.kind = r.read_string();
            obj.source = r.read_string();
            stored_hash = r.read_u64();
        } else if (s->tag == section::WOOD) {
            streams_ok = read_stream(r, obj.wood) && streams_ok;
        } else if (s->tag == section::CARD) {
            streams_ok = read_stream(r, obj.cards) && streams_ok;
        } else if (s->tag == section::GRND) {
            streams_ok = read_stream(r, obj.ground) && streams_ok;
        } else if (s->tag == section::BARK) {
            streams_ok = read_stream(r, obj.bark) && streams_ok;
        } else if (s->tag == section::HOUS) {
            const uint32_t count = r.read_u32();
            if (static_cast<uint64_t>(count) > MAX_ELEMENTS) {
                return std::nullopt;
            }
            obj.house.resize(count);
            for (HouseSubmesh& sub : obj.house) {
                sub.surface = r.read_u32();
                sub.tone = r.read_u32();
                sub.emissive = r.read_u32() != 0u;
                streams_ok = read_stream(r, sub.mesh) && streams_ok;
            }
        }
        // Unknown tags: next_section() steps over them (Rule 7).
    }
    if (!r.ok() || !streams_ok) {
        return std::nullopt;
    }
    // THE HASH IS VERIFIED ON EVERY READ. A registry is an index of
    // identities; an object whose bytes disagree with its stored identity is
    // refused whole, because "mostly the object you asked for" is not a thing
    // a registry can return and stay a registry.
    obj.content_hash = r.container_version() >= 2 ? object_content_hash(obj)
                                                   : object_content_hash_v1(obj);
    if (obj.content_hash != stored_hash) {
        std::fprintf(stderr, "[dfo] \"%s\": content hash mismatch (stored %llx, "
                             "computed %llx) -- REFUSED\n",
                     path.string().c_str(),
                     static_cast<unsigned long long>(stored_hash),
                     static_cast<unsigned long long>(obj.content_hash));
        return std::nullopt;
    }
    return obj;
}


ObjectExtent measure_object(const RegistryObject& obj) {
    ObjectExtent e;
    const auto scan = [&e](const MeshData& mesh) {
        for (const platform::Vertex& v : mesh.vertices) {
            e.radius = std::max(e.radius, std::sqrt(v.position.x * v.position.x
                                                    + v.position.z * v.position.z));
            e.bottom = std::min(e.bottom, v.position.y);
            e.top = std::max(e.top, v.position.y);
            e.lo = glm::min(e.lo, glm::vec2{v.position.x, v.position.z});
            e.hi = glm::max(e.hi, glm::vec2{v.position.x, v.position.z});
        }
    };
    const auto scan_solid = [&e](const MeshData& mesh) {
        for (const platform::Vertex& v : mesh.vertices) {
            e.slo = glm::min(e.slo, glm::vec2{v.position.x, v.position.z});
            e.shi = glm::max(e.shi, glm::vec2{v.position.x, v.position.z});
        }
    };
    scan_solid(obj.wood);
    scan_solid(obj.bark);
    scan(obj.wood);
    scan(obj.cards);
    scan(obj.ground);
    scan(obj.bark);
    // КУСКИ ПОСТРОЙКИ МЕРЯЮТСЯ КАК СПЛОШНЫЕ, и это не выбор из двух: постройка
    // и есть то, обо что человек стукается. Без этой пары строк запечённая
    // кровать отвечала бы судье и призраку редактора габаритом 0x0x0 —
    // «объект есть, размера у него нет», — и приёмка полки напечатала бы нули
    // ровно там, где владелец собирается прочесть свой замер.
    for (const HouseSubmesh& s : obj.house) {
        scan_solid(s.mesh);
        scan(s.mesh);
    }
    float solid_top = 0.0f;
    for (const MeshData* m : {&obj.wood, &obj.bark}) {
        for (const platform::Vertex& v : m->vertices) {
            solid_top = std::max(solid_top, v.position.y);
        }
    }
    for (const HouseSubmesh& s : obj.house) {
        for (const platform::Vertex& v : s.mesh.vertices) {
            solid_top = std::max(solid_top, v.position.y);
        }
    }
    // The threshold is PLAYER_STEP_HEIGHT and not a guess — see ObjectExtent.
    e.solid = solid_top > static_cast<float>(config::PLAYER_STEP_HEIGHT);
    return e;
}

} // namespace dfn::render
