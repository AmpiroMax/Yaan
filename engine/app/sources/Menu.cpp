/*
Created: 10:08:2026 - 10:27:20
Last updated: 27:08:2026 - 14:00:00
Module: engine/app
File: engine/app/sources/Menu.cpp

Responsibility:
- MenuModel navigation and draw_menu's layout. See the header for why no
  literal user-facing string appears here.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 10:08:2026 - 10:27:20: Created.
- 13:08:2026 - 16:40:00: Плашка под текстом ЭКРАНА ПАУЗЫ (зона ui). Вуаль кроет
  ровно половину строк по построению, поэтому на незакрытых строках слова стоят на
  живом мире: замерено по архивному игровому кадру — 81.0 % чернил над светлым фоном
  ближе двух шагов квантователя к тому, что они кроют, и 80 кромок букв потеряно.
  Вуаль своей работы не теряет, она просто перестаёт быть тем, на чём стоит ТЕКСТ.
  Заодно один читатель строк вместо двух копий switch: плашку нельзя мерить по
  тексту, отличному от нарисованного.
- 13:08:2026 - 16:50:00: Дверь дозы DFN_UI_PLATE=0 — обе руки приёмки из ОДНОЙ сборки
  (правило 47, оговорка, заведённая ведущим сегодня). Тот же ключ читает DebugOverlay.cpp.
- 13:08:2026 - 17:06:00: Своя копия двери и своя плашка сняты — обе взяты из общей
  draw_text_plate(). Земля под текстом одна на весь интерфейс, и вторая копия правила
  была бы теневой (правило 39).
- 13:08:2026 - 18:52:00: СТРАНИЦА КАЛИБРОВКИ ЯРКОСТИ. Поле — настоящий чёрный, три
  квадрата в 0/1/2 шага квантователя НАД полем «как видно на стекле» (значения прогнаны
  через обратную кривую подъёма, иначе мишень занижена до 0.56 шага). Левый квадрат —
  КОНТРОЛЬ в ноль шагов: он неотличим от поля при любой настройке, и игрок, «видящий»
  все три, читает своё ожидание, а не экран.
- 13:08:2026 - 19:45:00: СТРАНИЦА НАСТРОЕК (просьба пользователя): разрешение,
  сглаживание, палитра, покачивание камеры и переход на калибровку — пять строк
  settings.cfg, которые до сих пор менялись только текстовым редактором. Каждая
  строка — ЛЕСТНИЦА ЗНАЧЕНИЙ, а не поле ввода: 640x360 движок принимает, а 641x361
  игрок мог вписать в файл руками. Вторая строка корня стала «Настройки», яркость
  живёт внутри них, и выход со страницы калибровки возвращается туда, откуда пришёл.
  Enter на строке значения делает то же, что стрелка вправо, — страница работает на
  клавишах, которые приложение уже шлёт.
- 13:08:2026 - 19:50:00: Настройки достижимы С ПАУЗЫ (третья строка), и выход с них
  возвращает туда, откуда пришли. Настройка, которая нужна ПОСРЕДИ игры, — порог
  яркости: игрок узнаёт, что ничего не видит, стоя в пещере, а страница, живущая
  только на стартовом экране, отвечает на это «выйди из игры».
- 13:08:2026 - 20:00:00: 320×180 — разрешение, которое страница настроек ПРЕДЛАГАЕТ, и
  в котором она сама переставала читаться: длинные строки уходили за оба края
  (замер: 5 и 10 чернильных пикселей в крайних столбцах, стало 0). Строка выбирается
  ЗАМЕРОМ нарисованной ширины (`fits`), а не веткой по числу 320: ветка починила бы
  сегодняшний русский и сломалась бы на первом переводе шире него.
- 14:08:2026 - 16:11:00: Кнопка «Редактор» — вторая строка корня (запрос В39). Корень
  теперь Играть / Редактор / Настройки / Выход; activate и row_at обновлены.
- 14:08:2026 - 16:50:36: БРАУЗЕР КАРТ (контракт docs/MAP_LAYOUT.md). Страница Maps
  разбита на два уровня: Categories (папки) и CategoryMaps (.map в папке). Обе кнопки
  корня зовут open_browser() с целью Play/Editor; выбор карты — OpenMap. Отрисовка
  категорий/карт, заголовок по странице, строка статуса (напр. «печёной карты нет»).
- 14:08:2026 - 17:51:15: open_category() — прямой вход в список карт категории (дверь
  снимка второго уровня браузера, правило 27); индекс вне диапазона зажат в 0.
- 14:08:2026 - 18:57:57: Приватная копия fits() снята — правило «какой вариант строки
  влезает» живёт теперь один раз, рядом с draw_text_plate (DebugOverlay.h). Копию
  завели, когда потребитель был один; второй потребитель (блок редактора) — это ровно
  тот момент, когда копия правила становится теневой (правило 39). Поведение страницы
  не менялось: локальное имя fits() сохранено и зовёт общее.
- 14:08:2026 - 19:37:40: draw_controls() — страница управления, нарисованная ИЗ
  ТАБЛИЦЫ ПРИВЯЗОК (Controls.h), а не из списка, написанного здесь: рукописный
  список верен в день написания и молча неверен потом, потому что неправильный
  экран помощи выглядит как правильный. Строка «Управление» встала на странице
  настроек перед «Назад». Раскладка считается в controls_layout(), а не здесь:
  первая версия ужимала шаг строк по высоте и на 320x180 роняла две последние
  строки за край — проверить это было нечем, пока арифметика жила внутри
  отрисовки.
- 17:08:2026 - 16:35:20: страница паузы растит строки редактора; «в главное меню» появилось и в ходьбе.
- 17:08:2026 - 16:59:23: экран управления рисует вторую колонку, когда раскладка её просит.
- 27:08:2026 - 01:22:15: ГЛАВНОЕ МЕНЮ ПО ОБРАЗЦУ (заказ владельца 26.08). Что было:
  корень — четыре строки шрифтом 5×8 по центру чёрно-синего поля; на 1920×1080 это
  буквы в 8 пикселей, то есть пятая часть высоты строки образца. Что стало:
  * СПИСОК СПРАВА ВНИЗУ, выровненный по правому краю, первый пункт крупнее прочих,
    без рамок и подложек; ГЕРБ ИМПЕРИИ ЯАН в центре-слева; поле — настоящий чёрный
    с редкими медленно плывущими пылинками; знак студии мелко в углу.
  * КРУПНЫЙ ШРИФТ — целочисленное увеличение того же растрового шрифта (MenuArt),
    а не второй шрифт: 1-битная маска, растянутая с интерполяцией, даёт серую
    бахрому, которая читается как размытый снимок, а не как крупная буква.
  * ВЫБРАННЫЙ ПУНКТ ЯРЧЕ И ПОДЧЁРКНУТ, но НЕ КРУПНЕЕ — и это единственное место,
    где я разошёлся с образцом сознательно: строка, растущая под курсором, съезжает
    из-под него, соседняя строка прыгает следом, и мышью выбирается не то, на что
    смотришь. Ярче + черта под текстом — тот же сигнал без этой ловушки.
  * ЗАГЛУШКА ВМЕСТО СПРЯТАННОГО ПУНКТА: сохранений и титров у нас нет, «Продолжить»
    и «Загрузить» ведут на страницу с честной надписью и возвратом.
  * ПАУЗА РАЗВЯЗАНА С РЕДАКТОРОМ: шесть строк всегда, ветка по editing() снята —
    подробности в UPD заголовка.
  * menu_row_boxes() — раскладка строк, общая для отрисовки и для мыши.
  * Дверь дозы DFN_MENU_DUST=0 — поле без пылинок: два прогона дают побитово
    одинаковый кадр меню, и обе руки приёмки выходят из ОДНОЙ сборки (правило 47).
- 27:08:2026 - 03:25:00: Подсказка о клавишах — на своей строке, а не на полосе
  знака студии. Поймано КАДРОМ приёмки, а не рассуждением: на сетке интерфейса
  640×360 строка занимает 434 px из 640, знак — первые 134, и на общей полосе
  они соприкасаются. Два касающихся текста читаются как поломка обоих.
- 27:08:2026 - 14:00:00: МЕНЮ ПЕРЕШЛО НА НАСТОЯЩИЙ ШРИФТ И НА ОДНУ ЛЕСТНИЦУ
  РАЗМЕРОВ, И ПОТЕРЯЛО ВСЕ ПОДСКАЗКИ УПРАВЛЕНИЯ (три заказа владельца 27.08).
  * «шрифт интерфейса слишком угловатый, нужен нормальный шрифт»: каждая
    строка каждой страницы рисуется теперь испечённой антиквой (UiFont.h,
    assets/fonts/ui_serif.*, PT Serif под SIL OFL). Блочный 5×8 остался языком
    ПРИБОРОВ (отладочный вывод, чат) и игроку не показывается.
  * «в меню текст разных кнопок разным размером»: разнобой был не внутри
    страницы, а МЕЖДУ страницами — корень считал множитель от h/180, браузер
    брал base+1, титры h/300, заглушка h/260, а настройки и управление стояли
    на FONT_CELL_H и не масштабировались вовсе. Теперь размер спрашивают у
    РОЛИ, и ролей пять на всё меню.
  * «весь вспомогательный текст подсказок ВООБЩЕ отовсюду удалить»:
    draw_keys_hint() СНЯТА (не спрятана — мёртвую функцию следующий агент
    включит обратно), вместе с подвалом экрана управления и строками клавиш на
    калибровке и настройках. Ключи локализации убраны тем же движением: строка
    без читателя переживает любой рефакторинг.
  * СТРАНИЦА ГРАФИКИ: две новые строки — размер окна и полный экран, обе
    применяются живьём; строка сетки рендера переименована в «Качество
    картинки», и её лестница доросла до 1920×1080.
  * ЧТО НЕ СНЯТО И ПОЧЕМУ: две строки на калибровке («поднимайте, пока средний
    квадрат…») и предупреждение о перезапуске. Это не инструкции по клавишам,
    а задача экрана и состояние настройки; без первых страница превращается в
    три серых квадрата без вопроса.
*/

