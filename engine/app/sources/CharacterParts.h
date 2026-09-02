/*
Module: engine/app
File: engine/app/sources/CharacterParts.h

Responsibility:
- ЧАСТИ ПЕРСОНАЖА НА ТЕЛЕ (волна «части персонажа»): набор из .dfo с секцией
  PART (волосы, глаза, брови, ресницы, зубы, язык; костюм, ботинки)
  прикрепляется к скиннованному телу — те же суставы, та же палитра, та же
  матрица; каждая часть — свой меш, свои листы и свой материал (вырез по
  альфе, двусторонний свет, рельеф), и список вершин тела, которые она
  закрывает.
- ЧАСТИ СЛЕДУЮТ МОРФАМ (волна «части следуют морфам»): при прикреплении на
  НЕЙТРАЛЬНОМ теле строится карта «вершина части → ближайшие вершины тела и
  шов к ближайшей грани кожи» (MorphFollow.h), и на каждом бленде вершина
  части едет за своей точкой кожи; глаза едут жёстко за центроидом маски
  глаза (face.masks), зубы и язык — за центром рта. Костюм следует животу и
  плечам тем же швом.

Key items:
- CharacterParts::attach(): сверка скелета с телом (имена и порядок
  суставов), масштаб по костям (тело экрана создания масштабировано ростом —
  части едут тем же множителем), меши и листы на GPU, выбор частей по имени;
  карты следования по нейтральному телу и маскам.
- follow(): тело после бленда → вершины частей → GPU. Зовётся с движением
  ползунка, а не покадрово (тот же довод, что у MorphBlend.h).
- follow_report(): прибор — рост расстояния «часть ↔ тело», число вершин
  части под кожей (просвечивание костюма), сдвиг центра глаза от центроида
  маски.
- append_draws(): дро частей поверх дро тела — та же палитра, та же матрица.
- hidden_body_vertices(): объединение списков «часть закрывает» — телу для
  отбрасывания треугольников (SkinnedCharacter).
- parts_arm_door(): DFN_PARTS_ARM — контрольные руки приёмки (правило 47):
  nocutout / flat / nonormal. parts_follow_door(): DFN_PARTS_FOLLOW=0 —
  рука без следования.
- part_selected(): доза выбора по имени (DFN_PARTS, DFN_CLOTHES).
- FaceMasks / read_face_masks(): маски областей лица (номера вершин тела).

Dependencies:
- Uses: engine/render ObjectRegistry (SkinPart), MorphFollow, RenderSystem
  (register_skinned_mesh, replace_skinned_mesh, SkinnedDraw),
  CharacterTextures (sheet_asset), core skeleton, AppDoors.
- Used by: SkinnedCharacter (владелец), CharacterFactory, tests/app.

Notes:
- ЦЕЛЬ MORF АДРЕСУЕТ ТОЛЬКО ПОТОК SKIN ТЕЛА, у части свой поток — и потому
  часть следует не целью, а ПЕРЕНОСОМ: нейтральное тело (то, к которому
  часть печена --like) против тела сейчас, точка кожи под вершиной части
  едет — и вершина с ней. Нейтраль обязана быть ТЕМ ЖЕ телом в ТОМ ЖЕ
  масштабе: число вершин сверяется, масштаб — тот же множитель по костям.
- ПОЧЕМУ НЕЙТРАЛЬ ИЗ ФАЙЛА, А НЕ ИЗ ПАМЯТИ ТЕЛА. Экран после settle собирает
  тело из выпечки (MORF снята, кожа вылеплена), мир грузит player.dfo, тест
  масштабирует объект ростом — ни в одном из трёх «это тело и есть нейтраль»
  не проверить. Файл <тело>.dfo читается один раз на процесс (SkinnedCharacter).
- МАСШТАБ — ПО КОСТЯМ, А НЕ ПО ФАЙЛУ. Тело экрана создания масштабировано к
  росту (scale_registry_object) уже ПОСЛЕ выпечки, и файл частей об этом не
  знает; отношение длин костей тело/части — единственное число, которое
  знают обе стороны. Медиана по всем некорневым суставам, разброс > 1 % —
  отказ: это не то тело.
- НОМЕРА МЕШЕЙ — ПОЛОСА ХОЗЯИНА (CharacterFactory.h: игрок, экран, смотровая
  — по восемь), и восьмая часть сверх полосы отказывается вслух, а не
  забирает чужой номер.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead).
- Каждый отказ — вслух, с именем части и причиной; часть, которой «просто
  нет», — самая дорогая из ошибок этого проекта.
*/

#pragma once

#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/MorphFollow.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dfn::app {

