/*
Module: engine/app
File: engine/app/sources/AppStand.cpp

Responsibility:
- The stand's four camera poses and its clip queue, as data.

Dependencies:
- Uses: AppStand.h. Nothing else.
- Used by: dfn_app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- CONTENT, not constants (Rule 5/14): these timings and azimuths are a shot
  list. They belong beside the stand they photograph, not in NUMBERS.md, and
  the moment a second consumer needs one of them it stops being a shot list.
*/

#include "engine/app/sources/AppStand.h"

namespace dfn::app {
namespace {

/// THE AZIMUTH CONVENTION: `orbit_yaw` 0 is the third-person boom's home,
/// which is BEHIND the figure; 180 puts the camera in front of it, looking at
/// its face.
///
/// THIS TABLE HAD IT THE OTHER WAY ROUND UNTIL 31.08, AND THE FRAME AGREED
/// WITH IT, which is the part worth reading twice. The previous wave did look
/// at the frame — its own comment says so — and saw a face at azimuth 0, so it
/// wrote down "0 is the front". What it was actually looking at was a model
/// turned 180 degrees by a bug in the importer (the yaw baked into the bind
/// pose was not carried into the clips' root ROTATION channels, so the moment
/// a clip played the figure spun to face the way its author left it). Two
/// wrongs made a right-looking screenshot: the camera stood behind a man who
/// was walking backwards, and the picture showed a face.
///
/// So fixing the model alone would have turned every acceptance frame of this
/// wave into a photograph of a back. Both halves move together, and the pair
/// is why the acceptance for item 1 is not "the model faces -Z" — that is a
/// unit test — but "the FRONT camera sees a FACE while the figure walks
/// TOWARD it", which is a claim about the two of them at once.
constexpr StandCamera CAMERAS[STAND_CAMERA_COUNT] = {
    // 1. FRONT. Reads proportions and where the arms hang; the pose a
    //    character sheet is drawn from.
    {180.0f, -6.0f, 3.4f, 0.75f, "front"},
    // 2. PROFILE. THE GAIT CAMERA: stride length, knee flexion and the trunk's
    //    lean are all fore-and-aft quantities and are invisible head-on.
    {270.0f, -4.0f, 3.6f, 0.80f, "profile"},
    // 3. THREE-QUARTER. The compromise the eye is used to: silhouette plus
    //    depth. It is what the wave's before/after pair is shot on, because a
    //    procedural gait and a bought clip differ in BOTH.
    {225.0f, -8.0f, 3.4f, 0.80f, "three-quarter"},
    // 4. CLOSE. Head and shoulders: the camera that shows the skinning seam at
    //    the neck and what the clothing palette actually looks like.
    {205.0f, 6.0f, 2.4f, 0.90f, "close"},
    // 5. РУКА С ОРУЖИЕМ (заказ владельца 31.08, пункт 5: «меч торчит из кисти,
    //    не лежит в руке»). ПЯТАЯ КАМЕРА, А НЕ ПРАВКА ЧЕТВЁРТОЙ: четвёртая —
    //    голова и плечи, и все прежние кадры волн стойки и оружия сняты ею;
    //    подвинуть её значило бы, что «до» и «после» следующего сравнения
    //    сняты с разных точек. Азимут 250 ставит камеру со стороны ПРАВОЙ
    //    руки (0 — сзади, +90 — левая сторона фигуры), тангаж -33 наводит на
    //    кисть: она висит примерно в 0.9 м, глаз в 1.7, стрела 1.3 — то есть
    //    луч обязан опуститься на 0.8 м за 1.3, а это и есть 33 градуса.
    {250.0f, -33.0f, 1.3f, 0.05f, "hand"},
};

/// One phase of the queue: when it starts, what the hands are doing.
struct Phase {
    float from_s;
    StandStep step;
};

/// THE QUEUE THE OWNER ORDERED (31.08): Idle, Walk, Jog, Sprint, Jump,
/// Crouch, Sit. Every travelling gear runs OUT AND BACK in equal halves — the
/// turn is a yaw of pi, written directly (see StandStep::face_yaw) — so the
/// figure ends the queue where it began, beside the bench it is then told to
/// sit on. Three seconds out is roughly three walk cycles, two jog cycles and
/// four sprint cycles: the least that shows a cycle REPEATING rather than a
/// pose held.
///
/// THE TURNS ARE SNAPS, and the acceptance frames are taken inside the phases
/// rather than at their boundaries. A turn eased over half a second would be
/// prettier and would make the queue's timing depend on the easing, which is
/// the one thing a fixed shot list may not have.
constexpr float PI = 3.14159265358979f;

constexpr Phase PHASES[] = {
    {0.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.0f, "idle"}},
    {4.0f, {{0.0f, 1.0f}, false, false, false, false, false, 0.0f, "walk-out"}},
    {7.0f, {{0.0f, 1.0f}, false, false, false, false, false, PI, "walk-back"}},
    {10.0f, {{0.0f, 1.0f}, true, false, false, false, false, 0.0f, "jog-out"}},
    {13.0f, {{0.0f, 1.0f}, true, false, false, false, false, PI, "jog-back"}},
    {16.0f, {{0.0f, 1.0f}, false, true, false, false, false, 0.0f, "sprint-out"}},
    {18.0f, {{0.0f, 1.0f}, false, true, false, false, false, PI, "sprint-back"}},
    {20.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.0f, "settle"}},
    // The jump is an EDGE and the phase after it is the arc: the queue presses
    // once and then holds still, so the frames show take-off, flight and
    // landing rather than a man bouncing.
    {20.6f, {{0.0f, 0.5f}, false, false, true, false, false, 0.0f, "jump"}},
    {21.0f, {{0.0f, 0.5f}, false, false, false, false, false, 0.0f, "jump-arc"}},
    {23.0f, {{0.0f, 0.0f}, false, false, false, true, false, 0.0f, "crouch"}},
    {25.0f, {{0.0f, 0.7f}, false, false, false, true, false, 0.0f, "crouch-out"}},
    {27.0f, {{0.0f, 0.7f}, false, false, false, true, false, PI, "crouch-back"}},
    // FACING THE BENCH is what makes the sit possible at all: the seat is
    // taken by AIM, not by proximity (AppSeats::seat_aim), so a figure with
    // its back to the bench presses E at the grass.
    {29.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.5f * PI,
             "face-bench"}},
    {29.6f, {{0.0f, 0.0f}, false, false, false, false, true, 0.5f * PI,
             "sit-down"}},
    {30.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.5f * PI,
             "sitting"}},
    // --- ОРУЖИЕ В РУКАХ (заказ владельца 31.08, пункты 5-6) --------------
    // ВСТАЁМ ЗАЯВКОЙ, А НЕ ВТОРЫМ НАЖАТИЕМ E, и это починка ПРИБОРА. На стенде
    // посадка на лавку срабатывает НЕ КАЖДЫЙ ПРОГОН (прицел сиденья зависит
    // от тика — названный хвост волны «клипы»), а E — переключатель: тот же
    // ход, что поднимает севшего, УСАЖИВАЕТ стоящего. Замерено: два прогона
    // одной и той же очереди дали на 35-й секунде две разные позы, то есть
    // приёмочные кадры зависели от жребия. `stand` идемпотентен (StandStep).
    {33.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.0f,
             "idle-sheathed", false, true}},
    // ПАРА КАДРОВ ОДНОЙ ПОЗЫ. Ножны и клинок сняты на idle подряд, из одной
    // сборки и одной камеры: разница между двумя кадрами обязана быть ровно
    // состоянием рук (правило 47).
    {36.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.0f, "idle-drawn", true,
             true}},
    {39.0f, {{0.0f, 1.0f}, false, false, false, false, false, 0.0f, "walk-drawn", true,
             true}},
    {42.0f, {{0.0f, 1.0f}, false, false, false, false, false, PI,
             "walk-drawn-back", true, true}},
    {45.0f, {{0.0f, 1.0f}, false, true, false, false, false, 0.0f, "run-drawn",
             true, true}},
    {47.0f, {{0.0f, 1.0f}, false, true, false, false, false, PI,
             "run-drawn-back", true, true}},
    // --- РЕЛЬЕФ (пункт 7 на склоне) -------------------------------------
    // ДЛИННЫЙ ПРОХОД ВПЕРЁД, НЕ ТУДА-СЮДА: скат стенда лежит в 14 метрах от
    // спавна, и попасть на него можно только пройдя их. Клинок убран — на
    // склоне мерится нога, и вторая переменная в кадре мешала бы.
    {49.0f, {{0.0f, 0.0f}, false, false, false, false, false, 0.0f,
             "sheathe", false, true}},
    // ЛЕВЕЕ И ВПЕРЁД: марш стенда стоит в (126, 118), спавн — в (132.8, 135.9),
    // и попасть на ступени можно только идя наискось. Доля сноса 0.40 — это
    // отношение 6.8 м вбок к 16.9 м вперёд, то есть ГЕОМЕТРИЯ стенда, а не
    // подобранное число: она ведёт к подножию марша, пересекая по дороге скат.
    {50.0f, {{-0.40f, 1.0f}, false, false, false, false, false, 0.0f,
             "walk-to-slope-and-stairs", false, true}},
};

} // namespace

