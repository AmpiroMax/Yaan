/*
Module: engine/anim
File: engine/anim/sources/LocoTelemetry.h

Responsibility:
- ПРИБОРЫ ЛОКОМОЦИИ В ИГРЕ (владелец 04.09: «делать демки через прогон игры и
  скрины — не поможет, нужны инструменты внутри игры, что числами расскажут
  о проблемах; не стесняйся, надо много детектить»). Один тик тела →
  детекторы; каждый детектор ведёт худшее значение, число срабатываний и
  порог из реестра (docs/NUMBERS.md, «Приборы локомоции»). Итог — строки на
  экране (DFN_LOCO_HUD), таблица в stderr и CSV по тикам (DFN_LOCO_CSV).
- Детекторы: снос замкнутой стопы; зазор опорной стопы (парит/утонула);
  угловое ускорение бедра/колена/стопы (рывок при отрыве); линейное
  ускорение лодыжки в мире; перекрест лодыжек; боковой снос за окно
  («змейка»); снос без ввода (короткое нажатие); путь без постановки стопы;
  скачок скорости корня; разрыв фазы на смене роли; скрутка таза к стопам;
  минимальный сгиб колена; ошибка заказанной скорости; с какой ноги старт.

Key items:
- LocoTick: всё, что прибору нужно от одного тика — поза тика (локальные
  TRS), контакты, замки, зазор, проигрывание, заявка, ввод, корень до и
  после подтверждения. Плоские данные и указатели, без владения.
- LocoTelemetry: reset() по скелету, push() на тик, rows() — таблица
  детекторов, summary_lines() — четыре строки для экрана, csv_header() /
  csv_row() — строка тика.
- LocoProbe: перечень детекторов; порядок = порядок строк таблицы.

Dependencies:
- Uses: FootIk.h, RootMotion.h, ClipPlayer.h, Body.h, Pose.h, SkinnedBody.h
  (JointLocal), skeleton, glm.
- Used by: engine/app (SkinnedCharacter кормит, App показывает), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Пороги — строки реестра, не литералы; прибор ничего не правит в позе —
  только читает (правило 30).
*/
#pragma once

#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/ClipPlayer.h"
#include "engine/anim/sources/FootIk.h"
#include "engine/anim/sources/Pose.h"
#include "engine/anim/sources/RootMotion.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/skeleton/sources/Skeleton.h"

#include <array>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {

/// Всё, что прибор читает за один тик. Указатели живут только на время push().
struct LocoTick {
    float dt = 0.0f;
    std::span<const JointLocal> pose;          ///< поза тика (локальные TRS суставов)
    const ContactState* contacts = nullptr;
    const FootLockState* locks = nullptr;
    const FootLockRelease* release = nullptr;  ///< поправка замка последнего кадра (может быть null)
    std::array<glm::vec3, 2> contact_world{};  ///< точки касания в мире
    FootGap gap{};
    const ClipPlayback* play = nullptr;
    const LocomotionOut* loco = nullptr;
    const BodyDrive* drive = nullptr;
    BodyRoot root{};      ///< корень этого тика, подтверждённый
    BodyRoot root_prev{}; ///< корень прошлого тика
};

enum class LocoProbe : uint8_t {
    Slide = 0,     ///< снос замкнутой стопы к якорю В КАДРЕ (после IK и замка), мм
    Residual,      ///< остаток до замка на тике (что замку закрывать), мм — показание
    Gap,           ///< зазор опорной стопы, мм (модуль)
    ThighAccel,    ///< угловое ускорение бедра в клипе (до IK), рад/с²
    KneeAccel,     ///< угловое ускорение колена в клипе, рад/с²
    FootAccel,     ///< угловое ускорение стопы в клипе, рад/с²
    FrameThighAccel, ///< то же по кадру — после IK и замка; разница = наш слой
    FrameKneeAccel,
    FrameMismatch,   ///< кадр против интерполяции двух тиков (бедро/колено), град
    AnkleAccel,    ///< линейное ускорение лодыжки в мире, м/с²
    AnkleCross,    ///< перекрест лодыжек поперёк тела, мм
    LateralDrift,  ///< боковой снос за цикл (постановка той же стопы), мм
    TapDrift,      ///< путь без ввода за эпизод, мм
    NoStepTravel,  ///< путь без постановки стопы, доля от двух шагов
    RootAccel,     ///< ускорение корня, м/с²
    PhaseJump,     ///< разрыв фазы на смене роли, доля цикла
    Twist,         ///< скрутка стопы (носок−лодыжка) к тазу (перпендикуляр линии бёдер), град
    KneeBend,      ///< минимальный сгиб колена под опорой, град
    SpeedError,    ///< ошибка скорости при удержанном вводе, доля
    COUNT
};

struct LocoProbeRow {
    std::string_view name;
    std::string_view unit;
    float worst = 0.0f;   ///< худшее значение за всё время
    float last = 0.0f;    ///< значение последнего тика
    float limit = 0.0f;   ///< порог реестра; 0 — только показание
    uint32_t hits = 0;    ///< тиков сверх порога
    bool lower_is_bad = false; ///< порог — пол (сгиб колена)
};