/// МАСКИ ОБЛАСТЕЙ ЛИЦА (assets/characters/targets/face.masks): область →
/// номера вершин тела. Пишет tools/make_body_targets.py, читает судья лица и
/// посадка глаз/зубов. `verts` — число вершин тела, на которое маски
/// написаны: тело с другим числом их не принимает.
struct FaceMasks {
    std::uint32_t verts = 0;
    std::vector<std::pair<std::string, std::vector<std::uint32_t>>> masks;
    [[nodiscard]] const std::vector<std::uint32_t>* find(std::string_view name) const;
    [[nodiscard]] bool empty() const { return masks.empty(); }
};
inline constexpr const char* FACE_MASKS_PATH = "assets/characters/targets/face.masks";
/// False — файла нет или он не маски (сказано вслух); `out` тогда пуст.
[[nodiscard]] bool read_face_masks(const std::filesystem::path& path, FaceMasks& out);

/// КАК ЧАСТЬ СЛЕДУЕТ ЗА ТЕЛОМ.
enum class PartFollow : std::uint8_t {
    None,     ///< не следует (нет нейтрали, дверь закрыта, отказ)
    Transfer, ///< за точкой кожи по шву (волосы, брови, ресницы, одежда)
    Rigid,    ///< как целое, за рамками масок (глаза — две рамки, зубы, язык — одна)
};

/// ОДНА ЖЁСТКАЯ РАМКА: маска вершин тела, её рамка в ресте и какие вершины
/// части за ней едут.
struct RigidGroup {
    std::string mask_name;
    std::vector<std::uint32_t> mask;
    render::RigidFrame rest;
    std::vector<std::uint32_t> vertices;
    bool scale = false;
};

/// ОДНА ПРИКРЕПЛЁННАЯ ЧАСТЬ: что на GPU и с каким материалом её рисовать.
struct AttachedPart {
    std::string name;
    uint32_t mesh_asset = 0;
    uint32_t texture_asset = 0; ///< альбедо (0 — цвет вершин, белый)
    uint32_t normal_asset = 0;  ///< лист нормалей (0 — без рельефа)
    bool cutout = false;
    float alpha_cutoff = 0.5f;
    bool two_sided = false;
    std::size_t triangles = 0;
    /// Вершины тела, которые часть закрывает (номера в SKIN тела).
    std::vector<uint32_t> hide_body_vertices;
    // --- СЛЕДОВАНИЕ ЗА МОРФАМИ -------------------------------------------
    PartFollow follow = PartFollow::None;
    /// Рест части в масштабе тела (то, что легло на GPU при прикреплении) и
    /// её треугольники — для переноса, нормалей и перекладки.
    std::vector<platform::SkinnedVertex> rest;
    std::vector<uint32_t> indices;
    /// Карта соседей и швов к коже (Transfer; у Rigid — тоже, ради прибора).
    render::FollowMap map;
    std::vector<RigidGroup> rigid;
    /// Вершины после последнего follow(); пусто — часть стоит в ресте.
    std::vector<platform::SkinnedVertex> now;
};

/// КОНТРОЛЬНЫЕ РУКИ ПРИЁМКИ (DFN_PARTS_ARM, читается один раз): что из трёх
/// добавок материала выключить у ВСЕХ частей. Умолчание — всё включено.
struct PartsArm {
    bool cutout = true;    ///< nocutout — карточки сплошные (и тень сплошная)
    bool two_sided = true; ///< flat — изнанка пряди чёрная
    bool normal = true;    ///< nonormal — без листа нормалей волос
};
[[nodiscard]] PartsArm parts_arm_door();
/// DFN_PARTS_FOLLOW=0 — части морфам не следуют (рука «до», правило 47).
[[nodiscard]] bool parts_follow_door();

/// ВЫБОР ЧАСТИ ПО ДОЗЕ: nullptr/пусто — все; "none"/"0" — ни одной; иначе
/// список имён через запятую. Имя, которого нет в файле, печатается вслух
/// ПРИ ПРИКРЕПЛЕНИИ (attach), а не здесь.
[[nodiscard]] bool part_selected(const char* selection, std::string_view name);

/// ПРИБОР СЛЕДОВАНИЯ ОДНОЙ ЧАСТИ, метры и штуки.
struct PartFollowReport {
    std::string name;
    PartFollow follow = PartFollow::None;
    /// Наибольший РОСТ расстояния «вершина части ↔ кожа» против реста
    /// (follow_gap_change): часть отстала от кожи. У переноса — доли
    /// миллиметра; без переноса — ровно то, на сколько тело ушло из-под части.
    float gap_grow_m = 0.0f;
    /// Наибольшее СЖАТИЕ того же расстояния: кожа подошла к части (справочно).
    float gap_shrink_m = 0.0f;
    /// То же до ближайшей ВЕРШИНЫ тела (follow_vertex_gap_error), справочно.
    float vertex_gap_error_m = 0.0f;
    /// Сколько вершин части под кожей глубже 1 мм (follow_penetrations) в
    /// ресте и сейчас: рост — «тело проступило сквозь костюм». Знаменатель —
    /// вершины части.
    std::size_t under_skin_rest = 0;
    std::size_t under_skin_now = 0;
    std::size_t vertices = 0;
    /// У жёстких частей: наибольший сдвиг «центр группы ↔ центроид маски»
    /// против реста; ноль — глаз сидит в глазнице, куда бы та ни уехала.
    float rigid_offset_m = 0.0f;
};

