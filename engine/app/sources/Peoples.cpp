/*
Module: engine/app
File: engine/app/sources/Peoples.cpp

Responsibility:
- Чтение `.people`, проверка народных краёв против судейских и выборка.
  Договор, все доли и все доводы — в Peoples.h.

Dependencies:
- Uses: только std (см. запись в шапке заголовка о том, почему).
- Used by: engine/app CharGen / AppCharGen, tests/app/PeoplesTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- НИ ОДНОГО ЧИСЛА НАРОДА ЗДЕСЬ. Полосы, центры, частоты и имена — содержимое
  файла; новый народ обязан быть файлом, а не правкой этого разбора.
*/

#include "engine/app/sources/Peoples.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace dfn::app {

namespace {

/// СТРОКА БЕЗ КОММЕНТАРИЯ И БЕЗ КРАЕВЫХ ПРОБЕЛОВ. Решётка режет строку в
/// ЛЮБОМ месте — в этих файлах комментарий стоит и в конце строки данных
/// («stature = 1.66 1.84  # УСЕЧЕНО СУДЬЁЙ…»), и это не украшение: там
/// записано, ПОЧЕМУ край такой, и записано рядом с ним.
[[nodiscard]] std::string_view strip(std::string_view line) {
    if (const std::size_t hash = line.find('#'); hash != std::string_view::npos) {
        line = line.substr(0, hash);
    }
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
        line.remove_prefix(1);
    }
    while (!line.empty()
           && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    return line;
}

/// `ключ = хвост`, или false. Хвост отдаётся как есть: у одних секций это одно
/// число, у других два, у третьих число, слово и список.
[[nodiscard]] bool split_pair(std::string_view line, std::string_view& key,
                              std::string_view& tail) {
    const std::size_t eq = line.find('=');
    if (eq == std::string_view::npos) {
        return false;
    }
    key = strip(line.substr(0, eq));
    tail = strip(line.substr(eq + 1));
    return !key.empty();
}

/// Слова хвоста, по пробелам.
[[nodiscard]] std::vector<std::string> words_of(std::string_view tail) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < tail.size()) {
        while (at < tail.size() && (tail[at] == ' ' || tail[at] == '\t')) {
            ++at;
        }
        const std::size_t start = at;
        while (at < tail.size() && tail[at] != ' ' && tail[at] != '\t') {
            ++at;
        }
        if (at > start) {
            out.emplace_back(tail.substr(start, at - start));
        }
    }
    return out;
}

[[nodiscard]] float to_float(const std::string& s) {
    return std::strtof(s.c_str(), nullptr);
}

/// СУММА ВЕСОВ — СТО, С ДОПУСКОМ. Частоты пишет человек, и «25 + 15 + 10 + 12 +
/// 15 + 8 + 10 + 5» он складывает в уме; полпроцента разницы — опечатка,
/// которую не видно, а пять процентов — забытый типаж.
[[nodiscard]] bool sums_to_hundred(float sum) { return std::fabs(sum - 100.0f) < 0.5f; }

} // namespace

// --- ЧТО ЛЕЖИТ В ФАЙЛЕ ------------------------------------------------------

const PeopleBand* People::band(std::string_view knob) const {
    for (const PeopleBand& b : limits) {
        if (b.name == knob) {
            return &b;
        }
    }
    return nullptr;
}

const PeopleArchetype* People::archetype(std::string_view id) const {
    const std::size_t i = archetype_index(id);
    return i < archetypes.size() ? &archetypes[i] : nullptr;
}

std::size_t People::archetype_index(std::string_view id) const {
    for (std::size_t i = 0; i < archetypes.size(); ++i) {
        if (archetypes[i].id == id) {
            return i;
        }
    }
    return archetypes.size();
}

float People::centre_of(std::size_t archetype, std::string_view knob) const {
    const PeopleBand* b = band(knob);
    // МОЛЧАНИЕ ТИПАЖА — ЭТО СЕРЕДИНА НАРОДА, А НЕ НОЛЬ, и разница видна на
    // первой же ручке: у `belly` полоса [0, 0.45], и ноль на ней — КРАЙ, то
    // есть «этот типаж поголовно с самым плоским животом, какой бывает».
    const float middle = b != nullptr ? 0.5f * (b->lo + b->hi) : 0.0f;
    if (archetype >= archetypes.size()) {
        return middle;
    }
    for (const PeopleTrait& t : archetypes[archetype].traits) {
        if (t.name == knob) {
            return b != nullptr ? std::clamp(t.mu, b->lo, b->hi) : t.mu;
        }
    }
    return middle;
}