#include "engine/app/sources/Menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include "engine/core/config/sources/Constants.h"

// For the shared text plate and its dose door. The pause page stands on the
// SAME ground as the readout because it is the same decision, and a second copy
// of a six-line getenv would be a shadow copy of a rule (Rule 39).
#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/Controls.h"
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/IntroVideo.h"
#include "engine/app/sources/Localization.h"
#include "engine/app/sources/MenuArt.h"
#include "engine/app/sources/PngImage.h"
#include "engine/app/sources/UiFont.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

namespace dfn::app {

namespace {

// Palette of the screens. Deliberately few values: the menu is text on a
// ground, and every extra tone is a decision nobody made.
constexpr render::Color BACKGROUND{18, 20, 26};
constexpr render::Color TITLE{232, 228, 214};
constexpr render::Color ITEM{176, 172, 160};
constexpr render::Color ITEM_SELECTED{244, 226, 160};
constexpr render::Color BLURB{120, 118, 112};
constexpr render::Color RULE_LINE{54, 56, 64};

// THE RULER, and everything on this page is expressed in it: one step is the
// quantizer's own cell, which is why a one-step difference is the number behind
// the words "почти сливается" -- at one step a difference MAY round into the
// same palette entry, and two steps is the project's guaranteed-separation rule.
constexpr float SHADE_STEP = static_cast<float>(config::PALETTE_SHADE_STEP_REF);

// THE LIFT CURVE, AND WHY A COPY OF IT LIVES HERE. The floor is applied on the
// GPU, in fs_upscale.sc, before the dither and the palette lookup. This page
// has to know the same curve to do the OPPOSITE sum: given a value we want to
// SEE on the glass, what must be drawn into the canvas so the lift lands it
// there. Without the inverse, the patch drawn "one step above the field" would
// arrive compressed -- 0.56 of a step at the shipping numbers -- and the
// screen would be calibrating against a target it misstates.
//
// THE SHADER IS THE AUTHORITY, and nothing here can enforce that -- a CPU test
// cannot run a fragment shader, so no assertion in this repository can catch
// the two drifting apart. What keeps them together is the one thing that can:
// both sides read the SAME exponent from docs/NUMBERS.md
// (BLACK_FLOOR_FALLOFF), and the curve is a single line in each, so a change
// has one shape to copy rather than an algorithm to re-derive. Said out loud
// because an unenforced pair that nobody names is how a shadow copy is born.
[[nodiscard]] float lift(float c, float floor_value) {
    const float k = static_cast<float>(config::BLACK_FLOOR_FALLOFF);
    return std::min(1.0f, c + floor_value * std::pow(std::max(1.0f - c, 0.0f), k));
}

// Canvas value that the lift puts at `target` on the glass. Bisection rather
// than algebra: the curve has no closed-form inverse for a general exponent,
// and 24 halvings of [0,1] land inside a 255th of a level, which is finer than
// the framebuffer can hold anyway.
[[nodiscard]] float unlift(float target, float floor_value) {
    float lo = 0.0f;
    float hi = 1.0f;
    for (int i = 0; i < 24; ++i) {
        const float mid = 0.5f * (lo + hi);
        if (lift(mid, floor_value) < target) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 0.5f * (lo + hi);
}

[[nodiscard]] render::Color grey(float v) {
    const auto c = static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    return render::Color{c, c, c};
}

std::string_view loc(std::string_view key) {
    return localized(serialization::fnv1a64(key));
}

// THE SETTINGS PAGE IS A LIST OF LADDERS, not a set of fields, and that is what
// lets it replace the text editor safely: every rung is a value the engine is
// known to accept. 640x360 is a resolution the upscale is exact for; 641x361 is
// a resolution the player could type into settings.cfg today and get a picture
// nobody has ever looked at. A menu that cannot express the broken state is
// worth more than a menu that validates it afterwards.
constexpr uint32_t RES_W[] = {320, 640, 960, 1280, 1600, 1920};
constexpr uint32_t RES_H[] = {180, 360, 540, 720, 900, 1080};
// РАЗМЕРЫ ОКНА — та же лестница из тех же соображений: это значения, на
// которых картинка складывается целыми числами, а не поле ввода, куда можно
// вписать окно, которого никто не видел. 16:9 во всех ступенях, потому что
// таковы и сетка рендера, и кадры приёмки; смешивать сюда 4:3 значило бы
// показывать игроку выбор, который меняет КАДРИРОВАНИЕ мира, не сказав об этом.
constexpr uint32_t WIN_W[] = {1280, 1600, 1920, 2560};
constexpr uint32_t WIN_H[] = {720, 900, 1080, 1440};
constexpr uint32_t MSAA_STEPS[] = {0, 2, 4, 8};
// 0 is the motion-sickness setting the research mandated and it is a FULL stop
// of the motion, not a small one -- so it is a rung of its own, not the bottom
// of a slider the player has to hunt for.
constexpr float BOB_STEPS[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};

// The rows of the settings page, named once. Their ORDER is the order of the
// picture: what the frame is drawn on, then what is done to it, then how it
// moves, then how dark it is allowed to get.
// ИМЕНА СТРОК ЖИВУТ В ЗАГОЛОВКЕ (SettingsRow), а не здесь: их индексирует и
// рукав, и счётный индекс в тесте — это тот самый молча уезжающий номер, о
// котором предупреждает запись над RootRow. Локальные короткие имена оставлены,
// чтобы switch ниже читался, но ЗНАЧЕНИЕ у них одно и общее.
constexpr size_t RowWindow = static_cast<size_t>(SettingsRow::Window);
constexpr size_t RowFullscreen = static_cast<size_t>(SettingsRow::Fullscreen);
constexpr size_t RowResolution = static_cast<size_t>(SettingsRow::Resolution);
constexpr size_t RowMsaa = static_cast<size_t>(SettingsRow::Msaa);
constexpr size_t RowPalette = static_cast<size_t>(SettingsRow::Palette);
constexpr size_t RowHeadBob = static_cast<size_t>(SettingsRow::HeadBob);
constexpr size_t RowBrightness = static_cast<size_t>(SettingsRow::Brightness);
constexpr size_t RowControls = static_cast<size_t>(SettingsRow::Controls);
constexpr size_t RowBack = static_cast<size_t>(SettingsRow::Back);
constexpr size_t RowCount = static_cast<size_t>(SettingsRow::Count);

// Nearest rung to a value that may have come from a hand-edited settings.cfg.
// Never fails: a file saying msaa=3 has to land somewhere, and landing on the
// nearest legal rung is the only answer that keeps the page honest about what
// the game will actually run with.
template <typename T, size_t N>
size_t nearest_rung(const T (&rungs)[N], T value) {
    size_t best = 0;
    double best_d = -1.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = std::abs(static_cast<double>(rungs[i]) - static_cast<double>(value));
        if (best_d < 0.0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

size_t cycle(size_t index, size_t count, int delta) {
    const int n = static_cast<int>(count);
    int next = (static_cast<int>(index) + delta) % n;
    if (next < 0) {
        next += n;
    }
    return static_cast<size_t>(next);
}

// THE LINE THAT FITS. The rule and the measurement that produced it now live
// beside draw_text_plate in DebugOverlay.h, next to the other decision every
// overlay in this zone shares; this is the local name the page reads with.
//
// IT WAS A PRIVATE COPY HERE UNTIL THE EDITOR'S BLOCK NEEDED THE SAME RULE,
// and a second copy is how a rule stops being a rule (Rule 39): the settings
// page would have kept narrowing correctly while the new panel ran off the
// edge at exactly the resolution this page OFFERS as a rung.
[[nodiscard]] std::string_view fits(int w, std::string_view full,
                                    std::string_view brief) {
    return fits_width(w, full, brief);
}

// ТО ЖЕ ПРАВИЛО, НО МЕРЯЕТ НАСТОЯЩИМ ШРИФТОМ. fits_width() считает блочными
// ячейками 6×9, и после перехода интерфейса на антикву эта мера перестала
// описывать нарисованное: у пропорционального шрифта ширина строки не равна
// числу знаков. Правило одно и то же — «влезает полный вариант, иначе
// короткий», — и живёт оно по-прежнему в одном месте; здесь у него просто
// появилась вторая линейка, потому что линеек в дереве теперь две.
[[nodiscard]] std::string_view fits_px(int w, int px, std::string_view full,
                                       std::string_view brief) {
    if (!ui_font_ready()) {
        return fits_width(w, full, brief);
    }
    return ui_text_width(full, px) <= w - w / 12 ? full : brief;
}

// --- ОДИН ШРИФТ И ОДНА ЛЕСТНИЦА РАЗМЕРОВ НА ВСЕ СТРАНИЦЫ ---------------------
// Заказ владельца 27.08: «в меню текст разных кнопок разным размером; шрифт
// интерфейса слишком угловатый». Оба отказа жили здесь и оба — от одной
// причины: КАЖДАЯ страница называла свой размер сама. Корень считал множитель
// от h/180, браузер брал base+1, титры — h/300, заглушка — h/260, а настройки
// и управление вообще стояли на FONT_CELL_H, то есть не масштабировались
// вовсе. Ни одно из этих чисел не было неправильным по отдельности — вместе
// они и есть «разный размер у разных кнопок».
//
// Теперь размер спрашивают у РОЛИ (UiFont.h), и ролей пять на всё меню.
[[nodiscard]] int px_for(int canvas_h, UiText role) { return ui_px(canvas_h, role); }

[[nodiscard]] int tw_of(std::string_view text, int px) {
    return ui_text_width(text, px);
}

void draw_centered(render::PixelCanvas& canvas, int y, std::string_view text,
                   render::Color color, int px) {
    const int x = (static_cast<int>(canvas.width()) - tw_of(text, px)) / 2;
    ui_draw_text(canvas, x, y, text, color, px, /*shadow=*/true);
}

// --- THE START SCREEN'S OWN PALETTE -----------------------------------------
// The reference is BLACK, not the menu's blue-grey ground: the emblem is lit
// stone on nothing, and any tint behind it turns the frame into a slide.
constexpr render::Color SCREEN_BLACK{0, 0, 0};
constexpr render::Color ITEM_DIM{150, 146, 138};      // an unselected row
constexpr render::Color ITEM_BRIGHT{246, 240, 224};   // the selected one
constexpr render::Color STUDIO_MARK{96, 104, 118};    // the corner signature

// HOW MANY MOTES, AND WHY IT IS A DENSITY RATHER THAN A COUNT. The field has to
// read the same at 320x180 and at 1920x1080; a fixed count is a blizzard at one
// and an empty screen at the other.
[[nodiscard]] int dust_count(int w, int h) {
    // THE DOSE DOOR (Rule 47): with DFN_MENU_DUST=0 the field is empty and two
    // runs produce a bit-identical menu frame, which is what makes an
    // acceptance frame comparable at all. Both arms come out of ONE build.
    if (const char* v = door_value("DFN_MENU_DUST"); v != nullptr && v[0] == '0') {
        return 0;
    }
    return std::clamp(w * h / 26000, 12, 160);
}

// --- THE LIST OF THE REFERENCE ----------------------------------------------
// One arithmetic, used by the drawing and by the mouse. Everything is derived
// from the frame's HEIGHT: the screen has to hold together from 320x180 to
// 1920x1080, and a layout in absolute pixels only holds at the size it was
// eyeballed on.
struct ListMetrics {
    int item_px = 1;  // кегль ОБЫЧНОГО пункта -- один на всю страницу
    int big_px = 1;   // кегль первого пункта там, где он крупнее (корень, пауза)
    int blurb_px = 1; // кегль подписи под пунктом
    int gap = 4;      // vertical air between rows
    int edge = 0;     // the column's right edge (right-aligned pages)
    int top = 0;      // y of the first row's ink
    int total = 0;    // height of the whole block
};

[[nodiscard]] int row_px(const ListMetrics& m, size_t i) {
    return i == 0 ? m.big_px : m.item_px;
}

// Высота ЯЩИКА строки. Прописная, а не полная высота строки: выносных элементов
// в наших пунктах почти нет, и блок, сложенный по line_height, стоит заметно
// выше, чем выглядит, — именно так список уезжал за низ кадра.
[[nodiscard]] int row_box_h(int px) { return std::max(1, ui_cap_height(px)); }

[[nodiscard]] ListMetrics list_metrics(int w, int h, size_t n) {
    ListMetrics m;
    const int frame_h = h; // h ужимается внутри цикла; кадр — нет
    // The reference's column stands a good way in from the edge -- far enough
    // that the letters are not read against the bezel.
    m.edge = w - std::max(6, w / 12);
    const int rows = static_cast<int>(n);
    // ДВА РАЗМЕРА НА СПИСОК, И ОБА ИЗ ОДНОЙ ЛЕСТНИЦЫ (UiFont.h). Раньше здесь
    // считался множитель блочного шрифта от h/180, и «пункт» получался разным
    // на разных страницах — жалоба владельца 27.08. Пропорции образца те же,
    // что были: пункт около 4 % высоты кадра по прописной, крупный первый —
    // около 6 %; они теперь записаны один раз, в долях роли.
    m.item_px = px_for(h, UiText::Item);
    m.big_px = px_for(h, UiText::Accent);
    m.blurb_px = px_for(h, UiText::Caption);
    for (;;) {
        m.gap = std::max(2, row_box_h(m.item_px) * 5 / 8);
        m.total = 0;
        for (int i = 0; i < rows; ++i) {
            m.total += row_box_h(i == 0 ? m.big_px : m.item_px) + m.gap;
        }
        m.total = std::max(0, m.total - m.gap); // no gap after the last row
        // A BLOCK THAT DOES NOT FIT IS THE FAILURE THIS LOOP EXISTS FOR: the
        // controls page shipped once with its last two rows off the bottom
        // because its arithmetic lived inside the drawing and nothing could
        // measure it. Ужимаем, спускаясь ПО ЛЕСТНИЦЕ (ui_px от меньшей высоты),
        // а не деля кегль: половина испечённой ступени — это ступень, которой
        // нет, и рантайм всё равно взял бы ближайшую.
        if (m.total <= h * 3 / 5 || m.item_px <= px_for(h / 4, UiText::Item)) {
            break;
        }
        const int smaller = px_for(h * 4 / 5, UiText::Item);
        const int smaller_big = px_for(h * 4 / 5, UiText::Accent);
        if (smaller >= m.item_px) {
            break; // лестница кончилась: ниже честно ничего нет
        }
        m.item_px = smaller;
        m.big_px = smaller_big;
        m.blurb_px = px_for(h * 4 / 5, UiText::Caption);
        h = h * 4 / 5;
    }
    const int bottom = frame_h - std::max(4, frame_h / 9);
    m.top = std::max(frame_h / 10, bottom - m.total);
    return m;
}

// The label of one row of the current page. ONE READER, used by the layout, by
// the drawing and by the plate that has to be sized for the text that is
// actually drawn -- the mistake the pause plate was fixed for in August.
struct Row {
    std::string_view label;
    std::string_view blurb;
};

[[nodiscard]] Row menu_row(const MenuModel& model, size_t i) {
    switch (model.page()) {
    case MenuPage::Root:
        switch (static_cast<RootRow>(i)) {
        case RootRow::Continue: return {loc("menu.root.continue"), {}};
        case RootRow::NewGame:  return {loc("menu.root.new_game"), {}};
        case RootRow::Load:     return {loc("menu.root.load"), {}};
        case RootRow::Settings: return {loc("menu.root.settings"), {}};
        case RootRow::Editor:   return {loc("menu.root.editor"), {}};
        case RootRow::Credits:  return {loc("menu.root.credits"), {}};
        default:                return {loc("menu.root.quit"), {}};
        }
    case MenuPage::Pause:
        switch (static_cast<PauseRow>(i)) {
        case PauseRow::Resume:   return {loc("menu.resume"), {}};
        case PauseRow::SaveMap:  return {loc("menu.save_map"), {}};
        case PauseRow::Settings: return {loc("menu.settings"), {}};
        case PauseRow::ToRoot:   return {loc("menu.to_root"), {}};
        case PauseRow::Discard:  return {loc("menu.discard"), {}};
        default:                 return {loc("menu.quit"), {}};
        }
    case MenuPage::Categories: {
        const MapCatalog* cat = model.catalog();
        const size_t ncats = (cat != nullptr ? cat->categories.size() : 0);
        if (i < ncats) {
            // The label is localized ("map.category.<slug>"); the empty note is
            // a stable loc string so an empty folder reads as deliberate.
            const std::string key = "map.category." + cat->categories[i].slug;
            const std::string_view blurb =
                cat->categories[i].maps.empty() ? loc("map.empty") : std::string_view{};
            return {loc(key), blurb};
        }
        return {loc("menu.back"), {}};
    }
    case MenuPage::CategoryMaps: {
        const MapCatalog* cat = model.catalog();
        if (cat != nullptr && model.chosen_category() < cat->categories.size()) {
            const auto& maps = cat->categories[model.chosen_category()].maps;
            if (i < maps.size()) {
                // name and description come from the .map manifest (Rule 5);
                // the strings outlive this call because the catalog outlives
                // the model.
                return {maps[i].name, maps[i].description};
            }
        }
        return {loc("menu.back"), {}};
    }
    default:
        return {};
    }
}

} // namespace

float black_floor_max() { return static_cast<float>(config::BLACK_FLOOR_MAX); }
float black_floor_adjust_step() { return SHADE_STEP / 8.0f; }

void MenuModel::set_black_floor(float value) {
    black_floor_ = std::clamp(value, 0.0f, black_floor_max());
}

void MenuModel::set_settings(const MenuSettings& value) {
    settings_ = value;
    launched_ = value;
}

bool MenuModel::needs_restart() const {
    // ONLY the three rows the renderer swallows at init. head_bob and the
    // brightness floor are handed to a live frame, so listing them here would
    // be a warning about nothing -- and a warning that fires when it need not
    // is how players learn to ignore the one that matters.
    return settings_.internal_w != launched_.internal_w
        || settings_.internal_h != launched_.internal_h
        || settings_.msaa != launched_.msaa
        || settings_.palette != launched_.palette;
}

void MenuModel::open_browser(BrowseTarget target) {
    target_ = target;
    open(MenuPage::Categories);
}

void MenuModel::open_category(size_t category_index) {
    const size_t ncats = (catalog_ != nullptr ? catalog_->categories.size() : 0);
    chosen_category_ = (category_index < ncats) ? category_index : 0;
    open(MenuPage::CategoryMaps);
}

void MenuModel::open_stub(std::string_view message_key) {
    stub_message_ = serialization::fnv1a64(message_key);
    open(MenuPage::Stub);
}

void MenuModel::open(MenuPage page) {
    page_ = page;
    selection_ = 0;
    // Any navigation clears a stale browser message: a "no baked file" warning
    // from a previous pick must not linger over a different category.
    browser_status_.clear();
}

size_t MenuModel::item_count() const {
    switch (page_) {
    case MenuPage::Root:
        // Continue, New, Load, Settings, Editor, Credits, Quit -- see RootRow.
        // Rows whose system does not exist yet are HERE, drawn, and land on the
        // stub page; hiding them would make "not built yet" and "not planned"
        // the same picture (owner, 26.08).
        return static_cast<size_t>(RootRow::Count);
    case MenuPage::Categories:
        // Every category is shown (empty ones included, per the contract), plus
        // a Back row. Without a catalog it is just Back -- still navigable.
        return (catalog_ != nullptr ? catalog_->categories.size() : 0) + 1;
    case MenuPage::CategoryMaps: {
        // The maps in the entered category, plus Back. Empty categories show
        // only Back, which is how "this folder has no maps yet" reads.
        size_t maps = 0;
        if (catalog_ != nullptr && chosen_category_ < catalog_->categories.size()) {
            maps = catalog_->categories[chosen_category_].maps.size();
        }
        return maps + 1;
    }
    case MenuPage::Pause:
        // SIX ROWS, ALWAYS (owner, 26.08). This used to be `editing_ ? 6 : 4`,
        // i.e. the composition of the page was a function of what the player
        // happened to be doing -- which is the exact thing the order forbids.
        // The two rows that used to appear and disappear (save, discard) now
        // answer with a status line when they have nothing to act on.
        return static_cast<size_t>(PauseRow::Count);
    case MenuPage::Calibrate:
        return 0; // no list: up/down turn the dial itself
    case MenuPage::Settings:
        return RowCount;
    case MenuPage::Controls:
        return 0; // nothing to select: it is a list to READ, Esc leaves
    case MenuPage::Splash:
    case MenuPage::Credits:
    case MenuPage::Stub:
        return 0; // one thing to read and one key to leave it with
    }
    return 0;
}

void MenuModel::adjust(int delta) {
    if (page_ != MenuPage::Settings || delta == 0) {
        return;
    }
    switch (selection_) {
    case RowWindow: {
        const size_t i = cycle(nearest_rung(WIN_W, settings_.window_w),
                               std::size(WIN_W), delta);
        settings_.window_w = WIN_W[i];
        settings_.window_h = WIN_H[i];
        break;
    }
    case RowFullscreen:
        settings_.fullscreen = !settings_.fullscreen;
        break;
    case RowResolution: {
        const size_t i = cycle(nearest_rung(RES_W, settings_.internal_w),
                               std::size(RES_W), delta);
        settings_.internal_w = RES_W[i];
        settings_.internal_h = RES_H[i];
        // The two are ONE row on purpose: the grid is an aspect the whole
        // look is drawn for, and a page that lets width and height be turned
        // apart is a page that can produce a picture nobody has ever seen.
        break;
    }
    case RowMsaa:
        settings_.msaa = MSAA_STEPS[cycle(nearest_rung(MSAA_STEPS, settings_.msaa),
                                          std::size(MSAA_STEPS), delta)];
        break;
    case RowPalette:
        settings_.palette = !settings_.palette; // two rungs: either direction flips
        break;
    case RowHeadBob:
        settings_.head_bob = BOB_STEPS[cycle(nearest_rung(BOB_STEPS, settings_.head_bob),
                                             std::size(BOB_STEPS), delta)];
        break;
    default:
        break; // brightness and back are not values: Enter is their only verb
    }
}

void MenuModel::move(int delta) {
    // ON THE CALIBRATION PAGE UP AND DOWN ARE THE DIAL, not a selection, and
    // that is deliberate rather than lazy: it is the only page with nothing to
    // select, and reusing the keys the app already sends means the app needs no
    // new input mapping to drive it. Up is brighter, which is the only reading
    // of "up" anyone offers.
    if (page_ == MenuPage::Calibrate) {
        set_black_floor(black_floor_ - static_cast<float>(delta) * black_floor_adjust_step());
        return;
    }
    const size_t n = item_count();
    if (n == 0) {
        return;
    }
    const int cur = static_cast<int>(selection_);
    int next = (cur + delta) % static_cast<int>(n);
    if (next < 0) {
        next += static_cast<int>(n);
    }
    selection_ = static_cast<size_t>(next);
}

MenuAction MenuModel::activate() {
    switch (page_) {
    case MenuPage::Root:
        switch (static_cast<RootRow>(selection_)) {
        case RootRow::Continue:
            // NO SAVE SYSTEM YET, AND THE ROW SAYS SO. The reference's first row
            // loads the last save; we have nothing to load, so the row exists,
            // is drawn largest as in the reference, and lands on a page that
            // states the fact. A row that silently did nothing would read as a
            // broken menu, and a hidden row as a feature nobody planned.
            open_stub("menu.stub.no_saves");
            return MenuAction::None;
        case RootRow::NewGame:
            // BOTH map buttons open the SAME browser (В39: play changes map
            // through the same picker, editor flies it). Neither jumps straight
            // into a map -- the first cut's named mistake (docs/MAP_LAYOUT.md).
            open_browser(BrowseTarget::Play);
            return MenuAction::None;
        case RootRow::Load:
            open_stub("menu.stub.no_saves");
            return MenuAction::None;
        case RootRow::Settings:
            // SETTINGS. The dial did not move away from the player -- it is the
            // settings page's own row, one press further in.
            settings_return_ = MenuPage::Root;
            open(MenuPage::Settings);
            return MenuAction::None;
        case RootRow::Editor:
            open_browser(BrowseTarget::Editor);
            return MenuAction::None;
        case RootRow::Credits:
            open(MenuPage::Credits);
            return MenuAction::None;
        default:
            return MenuAction::Quit;
        }
    case MenuPage::Categories: {
        // A category row descends into its maps; the last row is Back to Root.
        const size_t ncats =
            (catalog_ != nullptr ? catalog_->categories.size() : 0);
        if (selection_ < ncats) {
            chosen_category_ = selection_;
            open(MenuPage::CategoryMaps);
            return MenuAction::None;
        }
        open(MenuPage::Root);
        return MenuAction::None;
    }
    case MenuPage::CategoryMaps: {
        // A map row opens it; the last row is Back to the category list.
        size_t nmaps = 0;
        if (catalog_ != nullptr && chosen_category_ < catalog_->categories.size()) {
            nmaps = catalog_->categories[chosen_category_].maps.size();
        }
        if (selection_ < nmaps) {
            chosen_map_ = &catalog_->categories[chosen_category_].maps[selection_];
            return MenuAction::OpenMap;
        }
        open(MenuPage::Categories);
        return MenuAction::None;
    }
    case MenuPage::Pause:
        // ROW ORDER IS THE ANSWER TO "WHAT DID I PRESS BY ACCIDENT". Resume is
        // first because it is the common case; the two irreversible rows
        // (discard, quit) are last, furthest from where the cursor starts.
        //
        // AND THERE IS NO SECOND SHAPE OF THIS PAGE. The branch that used to
        // stand here asked whether the player was editing and handed out two
        // different row orders -- so the row under "press Down twice" was one
        // thing in the world and another in the editor. Six rows, one order,
        // named in PauseRow.
        switch (static_cast<PauseRow>(selection_)) {
        case PauseRow::Resume:
            return MenuAction::Resume;
        case PauseRow::SaveMap:
            return MenuAction::SaveMap; // writes and STAYS on the page
        case PauseRow::Settings:
            // SETTINGS ARE REACHABLE FROM THE PAUSE SCREEN, and that is not a
            // convenience. The one setting the player is most likely to want
            // MID-GAME is the brightness floor -- he learns he cannot see in
            // the cave while standing in the cave -- and a settings screen that
            // is only on the start menu answers that with "quit the game".
            settings_return_ = MenuPage::Pause;
            open(MenuPage::Settings);
            return MenuAction::None;
        case PauseRow::ToRoot:
            return MenuAction::ToRoot;
        case PauseRow::Discard:
            return MenuAction::DiscardToRoot;
        default:
            return MenuAction::Quit;
        }
    case MenuPage::Calibrate:
        // THE PAGE CLOSES ITSELF, and the action only asks the app to SAVE. A
        // page whose exit depends on the app handling a new action is a page
        // that traps the player the moment the handler is missing -- and the
        // handler landing later than the page is exactly the order these two
        // halves shipped in.
        open(calibrate_return_);
        return MenuAction::CalibrationDone;
    case MenuPage::Controls:
        open(MenuPage::Settings); // the only way in, so the only way out
        return MenuAction::None;
    case MenuPage::Splash:
    case MenuPage::Credits:
    case MenuPage::Stub:
        // ONE KEY OUT, AND IT IS BOTH KEYS. These pages have nothing to select,
        // so Enter and Escape must mean the same thing -- a page a player can be
        // stuck on because he pressed the wrong one of two equally reasonable
        // keys is the whole reason back() exists.
        open(MenuPage::Root);
        return MenuAction::None;
    case MenuPage::Settings:
        if (selection_ == RowBrightness) {
            calibrate_return_ = MenuPage::Settings;
            open(MenuPage::Calibrate);
            return MenuAction::None;
        }
        if (selection_ == RowControls) {
            // READ-ONLY, AND THAT IS THE WHOLE FEATURE. The request was "я
            // должен уметь посмотреть на это" -- look at it. Rebinding is a
            // different thing needing a different page, and shipping a list
            // that looks editable and is not would be worse than a list.
            open(MenuPage::Controls);
            return MenuAction::None;
        }
        if (selection_ == RowBack) {
            open(settings_return_);
            return MenuAction::SettingsDone;
        }
        // ENTER IS THE SAME VERB AS RIGHT on a value row. The app routes up,
        // down, Enter and Escape today; left and right are a patch in its
        // file, and a page that needs a key nobody sends is a page that ships
        // dead. With this line it works on the keys that already exist and
        // gets nicer when the other two arrive.
        adjust(+1);
        return MenuAction::None;
    }
    return MenuAction::None;
}

MenuAction MenuModel::back() {
    switch (page_) {
    case MenuPage::Root:
        return MenuAction::Quit;
    case MenuPage::Categories:
        open(MenuPage::Root);
        return MenuAction::None;
    case MenuPage::CategoryMaps:
        open(MenuPage::Categories);
        return MenuAction::None;
    case MenuPage::Pause:
        return MenuAction::Resume;
    case MenuPage::Calibrate:
        // Escape SAVES too. A brightness the player turned until he could see
        // and then lost by leaving the wrong way is worse than no dial.
        open(calibrate_return_);
        return MenuAction::CalibrationDone;
    case MenuPage::Controls:
        open(MenuPage::Settings);
        return MenuAction::None;
    case MenuPage::Splash:
    case MenuPage::Credits:
    case MenuPage::Stub:
        open(MenuPage::Root);
        return MenuAction::None;
    case MenuPage::Settings:
        // And so does Escape here, for the same reason and with the same
        // guarantee: both exits from this page emit SettingsDone, so there is
        // no way out that silently discards what the player just turned.
        open(settings_return_);
        return MenuAction::SettingsDone;
    }
    return MenuAction::None;
}

namespace {

// --- WHERE EVERY ROW IS, ONCE -----------------------------------------------
// The plan is the whole layout of a list page: metrics, the ink rectangle of
// each label, and where its blurb goes. draw_* reads it; so does the mouse.

struct ListPlan {
    ListMetrics m;
    std::vector<MenuRowBox> boxes; // ink rect of each label
    std::vector<int> blurb_y;      // y of the blurb ink, or -1 when there is none
    int blurb_px = 1;
    int title_y = 0;
    bool right_aligned = false;
};

[[nodiscard]] bool page_is_right_aligned(MenuPage p) {
    // THE REFERENCE'S SHAPE IS FOR THE TWO PAGES THE PLAYER MEETS AS "THE MENU":
    // the start screen and the pause screen. The browser levels stay centred --
    // they are a FILE PICKER, and a right-aligned column of map names read
    // against the edge of the frame is a list you cannot scan.
    return p == MenuPage::Root || p == MenuPage::Pause;
}

[[nodiscard]] ListPlan plan_list(int w, int h, const MenuModel& model) {
    ListPlan p;
    p.right_aligned = page_is_right_aligned(model.page());
    const size_t n = model.item_count();
    p.m = list_metrics(w, h, n);
    // The first row is the largest on the two pages that have a "main" action
    // (the reference's CONTINUE, our Продолжить); a picker has no such row and
    // a list where one entry is bigger reads as that entry being special.
    if (!p.right_aligned) {
        p.m.big_px = p.m.item_px;
    }
    p.blurb_px = p.m.blurb_px;

    const auto row_h = [&](size_t i) { return row_box_h(row_px(p.m, i)); };
    const auto blurb_h = [&](size_t i) {
        return menu_row(model, i).blurb.empty()
                   ? 0
                   : row_box_h(p.blurb_px) + p.m.gap / 2;
    };

    if (!p.right_aligned) {
        // A CENTRED PAGE IS ANCHORED UNDER ITS TITLE and must fit ABOVE the
        // bottom, blurbs included. The shrink loop is the same one list_metrics
        // runs, redone here because the blurbs are what actually overflow: a
        // category with twenty maps is a real page.
        int scratch_h = h;
        for (;;) {
            p.m.gap = std::max(2, row_box_h(p.m.item_px) * 5 / 8);
            int total = 0;
            for (size_t i = 0; i < n; ++i) {
                total += row_h(i) + blurb_h(i) + p.m.gap;
            }
            p.m.total = std::max(0, total - p.m.gap);
            p.title_y = h / 8;
            p.m.top = p.title_y + row_box_h(px_for(h, UiText::Title)) + p.m.gap * 3;
            if (p.m.top + p.m.total <= h - h / 8) {
                break;
            }
            // Спуск ПО ЛЕСТНИЦЕ: просим размер от кадра поменьше и берём то, что
            // испечено. Кончилась лестница — принимаем как есть: нарисовать
            // мельче, чем испечено, значит нарисовать мыло.
            const int smaller = px_for(scratch_h * 4 / 5, UiText::Item);
            if (smaller >= p.m.item_px) {
                break;
            }
            scratch_h = scratch_h * 4 / 5;
            p.m.item_px = smaller;
            p.m.big_px = smaller;
            p.blurb_px = px_for(scratch_h, UiText::Caption);
        }
    } else {
        // The title (the pause word) stands above the block; the start screen
        // has none, and draws its emblem instead.
        p.title_y = std::max(h / 12, p.m.top - row_box_h(p.m.big_px) - p.m.gap * 2);
    }

    p.boxes.reserve(n);
    p.blurb_y.reserve(n);
    int y = p.m.top;
    for (size_t i = 0; i < n; ++i) {
        const int s = row_px(p.m, i);
        const Row r = menu_row(model, i);
        const int lw = tw_of(r.label, s);
        MenuRowBox b;
        b.x = p.right_aligned ? p.m.edge - lw : (w - lw) / 2;
        b.y = y;
        b.w = lw;
        b.h = row_box_h(s);
        p.boxes.push_back(b);
        y += b.h;
        if (r.blurb.empty()) {
            p.blurb_y.push_back(-1);
        } else {
            y += p.m.gap / 2;
            p.blurb_y.push_back(y);
            y += row_box_h(p.blurb_px);
        }
        y += p.m.gap;
    }
    return p;
}

// THE SETTINGS PAGE'S VERTICAL ARITHMETIC, named so the mouse can read it. The
// page itself is a two-column table drawn at the small font; only the ROW BAND
// is shared, because that is the only part a pointer needs.
struct SettingsLayout {
    int title_px = 1;
    int item_px = 1;
    int title_y = 0;
    int first_y = 0;
    int step = 0;
    int gap_after_brightness = 0;
};

[[nodiscard]] SettingsLayout settings_layout(int h) {
    SettingsLayout L;
    // РАЗМЕРЫ ИЗ ТОЙ ЖЕ ЛЕСТНИЦЫ, ЧТО У ОСТАЛЬНЫХ СТРАНИЦ. До сегодня эта
    // страница стояла на render::FONT_CELL_H — то есть НЕ масштабировалась
    // вовсе: на 1920×1080 её строки были в девять раз мельче строк корня.
    // Это и есть вторая половина жалобы «в меню текст разных кнопок разным
    // размером»: разнобой был не внутри страницы, а между страницами.
    L.title_px = px_for(h, UiText::Title);
    L.item_px = px_for(h, UiText::Item);
    const int row = std::max(1, ui_cap_height(L.item_px));
    L.title_y = h / 8;
    L.first_y = L.title_y + std::max(1, ui_cap_height(L.title_px)) + row;
    L.step = row + row / 2;
    // the two rows below the dial are verbs, not values
    L.gap_after_brightness = row / 2;
    // ВСЕ СТРОКИ ОБЯЗАНЫ ПОМЕСТИТЬСЯ. Страница выросла на две строки (окно и
    // полный экран), и без этой проверки последние две ушли бы за низ кадра —
    // ровно тот отказ, который экран управления уже один раз выпустил.
    while (L.item_px > 0
           && L.first_y + static_cast<int>(RowCount) * L.step + L.gap_after_brightness
                  > h - h / 12) {
        const int smaller = px_for(h * 4 / 5, UiText::Item);
        if (smaller >= L.item_px) {
            break;
        }
        L.item_px = smaller;
        const int r = std::max(1, ui_cap_height(L.item_px));
        L.step = r + r / 2;
        L.gap_after_brightness = r / 2;
        h = h * 4 / 5;
    }
    return L;
}

[[nodiscard]] int settings_row_y(const SettingsLayout& L, size_t i) {
    int y = L.first_y + static_cast<int>(i) * L.step;
    if (i > RowBrightness) {
        y += L.gap_after_brightness;
    }
    return y;
}

} // namespace

std::vector<MenuRowBox> menu_row_boxes(int canvas_w, int canvas_h,
                                       const MenuModel& model) {
    switch (model.page()) {
    case MenuPage::Root:
    case MenuPage::Pause:
    case MenuPage::Categories:
    case MenuPage::CategoryMaps:
        return plan_list(canvas_w, canvas_h, model).boxes;
    case MenuPage::Settings: {
        // A BAND, NOT THE INK. The row is a PAIR (label left, value right) with
        // a gap between them, so there is no single ink rectangle to click; the
        // band is the middle half of the frame, which is where the block is
        // centred.
        const SettingsLayout L = settings_layout(canvas_h);
        std::vector<MenuRowBox> out;
        out.reserve(RowCount);
        for (size_t i = 0; i < RowCount; ++i) {
            out.push_back(MenuRowBox{canvas_w / 4, settings_row_y(L, i), canvas_w / 2,
                                     std::max(1, ui_cap_height(L.item_px))});
        }
        return out;
    }
    default:
        return {}; // pages with nothing to select
    }
}

size_t menu_row_at(int canvas_w, int canvas_h, const MenuModel& model, int x, int y) {
    const std::vector<MenuRowBox> boxes = menu_row_boxes(canvas_w, canvas_h, model);
    for (size_t i = 0; i < boxes.size(); ++i) {
        // PADDED FROM THE BOX ITSELF, never from a second copy of the layout: a
        // pointer between two rows must land on one of them rather than on
        // nothing, or the selection blinks off every time the hand moves.
        const MenuRowBox& b = boxes[i];
        const int pad_y = std::max(2, b.h / 2);
        const int pad_x = std::max(2, b.h);
        if (x >= b.x - pad_x && x < b.x + b.w + pad_x && y >= b.y - pad_y
            && y < b.y + b.h + pad_y) {
            return i;
        }
    }
    return model.item_count();
}

// THE CALIBRATION PAGE (the user's request, and Skyrim's and Doom's first-run
// screen): a field at the game's TRUE black with three patches on it, and the
// player raises the floor until the middle one is barely there.
//
// WHY THREE, AND WHY THESE THREE. The patches sit at 0, 1 and 2 quantizer steps
// ABOVE the field AS SEEN ON THE GLASS (the canvas values are run back through
// the inverse of the lift, so the lift does not compress the target it is being
// judged by). One step is the ruler's own cell -- a one-step difference may
// round into the same palette entry, which is exactly what "почти сливается"
// means in numbers. Two steps is the project's guaranteed-separation rule, the
// same one the readout's plate was measured against.
//
// THE LEFT PATCH IS A CONTROL AND IT IS DRAWN AT ZERO STEPS, i.e. it is painted
// in the field's own colour and cannot be seen at any setting. It is here for
// the same reason every measurement in this project carries a control arm: a
// player who "sees" all three is reading his own expectation, and without the
// zero patch neither he nor we could tell that apart from a working eye. The
// digit under each square is what says where to look, so the invisible one is
// still locatable.
void draw_calibration(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    const float floor_value = model.black_floor();

    // TRUE black, not the menu's ground: the whole page is a statement about
    // what black looks like on this monitor, so it must not be tinted by ours.
    canvas.clear(render::Color{0, 0, 0});

    const int title_px = px_for(h, UiText::Title);
    const int item_px = px_for(h, UiText::Item);
    const int cap_px = px_for(h, UiText::Caption);
    const int line = std::max(1, ui_line_height(cap_px));
    draw_centered(canvas, h / 8, loc("menu.calibrate.title"), TITLE, title_px);
    // ЭТИ ДВЕ СТРОКИ — НЕ ПОДСКАЗКА УПРАВЛЕНИЯ, И ПОТОМУ ОСТАЛИСЬ. Заказ 27.08
    // снял инструкции по клавишам; здесь написано, ЧТО ГЛАЗ ДОЛЖЕН УВИДЕТЬ, —
    // без этого страница превращается в три серых квадрата без задачи.
    draw_centered(canvas, h / 8 + std::max(1, ui_cap_height(title_px)) + line,
                  fits_px(w, cap_px, loc("menu.calibrate.line1"),
                          loc("menu.calibrate.line1.short")), ITEM, cap_px);
    draw_centered(canvas, h / 8 + std::max(1, ui_cap_height(title_px)) + line * 2,
                  fits_px(w, cap_px, loc("menu.calibrate.line2"),
                          loc("menu.calibrate.line2.short")), BLURB, cap_px);

    // Squares big enough that the eye judges a TONE rather than a thin edge:
    // a one-step difference on a 4 px sliver is a different perceptual task
    // than the one the setting is for.
    const int patch = std::max(w / 10, 24);
    const int gap = patch / 2;
    const int total = 3 * patch + 2 * gap;
    const int x0 = (w - total) / 2;
    const int y0 = h / 2 - patch / 4;
    for (int i = 0; i < 3; ++i) {
        const float target = lift(0.0f, floor_value) + static_cast<float>(i) * SHADE_STEP;
        const int px = x0 + i * (patch + gap);
        canvas.fill_rect(px, y0, patch, patch, grey(unlift(target, floor_value)));
        char label[8];
        std::snprintf(label, sizeof(label), "%d", i);
        ui_draw_text(canvas, px + (patch - tw_of(label, cap_px)) / 2,
                     y0 + patch + line / 2, label, BLURB, cap_px, /*shadow=*/true);
    }

    // The number, in the ruler the page is built on: steps first, because that
    // is what the patches mean, and the raw value second for the settings file.
    char value[48];
    std::snprintf(value, sizeof(value), "%.2f  (%.3f)",
                  static_cast<double>(floor_value / SHADE_STEP),
                  static_cast<double>(floor_value));
    const std::string_view level = loc("menu.calibrate.level");
    const int space = std::max(4, item_px / 3);
    const int vw = tw_of(level, item_px) + space + tw_of(value, item_px);
    const int vx = (w - vw) / 2;
    const int vy = y0 + patch + line * 2;
    ui_draw_text(canvas, vx, vy, level, BLURB, item_px, true);
    ui_draw_text(canvas, vx + tw_of(level, item_px) + space, vy, value, ITEM_SELECTED,
                 item_px, true);
    // ПОДСКАЗКА О КЛАВИШАХ СНЯТА (заказ владельца 27.08). Её место внизу кадра
    // осталось пустым намеренно: строка, объясняющая игроку, что стрелки — это
    // стрелки, а Esc — это назад, была тем самым «вспомогательным текстом».
}

// THE SETTINGS PAGE (the user's request: settings.cfg had five rows describing
// the picture and no way to change any of them except a text editor).
//
// IT IS TWO COLUMNS, not the centred list the other pages use, and that is the
// one layout decision here worth defending: a settings row is a PAIR (what it
// is, what it is set to), and centring each pair separately makes the values
// jitter left and right as the player turns them, which reads as the page
// twitching rather than the value changing. Labels left, values right-aligned
// against one edge, so only the value that changed moves.
void draw_settings(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    const MenuSettings& s = model.settings();

    canvas.clear(BACKGROUND);

    // Values built once, into storage that outlives the two passes below --
    // the block is MEASURED before it is drawn, and measuring text that is not
    // the text drawn is the mistake the pause plate was fixed for.
    const std::string off(loc("menu.settings.off"));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%ux%u", s.internal_w, s.internal_h);
    const std::string res(buf);
    std::snprintf(buf, sizeof(buf), "%ux", s.msaa);
    const std::string msaa = (s.msaa <= 1) ? off : std::string(buf);
    std::snprintf(buf, sizeof(buf), "%.0f%%", static_cast<double>(s.head_bob) * 100.0);
    const std::string bob = (s.head_bob <= 0.0f) ? off : std::string(buf);
    // The floor in STEPS, the same ruler the calibration page is built on: the
    // number the player saw while turning it is the number he reads here.
    std::snprintf(buf, sizeof(buf), "%.2f",
                  static_cast<double>(model.black_floor() / SHADE_STEP));
    const std::string bright = (model.black_floor() <= 0.0f) ? off : std::string(buf);

    struct Row {
        std::string_view label;
        std::string_view value;
    };
    std::snprintf(buf, sizeof(buf), "%ux%u", s.window_w, s.window_h);
    const std::string window(buf);

    const Row rows[RowCount] = {
        {loc("menu.settings.window"), window},
        {loc("menu.settings.fullscreen"),
         s.fullscreen ? loc("menu.settings.on") : loc("menu.settings.off")},
        {loc("menu.settings.resolution"), res},
        {loc("menu.settings.msaa"), msaa},
        {loc("menu.settings.palette"),
         s.palette ? loc("menu.settings.on") : loc("menu.settings.off")},
        {loc("menu.settings.bob"), bob},
        {loc("menu.settings.brightness"), bright},
        {loc("menu.controls"), {}},
        {loc("menu.back"), {}},
    };

    // THE ROW ARITHMETIC LIVES IN settings_layout() so the MOUSE can read the
    // same numbers (menu_row_boxes). It used to be inline here, which is fine
    // for one reader and a shadow copy the moment there are two (Rule 39).
    const SettingsLayout L = settings_layout(h);

    int label_w = 0;
    int value_w = 0;
    for (const Row& r : rows) {
        label_w = std::max(label_w, tw_of(r.label, L.item_px));
        value_w = std::max(value_w, tw_of(r.value, L.item_px));
    }
    const int gap = L.item_px;
    const int block = label_w + gap + value_w;
    const int x0 = (w - block) / 2;

    draw_centered(canvas, L.title_y, loc("menu.settings.title"), TITLE, L.title_px);
    canvas.fill_rect(w / 4, L.title_y + ui_cap_height(L.title_px) + L.item_px / 3, w / 2,
                     std::max(1, L.item_px / 24), RULE_LINE);

    for (size_t i = 0; i < RowCount; ++i) {
        const int y = settings_row_y(L, i);
        const bool sel = (i == model.selection());
        const render::Color color = sel ? ITEM_SELECTED : ITEM;
        if (sel) {
            ui_draw_text(canvas, x0 - gap * 2, y, ">", ITEM_SELECTED, L.item_px, true);
        }
        ui_draw_text(canvas, x0, y, rows[i].label, color, L.item_px, /*shadow=*/true);
        if (!rows[i].value.empty()) {
            ui_draw_text(canvas, x0 + block - tw_of(rows[i].value, L.item_px), y,
                         rows[i].value, color, L.item_px, /*shadow=*/true);
        }
    }

    // SAID OUT LOUD, and only when it is true: three of these rows are
    // swallowed by the renderer at init, so turning them changes the file and
    // not the frame in front of the player. A page that lets that happen
    // silently teaches the player that the settings do nothing.
    //
    // ЭТО НЕ ПОДСКАЗКА УПРАВЛЕНИЯ, а СОСТОЯНИЕ: строка отвечает не «какую
    // клавишу нажать», а «эта настройка ещё не в кадре». Заказ 27.08 снял
    // первое и не трогал второго.
    if (model.needs_restart()) {
        const int cap = px_for(h, UiText::Caption);
        draw_centered(canvas, h - ui_line_height(cap) * 2,
                      fits_px(w, cap, loc("menu.settings.restart"),
                              loc("menu.settings.restart.short")),
                      BLURB, cap);
    }
}

// THE CONTROLS PAGE (the user's request: "я должен уметь посмотреть на это в
// настройках управления"). It is DRAWN FROM THE BINDING TABLE, not from a list
// written here, and that is the entire design: a hand-written help screen is
// correct on the day it is written and silently wrong afterwards, because a
// wrong one looks exactly like a right one. App dispatches through the same
// table, so a key that exists is a key with a row.
//
// Two columns for the same reason the settings page has two: a row is a PAIR
// (which key, what it does), and centring each pair separately makes the list
// read as ragged rather than as a table.
void draw_controls(render::PixelCanvas& canvas) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());

    canvas.clear(BACKGROUND);

    // Key names are built first and kept alive: the block is MEASURED before it
    // is drawn, and measuring text other than the text drawn is the mistake the
    // pause plate was fixed for.
    struct Row {
        std::string keys;
        std::string_view what;
        std::string_view note; // scope, when it is not everywhere
    };
    std::vector<Row> rows;
    rows.reserve(control_bindings().size() + movement_rows().size() + 2);

    for (const Binding& b : control_bindings()) {
        std::string keys = key_name(b.key);
        if (b.alias != platform::Key::UNKNOWN) {
            keys += ", ";
            keys += key_name(b.alias);
        }
        std::string_view note;
        if (b.scope == Scope::EditorOnly) {
            note = loc("controls.scope.editor");
        } else if (b.scope == Scope::PlayingOnly) {
            note = loc("controls.scope.playing");
        }
        rows.push_back({std::move(keys), loc(b.what), note});
    }
    for (const MovementRow& m : movement_rows()) {
        rows.push_back({std::string(loc(m.keys)), loc(m.what), {}});
    }

    // СПИСОК КЛАВИШ — ЭТО СПРАВОЧНИК, а не список кнопок: тридцать с лишним
    // строк, которые читают глазами один раз. Роль Small на всю страницу — и
    // это ОДНА роль, а не «сколько влезет»: строки одного смысла обязаны быть
    // одного размера (заказ 27.08). Самая мелкая из ролей выбрана замером, а не
    // вкусом: на Caption описания вроде «Подтвердить (в редакторе) или быстрый
    // коммент со снимком» не помещались в половину кадра и въезжали в клавиши
    // соседней колонки — видно на первом кадре приёмки этой страницы.
    const int item_px = px_for(h, UiText::Small);
    int keys_w = 0;
    for (const Row& r : rows) {
        keys_w = std::max(keys_w, tw_of(r.keys, item_px));
    }
    const int gap = item_px;

    // THE LAYOUT IS COMPUTED NEXT DOOR (Controls.h controls_layout) so a test
    // can read it. The first cut of this page did its arithmetic here, inline,
    // and did not fit at 320x180 -- the last rows ran off the bottom and the
    // footer sat on top of a row. Nothing could have caught that but a frame,
    // and a frame only catches the resolutions somebody remembers to shoot.
    const int title_px = px_for(h, UiText::Title);
    // ПОЛ ШАГА — ВЫСОТА СТРОКИ, а не прописной: см. довод у самой функции.
    const int line = std::max(1, ui_line_height(item_px));
    const ControlsLayout L = controls_layout(w, h, line, line * 9 / 8);
    const int title_y = L.title_y;
    const int row_h = L.row_h;

    draw_centered(canvas, title_y, loc("controls.title"), TITLE, title_px);
    canvas.fill_rect(w / 4, title_y + ui_cap_height(title_px) + item_px / 3, w / 2,
                     std::max(1, item_px / 16), RULE_LINE);

    // ГОРИЗОНТАЛЬ СЧИТАЕТСЯ ПО КОЛОНКЕ, А НЕ ПО КАДРУ. Прежняя арифметика знала
    // только про один столбец: ширина описаний была числом 200 (пикселей,
    // верных на сетке 640×360), а вторая колонка получала сдвиг от середины
    // кадра. На холсте 1920×1080 это дало ровно то, что видно на первом кадре
    // приёмки, — описания левой колонки въехали в клавиши правой. Теперь у
    // страницы есть ШИРИНА КОЛОНКИ, и всё внутри неё.
    const int margin = std::max(item_px, w / 24);
    const int col_w = (L.columns == 2 ? (w - margin * 2) / 2 : w - margin * 2);
    const int x_keys = L.columns == 2 ? margin : std::max(margin, (w - (keys_w + gap + col_w / 2)) / 2);
    const int x_what = x_keys + keys_w + gap;
    // Правый край текста колонки: за него не выходит ни описание, ни пометка.
    const int col_right = x_what + col_w - keys_w - gap;

    int y = L.first_y;
    const size_t key_rows = control_bindings().size();
    // TWO COLUMNS WHEN THE FRAME IS SHORT (Controls.h controls_layout). The
    // second column starts at the middle of the frame and the y restarts; in
    // one-column mode the offsets are zero and this reads exactly as before.
    const int col_shift = L.columns == 2 ? col_w : 0;
    for (size_t i = 0; i < rows.size(); ++i) {
        int x_off = 0;
        if (L.columns == 2) {
            const bool second = static_cast<int>(i) >= L.rows_per_column;
            x_off = second ? col_shift : 0;
            y = L.first_y + (static_cast<int>(i) - (second ? L.rows_per_column : 0)) * row_h;
        }
        // The two headings mark where dispatched keys end and the fly camera's
        // continuous inputs begin -- they behave differently and the screen
        // should not imply otherwise. They are the first thing given up when
        // the frame is too short, because the rows they group are already
        // adjacent and the ROWS are the thing nobody may lose.
        if (L.headings && i == 0) {
            ui_draw_text(canvas, x_keys, y, loc("controls.section.keys"), BLURB, item_px,
                         true);
            y += row_h;
        } else if (L.headings && i == key_rows) {
            ui_draw_text(canvas, x_keys, y, loc("controls.section.fly"), BLURB, item_px,
                         true);
            y += row_h;
        }
        ui_draw_text(canvas, x_keys + x_off, y, rows[i].keys, ITEM_SELECTED, item_px,
                     /*shadow=*/true);
        ui_draw_text(canvas, x_what + x_off, y, rows[i].what, ITEM, item_px, true);
        // ПОМЕТКА ОБЛАСТИ («только в редакторе») — ПЕРВОЕ, ЧЕМ СТРАНИЦА
        // ПЛАТИТ ЗА ТЕСНОТУ, и платит она молча: строка, которой пометка не
        // влезла, всё равно на месте и всё равно читается. Порог считается по
        // краю КОЛОНКИ, а не кадра, — иначе в две колонки пометка левой лезет
        // в клавиши правой, что и было на первом кадре.
        if (!rows[i].note.empty()) {
            const int nx = x_what + x_off + tw_of(rows[i].what, item_px) + item_px / 2;
            if (nx + tw_of(rows[i].note, item_px) <= col_right + x_off) {
                ui_draw_text(canvas, nx, y, rows[i].note, BLURB, item_px, true);
            }
        }
        if (L.columns == 1) {
            y += row_h;
        }
    }
    // ПОДВАЛ «Esc — назад» СНЯТ (заказ владельца 27.08). Полоса под списком
    // осталась полем — см. ControlsLayout::footer.
}

namespace {

// --- THE ITEM LIST OF THE REFERENCE -----------------------------------------
// Right-aligned on the start and pause screens, centred in the map browser; the
// selected row is BRIGHTER and UNDERSCORED. It is deliberately not BIGGER, and
// that is the one place this screen departs from the reference on purpose: a
// row that grows under the pointer slides out from under it and pushes its
// neighbours, so the mouse selects a different row than the eye is on. The
// first row is the largest at all times instead, which is where the
// reference's emphasis actually lives.
void draw_item_list(render::PixelCanvas& canvas, const MenuModel& model,
                    const ListPlan& p) {
    const size_t n = model.item_count();
    for (size_t i = 0; i < n && i < p.boxes.size(); ++i) {
        const MenuRowBox& b = p.boxes[i];
        const bool sel = (i == model.selection());
        const int px = row_px(p.m, i);
        const Row r = menu_row(model, i);
        ui_draw_text(canvas, b.x, b.y, r.label, sel ? ITEM_BRIGHT : ITEM_DIM, px,
                     /*shadow=*/true);
        if (sel) {
            // THE RULE UNDER THE WORD is what carries the selection when the
            // player is colour-blind or the monitor is uncalibrated -- the
            // brightness difference between the two item colours is a couple of
            // quantizer steps, which is the weakest signal this project has.
            const int thick = std::max(1, px / 20);
            canvas.fill_rect(b.x, b.y + b.h + std::max(2, px / 8), b.w, thick,
                             ITEM_BRIGHT);
        }
        if (p.blurb_y[i] >= 0) {
            const int bw = tw_of(r.blurb, p.blurb_px);
            const int bx = p.right_aligned ? p.m.edge - bw
                                           : (static_cast<int>(canvas.width()) - bw) / 2;
            ui_draw_text(canvas, bx, p.blurb_y[i], r.blurb, BLURB, p.blurb_px,
                         /*shadow=*/true);
        }
    }
}

// The bounding rectangle of everything draw_item_list will put on the canvas.
// The pause page needs it to know what to lay a plate under, and a plate sized
// from anything but the drawn text is the defect the pause screen was fixed for
// in August.
MenuRowBox list_bounds(const ListPlan& p) {
    MenuRowBox out;
    bool first = true;
    for (const MenuRowBox& b : p.boxes) {
        if (first) {
            out = b;
            first = false;
            continue;
        }
        const int x1 = std::max(out.x + out.w, b.x + b.w);
        const int y1 = std::max(out.y + out.h, b.y + b.h);
        out.x = std::min(out.x, b.x);
        out.y = std::min(out.y, b.y);
        out.w = x1 - out.x;
        out.h = y1 - out.y;
    }
    return out;
}

// The studio's signature, small, in the corner the reference leaves empty.
void draw_studio_mark(render::PixelCanvas& canvas) {
    const int h = static_cast<int>(canvas.height());
    const int icon = std::max(8, h / 22);
    const int margin = std::max(4, h / 36);
    const int y = h - margin - icon;
    draw_image_fit(canvas, cached_png(BRAND_SPIRAL_ICON_PNG), margin, y, icon, icon,
                   0.55f);
    const int px = px_for(h, UiText::Small);
    ui_draw_text(canvas, margin + icon + std::max(3, icon / 4),
                 y + (icon - std::max(1, ui_cap_height(px))) / 2, loc("menu.studio"),
                 STUDIO_MARK, px, /*shadow=*/true);
}

// --- ПОДСКАЗКА О КЛАВИШАХ: СНЯТА -------------------------------------------
// Здесь стояла draw_keys_hint() — одна строка внизу каждой страницы: «Стрелки
// или мышь — выбор, Enter или щелчок — принять, Esc — назад». Заказ владельца
// 27.08 дословно: «весь вспомогательный текст подсказок ВООБЩЕ отовсюду
// удалить — это ужасно выглядит, просто кошмарно такие вещи подписывать,
// особенно на лице игры — главном меню».
//
// СНЯТА, А НЕ СПРЯТАНА, и это разные вещи. Пока функция существует и не
// зовётся, она мёртвый код, который следующий агент включит обратно, приняв
// за забытое; ключи локализации, которые она читала (menu.hint, menu.hint.short,
// menu.hint.mouse), убраны из ru.txt вместе с ней, чтобы не осталось строки без
// читателя. Управление меню при этом не изменилось ни на клавишу: стрелки,
// мышь, Enter, щелчок и Esc работают ровно как работали — исчезла подпись, а не
// поведение. Экран управления (menu.controls) остался единственным местом, где
// клавиши названы, и это место, куда за ними ИДУТ.

// --- THE START SCREEN -------------------------------------------------------
// The owner's reference is Skyrim SE's main menu: black with rare, slowly
// drifting motes; a large emblem in the middle-left; a right-aligned column of
// thin capitals low on the right, no frames and no button plates, the first
// item larger than the rest. Ours puts the SEAL OF THE YAAN EMPIRE where the
// reference puts the dragon sigil (assets/branding/README.txt, owner 26.08),
// and the studio's mark in the opposite corner.
void draw_root(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    canvas.clear(SCREEN_BLACK);
    draw_dust(canvas, model.time(), dust_count(w, h));

    // THE EMBLEM'S BOX IS A FRACTION OF THE HEIGHT, never of the width: a wide
    // frame must move it sideways, not inflate it. Centre-left and above the
    // middle, so the wordmark beneath it has its own air.
    //
    // AND IT IS COMPOSITED AT LESS THAN FULL OPACITY. The seal is a PARCHMENT
    // disc -- nearly white -- and on true black at full strength it reads as a
    // moon rather than as an emblem: the first frame of this screen had the
    // brightest object on it be a background element, with the item column, the
    // thing the player has to read, a good deal dimmer. Taking it down to 0.82
    // puts it where the reference's sigil sits: present, large, and NOT the
    // brightest thing in the frame.
    const int box = h / 2;
    const int cx = w * 17 / 50;
    const int cy = h * 7 / 16;
    draw_image_fit(canvas, cached_png(BRAND_SEAL_PNG), cx - box / 2, cy - box / 2, box,
                   box, 0.82f);

    // The game's name under the emblem, spaced out: it is a wordmark here, not
    // a sentence, and the tracking is what makes five px letters read as one.
    const int title_px = px_for(h, UiText::Title);
    const std::string_view title = loc("app.title");
    const int tw = tw_of(title, title_px);
    ui_draw_text(canvas, cx - tw / 2, cy + box / 2 + h / 24, title, TITLE, title_px,
                 /*shadow=*/true);

    draw_item_list(canvas, model, plan_list(w, h, model));
    draw_studio_mark(canvas);
}

// --- THE PAUSE SCREEN -------------------------------------------------------
// The same column, over the world instead of over black, and with the SAME SIX
// ROWS whatever the player was doing when he pressed Escape (owner, 26.08).
void draw_pause(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());

