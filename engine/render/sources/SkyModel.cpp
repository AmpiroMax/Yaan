/*
Created: 09:08:2026 - 19:04:20
Last updated: 13:08:2026 - 18:59:13
Module: engine/render
File: engine/render/sources/SkyModel.cpp

Responsibility:
- SkyModel implementation: sun/moon geometry from the normalized clock and the
  dawn/day/dusk/night colour ramps written into RenderEnvironment.

Key items:
- sun_direction_at / moon_direction_at / moon_illumination / apply_sky_time.

Dependencies:
- Uses: SkyModel.h, Materials.h (day look-dev anchors), generated Constants.h
  (CAMERA_FAR for the fog span), glm.
- Used by: engine/app (per frame), tests.

Notes:
- Colour ramps are keyed off SUN ELEVATION (dot with up), not off the clock:
  the sky must look the same at a given sun height whatever the day length is,
  and the app's debug 50x time key must not change the palette, only its speed.
- Night is deliberately PLAYABLE-DARK under the moon (user decision): the night
  ambient keeps a navigable blue floor, and moonlight adds a real directional
  term on top. True darkness is reserved for interiors, where the torch and
  (once core lands sky visibility) the ambient falloff take over.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure: no GPU, no ECS, no wall clock. Inputs are fractions.
*/
/*
UPD:
- 09:08:2026 - 19:04:20: Created with the day/night stage.
- 12:08:2026 - 23:52:00: Two-moon geometry (W9). THE EPOCH SIGN WAS THE ONE THING THAT HAD TO BE
  DERIVED RATHER THAN COPIED: `+ epoch` puts Masser 162 deg from the meridian at
  the first frame, i.e. below the horizon, which is the exact defect the epoch
  rows were written to fix. `- epoch` reproduces FIVE of design's stated numbers
  at once (lit fractions 0.500 / 0.750 exactly, flat-arc hour angles +18 / +48,
  30 deg apart) and is the only sign under which moonrise is DELAYED, as
  MASSER_SYNODIC_DAYS claims. MEASURED against the shipped tilted arc: lit
  fractions and the 53.33 min/day delay come out exact; the hour angles read
  +20.25 / +46.65 and the separation 25.54 deg rather than 30, because
  SKY_ARC_TILT 0.45 is not in design's flat derivation. Both moons stand at 65
  and 41 deg elevation, 2.3x the separation floor — the row's claim holds, its
  two quoted angles are a flat-arc idealisation.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
*/

#include "engine/render/sources/SkyModel.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/Materials.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace dfn::render {