// --- ЧТЕНИЕ -----------------------------------------------------------------

bool read_people(const std::filesystem::path& in, People& out, std::string& why) {
    std::ifstream file(in);
    if (!file) {
        why = "файл не открылся";
        return false;
    }
    out = People{};

    // РАЗБОР ОДНИМ ПРОХОДОМ И ОДНИМ СОСТОЯНИЕМ — именем текущей секции. Формат
    // в духе .scene и .map: `[секция]`, `ключ = значение`, голая строка там,
    // где секция — это список.
    std::string section;
    PeopleArchetype current;
    bool in_archetype = false;
    const auto close_archetype = [&] {
        if (in_archetype) {
            out.archetypes.push_back(std::move(current));
            current = PeopleArchetype{};
            in_archetype = false;
        }
    };

    std::string raw;
    int line_no = 0;
    while (std::getline(file, raw)) {
        ++line_no;
        const std::string_view line = strip(raw);
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            const std::string next(line.substr(1, line.size() - 2));
            if (next != "archetype") {
                close_archetype();
            } else {
                close_archetype();
                in_archetype = true;
            }
            section = next;
            continue;
        }

        // СПИСКИ ИМЁН — ГОЛЫЕ СТРОКИ. Собственное имя печатается дословно и не
        // переводится (правило 5, то же исключение, что у чата), поэтому у
        // него нет ни ключа, ни знака равенства.
        if (section == "names.male" || section == "names.female"
            || section == "names.family") {
            std::vector<std::string>& list = section == "names.male"   ? out.male
                                             : section == "names.female" ? out.female
                                                                         : out.family;
            list.emplace_back(line);
            continue;
        }

        std::string_view key;
        std::string_view tail;
        if (!split_pair(line, key, tail)) {
            std::fprintf(stderr, "[народы] %s:%d: \"%.*s\" не «ключ = значение»\n",
                         in.string().c_str(), line_no, static_cast<int>(line.size()),
                         line.data());
            continue;
        }
        const std::vector<std::string> w = words_of(tail);

        if (section.empty()) {
            if (key == "id") {
                out.id = std::string(tail);
            } else if (key == "order") {
                out.order = std::atoi(std::string(tail).c_str());
            } else if (key == "name_key") {
                out.name_key = std::string(tail);
            } else if (key == "blurb_key") {
                out.blurb_key = std::string(tail);
            }
            continue;
        }
        if (section == "limits") {
            if (w.size() < 2) {
                why = "полоса без второго конца: " + std::string(key);
                return false;
            }
            out.limits.push_back(
                PeopleBand{std::string(key), to_float(w[0]), to_float(w[1])});
            continue;
        }
        if (section == "archetype") {
            if (key == "id") {
                current.id = std::string(tail);
            } else if (key == "name_key") {
                current.name_key = std::string(tail);
            } else if (key == "frequency") {
                current.frequency = to_float(std::string(tail));
            } else if (!w.empty()) {
                // ЦЕНТР И, ЕСЛИ НАЗВАН, СВОЙ РАЗБРОС. Второе число
                // необязательно: типаж, назвавший только центр, берёт σ народа
                // (PEOPLE_SIGMA_FRAC), и это молчание значит «разброс как у
                // всех», а не «разброса нет».
                PeopleTrait t;
                t.name = std::string(key);
                t.mu = to_float(w[0]);
                t.sigma = w.size() > 1 ? to_float(w[1]) : 0.0f;
                current.traits.push_back(std::move(t));
            }
            continue;
        }
        if (section == "naming") {
            if (key == "rule_key") {
                out.naming.rule_key = std::string(tail);
            } else if (key == "patronym_male") {
                out.naming.patronym_male = std::string(tail);
            } else if (key == "patronym_female") {
                out.naming.patronym_female = std::string(tail);
            } else if (key == "patronym_share") {
                out.naming.patronym_share = to_float(std::string(tail));
            }
            continue;
        }
        if (section == "skin" || section == "hair" || section == "eyes"
            || section == "layers") {
            std::vector<PeopleSwatch>& list = section == "skin"   ? out.skin
                                              : section == "hair" ? out.hair
                                              : section == "eyes" ? out.eyes
                                                                  : out.layers;
            list.push_back(PeopleSwatch{std::string(key),
                                        w.empty() ? 0.0f : to_float(w[0])});
            continue;
        }
        if (section == "marks") {
            PeopleMark m;
            m.id = std::string(key);
            m.chance = w.empty() ? 0.0f : to_float(w[0]);
            m.place = w.size() > 1 ? w[1] : std::string{};
            if (w.size() > 2) {
                std::stringstream ss(w[2]);
                std::string one;
                while (std::getline(ss, one, ',')) {
                    if (!one.empty()) {
                        m.archetypes.push_back(one);
                    }
                }
            }
            out.marks.push_back(std::move(m));
            continue;
        }
    }
    close_archetype();

    if (out.id.empty()) {
        why = "у народа нет id";
        return false;
    }
    if (out.archetypes.empty()) {
        why = "у народа нет ни одного типажа";
        return false;
    }
    if (out.limits.empty()) {
        why = "у народа нет ни одной полосы";
        return false;
    }
    return true;
}

