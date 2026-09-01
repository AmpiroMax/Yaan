/*
Module: engine/app
File: engine/app/sources/CharGen.h

Responsibility:
- ЭКРАН СОЗДАНИЯ ПЕРСОНАЖА: КАРКАС ИЗ КАТЕГОРИЙ И СТРОК, где что лежит на
  холсте, куда смотрит портретная камера и что происходит от стрелки, клавиши,
  колеса и мыши. Данные внутрь — данные наружу: весь экран проверяется без
  окна.

Key items:
- CharGenRow / CharGenCategory / chargen_describe(): ОПИСАНИЕ экрана. Что на
  нём есть, сказано в одном месте и данными.
- CharGenScreen: категории, выбор, ввод, отрисовка.
- CharGenView / chargen_orbit() / chargen_zoom() / chargen_in_camera():
  портретный облёт в границах и приближение к лицу — чистая арифметика.
- chargen_layout(): раскладка, ОДНА на глаз и на указатель.

Dependencies:
- Uses: engine/render PixelCanvas / FirstPersonCamera, engine/app UiSlider,
  CharGenBody (полоса роста и имя ручки роста), UiFont, Localization. НЕ
  видеокарта и НЕ файлы: тело и свет — в CharGenBody.h и AppCharGen.cpp.
- Used by: engine/app (AppCharGen.cpp, App.cpp), tests/app/CharGenTests.cpp.

Notes:
- ЭКРАН СТРОИТСЯ ОПИСАНИЕМ, А НЕ ОТРИСОВКОЙ (заказ владельца на дизайн-сессию
  редактора: «дать возможность редактировать как можно больше»). Категорий
  сегодня две, завтра будет семь — Тело, Лицо, Кожа и цвета, Волосы, Отметины,
  Имя и происхождение, — и каркас обязан принять их СТРОКОЙ В ОПИСАНИИ, а не
  правкой раскладки, ввода и рисования в трёх местах. Поэтому строка экрана —
  это ЗАПИСЬ С ВИДОМ (ползунок, переключатель вариантов, поле ввода, кнопка), а
  не отдельная ветка кода на каждый вопрос.
- КАТЕГОРИЯ «ТЕЛОСЛОЖЕНИЕ» ЗАПОЛНЯЕТСЯ ИЗ ФАЙЛА ТЕЛА, а не из списка здесь:
  полосы ползунков лежат в секции MORF и измерены приёмкой шага 1, и новая
  цель в .dfo сама становится строкой экрана. Остальные категории придут
  данными, когда дизайн-сессия назовёт их содержимое.
- ГЛАГОЛЫ («Сброс», «Готово», «Назад») НЕ ПРИНАДЛЕЖАТ КАТЕГОРИИ и стоят под
  списком всегда. Они про персонажа целиком; спрятать «Готово» внутрь вкладки
  значило бы, что кнопка выхода зависит от того, на какой вкладке стоял игрок.
- РОСТ — ДВЕНАДЦАТАЯ РУЧКА, НО НЕ ЦЕЛЬ MORF, и это вывод шага 1: чистым морфом
  рост невозможен — судья пропускает ±1 см, потому что морф двигает МЕШ, а не
  суставы, и голова отрывается от черепа (6.16 голов в фигуре при +1 против
  канона 7.5-8.0). Рост едет РАВНОМЕРНЫМ МАСШТАБОМ рест-скелета и меша.
- ПОЧЕМУ ЭКРАН НЕ СТРАНИЦА МЕНЮ. У страниц меню один орган управления —
  список строк, — и вся их арифметика (menu_row_boxes) построена на этом. У
  этого экрана их пять: вкладки, непрерывные ручки с захватом, переключатели,
  поле ввода и облёт камеры. Вписать их в MenuPage значило бы сделать пять
  исключений внутри модели, у которой сегодня нет ни одного.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- Чистые функции и никакого окна: время и размер холста — параметры. Два
  прогона одной дозы обязаны кадрировать одинаково (правило 13).
- Всякая видимая строка — КЛЮЧ ЛОКАЛИЗАЦИИ (правило 5). Имя персонажа —
  содержимое, которое ввёл игрок, и печатается дословно.
- НОВАЯ КАТЕГОРИЯ — ЭТО СТРОКА В chargen_describe(), а не ветка в draw().
*/

#pragma once

