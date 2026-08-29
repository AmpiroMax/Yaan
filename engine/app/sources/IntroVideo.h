/*
Module: engine/app
File: engine/app/sources/IntroVideo.h

Responsibility:
- ПРОИГРЫВАТЕЛЬ ПРЕДЗАПИСАННОГО ИНТРО. Заказ владельца 27.08 дословно: «Это
  должно быть ПРЕДЗАПИСАННОЕ ВИДЕО, а не кодом рисующиеся элементы». Здесь
  живёт ровно это и ничего больше: контейнер .dfv читается в память, кадр по
  времени ДЕКОДИРУЕТСЯ и кладётся на холст. Ни одной фигуры этот файл на экран
  не рисует — иначе интро снова стало бы кодом.

Key items:
- IntroVideo: открытый контейнер (кадры лежат сжатыми, декодируется один).
- intro_video(): единственный экземпляр, открытый по умолчанию из INTRO_PATH.
- draw_intro(): кадр на момент t секунд, вписанный в холст.

Dependencies:
- Uses: engine/app PngImage (тот же декодер, что читает бренд), engine/render
  PixelCanvas. Ни одной сторонней библиотеки: контейнер описан в tools/gen_intro.py
  и разбирается здесь руками.
- Used by: engine/app Menu (страница Splash) и App (длительность заставки).

Notes:
- КАДР ВЫБИРАЕТСЯ ПО ЧАСАМ, А НЕ ПО СЧЁТЧИКУ КАДРОВ. Если декодер не успел,
  проигрыватель ПРОПУСКАЕТ кадры и остаётся на стенных часах — это поведение
  любого видеоплеера и единственное, при котором интро длится столько, сколько
  обещало. Счётчик кадров растянул бы трёхсекундное интро на сколько угодно на
  медленной машине, а это ровно та жалоба, с которой всё началось.
- ОТСУТСТВИЕ АКТИВА — ГРОМКОЕ. Нет файла или он не разобран — duration() == 0,
  и приложение просто открывает меню, сказав об этом в stderr. Молча показать
  пустой экран нельзя: именно так выглядела заставка, которая «висела серым».

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/app/sources/PngImage.h"

namespace dfn::render {
class PixelCanvas;
}

namespace dfn::app {

/// Путь актива интро. Назван один раз здесь, потому что его читают двое —
/// проигрыватель и приложение (оно спрашивает длительность).
inline constexpr const char* INTRO_PATH = "assets/intro/spiral_intro.dfv";

class IntroVideo {
public:
    /// Читает контейнер целиком в память (единицы мегабайт — см. бюджет в
    /// tools/gen_intro.py). Кадры остаются СЖАТЫМИ: распакованное интро — это
    /// сотни мегабайт, и держать их ради трёх секунд нельзя.
    bool open(const std::string& path);

    [[nodiscard]] bool valid() const { return !offsets_.empty(); }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] size_t frame_count() const { return offsets_.size(); }
    /// Длительность в секундах. 0 — актива нет; вызывающий обязан считать это
    /// «интро не показывать», а не «интро мгновенное».
    [[nodiscard]] float duration_s() const;

    /// Кадр на момент t. Возвращает nullptr, когда актива нет или t за концом.
    /// Декодирование КЭШИРУЕТСЯ по индексу: несколько вызовов внутри одного
    /// кадра приложения стоят один inflate.
    [[nodiscard]] const Image* frame_at(float t_s);

private:
    struct Span {
        size_t at = 0;
        size_t size = 0;
    };
    std::vector<uint8_t> bytes_;
    std::vector<Span> offsets_;
    int width_ = 0;
    int height_ = 0;
    uint32_t fps_num_ = 30;
    uint32_t fps_den_ = 1;
    // Последний распакованный кадр и его номер. -1 — ещё ни одного.
    Image decoded_;
    long decoded_index_ = -1;
};

/// Единственный проигрыватель, открытый по INTRO_PATH при первом обращении.
/// Открывается один раз: файл не должен перечитываться на каждом кадре меню.
IntroVideo& intro_video();

/// Кладёт кадр интро на момент t секунд в холст, вписанным по высоте и с
/// сохранением пропорций; поля — настоящий чёрный. Возвращает false, когда
/// класть нечего (актива нет), и тогда вызывающий рисует свою запасную
/// заставку — ПУСТОЙ кадр отдавать нельзя.
bool draw_intro(render::PixelCanvas& canvas, float t_s);

} // namespace dfn::app