std::vector<People> read_peoples(const std::filesystem::path& dir) {
    std::vector<People> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        std::fprintf(stderr,
                     "[народы] каталога %s нет: вкладка происхождения будет пуста\n",
                     dir.string().c_str());
        return out;
    }
    // ПОРЯДОК ОБХОДА КАТАЛОГА НЕ ОПРЕДЕЛЁН, и это правило 13: два прогона на
    // двух машинах разложили бы народы по-разному, и «третья карточка слева» в
    // рецепте приёмки означала бы разные народы. Пути собираются и сортируются.
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().extension() == PEOPLES_EXT) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& p : files) {
        People one;
        std::string why;
        if (!read_people(p, one, why)) {
            std::fprintf(stderr, "[народы] %s отвергнут: %s\n", p.string().c_str(),
                         why.c_str());
            continue;
        }
        out.push_back(std::move(one));
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const People& a, const People& b) { return a.order < b.order; });
    if (out.empty()) {
        std::fprintf(stderr, "[народы] в %s не прочитано ни одного народа\n",
                     dir.string().c_str());
    }
    return out;
}

void peoples_fill_from_canon(People& people, const std::vector<PeopleBand>& canon) {
    for (const PeopleBand& c : canon) {
        if (people.band(c.name) == nullptr) {
            people.limits.push_back(c);
        }
    }
    // ПО АЛФАВИТУ, потому что порядок полос ЕСТЬ порядок вызовов генератора, а
    // дополненные ручки пришли в конец. Без сортировки одно зерно давало бы
    // разные тела у народа, который сузил три ручки, и у народа, который сузил
    // четыре (правило 13). Выборка сортирует и сама, но пусть здесь тоже:
    // читатель, глядящий на limits, видит их в том же порядке.
    std::sort(people.limits.begin(), people.limits.end(),
              [](const PeopleBand& a, const PeopleBand& b) { return a.name < b.name; });
}

bool peoples_validate(const People& people, const std::vector<PeopleBand>& canon,
                      std::string& why) {
    const auto canon_band = [&](std::string_view knob) -> const PeopleBand* {
        for (const PeopleBand& b : canon) {
            if (b.name == knob) {
                return &b;
            }
        }
        return nullptr;
    };
    for (const PeopleBand& b : people.limits) {
        if (!(b.hi > b.lo)) {
            why = "полоса " + b.name + " вывернута или пуста";
            return false;
        }
        const PeopleBand* c = canon_band(b.name);
        if (c == nullptr) {
            // ОПЕЧАТКА В ИМЕНИ ЦЕЛИ, ПРИНЯТАЯ МОЛЧА, даёт ползунок, который
            // никогда не двинется: народ сузил полосу ручки, которой нет.
            why = "народ называет ручку \"" + b.name + "\", о которой судья не знает";
            return false;
        }
        if (b.lo < c->lo - 1e-4f || b.hi > c->hi + 1e-4f) {
            why = "полоса " + b.name + " ШИРЕ судейской";
            return false;
        }
    }
    float freq = 0.0f;
    for (const PeopleArchetype& a : people.archetypes) {
        if (a.id.empty() || a.name_key.empty()) {
            why = "типаж без id или без ключа имени";
            return false;
        }
        freq += a.frequency;
        for (const PeopleTrait& t : a.traits) {
            const PeopleBand* b = people.band(t.name);
            if (b == nullptr) {
                why = "типаж " + a.id + " называет ручку \"" + t.name
                      + "\", которой у народа нет полосы";
                return false;
            }
            if (t.mu < b->lo - 1e-4f || t.mu > b->hi + 1e-4f) {
                why = "центр типажа " + a.id + " по ручке " + t.name
                      + " лежит ВНЕ народной полосы";
                return false;
            }
        }
    }
    if (!sums_to_hundred(freq)) {
        why = "частоты типажей в сумме не сто";
        return false;
    }
    if (people.male.empty() || people.female.empty()) {
        why = "у народа пуст список имён";
        return false;
    }
    for (const std::vector<PeopleSwatch>* list : {&people.skin, &people.hair,
                                                  &people.eyes}) {
        if (list->empty()) {
            continue; // палитры — данные впрок, потребителя у них ещё нет
        }
        float sum = 0.0f;
        for (const PeopleSwatch& s : *list) {
            sum += s.weight;
        }
        if (!sums_to_hundred(sum)) {
            why = "веса палитры в сумме не сто";
            return false;
        }
    }
    return true;
}

