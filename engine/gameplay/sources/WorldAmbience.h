/*
Created: 28:08:2026 - 14:20:00
Last updated: 28:08:2026 - 14:20:00
Module: engine/gameplay
File: engine/gameplay/sources/WorldAmbience.h

Responsibility:
- ФОНОВЫЙ ЗВУК МИРА КАК ИЗЛУЧАТЕЛИ, А НЕ КАК ФОН (заказ владельца 28.08:
  «не должно быть просто так фонового шума — у звука всегда должен быть
  источник, и звук должен распространяться по физике; фоновый шум привязать к
  деревьям, чтобы затихал с удалением от деревьев»).
- Кроны деревьев и русла воды становятся точками мира, у каждой точки —
  громкость от расстояния, ветра и размера, и срез верха от того, что стоит
  между ней и ухом.

Key items:
- CrownSource / WaterCourse: что мир кладёт на вход (крона, русло).
- AmbienceCluster + cluster_crowns(): рощи. Сотня деревьев — это не сотня
  голосов, это несколько групп; точка излучения группы — её БЛИЖАЙШАЯ крона.
- distance_gain() / leaves_base_gain() / water_base_gain() / occluded_mix():
  ЧИСТЫЕ функции модели. Приёмка меряет их, а не устройство, — потому что
  громкость на звуковой карте нельзя ни считать, ни сравнить с прошлой неделей.
- WorldAmbience: голоса, их бюджет, перекличка ветровых ступеней, прибор.

Dependencies:
- Uses: platform IAudio, glm, stdlib. Физику НЕ включает: окклюзия приходит
  замыканием OcclusionProbe (луч кладёт тот, у кого есть IPhysics).
- Used by: engine/app (заводит, кормит сценой, обновляет), tests/sim.

Notes:
- ЗАТУХАНИЕ СЧИТАЕМ МЫ, А НЕ miniaudio, и это решение, а не недосмотр.
  Спатиализация бэкенда оставлена ради НАПРАВЛЕНИЯ (панорама, «слева роща»),
  а её собственное затухание отключено тем, что min_distance ставится заведомо
  больше любого игрового расстояния. Причина: кривая громкости — то самое, что
  владелец заказал измерить, и у измеряемого числа обязан быть ОДИН автор.
  Две кривые (наша и бэкендовская) перемножились бы в третью, которую не
  предъявишь ни на графике, ни в тесте.
- ЗАКОН РАССТОЯНИЯ — 1/r с полкой у самой кроны и окном до нуля на пределе
  слышимости: −6 дБ на удвоение расстояния (физика точечного источника), а не
  линейная спадающая прямая, которая на середине пути всегда громче правды.
- СЛОЖЕНИЕ РОЩИ — ПО МОЩНОСТИ: amplitude = sqrt(Σ r²). Десять крон звучат
  громче одной в sqrt(10) раз, а не в десять: некогерентные источники
  складываются мощностями. Сумма амплитуд дала бы рощу громче грозы.
- ИЗМЕРЕННЫЕ УРОВНИ ФАЙЛОВ ВПИСАНЫ ЧИСЛАМИ (замер музыкальной сессии,
  docs/reports/world-ambience.html): ступени ветра растут ~3 дБ НАМЕРЕННО и
  здесь не выравниваются; выравнивается только РАЗНИЦА ПОРОД (хвойные
  записаны на 4.3 дБ громче лиственных), чтобы порода слышалась тембром, а не
  громкостью. Ни один множитель усиления не больше 1: пик файлов −6 дБ, и
  подъём выше единицы — это клиппинг на самой громкой роще.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- НЕ ЗАВОДИТЬ ЗДЕСЬ БЕЗЫСТОЧНИКОВЫХ ЗВУКОВ. Единственный способ услышать что-то
  из этого файла — иметь точку в мире; правило «у каждого излучателя есть
  хозяин» (engine/platform/audio/docs/README.md) этим и продолжено: хозяин
  теперь обязан быть ТОЧКОЙ.
*/
/*
UPD:
- 28:08:2026 - 14:20:00: Создан зоной «звук от источника». Заменяет
  gameplay::WindLoop — безысточниковый ветер, игравший «везде».
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "engine/platform/audio/interfaces/IAudio.h"

namespace dfn::gameplay {

// ---------------------------------------------------------------------------
// ЧИСЛА МОДЕЛИ. Все в одном месте, потому что каждое из них попадает в отчёт
// приёмки: «кривая по расстоянию» — это они.
// ---------------------------------------------------------------------------

/// Сторона ячейки, в которой кроны считаются ОДНОЙ рощей. 24 м выбрано по
/// городу: соседние деревья улицы сливаются, деревья через площадь — нет.
inline constexpr float AMBIENCE_CLUSTER_CELL_M = 24.0f;
/// Ниже этой верхушки крона не шелестит, а лежит: цветы, трава, стелющийся
/// можжевельник. Без порога карта с четырьмя тысячами травинок дала бы
/// четыре тысячи излучателей, и все — под ногами.
inline constexpr float AMBIENCE_CROWN_MIN_TOP_M = 1.2f;
/// sqrt(Σ r²), при котором роща звучит в полную силу. 5 м — это чуть больше
/// ОДНОЙ взрослой кроны радиусом 4 м, и это не опечатка: вблизи одинокий дуб
/// шумит почти как роща (ухо стоит в ближнем поле его же листвы), а
/// преимущество рощи — в ДАЛЬНОСТИ (cluster_far_m), а не в громкости под ней.
/// Возьми сюда 8 м, и одинокое дерево у дома окажется на 6 дБ тише самого
/// себя.
inline constexpr float AMBIENCE_CROWN_REF_M = 5.0f;

/// Общий множитель листвы и воды (запас до клиппинга; файлы −24.7 дБ средних).
inline constexpr float AMBIENCE_LEAVES_GAIN = 0.85f;
inline constexpr float AMBIENCE_WATER_GAIN = 0.85f;
/// Породная поправка: хвойные записи на 4.3 дБ громче лиственных (замер
/// музыкальной сессии), и разницу пород слушают тембром, а не уровнем.
inline constexpr float AMBIENCE_CONIFER_TRIM = 0.61f;
/// Широкая река записана на 6.1 дБ громче ручья; размер реки уже входит в
/// модель шириной русла, поэтому уровень выравнивается по ТИХОМУ файлу.
inline constexpr float AMBIENCE_RIVER_TRIM = 0.49f;

/// Ветер: ниже WIND_SILENT листва молчит, выше WIND_FULL громче не становится.
/// Шкала — та же, что гнёт листву в кадре (render::apply_wind: 0…0.8, среднее
/// 0.35), читаем её, а не выводим заново (Rule 35).
inline constexpr float AMBIENCE_WIND_SILENT = 0.05f;
inline constexpr float AMBIENCE_WIND_FULL = 0.60f;
/// Границы ветровых ступеней записи (три файла на породу) и гистерезис, чтобы
/// на границе не щёлкать файлами туда-сюда.
inline constexpr float AMBIENCE_STEP_1_2 = 0.28f;
inline constexpr float AMBIENCE_STEP_2_3 = 0.55f;
inline constexpr float AMBIENCE_STEP_HYSTERESIS = 0.04f;

/// Полка у источника (в её пределах громкость не растёт) и предел слышимости.
inline constexpr float AMBIENCE_NEAR_MIN_M = 3.0f;
inline constexpr float AMBIENCE_NEAR_MAX_M = 15.0f;
inline constexpr float AMBIENCE_FAR_BASE_M = 30.0f;
inline constexpr float AMBIENCE_FAR_PER_SQRT_CROWN_M = 10.0f;
inline constexpr float AMBIENCE_FAR_MAX_M = 110.0f;
/// Доля предела, на которой громкость доводится до РОВНОГО НУЛЯ. Без окна
/// источник пропадал бы ступенькой, и это слышно как щелчок.
inline constexpr float AMBIENCE_EDGE_FADE = 0.35f;

/// Стена между ухом и источником: во сколько раз тише и где режется верх.
inline constexpr float AMBIENCE_OCCLUDED_GAIN = 0.30f;      // −10.5 дБ
inline constexpr float AMBIENCE_OCCLUDED_CUTOFF_HZ = 800.0f;
inline constexpr float AMBIENCE_OPEN_CUTOFF_HZ = 20000.0f;  // прозрачно
/// Скорость, с которой перекрытие нарастает и спадает (1/с). Луч — это «да
/// или нет», а ухо на мгновенное «да/нет» отвечает щелчком.
inline constexpr float AMBIENCE_OCCLUSION_RATE = 3.0f;

/// В ЛОКАЦИИ (карман интерьера): улица глухая. Два числа — вглубь дома и в
/// открытом проёме; между ними ведёт открытость двери.
inline constexpr float AMBIENCE_INDOOR_GAIN = 0.08f;        // −22 дБ
inline constexpr float AMBIENCE_INDOOR_DOOR_GAIN = 0.35f;   // −9 дБ
inline constexpr float AMBIENCE_INDOOR_CUTOFF_HZ = 420.0f;
inline constexpr float AMBIENCE_INDOOR_DOOR_CUTOFF_HZ = 1400.0f;

/// БЮДЖЕТ ГОЛОСОВ. Шесть рощ и два русла — восемь мест; на переклички
/// (смена ветровой ступени, смена рощи под ухом) каждое место вправе держать
/// второй, затухающий голос, поэтому потолок пути — 16 живых ma_sound.
inline constexpr std::size_t AMBIENCE_LEAF_SLOTS = 6;
inline constexpr std::size_t AMBIENCE_WATER_SLOTS = 2;
inline constexpr std::size_t AMBIENCE_MAX_VOICES =
    2 * (AMBIENCE_LEAF_SLOTS + AMBIENCE_WATER_SLOTS);
/// Длительность переклички, секунды.
inline constexpr float AMBIENCE_FADE_S = 0.6f;
/// Ниже этой громкости голос не заводится и снимается: заводить неслышимое —
/// это тратить место в бюджете на тишину.
inline constexpr float AMBIENCE_AUDIBLE_FLOOR = 0.002f;

// ---------------------------------------------------------------------------
// ВХОД: что мир кладёт на стол
// ---------------------------------------------------------------------------

/// ОДНА КРОНА. Позиция — центр листвы, а не комель: шелестит крона.
struct CrownSource {
    glm::vec3 position{0.0f};
    float radius_m = 0.0f; ///< горизонтальный радиус листвы
    float top_m = 0.0f;    ///< верхушка над землёй (порог «это не трава»)
    bool conifer = false;  ///< хвойное — другой файл, не другая громкость
};

/// РУСЛО: ломаная по воде. Излучатель — БЛИЖАЙШАЯ ТОЧКА РУСЛА, а не его
/// середина: у реки нет одного места, откуда она звучит.
struct WaterCourse {
    std::vector<glm::vec3> points;
    float width_m = 6.0f;
};

/// РОЩА — то, что звучит одним голосом.
struct AmbienceCluster {
    glm::vec3 centroid{0.0f};
    float extent_m = 0.0f;  ///< радиус группы вокруг центроида
    float amplitude = 0.0f; ///< sqrt(Σ r²) — сложение по мощности
    std::uint32_t crowns = 0;
    bool conifer = false;   ///< порода большинства (по сумме радиусов)
    std::vector<glm::vec3> members;
};

// ---------------------------------------------------------------------------
// МОДЕЛЬ (чистые функции — приёмка меряет ИХ)
// ---------------------------------------------------------------------------

/// Породу называет ИМЯ ОБЪЕКТА сцены — соглашение содержимого, а не список в
/// коде: новая хвойная порода начинает звучать хвойной, как только её назвали.
[[nodiscard]] bool species_is_conifer(std::string_view object_name);

/// Рощи из крон: сетка ячейкой cell_m, кроны ниже порога отброшены.
[[nodiscard]] std::vector<AmbienceCluster>
cluster_crowns(std::span<const CrownSource> crowns,
               float cell_m = AMBIENCE_CLUSTER_CELL_M);

/// Точка излучения рощи для этого слушателя: её ближайшая крона.
[[nodiscard]] glm::vec3 cluster_emitter_point(const AmbienceCluster& cluster,
                                              const glm::vec3& listener);

/// Ближайшая точка ломаной русла (по отрезкам, не по вершинам).
[[nodiscard]] glm::vec3 nearest_point_on_course(std::span<const glm::vec3> points,
                                                const glm::vec3& listener);

/// ЗАКОН РАССТОЯНИЯ. 1 внутри near_m, дальше 1/r, ровный 0 на far_m и за ним.
[[nodiscard]] float distance_gain(float distance_m, float near_m, float far_m);

/// Полка и предел слышимости рощи — от её размера и числа крон.
[[nodiscard]] float cluster_near_m(const AmbienceCluster& cluster);
[[nodiscard]] float cluster_far_m(const AmbienceCluster& cluster);

/// Множитель ветра 0..1 по шкале render::apply_wind.
[[nodiscard]] float wind_gain(float wind_strength);

/// Громкость рощи БЕЗ окклюзии и без стен: то, что слышно в чистом поле.
[[nodiscard]] float leaves_base_gain(const AmbienceCluster& cluster, float distance_m,
                                     float wind_strength);
/// То же для воды. Ветер воде безразличен — река шумит и в штиль.
[[nodiscard]] float water_base_gain(float width_m, float distance_m);
[[nodiscard]] float water_near_m(float width_m);
[[nodiscard]] float water_far_m(float width_m);

/// Ветровая ступень записи (0..2) с гистерезисом от предыдущей.
[[nodiscard]] int wind_step(float wind_strength, int previous_step);

/// ЧТО ДЕЛАЕТ СО ЗВУКОМ ДОРОГА ДО УХА. occlusion 0 — чистая видимость, 1 —
/// стена; indoor_openness < 0 — слушатель на улице.
struct AmbienceMix {
    float gain = 1.0f;      ///< множитель к базовой громкости
    float cutoff_hz = 0.0f; ///< 0 = фильтр не нужен (прозрачно)
};
[[nodiscard]] AmbienceMix occluded_mix(float occlusion, bool indoors,
                                       float door_openness);

// ---------------------------------------------------------------------------
// ГОЛОСА
// ---------------------------------------------------------------------------

class WorldAmbience {
public:
    /// Загруженные записи мира. Пути — соглашение музыкальной сессии
    /// (assets/audio/world/*.ogg, 48 кГц МОНО: точечному источнику стерео
    /// вредно, спатиализация панорамит сама).
    struct Bank {
        platform::SoundHandle leaves[2][3]{}; ///< [0]=лиственные [1]=хвойные
        platform::SoundHandle stream_small{};
        platform::SoundHandle river_wide{};
        platform::BusHandle bus{}; ///< ШИНА МИРА: хозяин всего, что здесь звучит
        [[nodiscard]] bool has_leaves() const;
        [[nodiscard]] bool has_water() const;
    };

    /// Загружает набор из `audio_dir`/world. Отсутствующий файл — не поломка:
    /// его излучатель просто молчит (Rule 3 в духе).
    [[nodiscard]] static Bank load_bank(platform::IAudio& audio,
                                        std::string_view audio_dir,
                                        platform::BusHandle bus);

    /// ЛУЧИ ДО ИСТОЧНИКА, вынесенные наружу замыканием: 0 — видно насквозь,
    /// 1 — перекрыто полностью, между — перекрыто частью лучей. Так gameplay
    /// не знает ни про Jolt, ни про слои, а тест кладёт стену одной строкой.
    ///
    /// ДОЛЯ, А НЕ «ДА/НЕТ», ПОТОМУ ЧТО СТВОЛ — НЕ СТЕНА. Деревья стоят в
    /// физическом мире тем же слоем, что и дома, и одиночный луч в лесу
    /// щёлкал бы «перекрыто/свободно» на каждом шаге. Несколько лучей в
    /// стороны дают тонкому стволу половину, а стене дома — единицу.
    using OcclusionProbe = std::function<float(const glm::vec3& from, const glm::vec3& to)>;

    /// ОТКУДА СЛУШАЮТ МИР. Снаружи это глаз. В локации — точка ВОЗВРАТА у
    /// двери (карман интерьера лежит на километр ниже мира, и слушать оттуда
    /// значило бы получить тишину по расстоянию — то есть правильный ответ по
    /// неправильной причине).
    struct Listener {
        glm::vec3 position{0.0f};
        bool indoors = false;
        float door_openness = 0.0f; ///< 0 — вглубь дома, 1 — в открытом проёме
    };

    /// Прибор: строка на каждый живой излучатель (доза DFN_AMBIENCE_LOG).
    struct Emitter {
        glm::vec3 at{0.0f};
        float distance_m = 0.0f;
        float gain = 0.0f;
        float cutoff_hz = 0.0f;
        float occlusion = 0.0f;
        std::uint32_t crowns = 0;
        bool water = false;
        bool conifer = false;
    };

    void set_bank(const Bank& bank);
    /// Новая карта: кроны и русла. Голоса прежней карты снимаются здесь же —
    /// у излучателя есть хозяин, и хозяин у них был прошлый мир.
    void set_sources(platform::IAudio& audio, std::span<const CrownSource> crowns,
                     std::span<const WaterCourse> courses);
    /// Мир не идёт (меню, пауза, выгрузка): всё замолкает немедленно.
    void silence(platform::IAudio& audio);

    void update(platform::IAudio& audio, const Listener& listener, float wind_strength,
                float dt_seconds, const OcclusionProbe& occluded);

    [[nodiscard]] std::span<const AmbienceCluster> clusters() const { return clusters_; }
    [[nodiscard]] std::span<const Emitter> emitters() const { return report_; }
    [[nodiscard]] std::size_t live_voices() const;
    [[nodiscard]] int current_wind_step() const { return wind_step_; }

private:
    struct Slot {
        int source = -1; ///< индекс рощи либо русла; -1 — место свободно
        bool water = false;
        platform::SoundHandle sound{};
        platform::AudioVoiceHandle voice{};
        platform::AudioVoiceHandle fading{};
        float fade_level = 0.0f; ///< 0..1 — перекличка входящего голоса
        float fade_out = 0.0f;   ///< 0..1 — уходящего
        float occlusion = 0.0f;
        float gain = 0.0f;
    };

    void release(platform::IAudio& audio, Slot& slot);

    Bank bank_{};
    std::vector<AmbienceCluster> clusters_;
    std::vector<WaterCourse> courses_;
    Slot slots_[AMBIENCE_LEAF_SLOTS + AMBIENCE_WATER_SLOTS]{};
    std::vector<Emitter> report_;
    int wind_step_ = 1;
};

} // namespace dfn::gameplay