namespace {

constexpr float TAU = 6.28318530718f;
// The same turn in double, for the moon clock. A world clock in game DAYS runs
// to thousands, and float loses a whole day of resolution around day 8000 —
// the phase would quantise long before the world ran out.
constexpr double TAU_D = 6.283185307179586;

// The sun's arc is tilted toward the south (+Z) so it never passes exactly
// overhead: a zenith sun flattens every shadow to nothing at noon and the
// world loses its ground contact. Matches the afternoon look-dev sun.
constexpr float SKY_ARC_TILT = 0.45f;

// --- Palette anchors (look-dev; NUMBERS.md migration list) -----------------
// Day values come from Materials.h so the noon sky is identical to the look
// the design frames were accepted against; only dusk and night are new.
constexpr glm::vec3 SKY_ZENITH_NIGHT{0.020f, 0.035f, 0.085f};
constexpr glm::vec3 SKY_HORIZON_NIGHT{0.050f, 0.070f, 0.130f};
constexpr glm::vec3 SKY_ZENITH_DUSK{0.20f, 0.20f, 0.42f};
constexpr glm::vec3 SKY_HORIZON_DUSK{0.86f, 0.46f, 0.26f}; // low sun burn
constexpr glm::vec3 SUN_COLOR_LOW{1.00f, 0.55f, 0.28f};    // horizon warmth
constexpr glm::vec3 AMBIENT_NIGHT{0.070f, 0.090f, 0.150f}; // navigable blue
constexpr glm::vec3 AMBIENT_DUSK{0.26f, 0.24f, 0.28f};
// The moon: cold white, and CONSTANT — a dim moon is dim because moon_light
// drops, not because the disc changes colour. The same value tints the ground
// (the shader scales it by DFN_MOON_GROUND_MAX); the night's blue comes from
// the ambient floor, not from tinting the moon itself.
constexpr glm::vec3 MOON_DISC_COLOR{0.88f, 0.91f, 1.00f};

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float smooth01(float edge0, float edge1, float x) {
    const float t = clamp01((x - edge0) / std::max(edge1 - edge0, 1e-6f));
    return t * t * (3.0f - 2.0f * t);
}

glm::vec3 mix3(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + (b - a) * t;
}

// THE MOON'S ECLIPTIC LONGITUDE, and why the sun's is zero.
//
// A body's arc across the sky is set by its DECLINATION, and declination comes
// from ecliptic longitude through the obliquity. This world has no year: there
// is no seasonal constant anywhere in the registry and the SUN rides a fixed
// look-dev arc (SKY_ARC_TILT) chosen so shadows always have a direction. So
// the sun is placed at ecliptic longitude 0 — a permanent equinox — and the
// moon's longitude is measured from it, which is exactly what elongation is.
//
// The consequence is the point of the whole change: the sun's arc is CONSTANT
// and the moon's is not. Over one synodic month the moon's longitude walks the
// full ecliptic, so its declination sweeps +-(OBLIQUITY +- inclination) and its
// path swings between an arc that clears 55 deg and one that barely leaves the
// southern horizon — at OBSERVER_LATITUDE, the difference between a night you
// can see by and a night you cannot. That is «её траектория должна отличаться»
// in one sentence.
//
// NOT AN EPHEMERIS, and deliberately not: no perturbations, no parallax, no
// year. The behaviours this owes the player are the arc, the delay, the phase
// and daylight visibility, and each of them is one term below.
float moon_declination(float longitude, float latitude_out_of_plane) {
    const float sb = std::sin(latitude_out_of_plane);
    const float cb = std::cos(latitude_out_of_plane);
    return std::asin(std::clamp(sb * std::cos(ECLIPTIC_OBLIQUITY)
                                    + cb * std::sin(ECLIPTIC_OBLIQUITY)
                                          * std::sin(longitude),
                                -1.0f, 1.0f));
}

// Right ascension for the same pair. The moon's hour angle is the SUN'S hour
// angle minus this, and that subtraction is where the whole rise delay lives:
// right ascension grows one turn per synodic month, so the moon's hour angle
// advances at TAU*(1 - 1/P) per day and the interval between two moonrises is
// P/(P-1) days = DAY_LENGTH/(P-1) = 53.33 in-world minutes of delay a day.
// THAT IS `MASSER_SYNODIC_DAYS`' OWN ROW, arrived at from the geometry rather
// than applied as a correction — and the sign is the half the shipped model
// had backwards: it ADDED elongation to the arc angle, which made the moon
// rise 49.7 minutes EARLIER every day, the wrong direction with a plausible
// magnitude (Rule 36's trap, and the registry row warns about this exact
// family of near-miss).
float moon_right_ascension(float longitude, float latitude_out_of_plane) {
    const float cb = std::cos(latitude_out_of_plane);
    const float y = std::sin(longitude) * std::cos(ECLIPTIC_OBLIQUITY)
                    - (cb > 1e-6f ? std::tan(latitude_out_of_plane) : 0.0f)
                          * std::sin(ECLIPTIC_OBLIQUITY);
    return std::atan2(y, std::cos(longitude));
}

// Direction on the tilted arc for an angle where 0 = below (midnight) and
// pi/2 = due east horizon... expressed directly: `angle` runs with the clock.
glm::vec3 arc_direction(float angle) {
    // Rises in the east (+X), sets in the west (-X), culminating south (+Z).
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    const glm::vec3 dir{c, s, SKY_ARC_TILT * (1.0f - std::fabs(s) * 0.35f)};
    return glm::normalize(dir);
}

} // namespace

glm::vec3 horizon_direction(float hour_angle, float declination) {
    // The textbook equatorial -> horizontal rotation, written as a vector so
    // there is no altitude/azimuth round trip to lose a quadrant in. Hour
    // angle 0 is the meridian and grows westward; at declination 0 this puts
    // the body due south at altitude 90 - OBSERVER_LATITUDE, and six hours
    // earlier (H = -pi/2) due east on the horizon, whatever the declination —
    // which is the sanity pair worth keeping in mind while reading it.
    const float sd = std::sin(declination);
    const float cd = std::cos(declination);
    const float sh = std::sin(hour_angle);
    const float ch = std::cos(hour_angle);
    const float sp = std::sin(OBSERVER_LATITUDE);
    const float cp = std::cos(OBSERVER_LATITUDE);
    const float east = -cd * sh;
    const float north = sd * cp - cd * ch * sp;
    const float up = sd * sp + cd * ch * cp;
    // Engine frame: +X east, +Y up, +Z SOUTH (arc_direction's own convention —
    // "rises in the east (+X) ... culminating south (+Z)").
    return glm::normalize(glm::vec3{east, up, -north});
}

