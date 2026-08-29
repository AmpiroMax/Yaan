/*
Module: engine/app
File: engine/app/sources/AppAfterFrame.h

Responsibility:
- THE GATES OF THE WORK THAT MUST HAPPEN AFTER render(), as pure functions and
  small state machines that can be run without a window. The work itself is
  App::after_frame() in the .cpp beside this; what is here is every decision it
  makes, so that each one has an instrument.

Key items:
- SettleGate: has the world stopped changing (hysteresis + a reporting cap).
- telemetry_due(): one sample per period of COUNTED time.
- FlushCountdown: how many frames to keep drawing before closing.
- restore_landing(): how far a restore actually got, in the right quantity.

Dependencies:
- Uses: glm only. No App, no window, no renderer -- deliberately.
- Used by: AppAfterFrame.cpp, tests/app/AfterFrameTests.cpp.

Notes:
- WHY THESE FOUR AND NOT THE WHOLE BLOCK. What the after-render block does is
  side effects on files, on the renderer and on the window; none of that is
  testable here and pretending otherwise would be theatre. What IS testable is
  every place it CHOOSES: whether the world has settled, whether a sample is
  due, whether it is safe to close, and whether a restore landed. Each of those
  four has been wrong at least once in this project's history, and each of them
  was wrong inside a frame loop where nothing could see it.
- HEADER-ONLY ON PURPOSE. A test that needs a link line needs a CMake target,
  and a target that pulls App.cpp pulls a window. These are small enough to be
  inline, so the suite includes this file and nothing else.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/

#pragma once

#include <glm/vec3.hpp>

#include <cmath>

namespace dfn::app {

// ЗАТВОР ТУРА: УСПОКОИЛСЯ ЛИ МИР. Tour.cpp waits a fixed 45 RENDERED FRAMES for
// streaming that is driven in SIM STEPS off a wall clock -- Rule 42, a budget
// denominated in one clock's units enforcing a limit that only matters in
// another's. The cost was measured, not guessed: two runs of the SAME binary at
// the SAME commit differed by 17.4% of pixels (34.7% re-measured later), so no
// full-tour pixel claim below ~20% had ever certified anything.
//
// HYSTERESIS IS NOT OPTIONAL: a queue legitimately reads empty for one frame
// mid-refocus, so quiescence must HOLD before it counts. And the cap is a
// backstop that REPORTS -- an unreachable vantage must say so rather than hang,
// because a tour that quietly never finishes is the same silent-zero failure as
// a capture that wrote nothing.
struct SettleGate {
    // How many consecutive quiet frames make a settled world. Four rather than
    // one for the reason above; rather than forty because the cost of waiting
    // is paid at every vantage of every tour.
    static constexpr int QUIET_FRAMES_NEEDED = 4;
    // Frames since the world was last SETTLED before the gate gives up and
    // shoots anyway, loudly. It counts from the last settle, not from the start
    // of the tour, so it measures "this vantage is not converging" rather than
    // "the tour has been running a while" -- the second would fire on a long
    // but healthy route.
    static constexpr int SETTLE_CAP_FRAMES = 600;

    int quiet_frames = 0;
    int unsettled_frames = 0;

    struct Verdict {
        bool settled = false;
        bool capped = false;  // gave up waiting; the frame is NOT evidence
        bool shoot = false;   // settled || capped
    };

    Verdict observe(bool quiet) {
        quiet_frames = quiet ? quiet_frames + 1 : 0;
        Verdict v;
        v.settled = quiet_frames >= QUIET_FRAMES_NEEDED;
        unsettled_frames = v.settled ? 0 : unsettled_frames + 1;
        v.capped = unsettled_frames >= SETTLE_CAP_FRAMES;
        v.shoot = v.settled || v.capped;
        return v;
    }
};

// ПОРОГ СХОДИМОСТИ КАПСУЛЫ, метры за кадр. Восстановление ставит игрока в
// точку, а контроллер осаживает его на пол — дверная волна намерила 1.3 мм
// расхождения между прогонами именно на этой осадке. Затвор не выключает
// физику и не телепортирует: он ЖДЁТ, пока шаг осадки не станет меньше этого.
// Порог, а не равенство: контроллер шевелит последний бит и на сошедшейся
// позе, и требование точного нуля не выполнилось бы никогда — то есть затвор
// упирался бы в потолок и снимал бы «всё равно» каждый раз.
inline constexpr float PLAYER_SETTLE_EPS_M = 0.0001f;

// ОДНА ПРОБА ТЕЛЕМЕТРИИ НА ПЕРИОД СЧЁТНОГО ВРЕМЕНИ, не на кадр. Counted, so the
// log of a given walk has the same length on any machine; a per-frame sample
// would make the file's length a property of the machine that took it.
[[nodiscard]] inline bool telemetry_due(double game_seconds, double last_sample_s,
                                        double hz) {
    if (hz <= 0.0) {
        return false; // a rate of zero is "never", not "every frame"
    }
    return game_seconds - last_sample_s >= 1.0 / hz;
}

// СКОЛЬКО КАДРОВ ЕЩЁ РИСОВАТЬ, ПРЕЖДЕ ЧЕМ ЗАКРЫТЬСЯ. save_screenshot() returns
// true when the capture has been REQUESTED, not when the file exists -- the
// bgfx backend reads the framebuffer back over the following frames. Closing on
// the same frame produced a .txt with no .png beside it and, worse, a
// "[capture] ok" line above the pair.
struct FlushCountdown {
    // Eight frames, the same number the body probe's cooldown uses. It is a
    // backend property, not a taste: fewer has been measured to lose the file.
    static constexpr int FRAMES = 8;

    int left = 0;

    void arm() { left = FRAMES; }
    // ARMING TWICE MUST NOT SHORTEN THE WAIT. A capture and a chat entry can
    // both land on one frame (key 5 is both), and the second arm restarting the
    // count is what keeps the pair from closing the window under the first
    // one's flush.
    [[nodiscard]] bool armed() const { return left > 0; }
    // True exactly once, on the frame the wait is over.
    [[nodiscard]] bool tick() { return left > 0 && --left == 0; }
};

// КУДА НА САМОМ ДЕЛЕ ПРИЗЕМЛИЛОСЬ ВОССТАНОВЛЕНИЕ. IPhysics has no teleport, so
// a restore is a placement the controller then settles, and this is the check
// that keeps a half-completed restore from passing as a completed one.
//
// HORIZONTAL AND VERTICAL ERROR ARE DIFFERENT QUANTITIES and only one of them
// is a failure. A straight 3D distance called the first working restore BLOCKED
// at 1.138 m -- of which 0.07 m was horizontal and the rest was the capsule
// settling onto the ground, which is the controller doing its job. Which
// quantity the threshold sits on is itself a measurement (Rule 30), and it was
// on the wrong one: it would have cried wolf on every restore ever taken, which
// is precisely how a check gets ignored.
struct RestoreLanding {
    float horiz_m = 0.0f;
    float settle_m = 0.0f; // signed: negative means it settled downward
    bool blocked = false;
};

[[nodiscard]] inline RestoreLanding restore_landing(glm::vec3 got, glm::vec3 want) {
    RestoreLanding r;
    const float dx = got.x - want.x;
    const float dz = got.z - want.z;
    r.horiz_m = std::sqrt(dx * dx + dz * dz);
    r.settle_m = got.y - want.y;
    r.blocked = r.horiz_m > 1.0f;
    return r;
}

} // namespace dfn::app
