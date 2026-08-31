/*
Module: engine/anim
File: engine/anim/sources/PoseLibrary.cpp

Responsibility:
- САМ РЕЕСТР (таблица POSES) и три решателя, которыми его строки становятся
  позой: двузвенник руки под цель кисти, двузвенник ноги под опущенный таз и
  сложение углов в кватернион. Плюс граф переходов и его проигрывание.

Dependencies:
- Uses: PoseLibrary.h, Posture.h (сидение на сиденье считается ТОЙ ЖЕ
  функцией, что и посадка на лавку), Clips.h (HEAD_STABILIZE), glm.
- Used by: Body.cpp, engine/app, tests/character/PoseLibraryTests.cpp.

Notes:
- ТАБЛИЦА СТОИТ ПЕРВОЙ И ЧИТАЕТСЯ КАК ТАБЛИЦА. Всё, что ниже, — механизм; всё,
  что в ней, — данные. Правка позы это правка ОДНОЙ СТРОКИ, и это то самое
  «данные, а не код», которого просит заказ.
- ЦЕЛЬ КИСТИ РЕШАЕТСЯ, А НЕ ПОДБИРАЕТСЯ. Двузвенник с известными длинами
  имеет замкнутое решение: сгиб локтя однозначен по расстоянию (теорема
  косинусов), а плечо доворачивается КРАТЧАЙШЕЙ ДУГОЙ от прямой руки к цели.
  Остаётся ровно одна свобода — вращение вокруг оси плечо-кисть, — и она
  вынесена в реестр отдельным числом (swivel), потому что именно она отличает
  «руки в бока» от «руки перед собой» при одной и той же точке кисти.
- ПОЧЕМУ ЛОКОТЬ ВСЕГДА ПОЛОЖИТЕЛЬНЫЙ ТАНГАЖ. Предел рига (BODY_ELBOW_HYPEREXT_MAX
  = 0) запрещает локтю разгибаться назад, поэтому у решателя нет выбора знака:
  сгиб идёт в единственную законную сторону, и поза, пришедшая из этого файла,
  проходит apply_joint_limits без изменений. Это проверяется тестом, а не
  обещается здесь.
- СИДЕНИЕ НА СИДЕНЬЕ НЕ ПЕРЕПИСАНО ЗДЕСЬ ВТОРОЙ РАЗ. Поза стула берётся у
  anim::sit_pose — той же функции, которой садится игрок на лавку. Второй
  набор углов «для стенда» разошёлся бы с игровым в первый же день, а на вид
  разницы никто бы не заметил (правило 35).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions and plain data (Rule 8): no ECS, no IO, no clock.
*/

#include "engine/anim/sources/PoseLibrary.h"

