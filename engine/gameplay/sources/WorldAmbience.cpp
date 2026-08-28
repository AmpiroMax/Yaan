/*
Created: 28:08:2026 - 14:20:00
Last updated: 28:08:2026 - 14:20:00
Module: engine/gameplay
File: engine/gameplay/sources/WorldAmbience.cpp

Responsibility:
- Реализация модели и голосов фонового звука от источника (см. WorldAmbience.h).

Key items:
- cluster_crowns / distance_gain / leaves_base_gain / occluded_mix — модель.
- WorldAmbience::update — раздача восьми мест по громкости, перекличка,
  один луч на место за кадр.

Dependencies:
- Uses: WorldAmbience.h, platform IAudio, stdlib. Никакой физики и никакого
  рендера: ветер приходит числом, стены — замыканием.
- Used by: engine/app, tests/sim.

Notes:
- ОДИН ЛУЧ НА МЕСТО ЗА КАДР, не на каждую крону: восемь лучей в кадре — это
  цена, которую не видно ни в одном профиле, а восемьсот была бы видна.
- МЕСТА РАЗДАЮТСЯ ПО ГРОМКОСТИ, А НЕ ПО РАССТОЯНИЮ. Большая роща за площадью
  слышнее одинокой берёзы под окном, и место должно достаться ей.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Заводить голос можно только через слот: второе место, где играет мир, —
  это второе место, которое забудут выключить (правило 32).
*/
/*
UPD:
- 28:08:2026 - 14:20:00: Создан зоной «звук от источника».
*/

#include "engine/gameplay/sources/WorldAmbience.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace dfn::gameplay {

