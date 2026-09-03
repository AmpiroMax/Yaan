/*
Module: engine/anim
File: engine/anim/sources/LocoTelemetry.cpp

Responsibility:
- Реализация приборов локомоции (LocoTelemetry.h): прямая кинематика позы
  тика, детекторы с порогами реестра, строки экрана, таблица, CSV.

Key items:
- LocoTelemetry::push(): один тик — все детекторы.
- forward_kinematics(): локальные TRS → матрицы модели по родителям.

Dependencies:
- Uses: LocoTelemetry.h, Constants.h (пороги), glm.
- Used by: engine/app, tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Прибор читает и считает; ни одной записи в позу или состояние тела.
*/
#include "engine/anim/sources/LocoTelemetry.h"

#include "engine/core/config/sources/Constants.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dfn::anim {

namespace {

constexpr float PI_F = glm::pi<float>();

[[nodiscard]] float wrap_phase(float d) {
    d -= std::floor(d);
    return d > 0.5f ? 1.0f - d : d;
}

[[nodiscard]] float quat_angle(const glm::quat& a, const glm::quat& b) {
    // угол поворота между двумя ориентациями, рад, всегда по короткой дуге
    const float d = std::clamp(std::abs(glm::dot(glm::normalize(a), glm::normalize(b))),
                               0.0f, 1.0f);
    return 2.0f * std::acos(d);
}

[[nodiscard]] float yaw_of(const glm::vec3& v) {
    // рыск горизонтального вектора; система тела: 0 = −Z
    return std::atan2(v.x, -v.z);
}

[[nodiscard]] float wrap_angle(float a) {
    while (a > PI_F) {
        a -= 2.0f * PI_F;
    }
    while (a < -PI_F) {
        a += 2.0f * PI_F;
    }
    return a;
}

[[nodiscard]] glm::vec3 joint_pos(const glm::mat4& m) { return glm::vec3{m[3]}; }

} // namespace