#include "engine/anim/sources/Clips.h"
#include "engine/anim/sources/Posture.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {
namespace {

constexpr float DEG = 0.01745329252f;

// --- ТАБЛИЦА РЕЕСТРА ------------------------------------------------------
//
// ЕДИНИЦА ДЛИН — СТОЯЧАЯ ВЫСОТА БЕДРА (RigProportions::standing_hip_height,
// 0.947 м у капсулы 1.8). Все точки кистей — в ней, в системе таза.
// Знаки — АБСОЛЮТНЫЕ, как в docs/RIG.md: +X это правая сторона фигуры, +Y
// вверх, +Z назад. Зеркальная строка (symmetric) меняет знак у X цели, у
// закрутки локтя и у рыска с креном бедра.

// ВЫСОТА СИДЕНЬЯ, НА КОТОРОЙ РЕЕСТР РИСУЕТ СТУЛ, м. Замер лавки с самого
// чертежа (assets/houses/INDEX.txt, furn-bench: настил 0.4225 + полтолщины
// 0.055/2 = 0.45) — не выбранное число, а то же, которым садится игрок.
// Живому предмету высота приходит параметром pose_of(): реестр знает
// ОБРАЗЦОВОЕ сиденье, мир знает своё.
constexpr float DEFAULT_SEAT_M = 0.45f;

constexpr ArmTarget none_arm() { return ArmTarget{}; }
constexpr ArmTarget arm_at(float x, float y, float z, float swivel_deg) {
    return ArmTarget{.used = true, .at = {x, y, z}, .swivel_deg = swivel_deg};
}

const std::array<PoseRecord, POSE_COUNT> POSES = {{
    // --- ОПОРА 1: СТОЯ -----------------------------------------------------
    // Руки не заданы: покой рига И ЕСТЬ опущенная рука, и цель, поставленная
    // туда же, была бы вторым описанием того же места.
    PoseRecord{.name = "stand", .label = "стоя", .support = Support::Stand},

    // --- ОПОРА 2: ПРИСЯД ---------------------------------------------------
    // Таз на 0.54 стоячей высоты бедра — ровно половина ноги вниз, глубина
    // присяда локомоции (Clips.cpp, crouch_pelvis_drop). Ноги ВЕДОМЫЕ:
    // двузвенник решается под эту высоту, оттого стопы стоят на полу и на
    // фигуре с другой ногой.
    PoseRecord{.name = "crouch", .label = "присяд", .support = Support::Crouch,
               .hip_frac = 0.54f, .torso_pitch_deg = 25.0f, .head_pitch_deg = -15.0f,
               .fold_legs = true,
               .arm = {{arm_at(-0.21f, -0.10f, -0.28f, 20.0f),
                        arm_at(0.21f, -0.10f, -0.28f, 20.0f)}}},

    // --- ОПОРА 3: СИДЯ НА ПОЛУ, НОГИ ВПЕРЁД ---------------------------------
    // Таз на полу: 0.10 доли — полтолщины ноги над землёй. Корпус чуть
    // отклонён назад, руки в упоре позади таза, носки на себя.
    PoseRecord{.name = "sit_floor", .label = "сидя на полу, ноги вперёд",
               .support = Support::SitFloor, .source_clip = "Sit_Floor_Pose",
               .hip_frac = 0.13f, .torso_pitch_deg = -10.0f,
               .leg = {{LegAngles{.thigh_pitch_deg = 85.0f, .knee_deg = 5.0f,
                                  .ankle_deg = 15.0f},
                        LegAngles{}}},
               .arm = {{arm_at(-0.28f, -0.06f, 0.17f, 25.0f),
                        arm_at(0.28f, -0.06f, 0.17f, 25.0f)}}},

    // --- ОПОРА 4: ЛЁЖА НА ЖИВОТЕ, РУКИ ПО ШВАМ ------------------------------
    // Таз повёрнут на -90° вокруг X: местное «вверх» уходит в местное -Z, то
    // есть лицо смотрит в пол (обратный поворот к тому, которым Posture.h
    // кладёт на спину). Руки НЕ ЗАДАНЫ — и это не лень: висящая от плеча по
    // -Y рука после того же поворота ложится вдоль тела к ногам сама. «Руки
    // по швам» здесь не поза, а следствие поворота.
    PoseRecord{.name = "lie_prone", .label = "лёжа на животе, руки по швам",
               .support = Support::LieProne, .source_clip = "Lie_Pose",
               .hip_frac = 0.13f, .pelvis_pitch_deg = -90.0f},

    // --- ГРАЖДАНСКИЕ СТОЯЧИЕ ------------------------------------------------
    // РУКИ В БОКА — референс владельца. Кисть на гребне подвздошной кости:
    // вбок на полширины таза с запасом на толщину руки, чуть выше таза,
    // чуть назад. Локти разведены наружу закруткой.
    PoseRecord{.name = "hands_on_hips", .label = "руки в бока",
               .support = Support::Stand,
               .arm = {{arm_at(-0.26f, 0.11f, 0.03f, 55.0f),
                        arm_at(0.26f, 0.11f, 0.03f, 55.0f)}}},

    // РУКИ ЗА СПИНОЙ. Кисти сведены к средней линии позади крестца; локти
    // разведены наружу, иначе руки складываются внутрь корпуса.
    PoseRecord{.name = "hands_behind_back", .label = "стоя, руки за спиной",
               .support = Support::Stand,
               .arm = {{arm_at(-0.06f, 0.12f, 0.24f, 35.0f),
                        arm_at(0.06f, 0.12f, 0.24f, 35.0f)}}},

    // РУКИ ЗА ГОЛОВОЙ, ЛЁГКИЙ НАКЛОН («задержанный»). Кисти за затылком,
    // локти широко; корпус вперёд на 12°, голова следом.
    PoseRecord{.name = "hands_behind_head", .label = "руки за головой, наклон",
               .support = Support::Stand, .torso_pitch_deg = 12.0f,
               .head_pitch_deg = 8.0f,
               .arm = {{arm_at(-0.13f, 0.80f, 0.14f, 70.0f),
                        arm_at(0.13f, 0.80f, 0.14f, 70.0f)}}},

    // «СОЛДАТИК». Руки прижаты к бёдрам, кисть на ладонь ближе к телу, чем в
    // покое; корпус отвесно, взгляд в горизонт.
    PoseRecord{.name = "attention", .label = "«солдатик»", .support = Support::Stand,
               .arm = {{arm_at(-0.19f, -0.08f, 0.01f, -5.0f),
                        arm_at(0.19f, -0.08f, 0.01f, -5.0f)}}},

    // ЛЕВАЯ ВВЕРХ / ПРАВАЯ ВВЕРХ / ОБЕ ВВЕРХ. Несимметричные строки заданы
    // обеими сторонами явно: у зеркала нет способа сказать «а вторая как
    // была».
    PoseRecord{.name = "arm_up_left", .label = "левая рука вверх",
               .support = Support::Stand, .symmetric = false,
               .arm = {{arm_at(-0.26f, 1.15f, 0.0f, 0.0f), none_arm()}}},
    PoseRecord{.name = "arm_up_right", .label = "правая рука вверх",
               .support = Support::Stand, .symmetric = false,
               .arm = {{none_arm(), arm_at(0.26f, 1.15f, 0.0f, 0.0f)}}},
    PoseRecord{.name = "arms_up", .label = "обе руки вверх", .support = Support::Stand,
               .arm = {{arm_at(-0.30f, 1.14f, 0.0f, 0.0f),
                        arm_at(0.30f, 1.14f, 0.0f, 0.0f)}}},

    // --- НИЗКИЕ -------------------------------------------------------------
    // ПРИСЕД С КАСАНИЕМ РУКАМИ ПОЛА. Таз на 0.40 — глубже опорного присяда, и
    // это ПОТОЛОК: ниже колену пришлось бы согнуться сильнее 140°
    // (BODY_KNEE_FLEX_MAX), то есть поза упёрлась бы в предел и стала
    // неотличима от зажатой. Пол в системе таза лежит ровно на -hip_frac.
    PoseRecord{.name = "squat_touch", .label = "присед, руки на полу",
               .support = Support::Crouch, .hip_frac = 0.40f,
               .torso_pitch_deg = 68.0f, .head_pitch_deg = -35.0f, .fold_legs = true,
               .arm = {{arm_at(-0.22f, -0.40f, -0.28f, 15.0f),
                        arm_at(0.22f, -0.40f, -0.28f, 15.0f)}}},

    // УПОР ЛЁЖА. Тело — прямая наклонная: таз на 0.53 (плечо тогда стоит на
    // прямой руке), наклон -80° вместо -90° и есть те десять градусов, на
    // которые плечи выше носков. Кисти — под плечами, «вниз» в системе тела
    // это -Z (см. опору 4). Носки оттянуты: стоят на подушечках.
    PoseRecord{.name = "push_up", .label = "упор лёжа", .support = Support::LieProne,
               .hip_frac = 0.44f, .pelvis_pitch_deg = -65.0f,
               .leg = {{LegAngles{.ankle_deg = -50.0f}, LegAngles{}}},
               .arm = {{arm_at(-0.25f, 0.55f, -0.622f, 0.0f),
                        arm_at(0.25f, 0.55f, -0.622f, 0.0f)}}},

    // ПЛАНКА. То же тело, но опора на предплечья, поэтому плечо ниже на
    // длину плеча и таз с ним: 0.32. Кисть вынесена ВПЕРЁД от плеча — тем и
    // отличается предплечье, лежащее на полу, от прямой руки.
    PoseRecord{.name = "plank", .label = "планка", .support = Support::LieProne,
               .hip_frac = 0.34f, .pelvis_pitch_deg = -72.0f,
               .leg = {{LegAngles{.ankle_deg = -50.0f}, LegAngles{}}},
               .arm = {{arm_at(-0.25f, 0.83f, -0.36f, 0.0f),
                        arm_at(0.25f, 0.83f, -0.36f, 0.0f)}}},

    // СИДЯ СКРЕСТНО. Бёдра разведены рыском (вокруг отвеса), а не креном:
    // нога, поднятая тангажом почти в горизонт, креном уже не разводится.
    PoseRecord{.name = "sit_cross", .label = "сидя скрестно",
               .support = Support::SitFloor, .hip_frac = 0.17f,
               .leg = {{LegAngles{.thigh_pitch_deg = 88.0f, .thigh_yaw_deg = 45.0f,
                                  .thigh_roll_deg = 90.0f, .knee_deg = 110.0f},
                        LegAngles{}}},
               .arm = {{arm_at(-0.26f, -0.02f, -0.24f, 20.0f),
                        arm_at(0.26f, -0.02f, -0.24f, 20.0f)}}},

    // СИДЯ, НОГИ ВРОЗЬ. Та же опора, ноги прямые и разведены на 30°.
    PoseRecord{.name = "sit_apart", .label = "сидя, ноги врозь",
               .support = Support::SitFloor, .hip_frac = 0.10f,
               .torso_pitch_deg = -6.0f,
               .leg = {{LegAngles{.thigh_pitch_deg = 84.0f, .thigh_yaw_deg = -30.0f,
                                  .knee_deg = 6.0f, .ankle_deg = 10.0f},
                        LegAngles{}}},
               .arm = {{arm_at(-0.30f, -0.06f, 0.17f, 25.0f),
                        arm_at(0.30f, -0.06f, 0.17f, 25.0f)}}},

    // ШПАГАТ ПРОДОЛЬНЫЙ. Строка несимметрична по существу: левая нога
    // вперёд на 90°, правая назад на 85°. Тазобедренный у нас свободная
    // кость, поэтому разгибание назад выразимо; колени прямые.
    PoseRecord{.name = "split_front", .label = "шпагат продольный",
               .support = Support::SitFloor, .hip_frac = 0.09f, .symmetric = false,
               .leg = {{LegAngles{.thigh_pitch_deg = 90.0f},
                        LegAngles{.thigh_pitch_deg = -85.0f, .ankle_deg = -25.0f}}},
               .arm = {{arm_at(-0.55f, 0.50f, 0.0f, 0.0f),
                        arm_at(0.55f, 0.50f, 0.0f, 0.0f)}}},

    // ШПАГАТ ПОПЕРЕЧНЫЙ. Здесь тангаж нулевой, и разводит именно КРЕН —
    // единственный случай в реестре, ради которого крен и оставлен.
    PoseRecord{.name = "split_side", .label = "шпагат поперечный",
               .support = Support::SitFloor, .hip_frac = 0.09f,
               .leg = {{LegAngles{.thigh_roll_deg = -97.0f}, LegAngles{}}},
               .arm = {{arm_at(-0.55f, 0.50f, 0.0f, 0.0f),
                        arm_at(0.55f, 0.50f, 0.0f, 0.0f)}}},

    // --- СТУЛ ---------------------------------------------------------------
    // Обе строки берут НОГИ И ТАЗ у anim::sit_pose — той же функции, которой
    // игрок садится на лавку (см. Notes). Здесь заданы только руки и добавка
    // к наклону корпуса.
    PoseRecord{.name = "sit_chair_table", .label = "сидя на стуле, руки на столе",
               .support = Support::Crouch, .source_clip = "Sit_Chair_Pose",
               .torso_pitch_deg = 6.0f, .on_seat = true,
               .arm = {{arm_at(-0.20f, 0.34f, -0.42f, 10.0f),
                        arm_at(0.20f, 0.34f, -0.42f, 10.0f)}}},
    PoseRecord{.name = "sit_chair_knees", .label = "сидя на стуле, руки на коленях",
               .support = Support::Crouch, .source_clip = "Sit_Chair_Pose",
               .torso_pitch_deg = 14.0f, .on_seat = true,
               .arm = {{arm_at(-0.20f, 0.10f, -0.38f, 15.0f),
                        arm_at(0.20f, 0.10f, -0.38f, 15.0f)}}},

    // --- ОРУЖЕЙНЫЕ СТОЙКИ ---------------------------------------------------
    // ВЕРХ ТЕЛА ТОЛЬКО: ноги при них ходят обычной ходьбой, и разделение
    // делает маска ветвей (PoseLayers.h). Поэтому у всех трёх ноги пусты — не
    // «стоят», а НЕ УЧАСТВУЮТ.
    //
    // ОДНОРУЧНЫЙ. Заказ владельца дословно: «ведущая рука с клинком вбок-вниз,
    // левая полусогнута». Правая кисть вынесена вбок и вниз от плеча, левая
    // держится перед грудью полусогнутой.
    PoseRecord{.name = "guard_1h", .label = "стойка: одноручный",
               .support = Support::Stand, .source_clip = "Idle",
               .torso_pitch_deg = 8.0f, .symmetric = false, .upper_only = true,
               .arm = {{arm_at(-0.22f, 0.30f, -0.30f, 25.0f),
                        arm_at(0.44f, 0.08f, -0.20f, 30.0f)}}},
    // ДВУРУЧНЫЙ. Обе кисти сведены у правого плеча, клинок наискось — поза,
    // которую держит клип 2H_Melee_Idle открытой библиотеки.
    PoseRecord{.name = "guard_2h", .label = "стойка: двуручный",
               .support = Support::Stand, .source_clip = "2H_Melee_Idle",
               .torso_pitch_deg = 10.0f, .symmetric = false, .upper_only = true,
               .arm = {{arm_at(0.20f, 0.38f, -0.42f, 20.0f),
                        arm_at(0.30f, 0.50f, -0.30f, 25.0f)}}},
    // ЛУК. Левая рука вытянута вперёд (держит лук), правая у подбородка
    // (тетива) — поза клипа 2H_Ranged_Aiming.
    PoseRecord{.name = "guard_bow", .label = "стойка: лук",
               .support = Support::Stand, .source_clip = "2H_Ranged_Aiming",
               .torso_pitch_deg = 4.0f, .symmetric = false, .upper_only = true,
               .arm = {{arm_at(-0.28f, 0.54f, -0.58f, 0.0f),
                        arm_at(0.05f, 0.72f, 0.05f, 60.0f)}}},
}};

// --- МЕХАНИЗМ -------------------------------------------------------------

/// КРАТЧАЙШАЯ ДУГА МЕЖДУ ДВУМЯ НАПРАВЛЕНИЯМИ. Своя, а не glm::rotation из
/// gtx: экспериментальные заголовки glm требуют флага сборки, а восемь строк
/// не стоят флага, который придётся объяснять каждой зоне (правило 2).
[[nodiscard]] glm::quat arc_between(const glm::vec3& a, const glm::vec3& b) {
    const float d = glm::dot(a, b);
    if (d > 0.999999f) {
        return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    }
    if (d < -0.999999f) {
        // Противонаправленные: ось — любая перпендикулярная, и «любая» здесь
        // законна ровно потому, что поворот на 180° одинаков вокруг всех них.
        glm::vec3 axis = glm::cross(glm::vec3{1.0f, 0.0f, 0.0f}, a);
        if (glm::dot(axis, axis) < 1.0e-6f) {
            axis = glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, a);
        }
        return glm::angleAxis(3.14159265f, glm::normalize(axis));
    }
    const glm::vec3 axis = glm::cross(a, b);
    return glm::normalize(glm::quat{1.0f + d, axis.x, axis.y, axis.z});
}

