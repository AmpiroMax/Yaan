/*
Created: 13:08:2026 - 18:50:00
Last updated: 22:08:2026 - 13:45:06
Module: tools
File: tools/measure_shadow_jitter.cpp

Responsibility:
- Measure the SUN SHADOW's between-frames wobble: how far the shadow map's
  texel grid slides under a FIXED world point from one frame to the next while
  the sun moves and the player stands still. That slide is the user's
  "дергаются, колеблются" — the caster and the receiver turn together, so what
  moves an edge is where the grid's boundaries fall.

Key items:
- light_view(): update_shadow's light matrix, mirrored — same glm, same float
  precision, same floor(), same azimuth/elevation snap.
- argv[1]: the angular quantum in radians. 0 = the behaviour before the snap.

Dependencies:
- Uses: glm only. Build:
    clang++ -std=c++17 -O2 -I build_render/_deps/glm-src \
      tools/measure_shadow_jitter.cpp -o /tmp/shadow_jitter
- Used by: render's shadow work (docs/specs/render.md, SHADOW_DIR_SNAP_RAD).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- **WHY THIS IS NOT A FRAME MEASUREMENT, AND WHY IT MAY NOT BECOME ONE.** The
  defect lives BETWEEN two frames of ONE run, and every capture door in this
  project takes one frame per process. Measured: two IDENTICAL runs at the same
  sun position disagree by 32.8 % of the shadow mask, because streaming and LOD
  state differ every launch. That is TEN TIMES the effect under test, so a
  frame pair cannot carry this claim at any sample size. The same trap as the
  running shimmer and the dungeon flicker, and it ate a 26-run sweep here
  before its control was run — the control being two shots of the SAME sun
  position, which is the first thing to run and was not.
- **THE ONLY REASON THIS IS C++ AND NOT PYTHON** (tools/ is otherwise stdlib
  Python, Rule 24): the quantity turns on a floor() of a float, so it is
  decided at boundaries that double precision would put on the other side. It
  mirrors the shipped arithmetic or it measures a different fix.
- IT MIRRORS, WHICH MEANS IT CAN DRIFT. The constants below are copied from
  BgfxRendererImpl.h and SkyModel.cpp. If update_shadow's snapping changes,
  this file is wrong until it is changed too — check it before quoting it.
*/
/*
UPD:
- 13:08:2026 - 18:50:00: Created for the user's "тени дергаются, когда движется
  солнце". First numbers: without the direction snap the grid slid 0.1720
  texels per frame on average and stepped a full 0.156 m texel 11.5 times a
  second, against the sun's own 0.36 mm of shadow motion per frame; at the
  shipped 0.00182 rad quantum, 0.0037 texels, median exactly zero, 0.1 events
  per second.
- 22:08:2026 - 13:45:06: зеркало SHADOW_HALF_EXTENT_M 320 -> 160 вслед за
  движком (мягкие тени, BgfxRendererImpl.h).
*/

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

// Mirrored from BgfxRendererImpl.h and SkyModel.cpp / NUMBERS.md.
constexpr float SHADOW_HALF_EXTENT_M = 160.0f;
constexpr int   SHADOW_MAP_SIZE = 4096;
constexpr float SHADOW_TEXEL_M = 2.0f * SHADOW_HALF_EXTENT_M / SHADOW_MAP_SIZE;
constexpr float DAY_LENGTH_SECONDS = 2880.0f;
constexpr float SKY_ARC_TILT = 0.45f;
constexpr float TAU = 6.28318530718f;

glm::vec3 sun_direction_at(float day_fraction) {
    const float angle = (day_fraction - 0.25f) * TAU;
    const float s = std::sin(angle), c = std::cos(angle);
    return glm::normalize(glm::vec3{c, s, SKY_ARC_TILT * (1.0f - std::fabs(s) * 0.35f)});
}