glm::vec3 sun_direction_at(float day_fraction) {
    // 0.25 -> east horizon (elevation 0), 0.5 -> highest, 0.75 -> west horizon.
    const float angle = (day_fraction - 0.25f) * TAU;
    return arc_direction(angle);
}

float moon_illumination(float lunar_phase) {
    const float phase = lunar_phase - std::floor(lunar_phase);
    // 0 = new (dark), 0.5 = full. Smooth, symmetric around full.
    return 0.5f * (1.0f - std::cos(phase * TAU));
}

MoonElements masser() {
    return MoonElements{
        static_cast<float>(config::MASSER_SYNODIC_DAYS),
        static_cast<float>(config::MASSER_ELONGATION_EPOCH),
        static_cast<float>(config::MASSER_INCLINATION),
        static_cast<float>(config::MASSER_NODE_EPOCH),
        static_cast<float>(config::MASSER_NODE_PERIOD_DAYS),
        static_cast<float>(config::MASSER_ECCENTRICITY),
        static_cast<float>(config::MASSER_ANGULAR_DIAMETER),
        static_cast<float>(config::MASSER_DISC_LUMA)};
}

MoonElements secunda() {
    return MoonElements{
        static_cast<float>(config::SECUNDA_SYNODIC_DAYS),
        static_cast<float>(config::SECUNDA_ELONGATION_EPOCH),
        static_cast<float>(config::SECUNDA_INCLINATION),
        static_cast<float>(config::SECUNDA_NODE_EPOCH),
        static_cast<float>(config::SECUNDA_NODE_PERIOD_DAYS),
        static_cast<float>(config::SECUNDA_ECCENTRICITY),
        static_cast<float>(config::SECUNDA_ANGULAR_DIAMETER),
        static_cast<float>(config::SECUNDA_DISC_LUMA)};
}