namespace {

/// ЗАТУХАНИЕ БЭКЕНДА ВЫКЛЮЧЕНО ТАК: полка «полной громкости» ставится дальше
/// любого игрового расстояния, поэтому miniaudio всегда считает источник
/// «ближе минимума» и множит на 1. Панорама при этом остаётся — а кривую
/// расстояния считаем мы (заметка в заголовке).
constexpr float SPATIAL_NO_ATTENUATION_M = 1.0e9f;

[[nodiscard]] platform::PlayParams voice_params(platform::BusHandle bus,
                                                const glm::vec3& position) {
    platform::PlayParams p;
    p.bus = bus;
    p.volume = 0.0f; // настоящую громкость ставит первый же update
    p.loop = true;
    p.spatial = true;
    p.spatial_params.position = position;
    p.spatial_params.min_distance = SPATIAL_NO_ATTENUATION_M;
    p.spatial_params.max_distance = SPATIAL_NO_ATTENUATION_M * 2.0f;
    return p;
}

[[nodiscard]] float horizontal_distance(const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 d = a - b;
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

} // namespace

// ---------------------------------------------------------------------------
// МОДЕЛЬ
// ---------------------------------------------------------------------------

bool species_is_conifer(std::string_view object_name) {
    // Соглашение имён содержимого (assets/objects, кузницы флоры): порода —
    // первое слово имени. Список хвойных короткий и пополняется здесь, а не
    // в пяти местах: ель, сосна, пихта, можжевельник, кедр, тис.
    static constexpr std::string_view CONIFERS[] = {"spruce", "pine", "fir",
                                                    "juniper", "cedar", "yew"};
    for (const std::string_view needle : CONIFERS) {
        if (object_name.size() >= needle.size()
            && object_name.compare(0, needle.size(), needle) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<AmbienceCluster> cluster_crowns(std::span<const CrownSource> crowns,
                                            float cell_m) {
    std::vector<AmbienceCluster> out;
    if (cell_m <= 0.0f) {
        return out;
    }
    struct Acc {
        glm::vec3 weighted{0.0f};
        float weight = 0.0f;
        float power = 0.0f;      // Σ r²
        float conifer_r = 0.0f;  // Σ r хвойных — порода большинства
        float broad_r = 0.0f;
        std::vector<glm::vec3> members;
    };
    // std::map, а не unordered: порядок групп ОБЯЗАН быть одним и тем же на
    // двух прогонах одной карты, иначе «шестое место досталось другой роще»
    // будет плавать между запусками и ни один замер не повторится.
    std::map<std::pair<int, int>, Acc> cells;
    for (const CrownSource& c : crowns) {
        if (c.top_m < AMBIENCE_CROWN_MIN_TOP_M || c.radius_m <= 0.0f) {
            continue; // трава и цветы не шелестят на весь квартал
        }
        const auto key = std::pair<int, int>{
            static_cast<int>(std::floor(c.position.x / cell_m)),
            static_cast<int>(std::floor(c.position.z / cell_m))};
        Acc& a = cells[key];
        a.weighted += c.position * c.radius_m;
        a.weight += c.radius_m;
        a.power += c.radius_m * c.radius_m;
        (c.conifer ? a.conifer_r : a.broad_r) += c.radius_m;
        a.members.push_back(c.position);
    }
    out.reserve(cells.size());
    for (auto& [key, a] : cells) {
        if (a.weight <= 0.0f) {
            continue;
        }
        AmbienceCluster cl;
        cl.centroid = a.weighted / a.weight;
        cl.amplitude = std::sqrt(a.power);
        cl.crowns = static_cast<std::uint32_t>(a.members.size());
        cl.conifer = a.conifer_r > a.broad_r;
        for (const glm::vec3& m : a.members) {
            cl.extent_m = std::max(cl.extent_m, horizontal_distance(m, cl.centroid));
        }
        cl.members = std::move(a.members);
        out.push_back(std::move(cl));
    }
    return out;
}

glm::vec3 cluster_emitter_point(const AmbienceCluster& cluster,
                                const glm::vec3& listener) {
    // РОЩА ЗВУЧИТ СО СВОЕГО БЛИЖНЕГО КРАЯ. Центроид был бы неправдой у самой
    // опушки: стоя под первой берёзой, слышишь её, а не середину рощи.
    if (cluster.members.empty()) {
        return cluster.centroid;
    }
    const glm::vec3* best = &cluster.members.front();
    float best_d = horizontal_distance(*best, listener);
    for (const glm::vec3& m : cluster.members) {
        const float d = horizontal_distance(m, listener);
        if (d < best_d) {
            best_d = d;
            best = &m;
        }
    }
    return *best;
}

glm::vec3 nearest_point_on_course(std::span<const glm::vec3> points,
                                  const glm::vec3& listener) {
    if (points.empty()) {
        return listener;
    }
    if (points.size() == 1) {
        return points.front();
    }
    glm::vec3 best = points.front();
    float best_d2 = -1.0f;
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const glm::vec3 a = points[i];
        const glm::vec3 b = points[i + 1];
        const glm::vec3 ab = b - a;
        const float len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
        float t = 0.0f;
        if (len2 > 1e-6f) {
            const glm::vec3 ap = listener - a;
            t = std::clamp((ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / len2, 0.0f, 1.0f);
        }
        const glm::vec3 p = a + ab * t;
        const glm::vec3 d = p - listener;
        const float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
        if (best_d2 < 0.0f || d2 < best_d2) {
            best_d2 = d2;
            best = p;
        }
    }
    return best;
}

float distance_gain(float distance_m, float near_m, float far_m) {
    if (far_m <= near_m || distance_m >= far_m) {
        return 0.0f; // за пределом слышимости — РОВНЫЙ ноль, а не «почти»
    }
    const float d = std::max(distance_m, near_m);
    const float inverse = near_m / d; // −6 дБ на удвоение: физика точки
    const float edge =
        std::clamp((far_m - distance_m) / (AMBIENCE_EDGE_FADE * far_m), 0.0f, 1.0f);
    return inverse * edge;
}

float cluster_near_m(const AmbienceCluster& cluster) {
    return std::clamp(cluster.extent_m + AMBIENCE_NEAR_MIN_M, AMBIENCE_NEAR_MIN_M,
                      AMBIENCE_NEAR_MAX_M);
}

float cluster_far_m(const AmbienceCluster& cluster) {
    const float n = std::sqrt(static_cast<float>(std::max(1u, cluster.crowns)));
    return std::clamp(AMBIENCE_FAR_BASE_M + AMBIENCE_FAR_PER_SQRT_CROWN_M * n,
                      AMBIENCE_FAR_BASE_M, AMBIENCE_FAR_MAX_M);
}

float wind_gain(float wind_strength) {
    if (AMBIENCE_WIND_FULL <= AMBIENCE_WIND_SILENT) {
        return 0.0f;
    }
    return std::clamp((wind_strength - AMBIENCE_WIND_SILENT)
                          / (AMBIENCE_WIND_FULL - AMBIENCE_WIND_SILENT),
                      0.0f, 1.0f);
}

float leaves_base_gain(const AmbienceCluster& cluster, float distance_m,
                       float wind_strength) {
    const float size = std::clamp(cluster.amplitude / AMBIENCE_CROWN_REF_M, 0.0f, 1.0f);
    const float trim = cluster.conifer ? AMBIENCE_CONIFER_TRIM : 1.0f;
    return AMBIENCE_LEAVES_GAIN * trim * size * wind_gain(wind_strength)
           * distance_gain(distance_m, cluster_near_m(cluster), cluster_far_m(cluster));
}

float water_near_m(float width_m) {
    return std::clamp(width_m, AMBIENCE_NEAR_MIN_M, AMBIENCE_NEAR_MAX_M);
}

float water_far_m(float width_m) {
    return std::clamp(25.0f + 4.0f * width_m, 25.0f, AMBIENCE_FAR_MAX_M);
}

float water_base_gain(float width_m, float distance_m) {
    // РЕКА ШИРЕ — СЛЫШНЕЕ И ДАЛЬШЕ. Ширина 8 м принята за полную силу: это
    // река плана Вайтрана (7.2 м) с запасом, ручей 2 м звучит вчетверо тише.
    const float size = std::clamp(width_m / 8.0f, 0.25f, 1.0f);
    const float trim = width_m >= 4.0f ? AMBIENCE_RIVER_TRIM : 1.0f;
    return AMBIENCE_WATER_GAIN * trim * size
           * distance_gain(distance_m, water_near_m(width_m), water_far_m(width_m));
}

int wind_step(float wind_strength, int previous_step) {
    // ГИСТЕРЕЗИС СЧИТАЕТСЯ ОТ ТОГО, ГДЕ МЫ СЕЙЧАС: порог сдвигается навстречу
    // текущей ступени, поэтому ветер, дышащий вокруг границы, не перекликает
    // файлы по десять раз в минуту.
    const float h = AMBIENCE_STEP_HYSTERESIS;
    const float t12 = AMBIENCE_STEP_1_2 + (previous_step >= 1 ? -h : h);
    const float t23 = AMBIENCE_STEP_2_3 + (previous_step >= 2 ? -h : h);
    if (wind_strength >= t23) {
        return 2;
    }
    if (wind_strength >= t12) {
        return 1;
    }
    return 0;
}

AmbienceMix occluded_mix(float occlusion, bool indoors, float door_openness) {
    AmbienceMix mix;
    const float occ = std::clamp(occlusion, 0.0f, 1.0f);
    if (indoors) {
        // В ЛОКАЦИИ ЛУЧ НЕ ПУСКАЕТСЯ ВОВСЕ, И ЭТО НАЗВАНО ВСЛУХ. Карман
        // интерьера стоит на километр ниже мира: между ним и деревьями города
        // нет ни одной стены — там вообще ничего нет. Луч честно ответил бы
        // «свободно» и сделал бы улицу в доме ГРОМЧЕ, чем на улице. Поэтому
        // «я внутри» — это модель приглушения, а не измерение, и открытая
        // дверь ведёт её между двумя числами.
        const float open = std::clamp(door_openness, 0.0f, 1.0f);
        mix.gain = AMBIENCE_INDOOR_GAIN
                   + (AMBIENCE_INDOOR_DOOR_GAIN - AMBIENCE_INDOOR_GAIN) * open;
        mix.cutoff_hz = AMBIENCE_INDOOR_CUTOFF_HZ
                        + (AMBIENCE_INDOOR_DOOR_CUTOFF_HZ - AMBIENCE_INDOOR_CUTOFF_HZ)
                              * open;
        return mix;
    }
    mix.gain = 1.0f - (1.0f - AMBIENCE_OCCLUDED_GAIN) * occ;
    if (occ <= 0.01f) {
        mix.cutoff_hz = 0.0f; // фильтра нет вовсе: граф как до этой волны
        return mix;
    }
    // Срез ведётся ПО ЛОГАРИФМУ частоты: ухо слышит октавы, а не герцы, и
    // линейная проводка от 20 кГц к 800 Гц первые полпути неслышна.
    const float lo = std::log(AMBIENCE_OCCLUDED_CUTOFF_HZ);
    const float hi = std::log(AMBIENCE_OPEN_CUTOFF_HZ);
    mix.cutoff_hz = std::exp(hi + (lo - hi) * occ);
    return mix;
}

// ---------------------------------------------------------------------------
// ГОЛОСА
// ---------------------------------------------------------------------------

bool WorldAmbience::Bank::has_leaves() const {
    for (int species = 0; species < 2; ++species) {
        for (int step = 0; step < 3; ++step) {
            if (leaves[species][step].valid()) {
                return true;
            }
        }
    }
    return false;
}

bool WorldAmbience::Bank::has_water() const {
    return stream_small.valid() || river_wide.valid();
}

WorldAmbience::Bank WorldAmbience::load_bank(platform::IAudio& audio,
                                             std::string_view audio_dir,
                                             platform::BusHandle bus) {
    Bank bank;
    bank.bus = bus;
    const std::string base = std::string(audio_dir) + "/world/";
    static constexpr const char* SPECIES[2] = {"broad", "conifer"};
    for (int species = 0; species < 2; ++species) {
        for (int step = 0; step < 3; ++step) {
            bank.leaves[species][step] = audio.load_sound(
                base + "leaves_" + SPECIES[species] + "_" + std::to_string(step + 1)
                + ".ogg");
        }
    }
    bank.stream_small = audio.load_sound(base + "stream_small.ogg");
    bank.river_wide = audio.load_sound(base + "river_wide.ogg");
    return bank;
}

void WorldAmbience::set_bank(const Bank& bank) { bank_ = bank; }

void WorldAmbience::set_sources(platform::IAudio& audio,
                                std::span<const CrownSource> crowns,
                                std::span<const WaterCourse> courses) {
    for (Slot& s : slots_) {
        release(audio, s);
        s.source = -1;
    }
    clusters_ = cluster_crowns(crowns);
    courses_.assign(courses.begin(), courses.end());
    report_.clear();
}

void WorldAmbience::silence(platform::IAudio& audio) {
    for (Slot& s : slots_) {
        release(audio, s);
        s.source = -1;
    }
    report_.clear();
}

void WorldAmbience::release(platform::IAudio& audio, Slot& slot) {
    if (slot.voice.valid()) {
        audio.stop(slot.voice);
        slot.voice = {};
    }
    if (slot.fading.valid()) {
        audio.stop(slot.fading);
        slot.fading = {};
    }
    slot.fade_level = 0.0f;
    slot.fade_out = 0.0f;
    slot.gain = 0.0f;
    slot.occlusion = 0.0f;
    slot.sound = {};
}

std::size_t WorldAmbience::live_voices() const {
    std::size_t n = 0;
    for (const Slot& s : slots_) {
        n += (s.voice.valid() ? 1u : 0u) + (s.fading.valid() ? 1u : 0u);
    }
    return n;
}

void WorldAmbience::update(platform::IAudio& audio, const Listener& listener,
                           float wind_strength, float dt_seconds,
                           const OcclusionProbe& occluded) {
    wind_step_ = wind_step(wind_strength, wind_step_);
    const float step = dt_seconds > 0.0f ? dt_seconds : 0.0f;

    // 1. КАНДИДАТЫ. Каждый источник считает, насколько он был бы слышен, и
    // места достаются самым громким — не самым близким.
    struct Candidate {
        int source = -1;
        bool water = false;
        glm::vec3 at{0.0f};
        float distance = 0.0f;
        float base = 0.0f;
        std::uint32_t crowns = 0;
        bool conifer = false;
    };
    std::vector<Candidate> leaves;
    std::vector<Candidate> water;
    leaves.reserve(clusters_.size());
    for (std::size_t i = 0; i < clusters_.size(); ++i) {
        const AmbienceCluster& cl = clusters_[i];
        Candidate c;
        c.source = static_cast<int>(i);
        c.at = cluster_emitter_point(cl, listener.position);
        c.distance = horizontal_distance(c.at, listener.position);
        c.base = leaves_base_gain(cl, c.distance, wind_strength);
        c.crowns = cl.crowns;
        c.conifer = cl.conifer;
        if (c.base > AMBIENCE_AUDIBLE_FLOOR) {
            leaves.push_back(c);
        }
    }
    for (std::size_t i = 0; i < courses_.size(); ++i) {
        const WaterCourse& w = courses_[i];
        Candidate c;
        c.source = static_cast<int>(i);
        c.water = true;
        c.at = nearest_point_on_course(w.points, listener.position);
        c.distance = horizontal_distance(c.at, listener.position);
        c.base = water_base_gain(w.width_m, c.distance);
        if (c.base > AMBIENCE_AUDIBLE_FLOOR) {
            water.push_back(c);
        }
    }
    const auto louder = [](const Candidate& a, const Candidate& b) {
        // При равной громкости — по индексу: устойчивый порядок, повторимый
        // замер (правило 30 требует, чтобы два прогона сравнивались).
        return a.base != b.base ? a.base > b.base : a.source < b.source;
    };
    std::sort(leaves.begin(), leaves.end(), louder);
    std::sort(water.begin(), water.end(), louder);
    if (leaves.size() > AMBIENCE_LEAF_SLOTS) {
        leaves.resize(AMBIENCE_LEAF_SLOTS);
    }
    if (water.size() > AMBIENCE_WATER_SLOTS) {
        water.resize(AMBIENCE_WATER_SLOTS);
    }

    // 2. МЕСТА. Листва занимает первые AMBIENCE_LEAF_SLOTS, вода — остальные;
    // разделение жёсткое, чтобы шумная роща никогда не выжила реку из мира.
    report_.clear();
    const auto serve = [&](std::size_t first, std::size_t count,
                           const std::vector<Candidate>& want) {
        // Сначала удержать за местом тот же источник, что был: перезапуск
        // голоса ради того же дерева — это щелчок на ровном месте.
        std::vector<int> taken(count, -1);
        std::vector<bool> used(want.size(), false);
        for (std::size_t s = 0; s < count; ++s) {
            Slot& slot = slots_[first + s];
            if (slot.source < 0) {
                continue;
            }
            bool still = false;
            for (std::size_t w = 0; w < want.size(); ++w) {
                if (!used[w] && want[w].source == slot.source) {
                    taken[s] = static_cast<int>(w);
                    used[w] = true;
                    still = true;
                    break;
                }
            }
            if (!still) {
                release(audio, slot);
                slot.source = -1;
            }
        }
        for (std::size_t w = 0; w < want.size(); ++w) {
            if (used[w]) {
                continue;
            }
            for (std::size_t s = 0; s < count; ++s) {
                if (taken[s] < 0 && slots_[first + s].source < 0) {
                    taken[s] = static_cast<int>(w);
                    used[w] = true;
                    break;
                }
            }
        }

        for (std::size_t s = 0; s < count; ++s) {
            Slot& slot = slots_[first + s];
            // Уходящий голос гаснет до конца, где бы место ни оказалось.
            if (slot.fading.valid()) {
                slot.fade_out -= step / AMBIENCE_FADE_S;
                if (slot.fade_out <= 0.0f) {
                    audio.stop(slot.fading);
                    slot.fading = {};
                    slot.fade_out = 0.0f;
                } else {
                    audio.set_voice_volume(slot.fading, slot.gain * slot.fade_out);
                }
            }
            if (taken[s] < 0) {
                if (slot.source >= 0) {
                    release(audio, slot);
                    slot.source = -1;
                }
                continue;
            }
            const Candidate& c = want[static_cast<std::size_t>(taken[s])];

            // Какая запись нужна этому источнику ПРЯМО СЕЙЧАС.
            platform::SoundHandle want_sound{};
            if (c.water) {
                const float width = courses_[static_cast<std::size_t>(c.source)].width_m;
                want_sound = width >= 4.0f ? bank_.river_wide : bank_.stream_small;
                if (!want_sound.valid()) {
                    want_sound = bank_.river_wide.valid() ? bank_.river_wide
                                                          : bank_.stream_small;
                }
            } else {
                want_sound = bank_.leaves[c.conifer ? 1 : 0][wind_step_];
                if (!want_sound.valid()) {
                    // Ступени может не быть на диске — тогда играет соседняя.
                    for (int st = 2; st >= 0 && !want_sound.valid(); --st) {
                        want_sound = bank_.leaves[c.conifer ? 1 : 0][st];
                    }
                    for (int st = 2; st >= 0 && !want_sound.valid(); --st) {
                        want_sound = bank_.leaves[c.conifer ? 0 : 1][st];
                    }
                }
            }
            if (!want_sound.valid()) {
                continue; // записи нет — излучатель молчит, а не ломается
            }

            const bool fresh = slot.source != c.source || !slot.voice.valid();
            const bool swap = !fresh && slot.sound.id != want_sound.id;
            if (fresh || swap) {
                if (slot.voice.valid()) {
                    if (slot.fading.valid()) {
                        audio.stop(slot.fading);
                    }
                    slot.fading = slot.voice; // перекличка, а не обрыв
                    slot.fade_out = std::max(slot.fade_level, 0.0f);
                }
                slot.voice = audio.play(want_sound, voice_params(bank_.bus, c.at));
                slot.sound = want_sound;
                slot.source = c.source;
                slot.fade_level = 0.0f;
                if (fresh) {
                    slot.occlusion = 0.0f;
                }
            }
            if (!slot.voice.valid()) {
                continue;
            }

            // 3. ЛУЧИ ОДНОГО МЕСТА — ОДИН РАЗ ЗА КАДР. В локации не пускаются вовсе (см.
            // occluded_mix): там между ухом и миром нет геометрии.
            float target_occ = 0.0f;
            if (!listener.indoors && occluded) {
                target_occ = std::clamp(occluded(listener.position, c.at), 0.0f, 1.0f);
            }
            const float rate = AMBIENCE_OCCLUSION_RATE * step;
            slot.occlusion += std::clamp(target_occ - slot.occlusion, -rate, rate);
            const AmbienceMix mix =
                occluded_mix(slot.occlusion, listener.indoors, listener.door_openness);

            slot.fade_level = std::min(1.0f, slot.fade_level + step / AMBIENCE_FADE_S);
            slot.gain = c.base * mix.gain;
            audio.set_voice_position(slot.voice, c.at);
            audio.set_voice_volume(slot.voice, slot.gain * slot.fade_level);
            audio.set_voice_lowpass(slot.voice, mix.cutoff_hz);

            Emitter e;
            e.at = c.at;
            e.distance_m = c.distance;
            e.gain = slot.gain * slot.fade_level;
            e.cutoff_hz = mix.cutoff_hz;
            e.occlusion = slot.occlusion;
            e.crowns = c.crowns;
            e.water = c.water;
            e.conifer = c.conifer;
            report_.push_back(e);
        }
    };
    serve(0, AMBIENCE_LEAF_SLOTS, leaves);
    serve(AMBIENCE_LEAF_SLOTS, AMBIENCE_WATER_SLOTS, water);
}

} // namespace dfn::gameplay