void LocoTelemetry::reset(const skel::Skeleton& skeleton, const FootIkSetup& setup) {
    *this = LocoTelemetry{};
    skeleton_ = &skeleton;
    setup_ = setup;
    if (!setup.valid()) {
        return;
    }
    pelvis_ = skeleton.joints[static_cast<std::size_t>(setup.hip[0])].parent;
    joints_.assign(skeleton.size(), JointTrack{});
    frame_joints_.assign(skeleton.size(), JointTrack{});
    world_.assign(skeleton.size(), glm::mat4{1.0f});
    // знак «левая лодыжка левее правой» берём из покоя привязки
    std::vector<JointLocal> bind(skeleton.size());
    for (std::size_t j = 0; j < skeleton.size(); ++j) {
        bind[j].translation = skeleton.joints[j].bind_translation;
        bind[j].rotation = skeleton.joints[j].bind_rotation;
        bind[j].scale = skeleton.joints[j].bind_scale;
    }
    forward_kinematics(bind);
    const float dx = joint_pos(world_[static_cast<std::size_t>(setup.ankle[0])]).x
                     - joint_pos(world_[static_cast<std::size_t>(setup.ankle[1])]).x;
    cross_sign_ = dx < 0.0f ? -1.0f : 1.0f;
    // перед таза: перпендикуляр линии бёдер, направленный туда же, куда носки в покое
    if (setup.toe[0] >= 0 && setup.toe[1] >= 0) {
        const glm::vec3 hl = joint_pos(world_[static_cast<std::size_t>(setup.hip[0])]);
        const glm::vec3 hr = joint_pos(world_[static_cast<std::size_t>(setup.hip[1])]);
        const glm::vec3 al = joint_pos(world_[static_cast<std::size_t>(setup.ankle[0])]);
        const glm::vec3 ol = joint_pos(world_[static_cast<std::size_t>(setup.toe[0])]);
        const glm::vec3 hips{hr.x - hl.x, 0.0f, hr.z - hl.z};
        const glm::vec3 fwd{hips.z, 0.0f, -hips.x};
        const glm::vec3 toe{ol.x - al.x, 0.0f, ol.z - al.z};
        pelvis_fwd_sign_ = glm::dot(fwd, toe) < 0.0f ? -1.0f : 1.0f;
    }

    using P = LocoProbe;
    const auto set = [&](P p, std::string_view name, std::string_view unit, double limit,
                         bool lower_is_bad = false) {
        auto& r = rows_[static_cast<std::size_t>(p)];
        r.name = name;
        r.unit = unit;
        r.limit = static_cast<float>(limit);
        r.lower_is_bad = lower_is_bad;
        r.worst = lower_is_bad ? 1.0e9f : 0.0f;
    };
    set(P::Slide, "slide", "mm", 1000.0 * config::FOOT_SLIDE_MAX_M);
    set(P::Residual, "residual", "mm", 0.0);
    set(P::Gap, "gap", "mm", 1000.0 * config::LOCO_GAP_MAX_M);
    set(P::ThighAccel, "thigh_acc", "rad/s2", config::LOCO_JOINT_ACCEL_MAX_RADPS2);
    set(P::KneeAccel, "knee_acc", "rad/s2", config::LOCO_JOINT_ACCEL_MAX_RADPS2);
    set(P::FootAccel, "foot_acc", "rad/s2", config::LOCO_JOINT_ACCEL_MAX_RADPS2);
    set(P::FrameThighAccel, "f_thigh", "rad/s2", config::LOCO_JOINT_ACCEL_MAX_RADPS2);
    set(P::FrameKneeAccel, "f_knee", "rad/s2", config::LOCO_JOINT_ACCEL_MAX_RADPS2);
    set(P::FrameMismatch, "f_mismatch", "deg", config::LOCO_FRAME_MISMATCH_MAX_DEG);
    set(P::AnkleAccel, "ankle_acc", "m/s2", config::LOCO_ANKLE_ACCEL_MAX_MPS2);
    set(P::AnkleCross, "cross", "mm", 1000.0 * config::LOCO_CROSS_MAX_M);
    set(P::LateralDrift, "drift", "mm", 1000.0 * config::LOCO_DRIFT_MAX_M);
    set(P::TapDrift, "tap", "mm", 1000.0 * config::LOCO_TAP_DRIFT_MAX_M);
    set(P::NoStepTravel, "nostep", "x2step", config::LOCO_NO_STEP_MAX);
    set(P::RootAccel, "root_acc", "m/s2", config::LOCO_ROOT_ACCEL_MAX_MPS2);
    set(P::PhaseJump, "phase", "cycle", config::LOCO_PHASE_JUMP_MAX);
    set(P::Twist, "twist", "deg", config::FOOT_LOCK_TWIST_MAX_RAD * 180.0 / glm::pi<double>());
    set(P::KneeBend, "knee_bend", "deg", config::LOCO_KNEE_BEND_MIN_DEG, true);
    set(P::SpeedError, "speed_err", "frac", config::LOCO_SPEED_ERR_MAX);
    ready_ = true;
}

void LocoTelemetry::forward_kinematics(std::span<const JointLocal> pose) {
    const auto& joints = skeleton_->joints;
    for (std::size_t j = 0; j < joints.size() && j < pose.size(); ++j) {
        const glm::mat4 local = glm::translate(glm::mat4{1.0f}, pose[j].translation)
                                * glm::mat4_cast(glm::normalize(pose[j].rotation))
                                * glm::scale(glm::mat4{1.0f}, pose[j].scale);
        const int32_t p = joints[j].parent;
        world_[j] = p >= 0 ? world_[static_cast<std::size_t>(p)] * local : local;
    }
}

void LocoTelemetry::note(LocoProbe p, float value) {
    auto& r = rows_[static_cast<std::size_t>(p)];
    r.last = value;
    if (r.lower_is_bad) {
        r.worst = std::min(r.worst, value);
        if (r.limit > 0.0f && value < r.limit) {
            ++r.hits;
        }
    } else {
        r.worst = std::max(r.worst, value);
        if (r.limit > 0.0f && value > r.limit) {
            ++r.hits;
        }
    }
}

uint32_t LocoTelemetry::total_hits() const {
    uint32_t n = 0;
    for (const auto& r : rows_) {
        n += r.hits;
    }
    return n;
}

