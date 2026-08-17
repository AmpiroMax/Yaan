/*
Created: 10:08:2026 - 19:24:11
Last updated: 17:08:2026 - 22:01:29
Module: tests/app
File: tests/app/DebugOverlayTests.cpp

Responsibility:
- Proves the state capture is a ROUND TRIP (what is written comes back) and
  that the compass names the direction the player is actually facing.

Dependencies:
- Uses: engine/app DebugOverlay, doctest.
- Used by: ctest.

Notes:
- Every case here ships its control (Rule 30). The round trip's control is a
  DIFFERENT snapshot: a parser that returned a fixed struct would pass the
  round trip and fail the control, and that is the failure mode worth guarding
  -- "it parsed" is not "it parsed THIS".

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 10:08:2026 - 19:24:11: Created with the capture/restore pair.
- 17:08:2026 - 22:01:29: Прибор наложения: вывод, которому назвали занятую верхнюю полосу,
  не печатает в ней НИ ОДНОГО пикселя, а тот же вызов без полосы печатает больше
  сотни. Контроль обязателен: проверка «пусто сверху» одинаково зелена и для
  правильного отступа, и для вывода, переставшего рисовать вовсе.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/PixelCanvas.h"

#include <cmath>
#include <string>

using namespace dfn;

namespace {

app::DebugSnapshot sample() {
    app::DebugSnapshot s{};
    s.stand = 3;
    s.seed = 987654321ULL;
    s.build_commit = "abc1234";
    s.captured_at = "10:08:2026 - 19:24:11";
    s.game_seconds = 12345.678;
    s.day_fraction = 0.4275f;
    s.lunar_phase = 0.8125f;
    s.position = {145.5f, 22.25f, -110.75f};
    s.yaw = 1.0472f;
    s.pitch = -0.3f;
    s.look_dir = {0.5f, -0.25f, -0.75f};
    s.speed_mps = 3.0f;
    s.vertical_velocity = -1.5f;
    s.stride_phase = 0.75f;
    s.gait = 1;
    s.locomotion = 2;
    s.grounded = false;
    s.crouched = true;
    s.water_depth = 0.85f;
    s.internal_w = 640;
    s.internal_h = 360;
    s.fov_y_rad = 1.309f;
    s.head_bob = 0.5f;
    s.palette_post = true;
    s.wind_strength = 0.42f;
    s.cloud_cover = 0.6f;
    s.ambient_darkness = 0.125f;
    s.fps = 59.5f;
    s.frame_ms = 16.8f;
    s.frame_ms_worst = 33.2f;
    s.chunks_resident = 25;
    s.lod_nodes = 12;
    return s;
}

} // namespace

TEST_CASE("state capture survives the round trip") {
    const app::DebugSnapshot in = sample();
    const auto out = app::parse_snapshot(app::format_snapshot(in));
    REQUIRE(out.has_value());

    // The RESTORE-CRITICAL fields, checked to the precision the file carries
    // (six decimals). These are the ones a wrong value makes the reproduction
    // silently different rather than obviously broken.
    CHECK(out->stand == in.stand);
    CHECK(out->seed == in.seed);
    CHECK(out->game_seconds == doctest::Approx(in.game_seconds).epsilon(1e-9));
    CHECK(out->position.x == doctest::Approx(in.position.x));
    CHECK(out->position.y == doctest::Approx(in.position.y));
    CHECK(out->position.z == doctest::Approx(in.position.z));
    CHECK(out->yaw == doctest::Approx(in.yaw));
    CHECK(out->pitch == doctest::Approx(in.pitch));

    // The DESCRIPTIVE fields. They do not steer a restore, but they are what a
    // bug report is read against, so a silently dropped one costs an
    // investigation rather than a crash.
    CHECK(out->build_commit == in.build_commit);
    CHECK(out->captured_at == in.captured_at);
    CHECK(out->gait == in.gait);
    CHECK(out->locomotion == in.locomotion);
    CHECK(out->grounded == in.grounded);
    CHECK(out->crouched == in.crouched);
    CHECK(out->internal_w == in.internal_w);
    CHECK(out->internal_h == in.internal_h);
    CHECK(out->palette_post == in.palette_post);
    CHECK(out->head_bob == doctest::Approx(in.head_bob));
    CHECK(out->speed_mps == doctest::Approx(in.speed_mps));
    CHECK(out->water_depth == doctest::Approx(in.water_depth));
    CHECK(out->chunks_resident == in.chunks_resident);
    CHECK(out->lod_nodes == in.lod_nodes);
}

TEST_CASE("the round trip carries THIS snapshot, not A snapshot") {
    // THE CONTROL for the case above. A parser that ignored the text and
    // returned a default (or a cached) struct would pass every CHECK up there;
    // it cannot pass this one. Two snapshots that differ in every restore-
    // critical field must come back differing in all of them.
    app::DebugSnapshot a = sample();
    app::DebugSnapshot b = sample();
    b.stand = 7;
    b.seed = 42ULL;
    b.game_seconds = 999.5;
    b.position = {-1.0f, 2.0f, -3.0f};
    b.yaw = -2.5f;
    b.pitch = 0.4f;

    const auto ra = app::parse_snapshot(app::format_snapshot(a));
    const auto rb = app::parse_snapshot(app::format_snapshot(b));
    REQUIRE(ra.has_value());
    REQUIRE(rb.has_value());

    CHECK(ra->stand != rb->stand);
    CHECK(ra->seed != rb->seed);
    CHECK(ra->game_seconds != doctest::Approx(rb->game_seconds));
    CHECK(ra->position.x != doctest::Approx(rb->position.x));
    CHECK(ra->position.z != doctest::Approx(rb->position.z));
    CHECK(ra->yaw != doctest::Approx(rb->yaw));
    CHECK(ra->pitch != doctest::Approx(rb->pitch));
}

TEST_CASE("a capture from a newer build restores what it can") {
    // Unknown keys are ignored rather than rejected: a capture is EVIDENCE,
    // and evidence a version bump can invalidate stops being collected. The
    // fields this build understands must survive an unknown one sitting
    // between them.
    std::string text = app::format_snapshot(sample());
    text += "some_future_field = 12.5\nanother = hello world\n";
    const auto out = app::parse_snapshot(text);
    REQUIRE(out.has_value());
    CHECK(out->stand == 3);
    CHECK(out->position.x == doctest::Approx(145.5f));

    // But a file that is not a capture at all must be REFUSED, or "restore"
    // would silently mean "start at the origin".
    CHECK_FALSE(app::parse_snapshot("hello\nworld\n").has_value());
    CHECK_FALSE(app::parse_snapshot("").has_value());
}

TEST_CASE("the compass names the direction the player faces") {
    // Sim's convention: yaw 0 = -Z = north, positive turns clockwise from
    // above, so a quarter turn is east.
    const float pi = 3.14159265358979323846f;
    const auto key = [](const char* k) { return serialization::fnv1a64(k); };

    CHECK(app::compass_key_for_yaw(0.0f) == key("debug.compass.n"));
    CHECK(app::compass_key_for_yaw(pi * 0.5f) == key("debug.compass.e"));
    CHECK(app::compass_key_for_yaw(pi) == key("debug.compass.s"));
    CHECK(app::compass_key_for_yaw(pi * 1.5f) == key("debug.compass.w"));
    CHECK(app::compass_key_for_yaw(pi * 0.25f) == key("debug.compass.ne"));

    // NEGATIVE YAW. The look code produces one the moment the player turns
    // left past north, and std::fmod keeps the sign of the dividend -- so a
    // wrap that only handled positives would index outside the table here.
    // This is not a boundary case, it is the ordinary case for a left turn.
    CHECK(app::compass_key_for_yaw(-pi * 0.5f) == key("debug.compass.w"));
    CHECK(app::compass_key_for_yaw(-0.01f) == key("debug.compass.n"));

    // MULTIPLE TURNS. Yaw accumulates without being wrapped, so a player who
    // has spun a few times carries a large value into the readout.
    CHECK(app::compass_key_for_yaw(2.0f * pi * 5.0f) == key("debug.compass.n"));
    CHECK(app::compass_key_for_yaw(-2.0f * pi * 5.0f + pi) == key("debug.compass.s"));

    // THE CONTROL: names are CENTRED on their direction, not started at it.
    // Without the half-sector offset, "north" would mean 0..45 degrees, so
    // 40 degrees would read north instead of north-east and the readout would
    // disagree with the player's own sense of facing. 40 degrees is inside
    // the north-east sector; 20 is inside north.
    CHECK(app::compass_key_for_yaw(40.0f * pi / 180.0f) == key("debug.compass.ne"));
    CHECK(app::compass_key_for_yaw(20.0f * pi / 180.0f) == key("debug.compass.n"));
}

TEST_CASE("frame clock reports the worst frame, not only the mean") {
    // The complaint this exists to serve -- "всё дергает" -- is a claim about
    // the WORST frames, and a mean hides exactly those. One 100 ms hitch in a
    // second of 60 fps barely moves the average; if the readout could not show
    // it, it could not be used to report the thing it was asked for.
    app::FrameClock clock;
    for (int i = 0; i < 59; ++i) {
        clock.push(1.0f / 60.0f);
    }
    clock.push(0.100f);

    CHECK(clock.worst_ms() == doctest::Approx(100.0f).epsilon(0.01));
    // The mean moved by under 2 ms: the control for the claim above.
    CHECK(clock.mean_ms() < 18.5f);
    CHECK(clock.mean_ms() > 16.0f);

    // A zero or negative delta is a broken clock, not an infinitely fast
    // frame. Admitting one would report an absurd fps and poison the mean.
    app::FrameClock guarded;
    guarded.push(0.0f);
    guarded.push(-1.0f);
    CHECK(guarded.fps() == doctest::Approx(0.0f));
    guarded.push(1.0f / 30.0f);
    CHECK(guarded.fps() == doctest::Approx(30.0f).epsilon(0.001));
}

// ===========================================================================
// THE READOUT'S CORNER IS NOT ALWAYS FREE
// ===========================================================================
//
// User, 17.08.2026: «кнопки сверху пересекаются с дебаг текстом». The editor
// docks a toolbar along the top edge and the readout kept starting at (3, 3),
// under it. The fix is an ORIGIN, not a tuned offset — the number comes from
// EditorUi, which counted the strip — and this is the instrument that says so
// with painted pixels rather than with a promise.

TEST_CASE("the readout starts inside what the interface left it") {
    render::PixelCanvas canvas;
    canvas.resize(640, 360);

    // The toolbar's strip converted onto this canvas: 44.0 logical units of a
    // 1080-tall display is 15 rows of 360. App computes it, never types it.
    constexpr int STRIP = 15;
    const app::DebugSnapshot snap = sample();

    const auto ink_above = [&canvas](int top) {
        const auto px = canvas.pixels();
        int n = 0;
        for (int y = 0; y < top; ++y) {
            for (int x = 0; x < 640; ++x) {
                const size_t i = (static_cast<size_t>(y) * 640 + static_cast<size_t>(x)) * 4;
                if (px[i + 3] != 0) {
                    ++n;
                }
            }
        }
        return n;
    };

    canvas.clear_transparent();
    app::draw_debug_overlay(canvas, snap, /*origin_x=*/0, /*origin_y=*/STRIP);
    CHECK(ink_above(STRIP) == 0);

    // THE CONTROL. The same call, told nothing about the strip: it paints
    // inside it, in quantity. Without this arm the check above would pass
    // against a readout that had stopped drawing entirely — which is precisely
    // how an overlap instrument goes quiet without anyone noticing.
    canvas.clear_transparent();
    app::draw_debug_overlay(canvas, snap);
    CHECK(ink_above(STRIP) > 100);
}

TEST_CASE("whatever stacks under the readout moves with it") {
    // The editor's own block asks the readout where it ended. Told an origin,
    // the answer has to include it — otherwise the two blocks are laid out by
    // two arithmetics again, which is the defect of 14.08 wearing a hat.
    const app::DebugSnapshot snap = sample();
    const int plain = app::debug_overlay_bottom_y(snap);
    const int docked = app::debug_overlay_bottom_y(snap, /*origin_y=*/15);
    CHECK(docked - plain == 15);
}