#include "engine/app/sources/CharGenBody.h"
#include "engine/app/sources/UiSlider.h"
#include "engine/render/sources/PixelCanvas.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::render {
class FirstPersonCamera;
} // namespace dfn::render

namespace dfn::app {

// --- ПОРТРЕТНАЯ КАМЕРА ------------------------------------------------------

/// ОБЛЁТ В ГРАНИЦАХ. Фигура стоит, а поворачивается ВИД: рыскание — вокруг
/// её вертикали, тангаж — вокруг горизонтали кадра.
///
/// ГРАНИЦЫ, А НЕ СВОБОДА, и это заказ («облёт мышью в границах»): экран
/// создания персонажа — портрет, а портрет снимают с лица. Полный оборот
/// показал бы затылок как равноправный вид и позволил бы уехать под фигуру,
/// откуда не видно ничего, что ползунки меняют.
struct CharGenView {
    float yaw = 0.0f;    ///< рад; 0 = анфас, + поворачивает фигуру влево
    float pitch = 0.0f;  ///< рад; + смотрит сверху
    float zoom = 0.0f;   ///< 0 = вся фигура, 1 = лицо
};

inline constexpr float CHARGEN_YAW_LIMIT = 2.6180f;  ///< ±150°
inline constexpr float CHARGEN_PITCH_LIMIT = 0.3491f; ///< ±20°
/// ОДНА НОТКА КОЛЕСА — двадцатая пути от фигуры к лицу. Двадцать нажатий на
/// весь ход: столько же, сколько у стрелки ползунка, и по той же причине.
inline constexpr float CHARGEN_ZOOM_STEP = 0.05f;

/// РАЗВОРОТ, КОТОРЫЙ СТАВИТ ФИГУРУ ЛИЦОМ К ГЛАЗУ, радианы. Половина оборота,
/// и это не подгонка, а следствие двух соглашений, которые сходятся спиной к
/// спине: персонаж «смотрит вперёд» вдоль −Z (перёд бакает импортёр,
/// docs/RIG.md), и КАМЕРА тоже смотрит вдоль −Z. Поставленная перед камерой
/// как есть, фигура показывает затылок — что первый кадр приёмки и показал.
/// Число живёт здесь, а не прибавляется к рысканию у вызывающего, потому что
/// иначе «рыскание ноль» значило бы «анфас» в отрисовке и «затылок» в
/// границах облёта, а границы симметричны вокруг нуля.
inline constexpr float CHARGEN_MODEL_FACE_YAW = 3.14159265f;

/// ГДЕ СТОИТ ФИГУРА В КАДРЕ, доли. Правее середины — потому что слева стоит
/// колонка ползунков, и это единственная композиционная связь между двумя
/// половинами экрана. На КРУПНОМ ПЛАНЕ ось едет к середине: голова с плечами
/// вчетверо шире фигуры в полный рост и на прежней оси уходила за правый край.
inline constexpr float CHARGEN_FIGURE_X_FRAC = 0.685f;
inline constexpr float CHARGEN_FACE_X_FRAC = 0.640f;
/// Сколько высоты кадра занимает фигура целиком и сколько — голова.
inline constexpr float CHARGEN_BODY_FILL = 0.86f;
inline constexpr float CHARGEN_FACE_FILL = 0.54f;
/// Какая доля РОСТА видна на крупном плане и где её середина (0 — стопы,
/// 1 — макушка). Голова фигуры (макушка минус шея) — 0.132 м из 1.750, то
/// есть 0.0754 роста; в кадр берётся втрое больше, чтобы в него попали шея и
/// плечи — по ним и читается посадка головы.
inline constexpr float CHARGEN_FACE_SPAN_FRAC = 0.220f;
inline constexpr float CHARGEN_FACE_CENTER_FRAC = 0.915f;
/// Глубина фигуры — доля расстояния до холста экрана, как у герба меню: холст
/// висит на near*1.5, и выражение в его долях переживёт правку ближней
/// плоскости.
inline constexpr float CHARGEN_DEPTH_FRAC = 1.35f;

/// Движение мыши в облёт, пиксели внутрь, радианы наружу; зажато границами.
void chargen_orbit(CharGenView& view, float dx_px, float dy_px, float sensitivity);
/// Нотки колеса в приближение, зажатое в [0, 1].
void chargen_zoom(CharGenView& view, float notches);

/// ТОЧКА ФИГУРЫ, КОТОРАЯ СТОИТ В СЕРЕДИНЕ КАДРА, в её собственных осях.
/// Она же ось поворота облёта И ЯКОРЬ ПОРТРЕТНОГО СВЕТА: три источника стоят
/// в долях фигуры ОТ НЕЁ, поэтому на крупном плане свет приезжает к голове
/// вместе с кадром. Первая версия вешала их на середину габарита — и на
/// крупном плане ключевой оказывался у самого лба, выбеливая макушку.
[[nodiscard]] glm::vec3 chargen_pivot(const glm::vec3& lo, const glm::vec3& hi,
                                      float zoom);

/// МАТРИЦА ФИГУРЫ В ОСЯХ КАМЕРЫ (камера в нуле, смотрит в −Z). `lo`/`hi` —
/// габарит меша в его собственном пространстве; `height_scale` — множитель
/// роста, применённый ЗДЕСЬ же, чтобы кадрирование не зависело от того,
/// перепечён ли меш.
///
/// ОТДЕЛЬНО ОТ ВИДЕОКАРТЫ НАРОЧНО: раскладка ошибается молча, и померить её
/// должен прибор без окна.
[[nodiscard]] glm::mat4 chargen_in_camera(const render::FirstPersonCamera& camera,
                                          const glm::vec3& lo, const glm::vec3& hi,
                                          float height_scale, const CharGenView& view);

// --- ОПИСАНИЕ ЭКРАНА --------------------------------------------------------

/// ВИД СТРОКИ. Четыре, и это ровно те четыре органа, из которых складывается
/// любой редактор персонажа: непрерывная величина, выбор из списка, свободный
/// текст и действие. Пятого вида ждать неоткуда — цвет и причёска это выбор
/// из списка, а обхват это ползунок.
enum class CharGenRowKind : std::uint8_t {
    Slider,  ///< непрерывная величина в полосе (телосложение, рост)
    Option,  ///< выбор из списка стрелками (причёска, цвет, происхождение)
    Text,    ///< свободный ввод (имя)
    Button,  ///< действие (сброс, готово, назад)
};

/// ЧТО ЭКРАН ПРОСИТ СДЕЛАТЬ. Reset ползунки гасит сам; Done и Back — события
/// для приложения, потому что печь файл и уходить из режима умеет только оно.
enum class CharGenAction : std::uint8_t { None, Reset, Done, Back };

/// ОДНА СТРОКА ЭКРАНА. Поля, которых её вид не касается, просто не читаются —
/// и это дешевле разнотипной иерархии ровно потому, что строк на экране
/// десятки, а видов четыре: наследование здесь дало бы четыре класса, четыре
/// посетителя и ни одного нового ответа.
struct CharGenRow {
    CharGenRowKind kind = CharGenRowKind::Slider;
    /// ИДЕНТИФИКАТОР. У ползунка телосложения — имя цели MORF из ФАЙЛА тела;
    /// у прочих — имя поля пресета. Он же связывает строку с тем, что она
    /// меняет: приложение получает имя, а не номер, потому что номер меняется
    /// при первой же новой цели в .dfo.
    std::string name;
    /// КЛЮЧ ПОДПИСИ. Пусто — подпись ищется как "morph.slider." + name, чем и
    /// пользуется категория телосложения: подпись новой цели заводится
    /// строкой в локализации, а не строкой в коде.
    std::string label_key;