// --- ВЫБОРКА ----------------------------------------------------------------

float people_uniform(PeopleRng& rng) {
    // splitmix64 — свой, потому что стандарт не задаёт алгоритм ни одного
    // распределения, и два прогона на двух библиотеках разошлись бы кадром при
    // одном зерне (правило 13).
    std::uint64_t z = (rng.state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);
    // 24 старших бита в [0, 1): float несёт 24 бита мантиссы, и брать больше
    // значило бы выдавать за случайность то, что округление стирает.
    return static_cast<float>(z >> 40) / 16777216.0f;
}

float people_normal(PeopleRng& rng) {
    // Бокс-Мюллер, оба хвоста. Ноль под логарифмом отодвигается наименьшим
    // положительным значением сетки, а не проверкой «если ноль, бросить ещё
    // раз»: перебрасывание изменило бы число вызовов rng и с ним всю
    // последовательность (правило 13).
    const float u1 = std::max(people_uniform(rng), 1.0f / 16777216.0f);
    const float u2 = people_uniform(rng);
    return std::sqrt(-2.0f * std::log(u1))
           * std::cos(6.2831853071795864769f * u2);
}

std::size_t people_pick_archetype(const People& people, PeopleRng& rng) {
    if (people.archetypes.empty()) {
        return 0;
    }
    float total = 0.0f;
    for (const PeopleArchetype& a : people.archetypes) {
        total += std::max(0.0f, a.frequency);
    }
    if (!(total > 0.0f)) {
        return 0;
    }
    float roll = people_uniform(rng) * total;
    for (std::size_t i = 0; i < people.archetypes.size(); ++i) {
        roll -= std::max(0.0f, people.archetypes[i].frequency);
        if (roll <= 0.0f) {
            return i;
        }
    }
    return people.archetypes.size() - 1;
}

std::vector<std::pair<std::string, float>> people_sample_build(const People& people,
                                                               std::size_t archetype,
                                                               PeopleRng& rng) {
    // РУЧКИ ПО АЛФАВИТУ, И БРОСОК ИДЁТ В ЭТОМ ЖЕ ПОРЯДКЕ. Не аккуратность:
    // порядок вызовов генератора ЕСТЬ часть рецепта, и «то же зерно — то же
    // тело» держится только пока он один. Полосы уже лежат отсортированными в
    // файле, но полагаться на это нельзя — файл пишет человек.
    std::vector<PeopleBand> knobs = people.limits;
    std::sort(knobs.begin(), knobs.end(),
              [](const PeopleBand& a, const PeopleBand& b) { return a.name < b.name; });

    // ОДНА СКРЫТАЯ СВЯЗЬ — «КРУПНОСТЬ», и она бросается ПЕРВОЙ, до всякой
    // ручки: без неё в толпе заводятся коротышки с руками до колен, и это
    // видно с первого кадра (CHARGEN_UI.md, Р8).
    const float bulk = people_normal(rng);
    const float rho = std::clamp(PEOPLE_BULK_RHO, 0.0f, 1.0f);
    const float own_share = std::sqrt(std::max(0.0f, 1.0f - rho * rho));

    std::vector<std::pair<std::string, float>> out;
    out.reserve(knobs.size());
    for (const PeopleBand& b : knobs) {
        bool bulky = false;
        for (const char* name : PEOPLE_BULK_KNOBS) {
            bulky = bulky || b.name == name;
        }
        const float mu = people.centre_of(archetype, b.name);
        float sigma = 0.0f;
        if (archetype < people.archetypes.size()) {
            for (const PeopleTrait& t : people.archetypes[archetype].traits) {
                if (t.name == b.name) {
                    sigma = t.sigma;
                }
            }
        }
        if (!(sigma > 0.0f)) {
            sigma = PEOPLE_SIGMA_FRAC * (b.hi - b.lo);
        }
        float value = mu;
        // УСЕЧЕНИЕ ПЕРЕБРОСОМ СОБСТВЕННОЙ ЧАСТИ, А НЕ ВСЕЙ. Перебросив
        // «крупность», мы разорвали бы связь ровно у тех тел, что и так вышли
        // на край, — то есть у самых заметных в толпе.
        for (int tries = 0; tries < PEOPLE_TRUNCATION_TRIES; ++tries) {
            const float own = people_normal(rng);
            const float z = bulky ? (rho * bulk + own_share * own) : own;
            value = mu + sigma * z;
            if (value >= b.lo && value <= b.hi) {
                break;
            }
            value = std::clamp(value, b.lo, b.hi);
        }
        out.emplace_back(b.name, value);
    }
    return out;
}

