/*
Created: 18:08:2026 - 18:12:50
Last updated: 27:08:2026 - 20:28:48
Module: tests
File: tests/app/SettingsTests.cpp

Responsibility:
- НАСТРОЙКИ: разбор текста и печать. Заказ пользователя 18.08 — вынести их из
  App.cpp, — и вынос имеет смысл ровно потому, что даёт вот этот рукав: пока
  разбор сидел внутри файла, который владеет окном, проверить его было нечем.

Key items:
- Круговой прогон: настройки -> текст -> настройки.
- ОТКАЗ ОТ НЕВЕРНОГО значения оставляет прежнее, а не подгоняет к ближайшему.
- Мусор в файле не роняет разбор: файл правит человек.

Dependencies:
- Uses: doctest, AppSettings.cpp.
- Used by: ctest (app_settings).

AI Agents Notice (must follow):
- Правило 30: проверка обязана краснеть. Здесь она краснеет, если неверное
  значение начнут подгонять, если круговой прогон потеряет поле и если
  неизвестный ключ уронит разбор.
*/
/*
UPD:
- 18:08:2026 - 18:12:50: Создан вместе с выносом настроек.
- 18:08:2026 - 18:19:47: ПРЕЖНЕЕ ЗНАЧЕНИЕ В СЛУЧАЕ ПРО ОТКАЗ ВЫБРАНО ТАК, ЧТОБЫ ОТЛИЧАТЬСЯ ОТ
  ПОДГОНКИ. Сначала я ставил 4 и подавал msaa=3 — подгонка «к ближайшему» тоже
  даёт 4, ответ совпадал случайно, и контрфакт оставался ЗЕЛЁНЫМ. Проверка, чей
  правильный ответ совпадает с неправильным, не проверяет ничего.
- 27:08:2026 - 20:16:03: ГРОМКОСТЬ ДВУХ ШИН в круговом прогоне и в отказе
  (заказ владельца через музыкальную сессию). Три вещи, каждая — своя рука:
  ноль у музыки в круговом прогоне (это самая ценная громкость, и её теряет
  запись, печатающая только «истинные» значения), НЕЧИСЛО в отказе (sscanf на
  «громко» не заполняет переменную, и разбор, глядящий только на диапазон,
  записал бы мусор — чаще всего ноль, то есть «звук пропал сам»), и то, что
  два ключа кладутся в ДВА поля.
- 27:08:2026 - 20:28:48: третий ключ voice_volume (речевая шина заведена
  вперёд голосов). Три РАЗНЫХ числа в круговом прогоне — иначе перепутанные
  местами поля дают зелёный; и утверждение, что каждый ключ едет в своё поле.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/App.h"
#include "engine/app/sources/AppSettings.h"

using dfn::app::AppConfig;
using dfn::app::settings_from_text;
using dfn::app::settings_to_text;

TEST_CASE("круговой прогон настроек не теряет ни одного поля") {
    AppConfig a;
    a.internal_width = 640;
    a.internal_height = 360;
    a.msaa_samples = 4;
    a.fullscreen = true;
    a.palette_post = true;
    a.show_menu = false;
    a.head_bob = 0.5f;
    a.black_floor = 0.08f;
    // ЗВУК. Ноль у музыки выбран нарочно: «выключил музыку» — самая ценная из
    // громкостей, и именно её теряет запись, которая печатает только «истинные»
    // значения. 0.3 у эффектов, чтобы поля нельзя было перепутать местами и
    // получить зелёный.
    a.music_volume = 0.0f;
    a.sfx_volume = 0.3f;
    a.voice_volume = 0.6f; // три РАЗНЫХ числа: перепутанные поля не дадут зелёный

    AppConfig b;
    settings_from_text(settings_to_text(a), b);

    // ПОЛЕ ЗА ПОЛЕМ, а не «похоже». Настройка, потерянная при записи,
    // обнаружится не сразу: игра поднимется с умолчанием, и человек решит, что
    // это он что-то не так нажал.
    CHECK(b.internal_width == 640);
    CHECK(b.internal_height == 360);
    CHECK(b.msaa_samples == 4);
    CHECK(b.fullscreen);
    CHECK(b.palette_post);
    CHECK_FALSE(b.show_menu);
    CHECK(b.head_bob == doctest::Approx(0.5f));
    CHECK(b.black_floor == doctest::Approx(0.08f));
    CHECK(b.music_volume == doctest::Approx(0.0f));
    CHECK(b.sfx_volume == doctest::Approx(0.3f));
    CHECK(b.voice_volume == doctest::Approx(0.6f));

    // И текст СХОДИТСЯ САМ С СОБОЙ: два прогона одного состояния дают один
    // файл, иначе diff настроек перестанет что-либо значить.
    CHECK(settings_to_text(b) == settings_to_text(a));
}

TEST_CASE("неверное значение ОТВЕРГАЕТСЯ, а не подгоняется к ближайшему") {
    AppConfig cfg;
    // ПРЕЖНЕЕ ЗНАЧЕНИЕ ВЫБРАНО ТАК, ЧТОБЫ ОТЛИЧАТЬСЯ ОТ ПОДГОНКИ, и это не
    // мелочь. Сначала я ставил 4 и подавал msaa=3 — а подгонка «к ближайшему»
    // тоже даёт 4, ответ совпадал случайно, и контрфакт оставался ЗЕЛЁНЫМ.
    // Проверка, чей правильный ответ совпадает с неправильным, не проверяет
    // ничего. 8 против 3: отказ оставит 8, любая подгонка даст 2 или 4.
    cfg.msaa_samples = 8;

    // 3 не входит в допустимые 0/1/2/4/8. Молча приведённое значение выглядело
    // бы как работающая настройка и рисовало другой мир — а человек остался бы
    // уверен, что выставил три.
    settings_from_text("msaa=3\n", cfg);
    CHECK(cfg.msaa_samples == 8); // прежнее, не 2 и не 4

    // Допустимое — принимается. Контроль обязателен: без него утверждение
    // прошло бы и на разборе, который ИГНОРИРУЕТ msaa вообще.
    settings_from_text("msaa=2\n", cfg);
    CHECK(cfg.msaa_samples == 2);

    // То же про диапазоны: вне границ — прежнее.
    cfg.head_bob = 1.0f;
    settings_from_text("head_bob=5.0\n", cfg);
    CHECK(cfg.head_bob == doctest::Approx(1.0f));
    settings_from_text("head_bob=0.25\n", cfg);
    CHECK(cfg.head_bob == doctest::Approx(0.25f));

    cfg.black_floor = 0.05f;
    settings_from_text("min_brightness=0.9\n", cfg); // потолок 0.25
    CHECK(cfg.black_floor == doctest::Approx(0.05f));

    // ГРОМКОСТЬ: тот же отказ, и НЕЧИСЛО — отдельная рука. sscanf на «громко»
    // не заполняет переменную вовсе, и разбор, проверяющий только диапазон,
    // записал бы туда мусор — чаще всего ноль, то есть «звук пропал сам».
    // Прежнее значение 0.8: любая подгонка к границам дала бы 0 или 1.
    cfg.music_volume = 0.8f;
    settings_from_text("music_volume=1.7\n", cfg);
    CHECK(cfg.music_volume == doctest::Approx(0.8f)); // не 1.0
    settings_from_text("music_volume=-0.5\n", cfg);
    CHECK(cfg.music_volume == doctest::Approx(0.8f)); // не 0.0
    settings_from_text("music_volume=громко\n", cfg);
    CHECK(cfg.music_volume == doctest::Approx(0.8f)); // и не мусор из стека
    // КОНТРОЛЬ: допустимое принимается, иначе всё выше прошло бы и на разборе,
    // который про громкость не знает вовсе.
    settings_from_text("music_volume=0.4\n", cfg);
    CHECK(cfg.music_volume == doctest::Approx(0.4f));
    // И ДВЕ ШИНЫ — ДВА ПОЛЯ. Одна ветка на оба ключа читается как экономия, и
    // ровно там живёт опечатка «оба ключа кладутся в music_volume».
    cfg.sfx_volume = 0.9f;
    settings_from_text("sfx_volume=0.2\n", cfg);
    CHECK(cfg.sfx_volume == doctest::Approx(0.2f));
    CHECK(cfg.music_volume == doctest::Approx(0.4f)); // соседнее поле не тронуто
    cfg.voice_volume = 0.9f;
    settings_from_text("voice_volume=0.1\n", cfg);
    CHECK(cfg.voice_volume == doctest::Approx(0.1f));
    CHECK(cfg.sfx_volume == doctest::Approx(0.2f));   // и третий ключ — в третье поле
    CHECK(cfg.music_volume == doctest::Approx(0.4f));
}

TEST_CASE("мусор в файле не роняет разбор — файл правит человек") {
    AppConfig cfg;
    cfg.internal_width = 1920;
    cfg.msaa_samples = 2;

    // Комментарии, пустые строки, строка без знака равенства, неизвестный
    // ключ. Ни одно из этого не повод потерять остальные настройки: человек
    // редактирует файл руками, и опечатка в одной строке не должна стоить ему
    // всех остальных.
    settings_from_text(
        "# комментарий\n"
        "\n"
        "строка без равно\n"
        "неизвестный_ключ=42\n"
        "internal_resolution=800x600\n",
        cfg);
    CHECK(cfg.internal_width == 800);
    CHECK(cfg.internal_height == 600);
    CHECK(cfg.msaa_samples == 2); // не тронуто

    // А ВОТ ИСПОРЧЕННОЕ РАЗРЕШЕНИЕ — не принимается: ноль по любой стороне
    // означал бы окно нулевого размера, и это не «настройка со странным
    // значением», а неработающая игра.
    settings_from_text("internal_resolution=0x600\n", cfg);
    CHECK(cfg.internal_width == 800);
    settings_from_text("internal_resolution=мусор\n", cfg);
    CHECK(cfg.internal_width == 800);
}
