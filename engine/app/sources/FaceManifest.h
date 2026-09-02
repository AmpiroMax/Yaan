/*
Module: engine/app
File: engine/app/sources/FaceManifest.h

Responsibility:
- ОПИСЬ ЛИЦЕВЫХ РУЧЕК ЭКРАНА СОЗДАНИЯ, ПРОЧИТАННАЯ ИЗ ДАННЫХ: манифест
  assets/characters/targets/face.targets (группы, цели MPFB, полосы) даёт
  вкладке «Лицо» её разделы и порядок строк, файл калибровки
  assets/characters/targets/face.bands — какие ручки судья лица ВИДИТ
  (сплошной ромб) и какие нет (полый). Экран не знает ни одного имени
  лицевой ручки: всё, что здесь, — строки файла.

Key items:
- face_handle_name(): имя ручки из спецификации целей — ТО ЖЕ правило, что у
  экспортёра (tools/make_body_targets.py, handle_name): «{l,r}-eye-scale-decr/incr»
  → «eye-scale», «head-oval» → «head-oval».
- face_group_id(): id группы из первого стема — «nose», «eye», «head»; ключ
  заголовка раздела строится как "chargen.group.face." + id.
- FacePlan / FaceGroup / FaceHandle: манифест как данные.
- parse_face_manifest() / read_face_manifest(): разбор, ГРОМКИЙ отказ на
  кривой строке (молчаливый пропуск — это ручка, которую два часа ищут).
- read_face_bands(): калибровка судей — полоса и флаг «измерено».

Dependencies:
- Uses: stdlib. Used by: engine/app (CharGen.cpp — описание вкладки,
  AppCharGen.cpp — чтение файлов), tests/app/CharGenTests.cpp.

Notes:
- ПОЧЕМУ ИМЯ ВЫВОДИТСЯ, А НЕ ЗАПИСАНО В МАНИФЕСТЕ. Формат манифеста согласован
  с лидом (шесть колонок, имя ручки — по-русски); имя цели MORF — ASCII, и
  единственный его источник, общий с экспортёром, — стем цели. Два правила
  вывода — две копии; набор app_chargen сверяет, что каждая строка манифеста
  находит свою цель MORF в теле.
- СЛОВА МАНИФЕСТА (подпись RU, пара слов) ЗДЕСЬ НЕ ЧИТАЮТСЯ: видимое слово —
  ключ локализации (правило 5), и подписи лежат в ru.txt как
  morph.slider.<имя> / morph.edge.<имя>.lo/.hi, тем же правилом, что у тела.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- Ни одного имени ручки и ни одного числа полосы в коде (правило 6).
*/

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::app {

/// ГДЕ ЛЕЖАТ МАНИФЕСТ И КАЛИБРОВКА. Названы один раз: их знают экран и
/// экспортёр целей (tools/make_body_targets.py, DEFAULTS).
inline constexpr const char* FACE_MANIFEST_PATH = "assets/characters/targets/face.targets";
inline constexpr const char* FACE_BANDS_PATH = "assets/characters/targets/face.bands";

/// ОДНА РУЧКА ЛИЦА. `lo`/`hi` — полоса МАНИФЕСТА (предложение); полоса,
/// которой ходит ползунок, приходит из секции MORF тела, а не отсюда.
struct FaceHandle {
    std::string name;   ///< имя цели MORF (face_handle_name)
    std::string spec;   ///< спецификация целей, как в файле
    float lo = 0.0f;
    float hi = 1.0f;
    /// СУДЬЯ ВИДИТ ЭТУ РУЧКУ. Ставится калибровкой (face.bands): полосу
    /// мерил судья — ромб сплошной; судья слеп — ромб полый (CHARGEN_UI.md,
    /// Р3). Без файла калибровки ни одна ручка не «измерена».
    bool measured = false;
};

struct FaceGroup {
    std::string id;  ///< face_group_id первой ручки: nose, mouth, eye, ...
    std::vector<FaceHandle> handles;
};

struct FacePlan {
    std::vector<FaceGroup> groups;
    [[nodiscard]] bool empty() const { return groups.empty(); }
    [[nodiscard]] std::size_t handle_count() const;
    [[nodiscard]] const FaceHandle* find(std::string_view name) const;
    [[nodiscard]] FaceHandle* find(std::string_view name);
};

[[nodiscard]] std::string face_handle_name(std::string_view spec);
[[nodiscard]] std::string face_group_id(std::string_view spec);

/// Разбор текста манифеста. Ложь и `why` — на первой же кривой строке.
[[nodiscard]] bool parse_face_manifest(std::string_view text, FacePlan& out, std::string& why);
[[nodiscard]] bool read_face_manifest(const std::filesystem::path& path, FacePlan& out,
                                      std::string& why);

/// ОДНА СТРОКА КАЛИБРОВКИ: «имя lo hi measured|blind [упор...]».
struct FaceBand {
    std::string name;
    float lo = 0.0f;
    float hi = 0.0f;
    bool measured = false;
};

/// Читает файл калибровки; отсутствие файла — законно (ложь без `why`).
[[nodiscard]] bool read_face_bands(const std::filesystem::path& path,
                                   std::vector<FaceBand>& out, std::string& why);
/// Ставит флаг «измерено» ручкам плана по калибровке. Возвращает число
/// ручек, которых калибровка коснулась.
std::size_t face_plan_apply_bands(FacePlan& plan, const std::vector<FaceBand>& bands);

} // namespace dfn::app