[[nodiscard]] glm::quat pitch_q(float rad) {
    return glm::angleAxis(rad, glm::vec3{1.0f, 0.0f, 0.0f});
}

/// ЗЕРКАЛО ОДНОЙ СТОРОНЫ. Знак меняют ровно те три величины, у которых
/// «сторона» есть: X цели, закрутка локтя и обе горизонтальные оси бедра.
[[nodiscard]] ArmTarget mirrored(const ArmTarget& a) {
    return ArmTarget{.used = a.used,
                     .at = {-a.at.x, a.at.y, a.at.z},
                     .swivel_deg = -a.swivel_deg};
}
[[nodiscard]] LegAngles mirrored(const LegAngles& l) {
    return LegAngles{.thigh_pitch_deg = l.thigh_pitch_deg,
                     .thigh_yaw_deg = -l.thigh_yaw_deg,
                     .thigh_roll_deg = -l.thigh_roll_deg,
                     .knee_deg = l.knee_deg,
                     .ankle_deg = l.ankle_deg};
}

/// ДВУЗВЕННИК РУКИ ПОД ЦЕЛЬ КИСТИ. `target_torso` — точка в системе ТУЛОВИЩА
/// (метры). Возвращает поворот плеча и сгиб локтя.
struct ArmSolution {
    glm::quat shoulder{1.0f, 0.0f, 0.0f, 0.0f};
    float elbow_rad = 0.0f;
};
[[nodiscard]] ArmSolution solve_arm(float upper, float forearm,
                                    const glm::vec3& shoulder_offset,
                                    const glm::vec3& target_torso, float swivel_rad) {
    ArmSolution s;
    const glm::vec3 v = target_torso - shoulder_offset;
    // НЕДОСЯГАЕМАЯ ЦЕЛЬ НЕ ОШИБКА, А ПРЯМАЯ РУКА. Зажим здесь честнее отказа:
    // поза, у которой кисть чуть дальше руки, обязана нарисоваться вытянутой,
    // а не пропасть.
    const float reach = upper + forearm;
    const float d = std::clamp(glm::length(v), 0.05f, reach - 1.0e-3f);
    const float cos_e =
        std::clamp((d * d - upper * upper - forearm * forearm) / (2.0f * upper * forearm),
                   -1.0f, 1.0f);
    s.elbow_rad = std::acos(cos_e);
    // Где кисть окажется у НЕПОВЁРНУТОГО плеча с этим локтем: рука висит по
    // -Y, локоть уводит предплечье вперёд (-Z).
    const glm::vec3 rest{0.0f, -upper - forearm * cos_e,
                         -forearm * std::sin(s.elbow_rad)};
    const glm::vec3 want = glm::normalize(v) * d;
    s.shoulder = arc_between(glm::normalize(rest), glm::normalize(want));
    if (std::abs(swivel_rad) > 1.0e-4f) {
        s.shoulder = glm::angleAxis(swivel_rad, glm::normalize(want)) * s.shoulder;
    }
    return s;
}

