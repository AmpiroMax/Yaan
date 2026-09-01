/*
Module: engine/render
File: engine/render/sources/MorphBlend.h

Responsibility:
- CPU-БЛЕНД МОРФ-ЦЕЛЕЙ В РЕСТ-ПОЗУ: взять вершины привязки, прибавить
  Σ(вес × дельта) по целям секции MORF, ПЕРЕСЧИТАТЬ НОРМАЛИ по сдвинутой
  геометрии и отдать новый массив вершин. Ничего больше: скиннинг об этом не
  знает и знать не должен.

Key items:
- MorphState: веса ползунков, параллельные списку целей объекта.
- blend_morphs(): рест-поза + дельты -> вершины, готовые к загрузке.
- morph_moved_vertices(): сколько вершин сдвинулось больше порога — прибор, а
  не украшение (см. ниже).

WHY THE BLEND IS BEFORE THE SKINNING AND NOT INSIDE IT. Морф меняет ФОРМУ
ПОКОЯ, скиннинг — ПОЗУ. Сложи их в один проход, и каждая вершина будет
пересчитываться каждый кадр ради данных, которые меняются раз в секунду, когда
человек тянет ползунок; разведи — и морф стоит 0.03 мс НА ДВИЖЕНИЕ РУЧКИ, а
кадр не платит ничего (docs/research/CHARACTER_EDITOR_TOOLS.md §3в, замер
ресёрчера). Заодно это делает утверждение «морф не рвёт скиннинг» проверяемым:
joints и weights вершины blend_morphs НЕ ТРОГАЕТ, и это видно из подписи.

WHY THE SUM IS IN THE TARGETS' STORED ORDER. Сложение float не ассоциативно.
Пресет — это только числа ползунков, а «выпечка воспроизводима байт-в-байт»
держится ровно на том, что порядок слагаемых зафиксирован форматом (цели
отсортированы по имени экспортёром) и НЕ зависит от того, в каком порядке
пользователь двигал ручки.

WHY NORMALS ARE A DIFFERENCE AND NOT A REPLACEMENT. Дельта — это не поворот:
нормаль раздутого живота не есть нормаль плоского, и перенос старой нормали
даёт ровно тот «свет не по форме», из-за которого импортёр уже один раз
пересчитывал нормали после --reshape. Но пересчитать её ЗДЕСЬ и положить как
есть нельзя: вершины .dfo живут в пространстве привязки, а нормали в файле
посчитаны по РЕСТ-ПОЗЕ и занесены обратно — у тела, подогнанного под канон,
это разные вещи, и тело сменило бы освещение при всех ползунках на нуле.
Поэтому кладётся РАЗНИЦА: n_out = normalize(n_файла + (n1 − n0)), где n0 и n1 —
одна и та же площадно-взвешенная формула до и после сдвига. При нулевых весах
выход побитово равен входу — и это проверяется тестом, а не обещается.

Dependencies:
- Uses: ObjectRegistry.h (MorphTarget), platform SkinnedVertex.
- Used by: engine/app (SkinnedCharacter), tools/check_morph.cpp,
  tests/render/MorphTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЭТО ЧИСТАЯ ФУНКЦИЯ. Ни окна, ни рендера, ни файла: одни и те же входы дают
  побитово один и тот же выход на любой машине — иначе приёмка «пресет
  воспроизводим» проверяла бы погоду.
*/

#pragma once

#include "engine/render/sources/ObjectRegistry.h"

#include "engine/platform/render/interfaces/IRenderer.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace dfn::render {

/// ЧТО СЕЙЧАС НАКРУЧЕНО НА ПОЛЗУНКАХ. Вектор весов, параллельный
/// RegistryObject::morphs — по индексу, а не по имени, потому что имя уже
/// названо целью и вторая копия имён разошлась бы с первой.
///
/// ЭТО И ЕСТЬ ПРЕСЕТ. JSON пресета — сериализация ровно этого состояния
/// (имя цели -> вес), и ничего кроме: тело выпекается из чисел, а не из
/// картинки, поэтому один и тот же пресет даёт один и тот же файл.
struct MorphState {
    std::vector<float> weights;

    void resize_for(std::span<const MorphTarget> targets) {
        weights.assign(targets.size(), 0.0f);
    }
    [[nodiscard]] bool empty() const { return weights.empty(); }
};

/// НОМЕР ЦЕЛИ ПО ИМЕНИ, или -1. Одно определение на дозу DFN_MORPH, на пресет
/// и на панель (правило 32).
[[nodiscard]] int morph_index(std::span<const MorphTarget> targets,
                              std::string_view name);

/// РЕСТ-ПОЗА ПЛЮС ДЕЛЬТЫ. `out` получает копию `rest` со сдвинутыми позициями и
/// ПЕРЕСЧИТАННЫМИ нормалями; joints/weights/uv/цвет переносятся как есть.
///
/// `indices` нужны только нормалям. Пустые индексы — законный вызов (нормали
/// остаются исходными), и это НЕ тихий провал: тело без треугольников не имеет
/// нормалей по построению.
void blend_morphs(std::span<const platform::SkinnedVertex> rest,
                  std::span<const MorphTarget> targets,
                  std::span<const float> weights,
                  std::span<const std::uint32_t> indices,
                  std::vector<platform::SkinnedVertex>& out);

/// СКОЛЬКО ВЕРШИН УЕХАЛО ДАЛЬШЕ `threshold_m` — и на сколько уехала худшая.
/// Прибор приёмки «дельта не течёт из региона» и одновременно сторож тихого
/// брака §2б записки: «двенадцать целей готово» при нуле сдвигов — это брак,
/// который выглядит как успех.
struct MorphSpread {
    std::size_t moved = 0;
    float worst_m = 0.0f;
    float lowest_y = 0.0f;  ///< самая низкая сдвинутая вершина (стопы!)
    float highest_y = 0.0f;
};
[[nodiscard]] MorphSpread morph_spread(std::span<const platform::SkinnedVertex> rest,
                                       const MorphTarget& target, float weight,
                                       float threshold_m);

} // namespace dfn::render