MoonState moon_state_at(const MoonElements& m, float day_fraction,
                        double elapsed_days) {
    MoonState s{};
    const double period = m.synodic_days > 1e-3f
                              ? static_cast<double>(m.synodic_days)
                              : 1e-3;
    // ELONGATION IS A FUNCTION OF THE WORLD CLOCK, and that is the whole
    // difference from the one-moon model: there `lunar_phase` arrived as a
    // parameter, so two moons fed the same call would move together. Here each
    // moon walks its own synodic period from its own epoch, and the epochs are
    // NUMBERS rows chosen so that BOTH moons stand above the horizon, 30 deg
    // apart, with a readable terminator, in the game's very first frame —
    // that row exists because the old `angle = sun + phase*TAU` put the moon at
    // elongation 0 at day 0, i.e. inside the sun's glare at new moon, and the
    // game had literally never started with a visible moon.
    // THE EPOCH IS ADDED — CORRECTED 13:08, and the correction is the whole
    // reason this file's geometry changed. The entry below used to argue for
    // `- epoch` because it reproduced design's five numbers. It did, but only
    // in company with a SECOND sign error that cancelled it: the old arc added
    // elongation to the arc angle, i.e. it put the moon WEST of the sun by its
    // elongation, and two wrongs agreed on the hour angles while agreeing on
    // nothing else. What the pair could not reproduce was the DIRECTION of the
    // rise delay: it made moonrise 49.7 minutes EARLIER each day where
    // MASSER_SYNODIC_DAYS' own row derives 53.33 minutes LATER. A waxing
    // crescent stood in the morning sky, where only a waning one belongs.
    //
    // With the hour angle taken properly (sun's hour angle MINUS the moon's
    // right ascension, below) `+ epoch` is what the row's name says it is —
    // the elongation AT the epoch — and design's five numbers come back at the
    // same time as the delay: elongation at day 0 is 270 and 240 deg, lit
    // 0.500 and 0.750, hour angles at START_TIME_OF_DAY 0.30 +18 and +48 deg,
    // the pair 30 deg apart, and moonrise 53.33 in-world minutes later each
    // day. Six numbers now, and the sixth is the one that was wrong.
    double elong = TAU_D * elapsed_days / period
                   + static_cast<double>(m.elongation_epoch);
    elong = elong - std::floor(elong / TAU_D) * TAU_D;
    s.elongation = static_cast<float>(elong);
    // Phase in the shader's own convention (0 = new, 0.5 = full): the disc
    // terminator is driven by exactly this number, so it may not be a second
    // encoding of the same fact.
    s.phase = static_cast<float>(elong / TAU_D);
    s.illumination = 0.5f * (1.0f - std::cos(s.elongation));

    // THE EQUATION OF CENTRE, and it is only worth computing because the discs
    // are ENLARGED. 2e of true-vs-mean longitude is 6.30 deg for Masser = 25.8
    // px of wander off a uniform circle; at the moon's TRUE angular size those
    // 2.25 px of diameter swing would be 0.23 px and invisible. The enlargement
    // and the eccentricity are one decision — NUMBERS says so in the row — and
    // taking the first while dropping the second as a detail is not available.
    const float mean_anomaly = s.elongation;
    const float centre = 2.0f * m.eccentricity * std::sin(mean_anomaly);
    // ECLIPTIC LONGITUDE, measured from a sun that sits at zero (see the note
    // on moon_declination): mean elongation plus the equation of centre.
    const float longitude = s.elongation + centre;
    // Apparent size follows the same orbit: nearer at perigee.
    s.angular_radius =
        0.5f * m.angular_diameter * (1.0f + m.eccentricity * std::cos(mean_anomaly));

    // INCLINATION, about a node line that REGRESSES. Without the regression the
    // two orbits would keep a fixed relative geometry and the pair would repeat;
    // with it, latitude and longitude beat against each other on periods that
    // are again in golden ratio. The node period is the ONE number of the W9
    // block that NUMBERS marks as a choice rather than a derivation, and it says
    // why: the real 18.6 years is 226 real days of continuous play per cycle,
    // and what cannot be observed cannot be accepted by a frame.
    const double node_period = m.node_period_days > 1e-3f
                                   ? static_cast<double>(m.node_period_days)
                                   : 1e-3;
    double node = static_cast<double>(m.node_epoch)
                  - TAU_D * elapsed_days / node_period; // retrograde
    node = node - std::floor(node / TAU_D) * TAU_D;
    // Ecliptic LATITUDE: zero AT the nodes, extreme a quarter turn from them —
    // the definition of a node, and the reason the two moons never share a
    // latitude for long once their node lines are 90 deg apart at epoch. It is
    // measured from the LONGITUDE now, not from a position on the sun's arc,
    // because latitude is an orbital quantity and has nothing to do with the
    // time of day the old expression mixed into it.
    const float latitude =
        m.inclination * std::sin(longitude - static_cast<float>(node));

    // THE ARC. Declination decides its shape, hour angle where along it the
    // moon stands right now, and OBSERVER_LATITUDE turns the pair into a
    // direction. This is the line the user's complaint is about: the sun's
    // arc comes from arc_direction and never changes; this one is a different
    // curve every night because `declination` is a different number every
    // night.
    s.declination = moon_declination(longitude, latitude);
    s.right_ascension = moon_right_ascension(longitude, latitude);
    // Hour angle: the sun's (zero at local noon, one turn a day) minus the
    // moon's right ascension. The subtraction IS the rise delay.
    s.hour_angle = (day_fraction - 0.5f) * TAU - s.right_ascension;
    s.direction = horizon_direction(s.hour_angle, s.declination);

    const glm::vec3 sun = sun_direction_at(day_fraction);
    s.solar_separation =
        std::acos(std::clamp(glm::dot(s.direction, sun), -1.0f, 1.0f));
    // MOON_SOLAR_EXCLUSION is 20 deg, the real limit of naked-eye visibility
    // near the sun. Chosen for a REASON rather than for a value (Rule 36): a
    // moon that close is unobservable in the real sky too.
    s.observable =
        s.solar_separation > static_cast<float>(config::MOON_SOLAR_EXCLUSION);
    return s;
}

glm::vec3 moon_direction_at(float day_fraction, float lunar_phase) {
    // Elongation from the sun IS the phase angle: full (0.5) sits opposite the
    // sun and therefore rises at sunset; new (0) rides with the sun and is
    // invisible in daylight. NOT "the same arc offset in time" any more — that
    // sentence was the defect. A moon at elongation E stands at ecliptic
    // longitude E, and a body at longitude E has a declination, and the
    // declination is what makes its path across the sky a DIFFERENT CURVE from
    // the sun's rather than the same one an hour later.
    //
    // Zero inclination here, so this entry point is the flat, node-free
    // reading of the same geometry moon_state_at uses: one model in this file,
    // read two ways, rather than two models that can disagree (Rule 35).
    const float phase = lunar_phase - std::floor(lunar_phase);
    const float longitude = phase * TAU;
    const float dec = moon_declination(longitude, 0.0f);
    const float ra = moon_right_ascension(longitude, 0.0f);
    return horizon_direction((day_fraction - 0.5f) * TAU - ra, dec);
}