// update_shadow's light view, verbatim. `quantum` > 0 snaps the light DIRECTION
// to an angular grid first — the fix under test.
glm::mat4 light_view(float day_fraction, glm::vec3 eye, float quantum) {
    glm::vec3 dir = sun_direction_at(day_fraction);
    // MIRRORS update_shadow EXACTLY: azimuth and elevation each floor()ed onto
    // the quantum, then the direction rebuilt. Snapping the DAY ANGLE instead
    // would measure a different fix from the one that shipped.
    if (quantum > 0.0f) {
        const float az = std::atan2(dir.x, dir.z);
        const float el = std::asin(std::max(-1.0f, std::min(1.0f, dir.y)));
        const float qaz = std::floor(az / quantum) * quantum;
        const float qel = std::floor(el / quantum) * quantum;
        const float ce = std::cos(qel);
        dir = glm::vec3(ce * std::sin(qaz), std::sin(qel), ce * std::cos(qaz));
    }
    const glm::vec3 up = std::fabs(dir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const glm::mat4 rot = glm::lookAtRH(dir, glm::vec3(0.0f), up);
    glm::vec3 c = glm::vec3(rot * glm::vec4(eye, 1.0f));
    c.x = std::floor(c.x / SHADOW_TEXEL_M) * SHADOW_TEXEL_M;
    c.y = std::floor(c.y / SHADOW_TEXEL_M) * SHADOW_TEXEL_M;
    return glm::translate(glm::mat4(1.0f), -c) * rot;
}

int main(int argc, char** argv) {
    const float quantum = argc > 1 ? std::atof(argv[1]) : 0.0f;
    const float fps = 120.0f;
    const float dday = 1.0f / (DAY_LENGTH_SECONDS * fps);
    const glm::vec3 eye{620.0f, 16.2f, 846.0f};   // the probe's standpoint

    // Receivers at a spread of distances, all on the ground near the eye.
    std::vector<glm::vec3> pts;
    for (float d = 5.0f; d <= 80.0f; d += 5.0f) pts.push_back(eye + glm::vec3(0, -1.7f, -d));

    const int frames = 1200;                       // 10 s at 120 fps
    std::vector<double> per_frame;                 // texels of grid slide
    int jumps = 0;
    for (int k = 0; k < frames; ++k) {
        const float t0 = 0.30f + float(k) * dday;
        const glm::mat4 v0 = light_view(t0, eye, quantum);
        const glm::mat4 v1 = light_view(t0 + dday, eye, quantum);
        double worst = 0.0;
        for (const glm::vec3& p : pts) {
            const glm::vec3 a = glm::vec3(v0 * glm::vec4(p, 1.0f));
            const glm::vec3 b = glm::vec3(v1 * glm::vec4(p, 1.0f));
            const double dx = double(b.x - a.x) / SHADOW_TEXEL_M;
            const double dy = double(b.y - a.y) / SHADOW_TEXEL_M;
            worst = std::max(worst, std::sqrt(dx * dx + dy * dy));
        }
        per_frame.push_back(worst);
        if (worst > 0.5) ++jumps;
    }
    std::vector<double> s = per_frame;
    std::sort(s.begin(), s.end());
    double sum = 0; for (double v : s) sum += v;
    std::printf("quantum %.6f rad   SHADOW_TEXEL_M %.4f m\n", quantum, SHADOW_TEXEL_M);
    std::printf("  grid slide per frame, in TEXELS:  mean %.4f  p50 %.4f  p99 %.4f  max %.4f\n",
                sum / s.size(), s[s.size() / 2], s[size_t(s.size() * 0.99)], s.back());
    std::printf("  the same in METRES:               mean %.4f  p50 %.4f  p99 %.4f  max %.4f\n",
                sum / s.size() * SHADOW_TEXEL_M, s[s.size() / 2] * SHADOW_TEXEL_M,
                s[size_t(s.size() * 0.99)] * SHADOW_TEXEL_M, s.back() * SHADOW_TEXEL_M);
    std::printf("  frames whose grid jumped >= half a texel: %d / %d  (%.2f %%, i.e. %.1f per second)\n",
                jumps, frames, 100.0 * jumps / frames, jumps / (frames / fps));
    return 0;
}