[[nodiscard]] PoseId support_pose(Support s) {
    // Первые четыре записи реестра — сами опоры, в порядке цепи. Проверено
    // тестом, а не обещано здесь.
    return static_cast<PoseId>(static_cast<uint8_t>(s));
}

[[nodiscard]] bool adjacent(Support a, Support b) {
    const int ia = static_cast<int>(a);
    const int ib = static_cast<int>(b);
    return ia - ib == 1 || ib - ia == 1;
}

} // namespace

const PoseRecord& pose_record(PoseId p) {
    const uint32_t i = pose_index(p);
    return POSES[i < POSE_COUNT ? i : 0];
}

std::string_view pose_name(PoseId p) { return pose_record(p).name; }

Support pose_support(PoseId p) { return pose_record(p).support; }

PoseId pose_by_name(std::string_view name, bool* found) {
    for (uint32_t i = 0; i < POSE_COUNT; ++i) {
        if (POSES[i].name == name) {
            if (found != nullptr) {
                *found = true;
            }
            return static_cast<PoseId>(i);
        }
    }
    if (found != nullptr) {
        *found = false;
    }
    return PoseId::Stand;
}

LocalPose pose_of(const Rig& rig, PoseId p) { return pose_of(rig, p, DEFAULT_SEAT_M); }

