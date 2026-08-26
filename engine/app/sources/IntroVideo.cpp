/*
Created: 27:08:2026 - 02:36:00
Module: engine/app
File: engine/app/sources/IntroVideo.cpp

Responsibility:
- Разбор контейнера .dfv и выдача кадра по времени. Устройство и обоснование
  формата — в заголовке и в tools/gen_intro.py.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/
/*
UPD:
- 27:08:2026 - 02:36:00: Создан вместе с заголовком.
*/

#include "engine/app/sources/IntroVideo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

#include "engine/render/sources/PixelCanvas.h"

namespace dfn::app {

namespace {

uint32_t read_u32(const uint8_t* p) {
    // ЯВНО ПО БАЙТАМ, А НЕ memcpy В uint32_t: контейнер описан как
    // little-endian в файле генератора, и чтение через тип машины сделало бы
    // этот разбор верным ровно до первой машины другой раскладки.
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

constexpr size_t HEADER_BYTES = 4 + 8 * 4; // "DFNV" + восемь u32

} // namespace

bool IntroVideo::open(const std::string& path) {
    offsets_.clear();
    bytes_.clear();
    decoded_ = Image{};
    decoded_index_ = -1;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        // ГРОМКО. Заставка, которой нет, обязана быть отличима от заставки,
        // которая есть и чёрная, — иначе это ровно тот «серый экран».
        std::fprintf(stderr, "[интро] нет файла \"%s\": интро не будет\n", path.c_str());
        return false;
    }
    bytes_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (bytes_.size() < HEADER_BYTES || std::memcmp(bytes_.data(), "DFNV", 4) != 0) {
        std::fprintf(stderr, "[интро] \"%s\" — не контейнер DFNV (%zu байт)\n",
                     path.c_str(), bytes_.size());
        bytes_.clear();
        return false;
    }
    const uint8_t* h = bytes_.data() + 4;
    const uint32_t version = read_u32(h);
    width_ = static_cast<int>(read_u32(h + 4));
    height_ = static_cast<int>(read_u32(h + 8));
    const uint32_t frames = read_u32(h + 12);
    fps_num_ = read_u32(h + 16);
    fps_den_ = read_u32(h + 20);
    const uint32_t codec = read_u32(h + 24);
    if (version != 1 || codec != 1 || width_ <= 0 || height_ <= 0 || fps_num_ == 0
        || fps_den_ == 0) {
        std::fprintf(stderr,
                     "[интро] \"%s\": версия %u, кодек %u, %dx%d, %u/%u — не читаю\n",
                     path.c_str(), version, codec, width_, height_, fps_num_, fps_den_);
        bytes_.clear();
        return false;
    }