StandCamera stand_camera(uint32_t n) {
    if (n < 1 || n > STAND_CAMERA_COUNT) {
        return CAMERAS[1]; // profile: the pose a gait is read from
    }
    return CAMERAS[n - 1];
}

StandStep stand_sequence_at(float prev_t, float t) {
    StandStep out;
    for (const Phase& p : PHASES) {
        if (t >= p.from_s) {
            out = p.step;
        }
    }
    // EDGES ARE EDGES. `jump` and `interact` are latches the movement code
    // consumes once; held down, the first would re-jump on every landing and
    // the second would sit and stand on alternate ticks. True only on the tick
    // that CROSSED the phase boundary.
    if (out.jump || out.interact) {
        bool crossed = false;
        for (const Phase& p : PHASES) {
            if ((p.step.jump || p.step.interact) && prev_t < p.from_s
                && t >= p.from_s) {
                crossed = true;
            }
        }
        if (!crossed) {
            out.jump = false;
            out.interact = false;
        }
    }
    return out;
}


// --- ЛЕНТА ПОЗ ------------------------------------------------------------
//
// ОДИН ПРОХОД ПО КОЛЬЦУ И ОСТАНОВКА. Лента не зациклена намеренно: приёмочный
// прогон обязан кончаться, иначе «сколько кадров снято» становится вопросом о
// том, когда нажали выход, а не о том, сколько в реестре поз.

uint32_t pose_tape_slot_at(float t, uint32_t slot_count) {
    if (slot_count == 0) {
        return 0;
    }
    if (t < 0.0f) {
        return 0;
    }
    const auto k = static_cast<uint32_t>(t / POSE_TAPE_DWELL_S);
    return k >= slot_count ? slot_count - 1 : k;
}

float pose_tape_length_s(uint32_t slot_count) {
    return POSE_TAPE_DWELL_S * static_cast<float>(slot_count);
}

} // namespace dfn::app