    // Slider
    float lo = 0.0f;
    float hi = 1.0f;
    float value = 0.0f;
    /// Показывать ли значение в метрах (рост). Единица — ключ локализации.
    bool metres = false;

    // Option: варианты как КЛЮЧИ ЛОКАЛИЗАЦИИ, потому что вариант — это
    // видимое слово («каштановые», «северянин»), а не идентификатор.
    std::vector<std::string> choices;
    std::size_t choice = 0;

    // Text
    std::string text;
    std::size_t text_max = CHARGEN_NAME_MAX_CHARS_DEFAULT;

    // Button
    CharGenAction action = CharGenAction::None;

    static constexpr std::size_t CHARGEN_NAME_MAX_CHARS_DEFAULT = 24;
};

/// ОДНА ВКЛАДКА. `key` — ключ локализации её имени.
struct CharGenCategory {
    std::string key;
    std::vector<CharGenRow> rows;
};

/// СКОЛЬКО ЗНАКОВ ВЛЕЗАЕТ В ИМЯ. Не украшение: строка ввода рисуется в одну
/// колонку, и имя, которое туда не влезло, молча уехало бы за край панели.
inline constexpr std::size_t CHARGEN_NAME_MAX_CHARS =
    CharGenRow::CHARGEN_NAME_MAX_CHARS_DEFAULT;

/// ИМЯ СТРОКИ ИМЕНИ. Названо один раз: его знают описание, пресет и доза.
inline constexpr const char* CHARGEN_NAME_ROW = "name";

/// ОПИСАНИЕ ЭКРАНА — ЕДИНСТВЕННОЕ МЕСТО, ГДЕ СКАЗАНО, ЧТО НА НЁМ ЕСТЬ.
///
/// `body_rows` — ползунки категории «Телосложение»: они приходят ИЗ ФАЙЛА
/// тела (секция MORF плюс рост), потому что их полосы измерены приёмкой и
/// список меняется вместе с .dfo, а не вместе с этим файлом.
///
/// Остальные категории дизайн-сессии («Лицо», «Кожа и цвета», «Волосы»,
/// «Отметины») добавляются СТРОКОЙ ЗДЕСЬ и больше нигде: раскладка, ввод и
/// отрисовка спрашивают вид строки, а не её смысл.
[[nodiscard]] std::vector<CharGenCategory> chargen_describe(
    std::vector<CharGenRow> body_rows);

/// ТРИ ГЛАГОЛА ПОД СПИСКОМ, одни на все вкладки. Отдельно от категорий, см.
/// запись в шапке.
[[nodiscard]] std::vector<CharGenRow> chargen_verbs();

// --- РАСКЛАДКА --------------------------------------------------------------

/// РАСКЛАДКА ЭКРАНА, в пикселях холста. Считается ОДИН раз и читается и
/// отрисовкой, и указателем — тот же довод, что у menu_row_boxes: две копии
/// сходятся в день, когда написаны, и расходятся в день, когда меняется
/// размер строки, а симптом этого — «экран иногда не берёт мой клик».
struct CharGenLayout {
    int title_px = 1;
    int item_px = 1;
    int hint_px = 1;
    int title_y = 0;
    int tabs_y = 0;       ///< верх полосы вкладок
    int first_y = 0;
    int step = 1;
    int label_x = 0;      ///< левый край подписей
    int track_x = 0;      ///< левый конец жёлоба
    int track_w = 1;
    int value_right = 0;  ///< правый край колонки значений
    int panel_right = 0;  ///< докуда простирается колонка: правее — фигура
    int handle_w = 3;
    int handle_h = 3;
    [[nodiscard]] int row_y(std::size_t i) const {
        return first_y + static_cast<int>(i) * step;
    }
    [[nodiscard]] SliderTrack track_of(std::size_t i) const {
        return SliderTrack{track_x, row_y(i), track_w, handle_w, handle_h};
    }
};

[[nodiscard]] CharGenLayout chargen_layout(int canvas_w, int canvas_h,
                                           std::size_t row_count);

/// ЯЩИК ОДНОЙ ВКЛАДКИ, в пикселях холста.
struct CharGenTabBox {
    int x = 0;
    int w = 0;
};

/// ГДЕ ЛЕЖИТ КАЖДАЯ ВКЛАДКА. ПО ШИРИНЕ СВОЕЙ НАДПИСИ, а не поровну: первый же
/// кадр с двумя вкладками показал, почему — «Телосложение» шире половины
/// панели, и «Имя» въехало в него буквами. Одна арифметика на глаз и на
/// указатель (тот же довод, что у menu_row_boxes): вторая копия расходится с
/// первой в день, когда меняется шрифт, а симптом этого — «экран иногда не
/// берёт мой клик по вкладке».
[[nodiscard]] std::vector<CharGenTabBox> chargen_tab_boxes(
    const CharGenLayout& layout, const std::vector<CharGenCategory>& categories);

// --- ЭКРАН ------------------------------------------------------------------

class CharGenScreen {
public:
    void set_categories(std::vector<CharGenCategory> categories);
    [[nodiscard]] const std::vector<CharGenCategory>& categories() const {
        return categories_;
    }
    [[nodiscard]] std::size_t category() const { return category_; }
    void set_category(std::size_t index);
    /// Следующая/предыдущая вкладка, по кругу.
    void cycle_category(int delta);

