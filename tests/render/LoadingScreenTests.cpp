/*
Module: tests
File: tests/render/LoadingScreenTests.cpp

Responsibility:
- Экран загрузки (И15 волна А): что модель этапов считает то, что обещает, и
  что оформление ДЕЙСТВИТЕЛЬНО кладёт краску в холст.

Dependencies:
- Uses: doctest, dfn_render (LoadingScreen, PixelCanvas).
- Used by: ctest (render_loading_screen).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- КРАСКА МЕРИТСЯ РАЗНОСТЬЮ ПРОТИВ КОНТРОЛЬНОЙ РУКИ (правило 47). «На холсте
  есть светлые пиксели» — не утверждение об экране: светлым может быть фон.
  Обе руки рисуют ОДИН И ТОТ ЖЕ экран, отличающийся ровно заголовком и
  списком этапов, и сравнивается ЧИСЛО отличающихся пикселей.
*/

#include "engine/render/sources/LoadingScreen.h"
#include "engine/render/sources/PixelCanvas.h"

#include <doctest/doctest.h>
#include <string>

using dfn::render::LoadingScreen;
using dfn::render::PixelCanvas;

namespace {

/// Сколько пикселей у двух холстов одного размера различаются.
[[nodiscard]] std::size_t differing(const PixelCanvas& a, const PixelCanvas& b) {
    const auto pa = a.pixels();
    const auto pb = b.pixels();
    if (pa.size() != pb.size()) {
        return pa.size() + pb.size();
    }
    std::size_t n = 0;
    for (std::size_t i = 0; i + 3 < pa.size(); i += 4) {
        if (pa[i] != pb[i] || pa[i + 1] != pb[i + 1] || pa[i + 2] != pb[i + 2]) {
            ++n;
        }
    }
    return n;
}

} // namespace

TEST_CASE("loading screen: stages carry their own milliseconds and the bar "
          "counts stages, not guessed time") {
    LoadingScreen s;
    CHECK_FALSE(s.active());
    CHECK(s.progress() == doctest::Approx(0.0f));

    s.begin("Дом у рынка", "assets/scenes/int/whiterun/x100z84.scene");
    CHECK(s.active());
    CHECK(s.stages().empty());

    // Никто не сказал, сколько всего этапов — полоса честно стоит на нуле.
    s.stage("сцена прочитана");
    REQUIRE(s.stages().size() == 1);
    CHECK(s.stages()[0].done); // этап НАЗЫВАЕТ СДЕЛАННОЕ и сразу закрыт
    CHECK(s.progress() == doctest::Approx(0.0f));

    s.set_expected(4);
    CHECK(s.progress() == doctest::Approx(0.25f));
    s.stage("геометрия собрана");
    REQUIRE(s.stages().size() == 2);
    CHECK(s.progress() == doctest::Approx(0.5f));

    s.finish();
    CHECK(s.stages()[1].done);
    CHECK(s.progress() == doctest::Approx(1.0f)); // кончилась — полоса полна
    // Экран ЖИВ после finish: последний кадр обязан показать итог. Гасит hide.
    CHECK(s.active());
    const double total = s.elapsed_ms();
    CHECK(total >= 0.0);
    CHECK(s.elapsed_ms() == doctest::Approx(total)); // часы стоят
    s.hide();
    CHECK_FALSE(s.active());

    const std::string r = s.report();
    CHECK(r.find("Дом у рынка") != std::string::npos);
    CHECK(r.find("сцена прочитана") != std::string::npos);
    CHECK(r.find("геометрия собрана") != std::string::npos);
}

TEST_CASE("loading screen: an explicit fraction overrides the stage count") {
    LoadingScreen s;
    s.begin("x", "");
    s.set_expected(3);
    s.stage("a");
    s.stage("b");
    CHECK(s.progress() == doctest::Approx(2.0f / 3.0f));
    s.set_progress(0.25f);
    CHECK(s.progress() == doctest::Approx(0.25f));
    // Доля зажимается, а не доверяется вызывающему.
    s.set_progress(4.0f);
    CHECK(s.progress() == doctest::Approx(1.0f));
    s.set_progress(-1.0f);
    CHECK(s.progress() == doctest::Approx(2.0f / 3.0f)); // снова по этапам
}

TEST_CASE("loading screen: the drawing puts ink where the words are") {
    // ДВЕ РУКИ ИЗ ОДНОГО ЭКРАНА, отличающиеся ровно предметом проверки.
    PixelCanvas control;
    control.resize(320, 180);
    PixelCanvas work;
    work.resize(320, 180);

    LoadingScreen bare;
    bare.begin("", "");
    bare.draw(control);

    LoadingScreen full;
    full.begin("Дом у рынка", "Вайтран");
    full.set_expected(2);
    full.stage("сцена прочитана");
    full.stage("геометрия собрана");
    full.finish();
    full.draw(work);

    // Заголовок, подзаголовок, две строки этапов с числами и заполненная
    // полоса — это заведомо больше сотни пикселей разницы. Контрольная рука
    // рисует ту же рамку, ту же линейку и ту же пустую полосу, и они
    // вычитаются в ноль.
    const std::size_t diff = differing(control, work);
    CHECK(diff > 100);

    // И ОБРАТНАЯ СТОРОНА (правило 30): два одинаковых экрана дают ноль
    // отличий. Без этого «больше сотни» проходило бы и на приборе, который
    // считает мусор.
    PixelCanvas again;
    again.resize(320, 180);
    LoadingScreen twin;
    twin.begin("", "");
    twin.draw(again);
    CHECK(differing(control, again) == 0);
}