LocalPose pose_of(const Rig& rig, PoseId p, float seat_height_m) {
    const PoseRecord& r = pose_record(p);
    const RigProportions& pr = rig.proportions;
    const float unit = pr.standing_hip_height();

    LocalPose pose;

    // --- ТАЗ И НОГИ --------------------------------------------------------
    // СИДЕНЬЕ СЧИТАЕТСЯ ЧУЖОЙ ФУНКЦИЕЙ, И В ЭТОМ ВЕСЬ СМЫСЛ: у стула тут нет
    // ни одного собственного угла ноги.
    const bool on_seat = r.on_seat;
    float hip_m = unit;
    if (on_seat) {
        pose = sit_pose(rig, seat_height_m);
        hip_m = unit + pose.pelvis_offset.y;
    } else {
        hip_m = r.hip_frac * unit;
        pose.pelvis_offset.y = hip_m - unit;
        pose.rotation[bone_index(Bone::Pelvis)] = pitch_q(r.pelvis_pitch_deg * DEG);
        if (r.fold_legs) {
            // ВЕДОМЫЕ НОГИ: тот же двузвенник, что у apply_crouch, — бедро и
            // голень складываются так, чтобы лодыжка осталась под тазом на
            // своей стоячей высоте.
            const float span = pr.thigh_length() + pr.shin_length();
            const float cos_a =
                std::clamp((hip_m - pr.ankle_height) / std::max(0.01f, span), 0.05f, 1.0f);
            const float a = std::acos(cos_a);
            for (int side = 0; side < 2; ++side) {
                const Bone thigh = side == 0 ? Bone::ThighL : Bone::ThighR;
                const Bone shin = side == 0 ? Bone::ShinL : Bone::ShinR;
                const Bone foot = side == 0 ? Bone::FootL : Bone::FootR;
                pose.rotation[bone_index(thigh)] = pitch_q(a);
                pose.rotation[bone_index(shin)] = pitch_q(-2.0f * a);
                pose.rotation[bone_index(foot)] = pitch_q(a);
            }
        } else {
            for (int side = 0; side < 2; ++side) {
                const LegAngles l = (side == 1 && r.symmetric) ? mirrored(r.leg[0])
                                                               : r.leg[static_cast<std::size_t>(side)];
                const Bone thigh = side == 0 ? Bone::ThighL : Bone::ThighR;
                const Bone shin = side == 0 ? Bone::ShinL : Bone::ShinR;
                const Bone foot = side == 0 ? Bone::FootL : Bone::FootR;
                pose.rotation[bone_index(thigh)] =
                    glm::angleAxis(l.thigh_yaw_deg * DEG, glm::vec3{0.0f, 1.0f, 0.0f})
                    * glm::angleAxis(l.thigh_roll_deg * DEG, glm::vec3{0.0f, 0.0f, 1.0f})
                    * pitch_q(l.thigh_pitch_deg * DEG);
                // Сгиб колена — ОТРИЦАТЕЛЬНЫЙ тангаж: предел рига
                // (BODY_KNEE_FLEX_MAX) стоит именно на этой стороне, и поза,
                // написанная с другим знаком, была бы законна ровно до
                // apply_joint_limits.
                pose.rotation[bone_index(shin)] = pitch_q(-l.knee_deg * DEG);
                pose.rotation[bone_index(foot)] = pitch_q(l.ankle_deg * DEG);
            }
        }
    }

    // --- КОРПУС И ГОЛОВА ---------------------------------------------------
    // ПРЕДПИСЫВАЕТСЯ, А НЕ ЗАМЕЩАЕТСЯ: у сидения на стуле корпус уже наклонён
    // самой sit_pose, и записать угол значило бы стереть её наклон.
    const glm::quat torso_q =
        glm::angleAxis(r.torso_roll_deg * DEG, glm::vec3{0.0f, 0.0f, 1.0f})
        * pitch_q(-r.torso_pitch_deg * DEG);
    pose.rotation[bone_index(Bone::Torso)] = torso_q * pose.rotation[bone_index(Bone::Torso)];
    // Голова гасит долю наклона корпуса — тот же рефлекс, что у бега и
    // присяда (Clips.h, HEAD_STABILIZE): на пол не смотрят оттого, что
    // наклонились.
    pose.rotation[bone_index(Bone::Head)] =
        pitch_q((r.torso_pitch_deg * HEAD_STABILIZE - r.head_pitch_deg) * DEG)
        * pose.rotation[bone_index(Bone::Head)];

    // --- РУКИ --------------------------------------------------------------
    const glm::quat torso_inv = glm::inverse(pose.rotation[bone_index(Bone::Torso)]);
    for (int side = 0; side < 2; ++side) {
        const ArmTarget t = (side == 1 && r.symmetric) ? mirrored(r.arm[0])
                                                       : r.arm[static_cast<std::size_t>(side)];
        if (!t.used) {
            continue;
        }
        const Bone upper = side == 0 ? Bone::UpperArmL : Bone::UpperArmR;
        const Bone fore = side == 0 ? Bone::ForearmL : Bone::ForearmR;
        // Цель дана в системе ТАЗА; плечо висит на туловище, поэтому цель
        // переезжает в систему туловища ЕГО ЖЕ поворотом — второй записи
        // «где плечо» тут нет.
        const glm::vec3 in_torso = torso_inv * (t.at * unit);
        const ArmSolution s = solve_arm(pr.upper_arm_length, pr.forearm_length,
                                        rig.rest_offset[bone_index(upper)], in_torso,
                                        t.swivel_deg * DEG);
        pose.rotation[bone_index(upper)] = s.shoulder;
        pose.rotation[bone_index(fore)] = pitch_q(s.elbow_rad);
    }

    return pose;
}