void LocoTelemetry::push(const LocoTick& t) {
    if (!ready_ || t.dt <= 0.0f || t.pose.size() < skeleton_->size() || t.contacts == nullptr
        || t.play == nullptr || t.loco == nullptr || t.drive == nullptr || t.locks == nullptr) {
        return;
    }
    using P = LocoProbe;
    const float dt = t.dt;
    ++ticks_;
    seconds_ += dt;
    forward_kinematics(t.pose);

    const glm::vec3 delta = t.root.ground - t.root_prev.ground;
    const glm::vec3 flat{delta.x, 0.0f, delta.z};
    const float dist = glm::length(flat);
    const glm::vec3 vel = flat / dt;
    const float speed = dist / dt;
    const bool grounded = t.drive->grounded;
    const bool moving_role = locomotion_role(t.play->role);
    const float want = t.drive->want_speed_mps;
    const bool input = want > 1.0e-3f;

    // 1. снос замкнутой стопы к якорю
    {
        float worst = 0.0f;
        for (std::size_t s = 0; s < 2; ++s) {
            if (t.locks->locked[s]) {
                const glm::vec3 d = t.contact_world[s] - t.locks->anchor[s];
                worst = std::max(worst, glm::length(glm::vec2{d.x, d.z}));
            }
        }
        note(P::Residual, 1000.0f * worst);
    }
    tick_dt_ = dt;
    // 2. зазор опорной стопы
    note(P::Gap, 1000.0f * t.gap.worst_abs());

    // 3–5. угловое ускорение суставов ноги (локальное — то, что видно как рывок)
    const auto joint_accel = [&](int32_t j) {
        auto& tr = joints_[static_cast<std::size_t>(j)];
        const glm::quat q = t.pose[static_cast<std::size_t>(j)].rotation;
        float accel = 0.0f;
        if (tr.has_rot) {
            const float omega = quat_angle(tr.rot, q) / dt;
            if (tr.has_omega) {
                accel = std::abs(omega - tr.omega) / dt;
            }
            tr.omega = omega;
            tr.has_omega = true;
        }
        tr.prev_rot = tr.has_rot ? tr.rot : q;
        tr.rot = q;
        tr.has_rot = true;
        tr.accel = accel;
        return accel;
    };
    note(P::ThighAccel, std::max(joint_accel(setup_.hip[0]), joint_accel(setup_.hip[1])));
    note(P::KneeAccel, std::max(joint_accel(setup_.knee[0]), joint_accel(setup_.knee[1])));
    note(P::FootAccel, std::max(joint_accel(setup_.ankle[0]), joint_accel(setup_.ankle[1])));

    // 6. линейное ускорение лодыжки в мире
    {
        const glm::mat4 to_world =
            glm::translate(glm::mat4{1.0f}, t.root.ground)
            * glm::rotate(glm::mat4{1.0f}, -t.root.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
        float worst = 0.0f;
        for (std::size_t s = 0; s < 2; ++s) {
            auto& a = ankles_[s];
            const glm::vec3 p = glm::vec3{to_world * glm::vec4{t.contacts->ankle[s], 1.0f}};
            if (a.has_pos) {
                const glm::vec3 v = (p - a.pos) / dt;
                if (a.has_vel) {
                    a.accel = glm::length(v - a.vel) / dt;
                    worst = std::max(worst, a.accel);
                }
                a.vel = v;
                a.has_vel = true;
            }
            a.pos = p;
            a.has_pos = true;
        }
        note(P::AnkleAccel, worst);
    }

    // 7. перекрест лодыжек: левая ушла правее правой (поперёк тела)
    {
        const float xl = joint_pos(world_[static_cast<std::size_t>(setup_.ankle[0])]).x;
        const float xr = joint_pos(world_[static_cast<std::size_t>(setup_.ankle[1])]).x;
        const float across = -(xl - xr) * cross_sign_; // + когда перекрест
        note(P::AnkleCross, 1000.0f * std::max(0.0f, across));
    }

    // направление хода: последний ненулевой ввод, повёрнутый рыском тела
    if (input) {
        const glm::vec3 m = t.drive->move_dir_model;
        const float c = std::cos(t.root.yaw);
        const float s = std::sin(t.root.yaw);
        // модель −Z = вперёд; рыск сим'а: 0 = −Z, + = по часовой
        const glm::vec3 w{c * m.x + s * m.z, 0.0f, -s * m.x + c * m.z};
        if (glm::length(w) > 1.0e-4f) {
            travel_dir_ = glm::normalize(w);
            has_travel_dir_ = true;
        }
    }

    // 8. боковой снос за окно («змейка»)
    if (has_travel_dir_) {
        const glm::vec3 right{-travel_dir_.z, 0.0f, travel_dir_.x};
        const float lateral = glm::dot(flat, right);
        drift_.push_back({seconds_, lateral});
        drift_sum_ += lateral;
        const float window = static_cast<float>(config::LOCO_DRIFT_WINDOW_S);
        while (!drift_.empty() && seconds_ - drift_.front().t > window) {
            drift_sum_ -= drift_.front().lateral;
            drift_.pop_front();
        }
        // «ЗМЕЙКА» МЕРИТСЯ ОТ ПОСТАНОВКИ ДО ПОСТАНОВКИ ТОЙ ЖЕ СТОПЫ: за цикл
        // качание таза влево-вправо сходится в ноль, остаётся только увод.
        const float lat_now = glm::dot(glm::vec3{t.root.ground.x, 0.0f, t.root.ground.z}, right);
        for (std::size_t s = 0; s < 2; ++s) {
            if (!t.loco->footfall[s]) {
                continue;
            }
            if (cycle_has_[s]) {
                note(P::LateralDrift, 1000.0f * std::abs(lat_now - cycle_lateral_[s]));
            }
            cycle_lateral_[s] = lat_now;
            cycle_has_[s] = true;
        }
        if (!moving_role) {
            cycle_has_ = {false, false};
        }
    }

    // 9. путь без ввода: эпизод от отпускания до следующего нажатия
    if (!input && grounded) {
        tap_episode_m_ += dist;
        tap_open_ = true;
        note(P::TapDrift, 1000.0f * tap_episode_m_);
    } else if (input && tap_open_) {
        tap_open_ = false;
        tap_episode_m_ = 0.0f;
        rows_[static_cast<std::size_t>(P::TapDrift)].last = 0.0f;
    }

    // 10. путь без постановки стопы (в долях от двух шагов заказа)
    {
        const bool fell = t.loco->footfall[0] || t.loco->footfall[1];
        if (t.loco->footfall[0]) {
            ++footfalls_[0];
        }
        if (t.loco->footfall[1]) {
            ++footfalls_[1];
        }
        if (fell) {
            since_footfall_m_ = 0.0f;
        } else if (moving_role && grounded) {
            since_footfall_m_ += dist;
        } else {
            since_footfall_m_ = 0.0f;
        }
        const float two_steps = 2.0f * std::max(0.1f, t.drive->step_length_m);
        note(P::NoStepTravel, since_footfall_m_ / two_steps);
    }

    // 11. ускорение корня
    if (has_vel_prev_) {
        note(P::RootAccel, glm::length(vel - vel_prev_) / dt);
    }
    vel_prev_ = vel;
    has_vel_prev_ = true;

    // 12. разрыв фазы на смене роли; с какой ноги старт
    if (has_role_ && t.play->role != role_prev_) {
        ++role_changes_;
        if (locomotion_role(role_prev_) && moving_role) {
            note(P::PhaseJump, wrap_phase(t.play->phase - phase_prev_));
        }
        if (!locomotion_role(role_prev_) && moving_role) {
            start_pending_ = true;
            planted_ = {t.contacts->support[0] > 0.0f, t.contacts->support[1] > 0.0f};
        }
    }
    if (start_pending_) {
        const float off = static_cast<float>(config::FOOT_LOCK_OFF_WEIGHT);
        for (std::size_t s = 0; s < 2 && start_pending_; ++s) {
            if (planted_[s] && t.contacts->support[s] < off) {
                ++start_foot_[s];
                start_pending_ = false;
            }
        }
        if (!moving_role) {
            start_pending_ = false;
        }
    }
    role_prev_ = t.play->role;
    phase_prev_ = t.play->phase;
    has_role_ = true;

    // 13. скрутка: носок стопы против таза (перпендикуляр линии бёдер).
    // Линия лодыжек не годится: на шаге она ложится вдоль хода и даёт ±90°.
    if (setup_.toe[0] >= 0 && setup_.toe[1] >= 0) {
        const glm::vec3 hl = joint_pos(world_[static_cast<std::size_t>(setup_.hip[0])]);
        const glm::vec3 hr = joint_pos(world_[static_cast<std::size_t>(setup_.hip[1])]);
        const glm::vec3 hips{hr.x - hl.x, 0.0f, hr.z - hl.z};
        if (glm::length(hips) > 1.0e-3f) {
            // вперёд у таза: линия бёдер, повёрнутая на −90° вокруг Y — в системе
            // тела «правая» ось (R − L) и перед (−Z) образуют правую пару с Y вверх
            const glm::vec3 pelvis_fwd = pelvis_fwd_sign_ * glm::vec3{hips.z, 0.0f, -hips.x};
            const float pelvis_yaw = yaw_of(pelvis_fwd);
            pelvis_yaw_deg_ = pelvis_yaw * 180.0f / PI_F;
            float worst = 0.0f;
            for (std::size_t s = 0; s < 2; ++s) {
                // только опорная стопа: висящая в махе смотрит носком вниз и назад
                if (t.contacts->support[s] <= 0.0f) {
                    continue;
                }
                const glm::vec3 a = joint_pos(world_[static_cast<std::size_t>(setup_.ankle[s])]);
                const glm::vec3 o = joint_pos(world_[static_cast<std::size_t>(setup_.toe[s])]);
                const glm::vec3 f{o.x - a.x, 0.0f, o.z - a.z};
                // стопа, вставшая на носок, в плане — точка: её рыск не читается
                if (glm::length(f) > 0.05f) {
                    toe_yaw_deg_[s] = yaw_of(f) * 180.0f / PI_F;
                    worst = std::max(worst, std::abs(wrap_angle(yaw_of(f) - pelvis_yaw)));
                }
            }
            note(P::Twist, worst * 180.0f / PI_F);
        }
    }

    // 14. минимальный сгиб колена под опорой
    {
        float bend_min = 1.0e9f;
        for (std::size_t s = 0; s < 2; ++s) {
            if (t.contacts->support[s] <= 0.0f) {
                continue;
            }
            const glm::vec3 h = joint_pos(world_[static_cast<std::size_t>(setup_.hip[s])]);
            const glm::vec3 k = joint_pos(world_[static_cast<std::size_t>(setup_.knee[s])]);
            const glm::vec3 a = joint_pos(world_[static_cast<std::size_t>(setup_.ankle[s])]);
            const glm::vec3 u = glm::normalize(h - k);
            const glm::vec3 v = glm::normalize(a - k);
            const float ang = std::acos(std::clamp(glm::dot(u, v), -1.0f, 1.0f));
            bend_min = std::min(bend_min, 180.0f - ang * 180.0f / PI_F);
        }
        if (bend_min < 1.0e8f) {
            note(P::KneeBend, bend_min);
        }
    }

    // 15. ошибка скорости при удержанном вводе
    if (input && grounded) {
        input_held_s_ += dt;
        const float tau = 0.25f;
        const float k = 1.0f - std::exp(-dt / tau);
        speed_ema_ = has_speed_ema_ ? speed_ema_ + (speed - speed_ema_) * k : speed;
        has_speed_ema_ = true;
        if (input_held_s_ >= static_cast<float>(config::LOCO_SPEED_HOLD_S)) {
            note(P::SpeedError, std::abs(speed_ema_ - want) / want);
        }
    } else {
        input_held_s_ = 0.0f;
        has_speed_ema_ = false;
    }

    // ПРОГРЕВ: первые тики после сброса — прошлый корень и трекеры чужие
    // (стенд переставил тело, прибор только что включили); показания не судим.
    if (ticks_ <= WARM_TICKS) {
        for (auto& r : rows_) {
            r.last = 0.0f;
            r.worst = r.lower_is_bad ? 1.0e9f : 0.0f;
            r.hits = 0;
        }
        tap_episode_m_ = 0.0f;
        drift_.clear();
        drift_sum_ = 0.0f;
        cycle_has_ = {false, false};
        since_footfall_m_ = 0.0f;
        speed_ema_ = speed;
        input_held_s_ = 0.0f;
    }

    // строка CSV этого тика
    {
        char buf[640];
        const auto& c = *t.contacts;
        const auto& r = rows_;
        const auto v = [&](P p) {
            return static_cast<double>(r[static_cast<std::size_t>(p)].last);
        };
        std::snprintf(
            buf, sizeof(buf),
            "%.4f,%.4f,%.*s,%.3f,%.3f,%.3f,%.4f,%.4f,%.3f,%.3f,%d,%d,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,"
            "%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.1f,%.1f,%.3f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.4f,%.4f,%.4f,%.4f,%.4f",
            static_cast<double>(seconds_), static_cast<double>(dt),
            static_cast<int>(role_name(t.play->role).size()), role_name(t.play->role).data(),
            static_cast<double>(t.play->phase), static_cast<double>(want),
            static_cast<double>(speed), static_cast<double>(delta.x),
            static_cast<double>(delta.z), static_cast<double>(c.support[0]),
            static_cast<double>(c.support[1]), t.locks->locked[0] ? 1 : 0,
            t.locks->locked[1] ? 1 : 0, static_cast<double>(1000.0f * t.gap.gap[0]),
            static_cast<double>(1000.0f * t.gap.gap[1]),
            static_cast<double>(joints_[static_cast<std::size_t>(setup_.hip[0])].accel),
            static_cast<double>(joints_[static_cast<std::size_t>(setup_.hip[1])].accel),
            static_cast<double>(joints_[static_cast<std::size_t>(setup_.knee[0])].accel),
            static_cast<double>(joints_[static_cast<std::size_t>(setup_.knee[1])].accel),
            static_cast<double>(ankles_[0].accel), static_cast<double>(ankles_[1].accel),
            v(P::Residual), v(P::AnkleCross), v(P::LateralDrift), v(P::NoStepTravel),
            v(P::RootAccel), v(P::Twist), v(P::KneeBend), static_cast<double>(pelvis_yaw_deg_),
            static_cast<double>(toe_yaw_deg_[0]), static_cast<double>(toe_yaw_deg_[1]),
            static_cast<double>(frame_joints_[static_cast<std::size_t>(setup_.hip[0])].accel),
            static_cast<double>(frame_joints_[static_cast<std::size_t>(setup_.hip[1])].accel),
            static_cast<double>(frame_joints_[static_cast<std::size_t>(setup_.knee[0])].accel),
            static_cast<double>(frame_joints_[static_cast<std::size_t>(setup_.knee[1])].accel),
            static_cast<double>(t.locks->strength[0]), static_cast<double>(t.locks->strength[1]),
            static_cast<double>(mismatch_deg_[0]), static_cast<double>(mismatch_deg_[1]),
            t.release != nullptr && t.release->has[0]
                ? static_cast<double>(1000.0f * glm::length(t.release->offset[0])) : 0.0,
            t.release != nullptr && t.release->has[1]
                ? static_cast<double>(1000.0f * glm::length(t.release->offset[1])) : 0.0,
            static_cast<double>(c.point[0].x), static_cast<double>(c.point[0].z),
            static_cast<double>(c.point[1].x), static_cast<double>(c.point[1].z),
            static_cast<double>(t.loco->root_delta_model.z));
        csv_row_ = buf;
    }
}

void LocoTelemetry::push_frame(std::span<const JointLocal> sample, const BodyRoot& root,
                               float alpha, const FootLockState& locks) {
    if (!ready_ || ticks_ == 0 || sample.size() < skeleton_->size()) {
        return;
    }
    using P = LocoProbe;
    // часы кадра — от часов тика: конец прошлого тика + доля этого
    const float t = seconds_ - tick_dt_ + std::clamp(alpha, 0.0f, 1.0f) * tick_dt_;
    // КАДРЫ ЧАЩЕ ТИКА НЕ ДИФФЕРЕНЦИРУЕМ: производная по 4 мс (стенд рисует
    // четыре кадра на тик) раздувает любой шов тика в 16 раз; в игре кадр ≈ тик,
    // и цифры кадра сравнимы с цифрами клипа на тике.
    if (has_frame_t_ && t - frame_t_ < 0.9f * tick_dt_) {
        return;
    }
    const float fdt = has_frame_t_ ? t - frame_t_ : 0.0f;
    frame_t_ = t;
    has_frame_t_ = true;
    forward_kinematics(sample);
    // снос стопы в кадре: якорный сустав замка против якоря в мире
    const glm::mat4 to_world =
        glm::translate(glm::mat4{1.0f}, root.ground)
        * glm::rotate(glm::mat4{1.0f}, -root.yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    float worst = 0.0f;
    bool any = false;
    for (std::size_t s = 0; s < 2; ++s) {
        if (!locks.locked[s] || locks.strength[s] < 1.0f) {
            continue;
        }
        const int32_t j = locks.anchor_toe[s] && setup_.toe[s] >= 0 ? setup_.toe[s]
                                                                    : setup_.ankle[s];
        const glm::vec3 p =
            glm::vec3{to_world * glm::vec4{joint_pos(world_[static_cast<std::size_t>(j)]), 1.0f}};
        const glm::vec3 d = p - locks.anchor[s];
        worst = std::max(worst, glm::length(glm::vec2{d.x, d.z}));
        any = true;
    }
    if (any) {
        note(P::Slide, 1000.0f * worst);
    }
    // КАДР ПРОТИВ ТИКОВ: ожидание кадра — slerp двух тиков по alpha; всё сверх —
    // наш слой (IK, замок) или ошибка интерполяции. Незамкнутая нога на ровном
    // стенде обязана совпадать.
    {
        const float a = std::clamp(alpha, 0.0f, 1.0f);
        float worst = 0.0f;
        for (std::size_t s = 0; s < 2; ++s) {
            float side_worst = 0.0f;
            for (const int32_t j : {setup_.hip[s], setup_.knee[s]}) {
                const auto& tr = joints_[static_cast<std::size_t>(j)];
                if (!tr.has_rot) {
                    continue;
                }
                const glm::quat expect = glm::slerp(glm::normalize(tr.prev_rot),
                                                    glm::normalize(tr.rot), a);
                side_worst = std::max(
                    side_worst, quat_angle(expect, sample[static_cast<std::size_t>(j)].rotation));
            }
            mismatch_deg_[s] = side_worst * 180.0f / PI_F;
            worst = std::max(worst, mismatch_deg_[s]);
        }
        note(P::FrameMismatch, worst);
    }
    // рывки суставов по кадру — после IK и замка
    if (fdt > 1.0e-4f) {
        const auto accel = [&](int32_t j) {
            auto& tr = frame_joints_[static_cast<std::size_t>(j)];
            const glm::quat q = sample[static_cast<std::size_t>(j)].rotation;
            float a = 0.0f;
            if (tr.has_rot) {
                const float omega = quat_angle(tr.rot, q) / fdt;
                if (tr.has_omega) {
                    a = std::abs(omega - tr.omega) / fdt;
                }
                tr.omega = omega;
                tr.has_omega = true;
            }
            tr.rot = q;
            tr.has_rot = true;
            tr.accel = a;
            return a;
        };
        note(P::FrameThighAccel, std::max(accel(setup_.hip[0]), accel(setup_.hip[1])));
        note(P::FrameKneeAccel, std::max(accel(setup_.knee[0]), accel(setup_.knee[1])));
    } else {
        for (const int32_t j : {setup_.hip[0], setup_.hip[1], setup_.knee[0], setup_.knee[1]}) {
            auto& tr = frame_joints_[static_cast<std::size_t>(j)];
            tr.rot = sample[static_cast<std::size_t>(j)].rotation;
            tr.has_rot = true;
        }
    }
    if (ticks_ <= WARM_TICKS + 1) {
        for (const P p : {P::Slide, P::FrameThighAccel, P::FrameKneeAccel, P::FrameMismatch}) {
            auto& r = rows_[static_cast<std::size_t>(p)];
            r.last = 0.0f;
            r.worst = 0.0f;
            r.hits = 0;
        }
    }
}

std::string LocoTelemetry::csv_header() {
    return "t,dt,role,phase,want_mps,speed_mps,dx,dz,support_l,support_r,locked_l,locked_r,"
           "gap_l_mm,gap_r_mm,thigh_acc_l,thigh_acc_r,knee_acc_l,knee_acc_r,ankle_acc_l,"
           "ankle_acc_r,residual_mm,cross_mm,drift_mm,nostep,root_acc,twist_deg,knee_bend_deg,"
           "pelvis_yaw_deg,toe_yaw_l_deg,toe_yaw_r_deg,f_thigh_l,f_thigh_r,f_knee_l,f_knee_r,strength_l,strength_r,mismatch_l_deg,mismatch_r_deg,lock_corr_l_mm,lock_corr_r_mm,pt_l_x,pt_l_z,pt_r_x,pt_r_z,loco_dz";
}

std::vector<std::string> LocoTelemetry::summary_lines() const {
    using P = LocoProbe;
    const auto& r = [&](P p) -> const LocoProbeRow& {
        return rows_[static_cast<std::size_t>(p)];
    };
    char buf[160];
    std::vector<std::string> out;
    std::snprintf(buf, sizeof(buf),
                  "loco %.1fs hits %u  slide %.1f/%.1fmm(%u) resid %.1f gap %.0f/%.0f(%u)",
                  static_cast<double>(seconds_), total_hits(),
                  static_cast<double>(r(P::Slide).last), static_cast<double>(r(P::Slide).worst),
                  r(P::Slide).hits, static_cast<double>(r(P::Residual).worst),
                  static_cast<double>(r(P::Gap).last), static_cast<double>(r(P::Gap).worst),
                  r(P::Gap).hits);
    out.emplace_back(buf);
    std::snprintf(buf, sizeof(buf),
                  "acc clip thigh %.0f(%u) knee %.0f(%u) | frame %.0f(%u) %.0f(%u) mis %.1f(%u) | ankle %.0f(%u)",
                  static_cast<double>(r(P::ThighAccel).worst), r(P::ThighAccel).hits,
                  static_cast<double>(r(P::KneeAccel).worst), r(P::KneeAccel).hits,
                  static_cast<double>(r(P::FrameThighAccel).worst), r(P::FrameThighAccel).hits,
                  static_cast<double>(r(P::FrameKneeAccel).worst), r(P::FrameKneeAccel).hits,
                  static_cast<double>(r(P::FrameMismatch).worst), r(P::FrameMismatch).hits,
                  static_cast<double>(r(P::AnkleAccel).worst), r(P::AnkleAccel).hits);
    out.emplace_back(buf);
    std::snprintf(buf, sizeof(buf),
                  "drift %.0f(%u) tap %.0f(%u) nostep %.2f(%u) root_acc %.1f(%u) cross %.0f(%u)",
                  static_cast<double>(r(P::LateralDrift).worst), r(P::LateralDrift).hits,
                  static_cast<double>(r(P::TapDrift).worst), r(P::TapDrift).hits,
                  static_cast<double>(r(P::NoStepTravel).worst), r(P::NoStepTravel).hits,
                  static_cast<double>(r(P::RootAccel).worst), r(P::RootAccel).hits,
                  static_cast<double>(r(P::AnkleCross).worst), r(P::AnkleCross).hits);
    out.emplace_back(buf);
    std::snprintf(buf, sizeof(buf),
                  "phase %.2f(%u) twist %.0f(%u) knee %.0f(%u) spd %.0f%%(%u) start L%u R%u fall %u/%u",
                  static_cast<double>(r(P::PhaseJump).worst), r(P::PhaseJump).hits,
                  static_cast<double>(r(P::Twist).worst), r(P::Twist).hits,
                  static_cast<double>(r(P::KneeBend).worst < 1.0e8f ? r(P::KneeBend).worst : 0.0f),
                  r(P::KneeBend).hits, static_cast<double>(100.0f * r(P::SpeedError).worst),
                  r(P::SpeedError).hits, start_foot_[0], start_foot_[1], footfalls_[0],
                  footfalls_[1]);
    out.emplace_back(buf);
    return out;
}

std::string LocoTelemetry::report() const {
    std::string s;
    char buf[200];
    std::snprintf(buf, sizeof(buf),
                  "[loco] %u ticks %.1f s, role changes %u, footfalls L %u R %u, "
                  "start foot L %u R %u, hits %u\n",
                  ticks_, static_cast<double>(seconds_), role_changes_, footfalls_[0],
                  footfalls_[1], start_foot_[0], start_foot_[1], total_hits());
    s += buf;
    std::snprintf(buf, sizeof(buf), "[loco] %-10s %-7s %10s %10s %6s\n", "probe", "unit", "worst",
                  "limit", "hits");
    s += buf;
    for (const auto& r : rows_) {
        const float worst = r.lower_is_bad && r.worst > 1.0e8f ? 0.0f : r.worst;
        std::snprintf(buf, sizeof(buf), "[loco] %-10.*s %-7.*s %10.3f %10.3f %6u%s\n",
                      static_cast<int>(r.name.size()), r.name.data(),
                      static_cast<int>(r.unit.size()), r.unit.data(),
                      static_cast<double>(worst), static_cast<double>(r.limit), r.hits,
                      r.hits > 0 ? "  <-- RED" : "");
        s += buf;
    }
    return s;
}

} // namespace dfn::anim
