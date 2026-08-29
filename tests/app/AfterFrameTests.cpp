/*
Module: tests/app
File: tests/app/AfterFrameTests.cpp

Responsibility:
- Holds the four decisions the tail of a frame makes: has the world settled, is
  a telemetry sample due, is it safe to close, and did a restore land. Each of
  them has been wrong in this project at least once, and each was wrong inside
  a frame loop where nothing could see it.

Dependencies:
- Uses: engine/app/sources/AppAfterFrame.h (header-only), glm, doctest.
- Used by: ctest (app_after_frame).

Notes:
- THE SETTLE GATE IS THE ONE WITH A PRICE TAG. Before it existed, two runs of
  the SAME binary at the SAME commit differed by 17.4% of pixels (34.7%
  re-measured later), because the tour waited a fixed 45 RENDERED FRAMES for
  streaming driven in SIM STEPS off a wall clock. No full-tour pixel claim
  below ~20% had ever certified anything. The gate is the fix, and the fix
  itself had no instrument until now -- it lived in App::run().
- WHY HYSTERESIS GETS ITS OWN CASE. A queue legitimately reads empty for ONE
  frame mid-refocus. A gate without hysteresis fires on that frame, which is
  the same bug as no gate at all, and it is invisible in any single run.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/

#include <doctest/doctest.h>

#include "engine/app/sources/AppAfterFrame.h"

using dfn::app::FlushCountdown;
using dfn::app::SettleGate;

TEST_CASE("the numbers are the ones that were measured, written out by hand") {
    // THE CONTROL FOR EVERY OTHER CASE HERE, and it is not a formality: the
    // first cut of this suite expressed the hysteresis in terms of
    // QUIET_FRAMES_NEEDED, so setting that constant to 1 -- the exact defect
    // the gate exists to prevent -- left the whole suite GREEN. A test written
    // against the thing under test is not a test (Rule 30). These three numbers
    // are therefore spelled out, against what they were measured from:
    //
    //   4  quiet frames  -- a queue reads empty for ONE frame mid-refocus, so
    //                       one is not a settle; forty would be paid at every
    //                       vantage of every tour.
    //   600 frames cap   -- ten seconds at 60 Hz: long enough that a healthy
    //                       slow vantage is never cut off, short enough that an
    //                       unreachable one reports instead of hanging.
    //   8  flush frames  -- the backend's readback, the same number the body
    //                       probe's cooldown uses; fewer has been measured to
    //                       lose the .png.
    CHECK(SettleGate::QUIET_FRAMES_NEEDED == 4);
    CHECK(SettleGate::SETTLE_CAP_FRAMES == 600);
    CHECK(FlushCountdown::FRAMES == 8);

    // ...and the sequence itself, spelled out, so the gate's behaviour is
    // pinned even if someone rewrites the loop below.
    SettleGate g;
    CHECK_FALSE(g.observe(true).settled); // 1
    CHECK_FALSE(g.observe(true).settled); // 2
    CHECK_FALSE(g.observe(true).settled); // 3
    CHECK(g.observe(true).settled);       // 4
}

TEST_CASE("quiet must HOLD before the world counts as settled") {
    SettleGate g;

    // A single quiet frame is not a settled world. This is the whole point of
    // the hysteresis: a queue reads empty for one frame mid-refocus, and a gate
    // that fires there is a gate that certifies a half-streamed vantage.
    for (int i = 1; i < SettleGate::QUIET_FRAMES_NEEDED; ++i) {
        CAPTURE(i);
        const auto v = g.observe(true);
        CHECK_FALSE(v.settled);
        CHECK_FALSE(v.shoot);
    }
    CHECK(g.observe(true).settled);

    // AND ONE BUSY FRAME UNDOES IT COMPLETELY. Not "decrements": the run has to
    // be consecutive, or a world that is quiet every other frame would settle.
    CHECK_FALSE(g.observe(false).settled);
    for (int i = 1; i < SettleGate::QUIET_FRAMES_NEEDED; ++i) {
        CHECK_FALSE(g.observe(true).settled);
    }
    CHECK(g.observe(true).settled);
}

TEST_CASE("a vantage that never settles is shot anyway, loudly, and not sooner") {
    // THE CAP IS A BACKSTOP THAT REPORTS. A tour that quietly never finishes is
    // the same silent-zero failure as a capture that wrote nothing -- but a cap
    // that fires early would certify exactly the unsettled frames the gate
    // exists to reject, so both edges matter.
    SettleGate g;
    for (int i = 1; i < SettleGate::SETTLE_CAP_FRAMES; ++i) {
        CAPTURE(i);
        const auto v = g.observe(false);
        CHECK_FALSE(v.capped);
        CHECK_FALSE(v.shoot);
    }
    const auto v = g.observe(false);
    CHECK(v.capped);
    CHECK(v.shoot);
    CHECK_FALSE(v.settled); // shot, but the frame is NOT evidence

    // THE CAP COUNTS FROM THE LAST SETTLE, NOT FROM THE START OF THE TOUR. It
    // has to measure "this vantage is not converging" rather than "the tour has
    // been running a while" -- the second fires on a long but healthy route.
    SettleGate h;
    for (int i = 0; i < SettleGate::SETTLE_CAP_FRAMES - 1; ++i) {
        (void)h.observe(false);
    }
    for (int i = 0; i < SettleGate::QUIET_FRAMES_NEEDED; ++i) {
        (void)h.observe(true); // the world settles: the countdown is reset
    }
    for (int i = 1; i < SettleGate::SETTLE_CAP_FRAMES; ++i) {
        CAPTURE(i);
        CHECK_FALSE(h.observe(false).capped);
    }
    CHECK(h.observe(false).capped);
}

TEST_CASE("a telemetry sample is due on COUNTED time, never on frames") {
    // One sample per period of the counted clock, so the log of a given walk
    // has the same length on any machine. A per-frame sample would make the
    // file's length a property of the machine that took it.
    const double hz = 4.0;
    CHECK_FALSE(dfn::app::telemetry_due(10.0, 10.0, hz));
    CHECK_FALSE(dfn::app::telemetry_due(10.24, 10.0, hz));
    CHECK(dfn::app::telemetry_due(10.25, 10.0, hz));
    CHECK(dfn::app::telemetry_due(99.0, 10.0, hz));

    // A RATE OF ZERO IS "NEVER", NOT "EVERY FRAME". Written out because the
    // arithmetic answers the other way on its own: 1.0/0.0 is infinity, and a
    // comparison against it happens to be right here -- but a rewrite that
    // multiplies instead of dividing would silently flip it, and the failure
    // would be a telemetry file the size of the session.
    CHECK_FALSE(dfn::app::telemetry_due(1e9, 0.0, 0.0));
}

TEST_CASE("the window does not close until the picture has landed") {
    // save_screenshot() returns true when the capture has been REQUESTED, not
    // when the file exists -- the backend reads the framebuffer back over the
    // following frames. Closing on the same frame produced a .txt with no .png
    // beside it and a "[capture] ok" line above the pair.
    FlushCountdown c;
    CHECK_FALSE(c.armed());
    CHECK_FALSE(c.tick()); // an unarmed countdown never fires

    c.arm();
    CHECK(c.armed());
    for (int i = 1; i < FlushCountdown::FRAMES; ++i) {
        CAPTURE(i);
        CHECK_FALSE(c.tick());
    }
    CHECK(c.tick());        // exactly once, on the last frame of the wait
    CHECK_FALSE(c.armed());
    CHECK_FALSE(c.tick());  // ...and never again

    // ARMING TWICE MUST NOT SHORTEN THE WAIT. Key 5 is both a capture and a
    // chat entry, so two arms land on one frame; if the second restarted a
    // shorter count the window would close under the first one's flush.
    FlushCountdown d;
    d.arm();
    CHECK_FALSE(d.tick());
    d.arm(); // partway through: the wait starts over, it does not shrink
    for (int i = 1; i < FlushCountdown::FRAMES; ++i) {
        CHECK_FALSE(d.tick());
    }
    CHECK(d.tick());
}

TEST_CASE("a restore is judged on the quantity that can actually be wrong") {
    // THE MEASUREMENT THIS ENCODES: a straight 3D distance called the first
    // working restore BLOCKED at 1.138 m, of which 0.07 m was horizontal and
    // the rest was the capsule settling onto the ground -- the controller doing
    // its job. A threshold on the wrong quantity would cry wolf on every
    // restore ever taken, which is precisely how a check gets ignored.
    const glm::vec3 want{100.0f, 20.0f, -50.0f};

    const auto settled = dfn::app::restore_landing({100.07f, 18.932f, -50.0f}, want);
    CHECK(settled.horiz_m == doctest::Approx(0.07f).epsilon(0.01));
    CHECK(settled.settle_m == doctest::Approx(-1.068f).epsilon(0.01));
    CHECK_FALSE(settled.blocked); // a metre of settling is not a failure

    // ...and the real failure IS caught: a body that could not be placed where
    // it was asked ends up somewhere else horizontally.
    const auto blocked = dfn::app::restore_landing({102.5f, 20.0f, -50.0f}, want);
    CHECK(blocked.horiz_m == doctest::Approx(2.5f).epsilon(0.001));
    CHECK(blocked.blocked);

    // A DEAD-ON LANDING reports zero on both, which is the control that keeps
    // the two numbers from being the same number under two names.
    const auto exact = dfn::app::restore_landing(want, want);
    CHECK(exact.horiz_m == doctest::Approx(0.0f));
    CHECK(exact.settle_m == doctest::Approx(0.0f));
    CHECK_FALSE(exact.blocked);
}