// --- ГРАФ ------------------------------------------------------------------

float support_step_s(Support a, Support b) {
    if (a == b) {
        return 0.0f;
    }
    const int lo = std::min(static_cast<int>(a), static_cast<int>(b));
    switch (lo) {
    case 0:
        return SUPPORT_STAND_CROUCH_S;
    case 1:
        return SUPPORT_CROUCH_SITFLOOR_S;
    default:
        return SUPPORT_SITFLOOR_LIEPRONE_S;
    }
}

float PoseRoute::total_s() const {
    float sum = 0.0f;
    for (uint32_t i = 0; i < legs(); ++i) {
        sum += leg_s[i];
    }
    return sum;
}

PoseRoute pose_route(PoseId from, PoseId to) {
    PoseRoute r;
    const auto push = [&r](PoseId p) {
        if (r.count > 0 && r.step[r.count - 1] == p) {
            return; // повтор узла — не колено нулевой длины
        }
        if (r.count < POSE_ROUTE_MAX) {
            r.step[r.count++] = p;
        }
    };
    push(from);
    if (from != to) {
        const int sf = static_cast<int>(pose_support(from));
        const int st = static_cast<int>(pose_support(to));
        push(support_pose(static_cast<Support>(sf)));
        const int dir = st > sf ? 1 : -1;
        for (int i = sf; i != st; i += dir) {
            push(support_pose(static_cast<Support>(i + dir)));
        }
        push(to);
    }
    for (uint32_t i = 0; i + 1 < r.count; ++i) {
        const Support a = pose_support(r.step[i]);
        const Support b = pose_support(r.step[i + 1]);
        // Колено между ДВУМЯ ОПОРАМИ — перенос веса; всё прочее — перекладка
        // конечностей на неизменной опоре.
        const bool between_supports = r.step[i] == support_pose(a)
                                   && r.step[i + 1] == support_pose(b) && adjacent(a, b);
        r.leg_s[i] = between_supports ? support_step_s(a, b) : POSE_TO_SUPPORT_S;
    }
    return r;
}