    /// Строки ТЕКУЩЕЙ вкладки.
    [[nodiscard]] const std::vector<CharGenRow>& rows() const;
    /// Строка по имени, В ЛЮБОЙ категории, — для того, кто собирает пресет
    /// или пришёл дозой. nullptr, если такой строки нет.
    [[nodiscard]] const CharGenRow* find(std::string_view name) const;

    /// Строк на экране: строки вкладки плюс глаголы.
    [[nodiscard]] std::size_t row_count() const;
    [[nodiscard]] CharGenRowKind row_kind(std::size_t row) const;
    /// Строка по номеру экрана (вкладка, затем глаголы), или nullptr.
    [[nodiscard]] const CharGenRow* row_at_index(std::size_t row) const;

    [[nodiscard]] std::size_t selection() const { return selection_; }
    void set_selection(std::size_t row);
    void move(int delta);

    /// СТРЕЛКА ВБОК ПО ВЫБРАННОЙ СТРОКЕ: ползунок двигает, переключатель
    /// листает. Возвращает номер изменённой строки или row_count(), если
    /// строка от стрелки не меняется.
    [[nodiscard]] std::size_t adjust(int delta, bool fine);
    /// Ставит значение ползунка, ЗАЖИМАЯ его в полосу. true — изменилось.
    bool set_value(std::size_t row, float value);
    /// То же по имени — так его зовёт доза и пресет.
    bool set_value(std::string_view name, float value);
    /// Все ползунки в ноль, рост — в канон, переключатели — в первый вариант.
    /// Текст НЕ трогается: сброс — про облик, а не про имя.
    void reset_rows();