std::string people_pick_swatch(const std::vector<PeopleSwatch>& list,
                               PeopleRng& rng) {
    if (list.empty()) {
        return {};
    }
    float total = 0.0f;
    for (const PeopleSwatch& s : list) {
        total += std::max(0.0f, s.weight);
    }
    if (!(total > 0.0f)) {
        return list.front().id;
    }
    float roll = people_uniform(rng) * total;
    for (const PeopleSwatch& s : list) {
        roll -= std::max(0.0f, s.weight);
        if (roll <= 0.0f) {
            return s.id;
        }
    }
    return list.back().id;
}

std::string people_sample_name(const People& people, PeopleSex sex,
                               PeopleRng& rng) {
    const std::vector<std::string>& personal =
        sex == PeopleSex::Male ? people.male : people.female;
    if (personal.empty()) {
        return {};
    }
    const std::size_t i = static_cast<std::size_t>(
        people_uniform(rng) * static_cast<float>(personal.size()));
    std::string name = personal[std::min(i, personal.size() - 1)];

    // ПАТРОНИМ СТРОИТСЯ, А НЕ БЕРЁТСЯ ИЗ СПИСКА, и это не оптимизация списка:
    // у скельдов родовое имя ЕСТЬ имя отца плюс суффикс, то есть сорок
    // мужских имён дают сорок настоящих патронимов, а список из сорока готовых
    // был бы вторым, не связанным с первым (лор: «-сон/-доттер от любого
    // мужского»).
    const bool can_build = !people.naming.patronym_male.empty()
                           && !people.naming.patronym_female.empty()
                           && !people.male.empty();
    const bool build = can_build
                       && people_uniform(rng) * 100.0f < people.naming.patronym_share;
    // БРОСОК ДЕЛАЕТСЯ ВСЕГДА, даже когда родовых нет: число вызовов генератора
    // не имеет права зависеть от содержимого файла, иначе одно зерно дало бы
    // разные толпы на разных народах в одном прогоне (правило 13).
    const float pick = people_uniform(rng);
    if (build) {
        const std::size_t f = static_cast<std::size_t>(
            pick * static_cast<float>(people.male.size()));
        name += " " + people.male[std::min(f, people.male.size() - 1)]
                + (sex == PeopleSex::Male ? people.naming.patronym_male
                                          : people.naming.patronym_female);
    } else if (!people.family.empty()) {
        const std::size_t f = static_cast<std::size_t>(
            pick * static_cast<float>(people.family.size()));
        name += " " + people.family[std::min(f, people.family.size() - 1)];
    }
    return name;
}

PeopleDraw people_sample(const People& people, PeopleSex sex, PeopleRng& rng) {
    PeopleDraw draw;
    draw.people = &people;
    draw.sex = sex;
    draw.archetype = people_pick_archetype(people, rng);
    draw.sliders = people_sample_build(people, draw.archetype, rng);
    draw.name = people_sample_name(people, sex, rng);
    draw.skin = people_pick_swatch(people.skin, rng);
    draw.hair = people_pick_swatch(people.hair, rng);
    draw.eyes = people_pick_swatch(people.eyes, rng);
    return draw;
}

PeopleDraw people_sample(const PeopleMix& mix, PeopleSex sex, PeopleRng& rng) {
    // СПЕРВА НАРОД ПО ВЕСАМ, ДАЛЬШЕ ВСЁ ТО ЖЕ. Пограничья — не украшение лора,
    // а вход генератора населения: в столице и портах 10-15 % соседей, в
    // Курганлы в сезон до половины кайсаков.
    float total = 0.0f;
    for (const PeopleMixEntry& e : mix) {
        if (e.people != nullptr) {
            total += std::max(0.0f, e.weight);
        }
    }
    if (!(total > 0.0f)) {
        return PeopleDraw{};
    }
    float roll = people_uniform(rng) * total;
    const People* chosen = nullptr;
    for (const PeopleMixEntry& e : mix) {
        if (e.people == nullptr) {
            continue;
        }
        chosen = e.people;
        roll -= std::max(0.0f, e.weight);
        if (roll <= 0.0f) {
            break;
        }
    }
    return chosen != nullptr ? people_sample(*chosen, sex, rng) : PeopleDraw{};
}

} // namespace dfn::app