PoseId PoseTransit::from() const {
    return route.count > 0 ? route.step[std::min(leg, route.count - 1)] : PoseId::Stand;
}
PoseId PoseTransit::to() const {
    return route.count > 0 ? route.step[std::min(leg + 1, route.count - 1)] : PoseId::Stand;
}
PoseId PoseTransit::target() const {
    return route.count > 0 ? route.step[route.count - 1] : PoseId::Stand;
}

PoseTransit pose_transit_at(PoseId p) {
    PoseTransit tr;
    tr.route = pose_route(p, p);
    tr.leg = 0;
    tr.t_s = 0.0f;
    tr.moving = false;
    return tr;
}

void pose_transit_begin(PoseTransit& tr, PoseId to) {
    // ОТ БЛИЖАЙШЕГО УЗЛА, а не от начала маршрута: смена цели посреди
    // вставания не имеет права утащить тело обратно на пол. Цена решения
    // названа вслух — до полуколена скачка при перехвате, — и она меньше
    // цены обратного хода, который зритель читает как сбой.
    PoseId here = tr.from();
    if (tr.moving && tr.route.legs() > 0) {
        const float span = std::max(1.0e-4f, tr.route.leg_s[tr.leg]);
        here = tr.t_s / span >= 0.5f ? tr.to() : tr.from();
    }
    tr.route = pose_route(here, to);
    tr.leg = 0;
    tr.t_s = 0.0f;
    tr.moving = tr.route.legs() > 0;
}