    // --- ТЕКСТОВАЯ СТРОКА -------------------------------------------------
    /// Кодпоинты кадра (IInput::text_input) в выбранное поле ввода, UTF-8.
    /// Управляющие знаки отбрасываются — тем же правилом, что и в чате.
    void feed_text(const std::vector<std::uint32_t>& codepoints);
    void backspace();
    /// Стоит ли курсор на поле ввода: пока да, буквы идут в текст, а не в
    /// горячие клавиши. Один вопрос — один ответ.
    [[nodiscard]] bool text_focused() const;
    /// Имя персонажа — строка CHARGEN_NAME_ROW, где бы её ни поставило
    /// описание.
    [[nodiscard]] std::string name() const;
    void set_name(std::string value);

    // --- МЫШЬ -------------------------------------------------------------
    /// Строка под точкой, или row_count() — «ни на чём». Пустой ответ здесь
    /// настоящий: наведение на фигуру не должно двигать выбор.
    [[nodiscard]] std::size_t row_at(int canvas_w, int canvas_h, int x, int y) const;
    /// Вкладка под точкой, или categories().size().
    [[nodiscard]] std::size_t tab_at(int canvas_w, int canvas_h, int x, int y) const;
    /// Нажатие. Возвращает номер захваченной строки-ползунка, или row_count().
    [[nodiscard]] std::size_t press(int canvas_w, int canvas_h, int x, int y);
    /// Ведение захваченной ручки. true — значение изменилось.
    bool drag(int canvas_w, int canvas_h, int x);
    void release();
    [[nodiscard]] bool dragging() const { return drag_row_ < row_count(); }
    [[nodiscard]] std::size_t dragged_row() const { return drag_row_; }
    /// Правее этой границы холста лежит фигура: нажатие там крутит облёт.
    [[nodiscard]] bool over_figure(int canvas_w, int canvas_h, int x) const;

    /// ENTER / ЩЕЛЧОК ПО ВЫБРАННОЙ СТРОКЕ.
    [[nodiscard]] CharGenAction activate();

    [[nodiscard]] CharGenView& view() { return view_; }
    [[nodiscard]] const CharGenView& view() const { return view_; }

    /// СТРОКА СОСТОЯНИЯ — то, что экран сказал в ответ на «Готово»: куда лёг
    /// пресет, чем кончилась выпечка. Пишет приложение, рисует экран.
    void set_status(std::string text) { status_ = std::move(text); }
    [[nodiscard]] const std::string& status() const { return status_; }

    void draw(render::PixelCanvas& canvas) const;

private:
    [[nodiscard]] CharGenRow* mutable_row(std::size_t row);
    [[nodiscard]] CharGenRow* mutable_row(std::string_view name);

    std::vector<CharGenCategory> categories_;
    std::vector<CharGenRow> verbs_ = chargen_verbs();
    std::string status_;
    std::size_t category_ = 0;
    std::size_t selection_ = 0;
    std::size_t drag_row_ = 0; ///< == row_count() значит «не тянем»
    CharGenView view_{};
};

} // namespace dfn::app