class CharacterParts {
public:
    /// Прикрепляет части `object` (секция PART) к телу со скелетом
    /// `body_skeleton`; меши берут номера `first_mesh_id`.. по порядку, не
    /// больше `max_parts` на ВСЕ вызовы attach этого набора. `selection` —
    /// доза выбора по имени (part_selected). `label` — имя файла для
    /// журнала и поиска листов. `neutral_body` и `neutral_indices` — тело, к
    /// которому части печены, В МАСШТАБЕ ФАЙЛА (сюда же применится множитель
    /// по костям), и его треугольники; пусто — части не следуют морфам
    /// (сказано вслух). `masks` — маски лица
    /// для жёсткой посадки глаз/зубов; nullptr — они следуют переносом.
    /// False — ни одна часть не прикреплена (причина вслух); части,
    /// прикреплённые раньше, остаются.
    [[nodiscard]] bool attach(render::RenderSystem& render_system,
                              platform::IRenderer& renderer,
                              const render::RegistryObject& object,
                              const std::filesystem::path& label,
                              const skel::Skeleton& body_skeleton, uint32_t first_mesh_id,
                              uint32_t max_parts, const char* selection,
                              std::span<const platform::SkinnedVertex> neutral_body = {},
                              std::span<const uint32_t> neutral_indices = {},
                              const FaceMasks* masks = nullptr);
    /// Снимает все меши; листы остаются в кэше процесса (CharacterTextures).
    void release(render::RenderSystem& render_system, platform::IRenderer& renderer);

    /// ТЕЛО ПОСЛЕ БЛЕНДА → ЧАСТИ → GPU. `body_now` — вершины тела в том же
    /// пространстве и масштабе, что нейтраль (bind, масштаб тела). Части без
    /// следования не трогаются. False — хоть одна перекладка не удалась
    /// (вслух). Стоимость — last_follow_ms().
    bool follow(render::RenderSystem& render_system, platform::IRenderer& renderer,
                std::span<const platform::SkinnedVertex> body_now);
    /// Есть ли хоть одна часть, следующая за телом.
    [[nodiscard]] bool following() const;
    [[nodiscard]] double last_follow_ms() const { return last_follow_ms_; }
    /// Нейтральное тело в масштабе частей (пусто — следования нет) и его
    /// треугольники.
    [[nodiscard]] const std::vector<platform::SkinnedVertex>& neutral() const {
        return neutral_;
    }
    [[nodiscard]] const std::vector<uint32_t>& neutral_indices() const {
        return neutral_indices_;
    }
    /// Прибор по каждой части для тела `body_now` (то, что ушло в follow).
    [[nodiscard]] std::vector<PartFollowReport>
    follow_report(std::span<const platform::SkinnedVertex> body_now) const;

    [[nodiscard]] const std::vector<AttachedPart>& parts() const { return parts_; }
    [[nodiscard]] bool empty() const { return parts_.empty(); }
    /// Дро частей поверх дро тела: та же палитра, та же матрица. Тело
    /// берётся ПО ЗНАЧЕНИЮ нарочно: вызывающий передаёт out[0], и ссылка на
    /// него умерла бы на первом же push_back с переездом вектора.
    void append_draws(render::RenderSystem::SkinnedDraw body,
                      std::vector<render::RenderSystem::SkinnedDraw>& out) const;
    /// Объединение «часть закрывает» по всем прикреплённым частям, отсортировано.
    [[nodiscard]] std::vector<uint32_t> hidden_body_vertices() const;
    /// Множитель, которым вершины частей последнего attach доведены до тела.
    [[nodiscard]] float last_scale() const { return last_scale_; }

private:
    /// Карты и рамки одной части по нейтрали и маскам (вслух, если чего-то
    /// не хватает, — часть остаётся без следования).
    void bind_follow(AttachedPart& part, const std::string& label, const FaceMasks* masks);
    /// Вершины части для тела `body_now` в `out` (рест, если не следует).
    void pose_part(const AttachedPart& part,
                   std::span<const platform::SkinnedVertex> body_now,
                   std::vector<platform::SkinnedVertex>& out) const;

    std::vector<AttachedPart> parts_;
    float last_scale_ = 1.0f;
    /// Нейтральное тело В МАСШТАБЕ ЧАСТЕЙ: одно на набор, ставится первым
    /// attach с нейтралью; следующие сверяют число вершин.
    std::vector<platform::SkinnedVertex> neutral_;
    std::vector<uint32_t> neutral_indices_;
    double last_follow_ms_ = 0.0;
};

} // namespace dfn::app