void pose_transit_advance(PoseTransit& tr, float dt) {
    if (!tr.moving || tr.route.legs() == 0) {
        tr.moving = false;
        return;
    }
    float left = std::max(0.0f, dt);
    while (left > 0.0f) {
        const float span = std::max(1.0e-4f, tr.route.leg_s[tr.leg]);
        const float room = span - tr.t_s;
        if (left < room) {
            tr.t_s += left;
            return;
        }
        left -= room;
        ++tr.leg;
        tr.t_s = 0.0f;
        if (tr.leg >= tr.route.legs()) {
            // Доехали: маршрут схлопывается в стояние на последнем узле, и
            // дальше tr.from() == tr.to() == цель.
            const PoseId end = tr.route.step[tr.route.count - 1];
            tr = pose_transit_at(end);
            return;
        }
    }
}

float pose_ease(float u) {
    const float x = std::clamp(u, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

LocalPose pose_transit_pose(const Rig& rig, const PoseTransit& tr) {
    if (!tr.moving || tr.route.legs() == 0) {
        return pose_of(rig, tr.from());
    }
    const float span = std::max(1.0e-4f, tr.route.leg_s[tr.leg]);
    return blend(pose_of(rig, tr.from()), pose_of(rig, tr.to()),
                 pose_ease(tr.t_s / span));
}

} // namespace dfn::anim