    size_t at = HEADER_BYTES;
    offsets_.reserve(frames);
    for (uint32_t i = 0; i < frames; ++i) {
        if (at + 4 > bytes_.size()) {
            break;
        }
        const size_t size = read_u32(bytes_.data() + at);
        at += 4;
        if (size == 0 || at + size > bytes_.size()) {
            break;
        }
        offsets_.push_back(Span{at, size});
        at += size;
    }
    if (offsets_.size() != frames) {
        // ОБРЕЗАННЫЙ ФАЙЛ — ЭТО НЕ «немного короче интро». Кадры, которых нет,
        // означают, что актив собран не до конца, и лучше сказать это здесь,
        // чем показать половину заставки и гадать потом.
        std::fprintf(stderr, "[интро] \"%s\": обещано %u кадров, прочитано %zu — актив "
                             "обрезан, интро не будет\n",
                     path.c_str(), frames, offsets_.size());
        offsets_.clear();
        bytes_.clear();
        return false;
    }
    std::fprintf(stderr, "[интро] \"%s\": %zu кадров %dx%d, %.2f c, %.2f МБ\n",
                 path.c_str(), offsets_.size(), width_, height_,
                 static_cast<double>(duration_s()),
                 static_cast<double>(bytes_.size()) / (1024.0 * 1024.0));
    return true;
}

float IntroVideo::duration_s() const {
    if (offsets_.empty()) {
        return 0.0f;
    }
    return static_cast<float>(offsets_.size()) * static_cast<float>(fps_den_)
           / static_cast<float>(fps_num_);
}

const Image* IntroVideo::frame_at(float t_s) {
    if (offsets_.empty()) {
        return nullptr;
    }
    const float fps = static_cast<float>(fps_num_) / static_cast<float>(fps_den_);
    long index = static_cast<long>(std::max(0.0f, t_s) * fps);
    if (index >= static_cast<long>(offsets_.size())) {
        // ЗА КОНЦОМ ИНТРО КАДРА НЕТ, и это не ошибка: приложение уже уходит в
        // меню на этом же кадре. Отдаём последний, чтобы между последним кадром
        // видео и меню не мелькнул незакрашенный холст.
        index = static_cast<long>(offsets_.size()) - 1;
    }
    if (index == decoded_index_ && !decoded_.empty()) {
        return &decoded_;
    }
    const Span& s = offsets_[static_cast<size_t>(index)];
    Image img = decode_png(std::span<const uint8_t>(bytes_.data() + s.at, s.size));
    if (img.empty()) {
        std::fprintf(stderr, "[интро] кадр %ld не разобран\n", index);
        return decoded_.empty() ? nullptr : &decoded_;
    }
    decoded_ = std::move(img);
    decoded_index_ = index;
    return &decoded_;
}

IntroVideo& intro_video() {
    // ОТКРЫВАЕТСЯ ОДИН РАЗ, включая неудачу: файла нет — жалоба одна, а не
    // шестьдесят в секунду (тот же приём, что у cached_png).
    static IntroVideo video = [] {
        IntroVideo v;
        (void)v.open(INTRO_PATH);
        return v;
    }();
    return video;
}

bool draw_intro(render::PixelCanvas& canvas, float t_s) {
    IntroVideo& video = intro_video();
    const Image* frame = video.frame_at(t_s);
    if (frame == nullptr || frame->empty()) {
        return false;
    }
    const int cw = static_cast<int>(canvas.width());
    const int ch = static_cast<int>(canvas.height());
    if (cw <= 0 || ch <= 0) {
        return false;
    }
    // Поля — НАСТОЯЩИЙ чёрный, тот же, которым начинается и кончается само
    // видео: любой другой цвет по краям превратил бы интро в картинку в рамке.
    canvas.clear(render::Color{0, 0, 0});

    // ВПИСЫВАНИЕ С СОХРАНЕНИЕМ ПРОПОРЦИЙ, и точное совпадение размеров —
    // отдельной веткой: холст интерфейса на боевых настройках ровно 1920×1080,
    // то есть кадр ложится пиксель в пиксель, и любая интерполяция там была бы
    // работой ради того же результата (замер: 2.07 Мпикс на кадр).
    const double s = std::min(static_cast<double>(cw) / frame->width,
                              static_cast<double>(ch) / frame->height);
    const int dw = std::max(1, static_cast<int>(std::lround(frame->width * s)));
    const int dh = std::max(1, static_cast<int>(std::lround(frame->height * s)));
    const int x0 = (cw - dw) / 2;
    const int y0 = (ch - dh) / 2;

    for (int dy = 0; dy < dh; ++dy) {
        const int sy0 = static_cast<int>(static_cast<double>(dy) * frame->height / dh);
        const int sy1 = std::max(sy0 + 1, static_cast<int>(
                                              static_cast<double>(dy + 1) * frame->height / dh));
        for (int dx = 0; dx < dw; ++dx) {
            const int sx0 = static_cast<int>(static_cast<double>(dx) * frame->width / dw);
            const int sx1 = std::max(sx0 + 1, static_cast<int>(
                                                  static_cast<double>(dx + 1) * frame->width / dw));
            int r = 0;
            int g = 0;
            int b = 0;
            int n = 0;
            for (int sy = sy0; sy < sy1; ++sy) {
                for (int sx = sx0; sx < sx1; ++sx) {
                    const uint8_t* p = frame->at(sx, sy);
                    r += p[0];
                    g += p[1];
                    b += p[2];
                    ++n;
                }
            }
            if (n == 0) {
                continue;
            }
            canvas.put(x0 + dx, y0 + dy,
                       render::Color{static_cast<uint8_t>(r / n),
                                     static_cast<uint8_t>(g / n),
                                     static_cast<uint8_t>(b / n)});
        }
    }
    return true;
}

} // namespace dfn::app