    // The veil is a HALF cover by construction -- every other scanline -- so the
    // player can still see where he left off. It is not what the text stands on:
    // measured on the archived playing frame, 81.0 % of the ink over bright
    // background sat closer than two quantizer steps to what it covered. The
    // plate below is what the text stands on.
    canvas.clear_transparent();
    for (int y = 0; y < h; ++y) {
        if ((y & 1) == 0) {
            canvas.fill_rect(0, y, w, 1, BACKGROUND);
        }
    }

    const ListPlan p = plan_list(w, h, model);
    const MenuRowBox block = list_bounds(p);
    const int title_px = px_for(h, UiText::Title);
    const std::string_view title = loc("menu.paused");
    const int title_w = tw_of(title, title_px);
    const int title_x = p.m.edge - title_w;

    // ONE PLATE UNDER THE TITLE AND THE ROWS TOGETHER, sized from the rectangle
    // the rows actually occupy plus the title above them.
    const int plate_x = std::min(block.x, title_x);
    const int plate_y = std::min(block.y, p.title_y);
    draw_text_plate(canvas, plate_x, plate_y, p.m.edge - plate_x,
                    block.y + block.h - plate_y, /*pad=*/std::max(4, p.m.gap));

    ui_draw_text(canvas, title_x, p.title_y, title, TITLE, title_px, /*shadow=*/true);
    draw_item_list(canvas, model, p);
}

// --- THE MAP BROWSER --------------------------------------------------------
// Centred rather than right-aligned: this is a FILE PICKER, and its rows are
// names of unpredictable length that have to be scanned rather than aimed at.
void draw_browser(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    canvas.clear(SCREEN_BLACK);
    draw_dust(canvas, model.time(), dust_count(w, h) / 2);

    const ListPlan p = plan_list(w, h, model);
    // The two levels NAME THEMSELVES, so the player always knows which of them
    // he is on. cat_title_key outlives the draw below because it is a local
    // string and loc() returns a view into the localization table, not into it.
    std::string cat_title_key;
    std::string_view title = loc("map.browser.title");
    if (model.page() == MenuPage::CategoryMaps) {
        const MapCatalog* cat = model.catalog();
        if (cat != nullptr && model.chosen_category() < cat->categories.size()) {
            cat_title_key = "map.category." + cat->categories[model.chosen_category()].slug;
            title = loc(cat_title_key);
        }
    }
    const int title_px = px_for(h, UiText::Title);
    const int tw = tw_of(title, title_px);
    ui_draw_text(canvas, (w - tw) / 2, p.title_y, title, TITLE, title_px,
                 /*shadow=*/true);
    canvas.fill_rect(w / 4, p.title_y + ui_cap_height(title_px) + p.m.gap, w / 2,
                     std::max(1, title_px / 24), RULE_LINE);

    draw_item_list(canvas, model, p);

    // BROWSER STATUS: a non-fatal message the app handed in, e.g. a map whose
    // source is a .dfw the baker has not produced yet. Honest-failure surface
    // (docs/MAP_LAYOUT.md): a source that cannot open says so on screen instead
    // of doing nothing.
    if (!model.browser_status().empty()) {
        const int s = px_for(h, UiText::Caption);
        const int sw = tw_of(model.browser_status(), s);
        ui_draw_text(canvas, (w - sw) / 2, h - std::max(4, h / 12), model.browser_status(),
                     ITEM_SELECTED, s, /*shadow=*/true);
    }
}

// --- CREDITS ----------------------------------------------------------------
// THE OAK LINE IS A LICENCE CONDITION, NOT A COURTESY. The emblem's silhouette
// is "Quercus robur Silhouette" by oddsock, CC BY 2.0 (assets/branding/
// README.txt), and CC BY requires the attribution to be shown wherever the work
// is used. It is drawn from its own localization key so no translation can drop
// it by shortening a paragraph.
void draw_credits(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    canvas.clear(SCREEN_BLACK);
    draw_dust(canvas, model.time(), dust_count(w, h));

    const int title_px = px_for(h, UiText::Title);
    const int body_px = px_for(h, UiText::Caption);
    const int gap = std::max(3, body_px / 2);
    const int icon = std::max(10, h / 10);
    draw_image_fit(canvas, cached_png(BRAND_SPIRAL_ICON_PNG), (w - icon) / 2, h / 12,
                   icon, icon, 0.9f);

    struct Line {
        const char* key;
        int px;
        render::Color color;
    };
    const Line lines[] = {
        {"menu.credits.studio", title_px, TITLE},
        {"menu.credits.game", body_px, ITEM_DIM},
        {"menu.credits.engine", body_px, BLURB},
        {"menu.credits.art", body_px, BLURB},
        // The mandatory one. Kept in the item colour rather than the blurb
        // colour: an attribution nobody can read is an attribution nobody made.
        {"menu.credits.oak", body_px, ITEM_DIM},
        // ВТОРАЯ ОБЯЗАТЕЛЬНАЯ СТРОКА, и обязательна она по той же причине, что
        // и первая: SIL OFL 1.1 требует сохранять уведомление об авторских
        // правах шрифта всюду, где шрифт используется, а используется он
        // ровно на этом экране и на всех соседних (assets/fonts/OFL.txt).
        {"menu.credits.font", body_px, ITEM_DIM},
    };
    int y = h / 12 + icon + gap * 2;
    for (const Line& l : lines) {
        const std::string_view text = loc(l.key);
        const int tw = tw_of(text, l.px);
        ui_draw_text(canvas, (w - tw) / 2, y, text, l.color, l.px, /*shadow=*/true);
        y += std::max(1, ui_line_height(l.px)) + gap;
    }
}

// --- THE "NOT YET" PAGE -----------------------------------------------------
// Where a row whose system does not exist lands. It says WHICH thing is missing
// and takes one key to leave. See MenuModel::open_stub for why the row is drawn
// at all rather than hidden.
void draw_stub(render::PixelCanvas& canvas, const MenuModel& model) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    canvas.clear(SCREEN_BLACK);
    draw_dust(canvas, model.time(), dust_count(w, h));