class LocoTelemetry {
public:
    void reset(const skel::Skeleton& skeleton, const FootIkSetup& setup);
    [[nodiscard]] bool ready() const { return ready_; }
    void push(const LocoTick& tick);
    /// Кадр после IK и замка: снос стопы к якорю и рывки суставов уже с нашим
    /// слоем. root — корень кадра (интерполированный), alpha — доля тика.
    void push_frame(std::span<const JointLocal> sample, const BodyRoot& root, float alpha,
                    const FootLockState& locks);

    [[nodiscard]] std::span<const LocoProbeRow> rows() const { return rows_; }
    [[nodiscard]] const LocoProbeRow& row(LocoProbe p) const {
        return rows_[static_cast<std::size_t>(p)];
    }
    [[nodiscard]] uint32_t ticks() const { return ticks_; }
    [[nodiscard]] float seconds() const { return seconds_; }
    [[nodiscard]] std::array<uint32_t, 2> footfalls() const { return footfalls_; }
    [[nodiscard]] std::array<uint32_t, 2> start_foot() const { return start_foot_; }
    [[nodiscard]] uint32_t role_changes() const { return role_changes_; }
    [[nodiscard]] uint32_t total_hits() const;

    /// Первые тики после сброса: корень и трекеры ещё без прошлого — не судим.
    static constexpr uint32_t WARM_TICKS = 2;
    /// Четыре короткие строки для экрана.
    [[nodiscard]] std::vector<std::string> summary_lines() const;
    /// Таблица детекторов для stderr.
    [[nodiscard]] std::string report() const;
    [[nodiscard]] static std::string csv_header();
    [[nodiscard]] const std::string& csv_row() const { return csv_row_; }

private:
    struct JointTrack {
        glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
        glm::quat prev_rot{1.0f, 0.0f, 0.0f, 0.0f}; ///< тик назад (для ожидания кадра)
        float omega = 0.0f;
        float accel = 0.0f;
        bool has_rot = false;
        bool has_omega = false;
    };
    struct PointTrack {
        glm::vec3 pos{0.0f};
        glm::vec3 vel{0.0f};
        float accel = 0.0f;
        bool has_pos = false;
        bool has_vel = false;
    };
    struct DriftSample {
        float t = 0.0f;
        float lateral = 0.0f;
    };

    void note(LocoProbe p, float value);
    void forward_kinematics(std::span<const JointLocal> pose);

    bool ready_ = false;
    const skel::Skeleton* skeleton_ = nullptr;
    FootIkSetup setup_{};
    int32_t pelvis_ = -1;
    float cross_sign_ = 1.0f; ///< знак (x_L − x_R) лодыжек в покое
    float pelvis_fwd_sign_ = 1.0f; ///< перпендикуляр линии бёдер, глядящий по носкам покоя
    std::vector<glm::mat4> world_;
    std::array<LocoProbeRow, static_cast<std::size_t>(LocoProbe::COUNT)> rows_{};

    uint32_t ticks_ = 0;
    float seconds_ = 0.0f;
    std::vector<JointTrack> joints_;
    std::vector<JointTrack> frame_joints_;
    float frame_t_ = 0.0f;   ///< часы последнего ПРИНЯТОГО кадра (реже полтика — пропуск)
    bool has_frame_t_ = false;
    float pelvis_yaw_deg_ = 0.0f;
    std::array<float, 2> mismatch_deg_{}; ///< кадр против тиков, бедро+колено, по сторонам
    std::array<float, 2> toe_yaw_deg_{};
    float tick_dt_ = 0.0f;
    std::array<PointTrack, 2> ankles_{};
    glm::vec3 vel_prev_{0.0f};
    bool has_vel_prev_ = false;

    std::deque<DriftSample> drift_; ///< боковые смещения окна (для CSV/сводки)
    float drift_sum_ = 0.0f;
    std::array<float, 2> cycle_lateral_{}; ///< боковая координата на прошлой постановке стопы
    std::array<bool, 2> cycle_has_{};
    glm::vec3 travel_dir_{0.0f, 0.0f, -1.0f}; ///< последний ненулевой ввод, мир
    bool has_travel_dir_ = false;

    float tap_episode_m_ = 0.0f;
    bool tap_open_ = false;
    float since_footfall_m_ = 0.0f;
    std::array<uint32_t, 2> footfalls_{};
    std::array<uint32_t, 2> start_foot_{};
    bool start_pending_ = false;
    std::array<bool, 2> planted_{};

    ClipRole role_prev_ = ClipRole::Idle;
    bool has_role_ = false;
    float phase_prev_ = 0.0f;
    uint32_t role_changes_ = 0;

    float input_held_s_ = 0.0f;
    float speed_ema_ = 0.0f;
    bool has_speed_ema_ = false;

    std::string csv_row_;
};

} // namespace dfn::anim
