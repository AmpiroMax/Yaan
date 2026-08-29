/*
Module: engine/app
File: engine/app/sources/MenuArt.cpp

Responsibility:
- Implementation of MenuArt.h: magnified text, image fitting, the spark field and
  the studio splash frame.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone app (lead) owns this file.
*/

#include "engine/app/sources/MenuArt.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/PngImage.h"
#include "engine/render/sources/BitmapFont.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace dfn::app {

namespace {

int advance_px(int scale, int tracking) {
    return render::FONT_CELL_W * scale + tracking;
}

// A cheap, fixed integer hash. Two motes must not share a lane, and a real RNG
// would be state the caller has to carry for a decorative effect.
uint32_t mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float unit(uint32_t h) {
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

// Composites one source pixel over the canvas. PixelCanvas has no blend of its
// own on purpose (it is a raster surface, not a compositor), so the read is
// done here, once, next to the only two callers that need it.
void blend(render::PixelCanvas& canvas, int x, int y, int r, int g, int b, float a) {
    if (a <= 0.0f || x < 0 || y < 0 || x >= static_cast<int>(canvas.width())
        || y >= static_cast<int>(canvas.height())) {
        return;
    }
    a = std::min(a, 1.0f);
    const auto px = canvas.pixels();
    const size_t at = (static_cast<size_t>(y) * canvas.width() + static_cast<size_t>(x)) * 4u;
    const float inv = 1.0f - a;
    const auto mixc = [&](int src, uint8_t dst) {
        return static_cast<uint8_t>(std::lround(
            std::clamp(static_cast<float>(src) * a + static_cast<float>(dst) * inv,
                       0.0f, 255.0f)));
    };
    canvas.put(x, y,
               render::Color{mixc(r, px[at]), mixc(g, px[at + 1]), mixc(b, px[at + 2])});
}

// --- ПОЛЕ ИСКР: УСТРОЙСТВО --------------------------------------------------
// Заказ владельца 27.08 (вечер) дословно: «сделать больше частиц, белых и голубых
// цветов — цветов spiral; частицы должны появляться и пропадать; они должны
// как кометы следы оставлять и пропадать; частицы должны вверх лететь».
// Отсюда ровно четыре свойства, и ни одно из них не украшение соседнего:
//   * ПЛОТНЕЕ — доза выросла втрое (spark_count в Menu.cpp);
//   * ДВА ЦВЕТА СТУДИИ — белый и голубой, по одному на искру целиком;
//   * ЖИЗНЬ — у каждой свой цикл: рождение, полёт, угасание и тёмная пауза;
//   * ХВОСТ — шлейф рисуется ЗА искрой, то есть НИЖЕ головы: летят вверх.
//
// ПОЧЕМУ ПОЛЕ ПО-ПРЕЖНЕМУ БЕЗ СОСТОЯНИЯ. Ни одной живой частицы в памяти:
// вся искра выводится из своего номера и часов меню. Это не экономия — это
// единственный способ снять кадр приёмки: у поля с состоянием «та же секунда»
// зависит от того, сколько кадров успела нарисовать машина до неё, и два
// прогона расходятся (правило 13). Отсюда же и дверь фазы ниже.
constexpr float SPARK_TAU = 6.283185307179586f;

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;
};

// ЦВЕТА СТУДИИ SPIRAL, названные владельцем: белый и голубой. Взяты из знака
// (assets/branding/spiral_logo), а не подобраны на глаз, — поле обязано читаться
// продолжением марки в углу, а не случайной подсветкой.
constexpr Rgb SPARK_WHITE{0xf0, 0xf2, 0xf4};
constexpr Rgb SPARK_BLUE{0x4a, 0x90, 0xd9};

// СГЛАЖЕННАЯ СТУПЕНЬ. Линейное затухание даёт излом на обоих концах, и глаз
// ловит именно излом: искра «включается», а не появляется.
float smooth01(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// Поток независимых долей из одного зерна: у искры их дюжина, и писать
// mix(mix(mix(...))) руками — верный способ дать двум свойствам одно число.
struct SeedStream {
    uint32_t h;
    explicit SeedStream(uint32_t seed) : h(mix(seed)) {}
    float next() {
        h = mix(h ^ 0x9E3779B9u);
        return unit(h);
    }
};

// ОДНА ИСКРА, вся целиком. Доли кадра, а не пиксели: поле обязано читаться
// одинаково и на 320x180, и на 1920x1080.
struct Spark {
    float x0 = 0.0f;        ///< доля ширины: где искра всплывает
    float y0 = 0.0f;        ///< доля высоты, откуда всплывает (Y растёт вниз)
    float speed = 0.0f;     ///< долей ВЫСОТЫ в секунду, вверх
    float life_s = 1.0f;    ///< сколько секунд живёт
    float cycle_s = 1.0f;   ///< жизнь плюс тёмная пауза до следующего рождения
    float offset = 0.0f;    ///< доля цикла: сдвиг рождения, чтобы не вспыхивали хором
    float sway_amp = 0.0f;  ///< доля ширины: лёгкий дрейф вбок
    float sway_hz = 0.0f;
    float sway_phase = 0.0f;
    float tail_frac = 0.0f; ///< длина шлейфа в долях высоты
    float bright = 1.0f;    ///< яркость головы, 0..1
    Rgb color{};
};

Spark spark_of(int i) {
    SeedStream r(static_cast<uint32_t>(i) * 2654435761u + 17u);
    Spark s;
    s.x0 = r.next();
    // РОЖДАЮТСЯ НЕ У НИЖНЕГО КРАЯ, А ПО ВСЕМУ ПОЛЮ. Фонтан от нижней кромки
    // читается сценой («что-то горит за кадром»); заказ был про поле, в
    // котором частицы ПОЯВЛЯЮТСЯ и ПРОПАДАЮТ, — значит рождение обязано
    // случаться и в середине кадра.
    s.y0 = 0.30f + 0.80f * r.next();
    // МЕДЛЕННОЕ ВСПЛЫТИЕ: 3.0-7.5 % высоты в секунду, то есть 32-81 px/с на
    // 1080. Прежние пылинки ползли 11-27 px/с и с хвостом читались бы стоячими,
    // а всё, что быстрее сотни, превращается в дождь наоборот.
    s.speed = 0.030f + 0.045f * r.next();
    s.life_s = 9.0f + 7.0f * r.next();
    s.cycle_s = s.life_s * (1.06f + 0.30f * r.next());
    s.offset = r.next();
    s.sway_amp = 0.004f + 0.010f * r.next();
    s.sway_hz = 0.05f + 0.09f * r.next();
    s.sway_phase = SPARK_TAU * r.next();
    s.tail_frac = 0.030f + 0.040f * r.next();
    s.bright = 0.45f + 0.50f * r.next();
    // ЦВЕТ ОДИН НА ВСЮ ИСКРУ, включая хвост: комета не меняет цвет по длине,
    // а смесь двух цветов в одном шлейфе читается грязью, а не переливом.
    s.color = r.next() < 0.55f ? SPARK_WHITE : SPARK_BLUE;
    return s;
}

// Огибающая жизни: вспышка, полёт, угасание. Ноль вне отрезка жизни — на этом
// же нуле обрывается и хвост, каждое звено которого спрашивает СВОЙ возраст.
float spark_alpha(const Spark& s, float age_s) {
    if (age_s < 0.0f || age_s > s.life_s) {
        return 0.0f;
    }
    return smooth01(age_s / (s.life_s * 0.18f))
           * smooth01((s.life_s - age_s) / (s.life_s * 0.34f));
}

// ПРИКОЛОТАЯ ФАЗА ПОЛЯ (DFN_MENU_SPARK_PHASE), родная сестра DFN_MENU_OAK_PHASE
// у герба и по той же причине: беспилотный прогон снимает кадр после
// произвольного числа кадров, то есть в произвольной секунде часов меню, — а
// кадр приёмки обязан быть повторяемым. Читается ОДИН раз за прогон.
struct PinnedSparkPhase {
    bool on = false;
    float seconds = 0.0f;
};

const PinnedSparkPhase& pinned_spark_phase() {
    static const PinnedSparkPhase pinned = [] {
        PinnedSparkPhase p;
        const char* v = door_value("DFN_MENU_SPARK_PHASE");
        if (v == nullptr || *v == '\0') {
            return p;
        }
        char* end = nullptr;
        const float parsed = std::strtof(v, &end);
        if (end == v) {
            return p; // не число — дверь молча закрыта, поле живое
        }
        p.on = true;
        p.seconds = parsed;
        return p;
    }();
    return pinned;
}

} // namespace

int text_width_scaled(std::string_view utf8, int scale, int tracking) {
    const int n = render::text_glyph_count(utf8);
    if (n <= 0) {
        return 0;
    }
    // The trailing tracking is subtracted: the ink ends at the last glyph, and a
    // right-aligned column measured with a trailing gap sits a gap short of its
    // own edge -- which reads as the list being crooked, not as it being padded.
    return n * advance_px(scale, tracking) - tracking;
}

int text_height_scaled(int scale) { return render::FONT_INK_H * scale; }

int draw_text_scaled(render::PixelCanvas& canvas, int x, int y, std::string_view utf8,
                     render::Color color, int scale, int tracking, bool shadow) {
    scale = std::max(1, scale);
    if (shadow) {
        draw_text_scaled(canvas, x + scale, y + scale, utf8, render::Color{0, 0, 0},
                         scale, tracking, /*shadow=*/false);
    }
    const render::FontAtlas& atlas = render::font_atlas();
    int pen = x;
    size_t pos = 0;
    while (pos < utf8.size()) {
        const uint32_t cp = render::utf8_next(utf8, pos);
        const int slot = render::font_slot_for_codepoint(cp);
        for (int gy = 0; gy < render::FONT_INK_H; ++gy) {
            for (int gx = 0; gx < render::FONT_INK_W; ++gx) {
                if (atlas.ink(slot, gx, gy)) {
                    canvas.fill_rect(pen + gx * scale, y + gy * scale, scale, scale, color);
                }
            }
        }
        pen += advance_px(scale, tracking);
    }
    return pen - x - tracking;
}

void draw_image_fit(render::PixelCanvas& canvas, const Image& image, int box_x,
                    int box_y, int box_w, int box_h, float alpha) {
    if (image.empty() || box_w <= 0 || box_h <= 0 || alpha <= 0.0f) {
        return;
    }
    // ASPECT PRESERVED, and the box is a bound rather than a target: an emblem
    // stretched to a box is a different emblem.
    const double sx = static_cast<double>(box_w) / image.width;
    const double sy = static_cast<double>(box_h) / image.height;
    const double s = std::min(sx, sy);
    const int dw = std::max(1, static_cast<int>(std::lround(image.width * s)));
    const int dh = std::max(1, static_cast<int>(std::lround(image.height * s)));
    const int dx0 = box_x + (box_w - dw) / 2;
    const int dy0 = box_y + (box_h - dh) / 2;

    for (int dy = 0; dy < dh; ++dy) {
        // Source rows this destination row covers. On an upscale the rectangle
        // collapses to one pixel and the loop below is a nearest-neighbour
        // sample, which is the right answer for a magnified logo too.
        const int sy0 = static_cast<int>(static_cast<double>(dy) * image.height / dh);
        const int sy1 = std::max(sy0 + 1,
                                 static_cast<int>(static_cast<double>(dy + 1) * image.height / dh));
        for (int dx = 0; dx < dw; ++dx) {
            const int sx0 = static_cast<int>(static_cast<double>(dx) * image.width / dw);
            const int sx1 = std::max(sx0 + 1,
                                     static_cast<int>(static_cast<double>(dx + 1) * image.width / dw));
            // PREMULTIPLIED AVERAGE. Averaging colour without weighting by
            // alpha pulls the transparent pixels' (usually black) colour into
            // the edge, which is the classic dark halo around a logo.
            double ar = 0.0;
            double ag = 0.0;
            double ab = 0.0;
            double aa = 0.0;
            int n = 0;
            for (int sy_ = sy0; sy_ < sy1; ++sy_) {
                for (int sx_ = sx0; sx_ < sx1; ++sx_) {
                    const uint8_t* p = image.at(sx_, sy_);
                    const double a = p[3] / 255.0;
                    ar += p[0] * a;
                    ag += p[1] * a;
                    ab += p[2] * a;
                    aa += a;
                    ++n;
                }
            }
            if (n == 0 || aa <= 0.0) {
                continue;
            }
            blend(canvas, dx0 + dx, dy0 + dy, static_cast<int>(std::lround(ar / aa)),
                  static_cast<int>(std::lround(ag / aa)),
                  static_cast<int>(std::lround(ab / aa)),
                  static_cast<float>(aa / n) * alpha);
        }
    }
}

void draw_sparks(render::PixelCanvas& canvas, float time_s, int count) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    if (w <= 0 || h <= 0 || count <= 0) {
        return;
    }
    const PinnedSparkPhase& pin = pinned_spark_phase();
    const float t = pin.on ? pin.seconds : time_s;

