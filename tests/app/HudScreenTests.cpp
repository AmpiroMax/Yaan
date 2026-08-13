/*
Created: 13:08:2026 - 20:55:00
Last updated: 13:08:2026 - 20:55:00
Module: tests/app
File: tests/app/HudScreenTests.cpp

Responsibility:
- Proves the in-game furniture does the two things a frame cannot argue about:
  the ribbon's marks move in the SAME direction the head turns, and each
  element is silent exactly where it is supposed to be silent.

Dependencies:
- Uses: engine/app HudScreen, engine/render PixelCanvas, doctest.
- Used by: ctest.

Notes:
- THE SIGN IS THE WHOLE TEST. A compass that slides the wrong way is drawn
  perfectly, passes every legibility measurement, looks right in a still frame
  and is useless in motion -- and a still acceptance frame CANNOT catch it,
  which is exactly why it is worth a test rather than a screenshot.
- Membership is by COLOUR here, and that is safe for once: the north mark is
  drawn in a value nothing else on the HUD uses, and the canvas starts
  transparent, so there is no world to be confused with. Elsewhere in this
  project colour membership is forbidden (Rule 47) because the background is a
  rendered world; here there is no background at all.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone ui owns this file.
*/
/*
UPD:
- 13:08:2026 - 20:55:00: Created with the ribbon and the bars.
*/

#include <doctest/doctest.h>

#include <cmath>
#include <optional>

#include "engine/app/sources/HudScreen.h"
#include "engine/app/sources/Localization.h"
#include "engine/render/sources/PixelCanvas.h"

using dfn::app::HudFacts;

namespace {

constexpr int W = 640;
constexpr int H = 360;

dfn::render::PixelCanvas fresh() {
    dfn::render::PixelCanvas c;
    c.resize(W, H);
    c.clear_transparent();
    return c;
}

// Mean column of every pixel painted in `colour`, or nullopt when none is.
std::optional<double> centroid_x(const dfn::render::PixelCanvas& c, uint8_t r, uint8_t g,
                                 uint8_t b) {
    const auto px = c.pixels();
    double sum = 0.0;
    int n = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const size_t i = (static_cast<size_t>(y) * W + static_cast<size_t>(x)) * 4;
            if (px[i] == r && px[i + 1] == g && px[i + 2] == b && px[i + 3] == 255) {
                sum += x;
                ++n;
            }
        }
    }
    if (n == 0) {
        return std::nullopt;
    }
    return sum / n;
}

int count_colour(const dfn::render::PixelCanvas& c, uint8_t r, uint8_t g, uint8_t b) {
    const auto px = c.pixels();
    int n = 0;
    for (size_t i = 0; i + 3 < px.size(); i += 4) {
        if (px[i] == r && px[i + 1] == g && px[i + 2] == b && px[i + 3] == 255) {
            ++n;
        }
    }
    return n;
}

// The north mark's colour and the three bar fills, as HudScreen.cpp draws them.
constexpr uint8_t NORTH[3] = {244, 226, 160};
constexpr uint8_t HEALTH[3] = {152, 40, 40};
constexpr uint8_t STAMINA[3] = {120, 196, 96};
constexpr uint8_t MAGICKA[3] = {88, 120, 208};

struct LocalizationOnce {
    LocalizationOnce() {
        // Marks are localization keys (Rule 5). Without the table they draw as
        // placeholders -- still ink, still positioned, so the sign test holds
        // either way, but the real strings are better evidence.
        (void)dfn::app::load_localization("games/daggerfall_n/assets/localization/ru.txt");
    }
};
const LocalizationOnce LOCALIZATION{};

} // namespace

TEST_CASE("the ribbon's marks travel the way the head turns") {
    HudFacts facts;
    facts.yaw_rad = 0.0f;
    auto north_ahead = fresh();
    REQUIRE(dfn::app::draw_compass_ribbon(north_ahead, facts));
    const auto centre = centroid_x(north_ahead, NORTH[0], NORTH[1], NORTH[2]);
    REQUIRE(centre.has_value());
    // Facing north, north is dead ahead: on the middle column, and the
    // tolerance is the mark's own half-width -- a one-glyph mark cannot be
    // centred more precisely than the glyph it is made of.
    CHECK(std::abs(*centre - W / 2.0) <= 4.0);

    // Turn RIGHT (yaw grows clockwise from above): north falls BEHIND to the
    // left, exactly as the world does.
    facts.yaw_rad = 0.3f;
    auto turned_right = fresh();
    REQUIRE(dfn::app::draw_compass_ribbon(turned_right, facts));
    const auto right = centroid_x(turned_right, NORTH[0], NORTH[1], NORTH[2]);
    REQUIRE(right.has_value());
    CHECK(*right < *centre);

    // ...and turning left brings it back the other way. THE CONTROL for the
    // case above: a mark that moved left for BOTH signs would pass the first
    // check and be nonsense.
    facts.yaw_rad = -0.3f;
    auto turned_left = fresh();
    REQUIRE(dfn::app::draw_compass_ribbon(turned_left, facts));
    const auto left = centroid_x(turned_left, NORTH[0], NORTH[1], NORTH[2]);
    REQUIRE(left.has_value());
    CHECK(*left > *centre);
}