    const int title_px = px_for(h, UiText::Title);
    const int body_px = px_for(h, UiText::Item);
    const std::string_view title = loc("menu.stub.title");
    const int tw = tw_of(title, title_px);
    ui_draw_text(canvas, (w - tw) / 2, h / 3, title, TITLE, title_px, /*shadow=*/true);

    const std::string_view msg = localized(model.stub_message());
    const int mw = tw_of(msg, body_px);
    ui_draw_text(canvas, (w - mw) / 2, h / 2, msg, ITEM_DIM, body_px, /*shadow=*/true);
}

} // namespace

void draw_menu(render::PixelCanvas& canvas, const MenuModel& model) {
    canvas.resize(canvas.width(), canvas.height());
    switch (model.page()) {
    case MenuPage::Splash:
        // ПРЕДЗАПИСАННОЕ ВИДЕО, А НЕ РИСОВАНИЕ В КАДРЕ (заказ владельца 27.08).
        // Запасная ветка — прежняя нарисованная заставка, и она НЕ мёртвый код:
        // актив интро может не собраться, а экран, который в этом случае
        // остаётся пустым, — это ровно та жалоба, с которой всё началось.
        if (!draw_intro(canvas, model.time())) {
            draw_studio_splash(canvas, model.time(), model.splash_seconds());
        }
        return;
    case MenuPage::Root:
        draw_root(canvas, model);
        return;
    case MenuPage::Pause:
        draw_pause(canvas, model);
        return;
    case MenuPage::Categories:
    case MenuPage::CategoryMaps:
        draw_browser(canvas, model);
        return;
    case MenuPage::Credits:
        draw_credits(canvas, model);
        return;
    case MenuPage::Stub:
        draw_stub(canvas, model);
        return;
    case MenuPage::Calibrate:
        draw_calibration(canvas, model);
        return;
    case MenuPage::Settings:
        draw_settings(canvas, model);
        return;
    case MenuPage::Controls:
        draw_controls(canvas);
        return;
    }
}

} // namespace dfn::app
