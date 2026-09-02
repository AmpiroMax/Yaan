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

Key items:
- CharacterParts::attach(): сверка скелета с телом (имена и порядок
  суставов), масштаб по костям (тело экрана создания масштабировано ростом —
  части едут тем же множителем), меши и листы на GPU, выбор частей по имени.
- append_draws(): дро частей поверх дро тела — та же палитра, та же матрица.
- hidden_body_vertices(): объединение списков «часть закрывает» — телу для
  отбрасывания треугольников (SkinnedCharacter).
- parts_arm_door(): DFN_PARTS_ARM — контрольные руки приёмки (правило 47):
  nocutout / flat / nonormal.
- part_selected(): доза выбора по имени (DFN_PARTS, DFN_CLOTHES).

Dependencies:
- Uses: engine/render ObjectRegistry (SkinPart), RenderSystem
  (register_skinned_mesh, SkinnedDraw), CharacterTextures (sheet_asset),
  core skeleton, AppDoors.
- Used by: SkinnedCharacter (владелец), CharacterFactory, tests/app.

Notes:
- ЧАСТИ НЕ МОРФЯТСЯ. Цель MORF адресует вершины потока SKIN тела по номеру,
  у части свой поток; голова едет целиком с суставом головы. Глаз, уехавший
  от морфа лица, — известный хвост этой волны, а не свойство формата.
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
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::app {

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
};

/// КОНТРОЛЬНЫЕ РУКИ ПРИЁМКИ (DFN_PARTS_ARM, читается один раз): что из трёх
/// добавок материала выключить у ВСЕХ частей. Умолчание — всё включено.
struct PartsArm {
    bool cutout = true;    ///< nocutout — карточки сплошные (и тень сплошная)
    bool two_sided = true; ///< flat — изнанка пряди чёрная
    bool normal = true;    ///< nonormal — без листа нормалей волос
};
[[nodiscard]] PartsArm parts_arm_door();

/// ВЫБОР ЧАСТИ ПО ДОЗЕ: nullptr/пусто — все; "none"/"0" — ни одной; иначе
/// список имён через запятую. Имя, которого нет в файле, печатается вслух
/// ПРИ ПРИКРЕПЛЕНИИ (attach), а не здесь.
[[nodiscard]] bool part_selected(const char* selection, std::string_view name);

class CharacterParts {
public:
    /// Прикрепляет части `object` (секция PART) к телу со скелетом
    /// `body_skeleton`; меши берут номера `first_mesh_id`.. по порядку, не
    /// больше `max_parts` на ВСЕ вызовы attach этого набора. `selection` —
    /// доза выбора по имени (part_selected). `label` — имя файла для
    /// журнала и поиска листов. False — ни одна часть не прикреплена (причина
    /// вслух); части, прикреплённые раньше, остаются.
    [[nodiscard]] bool attach(render::RenderSystem& render_system,
                              platform::IRenderer& renderer,
                              const render::RegistryObject& object,
                              const std::filesystem::path& label,
                              const skel::Skeleton& body_skeleton, uint32_t first_mesh_id,
                              uint32_t max_parts, const char* selection);
    /// Снимает все меши; листы остаются в кэше процесса (CharacterTextures).
    void release(render::RenderSystem& render_system, platform::IRenderer& renderer);

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
    std::vector<AttachedPart> parts_;
    float last_scale_ = 1.0f;
};

} // namespace dfn::app