void apply_sky_time(platform::RenderEnvironment& env, float day_fraction,
                    float lunar_phase, double elapsed_days) {
    const float day = day_fraction - std::floor(day_fraction);
    const glm::vec3 sun = sun_direction_at(day);
    // THE SHIPPED MOON IS MASSER, AND IT IS THE ORBITAL MODEL — the W9 half
    // that has sat unused since 12.08 because nothing called it. The user's
    // «луна двигается за солнцем, почти одинаково» was a report on the OTHER
    // half: apply_sky_time drove the moon from a phase parameter along the
    // sun's own arc, so it was the sun's path with a delay, by construction.
    //
    // See the header on `elapsed_days`: a negative value means the caller has
    // not been taught the world clock yet and it is reconstructed from the
    // phase, exact for everything except the node regression.
    const double clock = elapsed_days >= 0.0
                             ? elapsed_days
                             : static_cast<double>(lunar_phase
                                                   - std::floor(lunar_phase))
                                   * config::MASSER_SYNODIC_DAYS;
    const MoonState masser_state = moon_state_at(masser(), day, clock);
    const glm::vec3 moon = masser_state.direction;

    // Everything below is keyed off sun ELEVATION, never off the clock, so the
    // palette is identical at a given sun height whatever the day length.
    const float elevation = sun.y;
    const float day_t = smooth01(SKY_NIGHT_ELEVATION, SKY_DAY_ELEVATION, elevation);
    // Dusk peaks when the sun sits ON the horizon and fades both ways.
    const float dusk_t = 1.0f - smooth01(0.0f, 0.30f, std::fabs(elevation));
    const float night_t = 1.0f - day_t;

    env.sun_direction = sun;
    // Sun colour reddens as it drops; below the horizon it stops contributing
    // (the backend also disables shadows there).
    const float low_t = 1.0f - smooth01(0.0f, 0.35f, elevation);
    const glm::vec3 sun_hue = mix3(LOOKDEV_SUN_COLOR, SUN_COLOR_LOW, low_t);
    env.sun_color = sun_hue * clamp01(smooth01(SKY_NIGHT_ELEVATION, 0.10f, elevation));

    // Ambient: day -> dusk -> a navigable night blue (user decision: night is
    // playable-dark under the moon, not a torch-only experience).
    glm::vec3 ambient = mix3(AMBIENT_NIGHT, LOOKDEV_AMBIENT_COLOR, day_t);
    ambient = mix3(ambient, AMBIENT_DUSK, dusk_t * 0.5f);
    env.ambient_color = ambient;

    // Sky gradient: night -> dusk burn -> day.
    glm::vec3 zenith = mix3(SKY_ZENITH_NIGHT, LOOKDEV_SKY_ZENITH, day_t);
    glm::vec3 horizon = mix3(SKY_HORIZON_NIGHT, LOOKDEV_SKY_HORIZON, day_t);
    zenith = mix3(zenith, SKY_ZENITH_DUSK, dusk_t * 0.65f);
    horizon = mix3(horizon, SKY_HORIZON_DUSK, dusk_t * 0.85f);
    env.sky_zenith_color = zenith;
    env.sky_horizon_color = horizon;
    // Fog always matches the horizon so distant terrain melts into the sky at
    // every hour — the rule that keeps the world edge invisible.
    env.fog_color = horizon;
    env.fog_start_m = LOOKDEV_FOG_START_FRAC * static_cast<float>(config::CAMERA_FAR);
    env.fog_end_m = LOOKDEV_FOG_END_FRAC * static_cast<float>(config::CAMERA_FAR);

    // Moon: the disc is drawn whenever it is up; its LIGHT also scales with
    // how much of it is lit (user decision: phases change real brightness).
    // Phase and illumination come from the SAME state the direction came from,
    // so the disc's terminator and its light cannot describe different moons.
    env.moon_direction = moon;
    env.moon_phase = masser_state.phase;
    env.moon_color = MOON_DISC_COLOR;
    const float moon_up = smooth01(-0.05f, 0.20f, moon.y);
    env.moon_light = masser_state.illumination * moon_up * night_t;

    // Stars: out only once the sun is properly down, and washed out by a bright
    // moon. Explicit field, so overcast can later zero it without touching
    // anything else.
    const float star_night = 1.0f - smooth01(SKY_NIGHT_ELEVATION, 0.02f, elevation);
    env.star_intensity = star_night * (1.0f - 0.45f * env.moon_light);
}

} // namespace dfn::render