TEST_CASE("a direction behind the player is not on the ribbon at all") {
    HudFacts facts;
    facts.yaw_rad = 3.14159265f; // facing south: north is directly behind
    auto canvas = fresh();
    REQUIRE(dfn::app::draw_compass_ribbon(canvas, facts));
    // Nothing of north is drawn -- and the ribbon is still there, which is what
    // separates "the mark is off the strip" from "the strip failed to draw".
    CHECK_FALSE(centroid_x(canvas, NORTH[0], NORTH[1], NORTH[2]).has_value());
}

TEST_CASE("each element is silent exactly where it must be") {
    HudFacts facts;
    auto canvas = fresh();

    facts.map_open = true;
    CHECK_FALSE(dfn::app::draw_compass_ribbon(canvas, facts));
    CHECK_FALSE(dfn::app::draw_condition_bars(canvas, facts));
    CHECK_FALSE(dfn::app::draw_crosshair(canvas, facts));

    facts.map_open = false;
    facts.debug_readout = true;
    CHECK_FALSE(dfn::app::draw_compass_ribbon(canvas, facts)); // the readout owns the top
    // ...but the readout does not speak for the bars or the aim, and they stay.
    CHECK(dfn::app::draw_condition_bars(canvas, facts));
    CHECK(dfn::app::draw_crosshair(canvas, facts));

    facts.debug_readout = false;
    facts.third_person = true;
    CHECK_FALSE(dfn::app::draw_crosshair(canvas, facts)); // the ray left the eye
    CHECK(dfn::app::draw_compass_ribbon(canvas, facts));  // facing is still facing
    CHECK(dfn::app::draw_condition_bars(canvas, facts));
}

TEST_CASE("a bar shows the share it is given, and full is the honest default") {
    HudFacts facts; // health/stamina/magicka default to 1.0
    auto full = fresh();
    REQUIRE(dfn::app::draw_condition_bars(full, facts));
    const int full_health = count_colour(full, HEALTH[0], HEALTH[1], HEALTH[2]);
    CHECK(full_health > 0);
    CHECK(count_colour(full, STAMINA[0], STAMINA[1], STAMINA[2]) == full_health);
    CHECK(count_colour(full, MAGICKA[0], MAGICKA[1], MAGICKA[2]) == full_health);

    facts.health = 0.5f;
    auto half = fresh();
    REQUIRE(dfn::app::draw_condition_bars(half, facts));
    const int half_health = count_colour(half, HEALTH[0], HEALTH[1], HEALTH[2]);
    CHECK(half_health == doctest::Approx(full_health / 2).epsilon(0.05));
    // THE CONTROL: the other two are untouched, so "half" is a property of the
    // value passed and not of the drawing.
    CHECK(count_colour(half, STAMINA[0], STAMINA[1], STAMINA[2]) == full_health);

    facts.health = 0.0f;
    auto empty = fresh();
    REQUIRE(dfn::app::draw_condition_bars(empty, facts));
    CHECK(count_colour(empty, HEALTH[0], HEALTH[1], HEALTH[2]) == 0);

    // Out of range on either side is clamped rather than drawn past the frame.
    facts.health = 4.0f;
    auto over = fresh();
    REQUIRE(dfn::app::draw_condition_bars(over, facts));
    CHECK(count_colour(over, HEALTH[0], HEALTH[1], HEALTH[2]) == full_health);
}

TEST_CASE("the aiming mark leaves its own centre unpainted") {
    HudFacts facts;
    auto canvas = fresh();
    REQUIRE(dfn::app::draw_crosshair(canvas, facts));
    const auto px = canvas.pixels();
    const size_t centre = (static_cast<size_t>(H / 2) * W + static_cast<size_t>(W / 2)) * 4;
    // The hole is the design: the pixel being aimed at must survive the mark
    // that names it.
    CHECK(px[centre + 3] == 0);
}