    // Голова растёт с кадром: 3-4 px на 1080, 1 px на ретро-ступенях. Мельче —
    // хвост не отличить от головы, крупнее — комета становится кляксой.
    const int head_px = std::max(1, h / 300);
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);

    for (int i = 0; i < count; ++i) {
        const Spark s = spark_of(i);
        // Возраст ВНУТРИ своего цикла. fmod, а не вычитание целой части: цикл
        // у каждой искры свой, и общего кадра, по которому можно было бы
        // «завернуть» долю, здесь нет.
        const float age = std::fmod(t + s.offset * s.cycle_s, s.cycle_s);
        // За сколько секунд искра проходит длину собственного хвоста.
        const float tail_s = s.tail_frac / s.speed;
        // ХВОСТ ПЕРЕЖИВАЕТ ГОЛОВУ, и это не небрежность: у погасшей кометы
        // шлейф ещё виден, пока его дальние звенья считают СВОЙ, более
        // ранний возраст. Отсекаем только то, что не даёт ни одного звена.
        if (age > s.life_s + tail_s) {
            continue;
        }
        // ЗВЕНО НА ПИКСЕЛЬ ДЛИНЫ, и это не запас: шаг в полголовы был первым
        // подходом, и хвост на кадре вышел ПУНКТИРОМ — звенья хвоста тонкие
        // (радиус падает по квадрату), и между ними оставалась дыра. На 1080
        // выходит 32-76 звеньев; потолок держит цену кадра.
        const int steps =
            std::clamp(static_cast<int>(std::lround(s.tail_frac * fh)), 8, 96);
        // ОТ ХВОСТА К ГОЛОВЕ: голова кладётся последней и потому не тонет под
        // звеном соседней искры.
        for (int j = steps; j >= 0; --j) {
            const float f = static_cast<float>(j) / static_cast<float>(steps);
            const float sample_age = age - f * tail_s;
            const float life = spark_alpha(s, sample_age);
            if (life <= 0.0f) {
                continue; // ещё не родилась (у молодой хвоста нет) или уже угасла
            }
            // ЯРКОСТЬ ГАСНЕТ МЕДЛЕННЕЕ, ЧЕМ ТОЛЩИНА. Это и есть разница между
            // кометой и штрихом дождя: голова круглая и яркая, а шлейф уходит
            // в волосок, оставаясь при этом видимым почти на всю длину.
            // Показатели подобраны по кадрам (1.3 у яркости, 2.0 у радиуса);
            // равные показатели дают либо «головастика», либо ровную черту.
            const float taper = 1.0f - f;
            const float a = s.bright * life * std::pow(taper, 1.3f);
            if (a <= 0.004f) {
                continue;
            }
            const float fx = s.x0
                             + s.sway_amp * std::sin(SPARK_TAU * s.sway_hz * sample_age
                                                     + s.sway_phase);
            const float fy = s.y0 - s.speed * sample_age;
            const int x = static_cast<int>(std::lround(fx * fw));
            const int y = static_cast<int>(std::lround(fy * fh));
            const int r = std::max(
                1, static_cast<int>(std::lround(static_cast<float>(head_px)
                                                * std::pow(taper, 2.0f))));
            const auto stamp = [&](int side, const Rgb& c, float alpha) {
                for (int oy = 0; oy < side; ++oy) {
                    for (int ox = 0; ox < side; ++ox) {
                        blend(canvas, x + ox - side / 2, y + oy - side / 2, c.r, c.g, c.b,
                              alpha);
                    }
                }
            };
            if (j == 0) {
                // ГОЛОВА В ТРИ СЛОЯ: слабое гало (иначе комета вырезана из
                // бумаги), тело в цвете искры и БЕЛОЕ ЯДРО — даже у голубой.
                // Комета горячее своего шлейфа, и без ядра голубая искра
                // читается отрезком краски, а не источником света.
                stamp(r + 3, s.color, a * 0.16f);
                stamp(r, s.color, a);
                stamp(std::max(1, r / 2), SPARK_WHITE, std::min(1.0f, a * 0.9f));
            } else {
                stamp(r, s.color, a);
            }
        }
    }
}

void draw_studio_splash(render::PixelCanvas& canvas, float t_s, float total_s) {
    const int w = static_cast<int>(canvas.width());
    const int h = static_cast<int>(canvas.height());
    // The brand's own dark, not the menu's: the lock-up was drawn on #0c0e12
    // and its PNG carries that ground, so any other clear colour would show as
    // a rectangle around it.
    canvas.clear(render::Color{0x0c, 0x0e, 0x12});

    // A quarter in, a quarter out, held in between. The fade is what makes a
    // two-second frame read as a title card rather than as a stall.
    const float fade = std::max(0.15f, total_s * 0.25f);
    float alpha = 1.0f;
    if (t_s < fade) {
        alpha = t_s / fade;
    } else if (t_s > total_s - fade) {
        alpha = std::max(0.0f, (total_s - t_s) / fade);
    }
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    const int box = std::min(w, h) * 3 / 5;
    draw_image_fit(canvas, cached_png(BRAND_SPIRAL_FULL_PNG), (w - box) / 2,
                   (h - box) / 2, box, box, alpha);
}

} // namespace dfn::app
