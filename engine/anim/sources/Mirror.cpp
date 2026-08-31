/*
Module: engine/anim
File: engine/anim/sources/Mirror.cpp

Responsibility:
- Реализация зеркальной разметки и антисимметризации цикла.

Dependencies:
- Uses: Mirror.h, core skeleton, glm.
- Used by: dfn_anim (ClipPlayer), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Чисто и детерминированно: тот же скелет и та же поза — те же числа.
*/

#include "engine/anim/sources/Mirror.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dfn::anim {
namespace {

/// ОТРАЖЕНИЕ поперёк плоскости x = 0, в системе самой модели. Диагональная
/// матрица, а не «поменять знак у x»: сопряжение ею — единственное, что
/// превращает поворот в поворот (det = +1), тогда как отражение одной оси
/// поворота дало бы зеркальную, то есть недопустимую, матрицу.
[[nodiscard]] glm::mat4 reflection() {
    glm::mat4 m{1.0f};
    m[0][0] = -1.0f;
    return m;
}

void model_matrices(const skel::Skeleton& skeleton, std::span<const JointLocal> sample,
                    std::vector<glm::mat4>& local, std::vector<glm::mat4>& out) {
    const std::size_t n = skeleton.size();
    local.assign(n, glm::mat4{1.0f});
    out.assign(n, glm::mat4{1.0f});
    for (std::size_t j = 0; j < n && j < sample.size(); ++j) {
        local[j] = glm::translate(glm::mat4{1.0f}, sample[j].translation)
                   * glm::mat4_cast(glm::normalize(sample[j].rotation))
                   * glm::scale(glm::mat4{1.0f}, sample[j].scale);
    }
    skel::skeleton_model_matrices(skeleton, local, out);
}

[[nodiscard]] glm::quat rotation_of(const glm::mat4& m) {
    glm::mat3 r{m};
    for (int c = 0; c < 3; ++c) {
        const float len = glm::length(r[c]);
        r[c] = len > 1.0e-8f ? r[c] / len : glm::vec3{c == 0, c == 1, c == 2};
    }
    return glm::normalize(glm::quat_cast(r));
}

[[nodiscard]] glm::vec3 bind_position(const skel::Skeleton& skeleton, std::size_t j) {
    return glm::vec3{glm::inverse(skeleton.joints[j].inverse_bind)[3]};
}

} // namespace

MirrorMap build_mirror_map(const skel::Skeleton& skeleton) {
    MirrorMap map;
    const std::size_t n = skeleton.size();
    if (n == 0) {
        return map;
    }
    std::vector<glm::vec3> p(n);
    float lo = std::numeric_limits<float>::max();
    float hi = -std::numeric_limits<float>::max();
    for (std::size_t j = 0; j < n; ++j) {
        p[j] = bind_position(skeleton, j);
        lo = std::min(lo, p[j].y);
        hi = std::max(hi, p[j].y);
    }
    const float height = std::max(hi - lo, 1.0e-3f);
    const float tol = MIRROR_TOLERANCE_FRAC * height;

    map.partner.assign(n, -1);
    // ПЕРВЫЙ ПРОХОД — ПО ТОЧКАМ. Ближайший к отражению, если он ближе допуска.
    for (std::size_t j = 0; j < n; ++j) {
        const glm::vec3 want{-p[j].x, p[j].y, p[j].z};
        std::size_t best = j;
        float best_d = std::numeric_limits<float>::max();
        for (std::size_t k = 0; k < n; ++k) {
            const float d = glm::length(p[k] - want);
            if (d < best_d) {
                best_d = d;
                best = k;
            }
        }
        if (best_d <= tol) {
            map.partner[j] = static_cast<int32_t>(best);
        }
    }
    // ВТОРОЙ ПРОХОД — ПО ЦЕПЯМ, и он не формальность (см. заголовок): пара
    // засчитывается, только если родитель пары есть пара родителя. Корень
    // считается парой самому себе, иначе цепь не с чего начинать.
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t j = 0; j < n; ++j) {
            const int32_t q = map.partner[j];
            if (q < 0) {
                continue;
            }
            if (map.partner[static_cast<std::size_t>(q)] != static_cast<int32_t>(j)) {
                map.partner[j] = -1; // пара обязана быть взаимной
                changed = true;
                continue;
            }
            const int32_t pj = skeleton.joints[j].parent;
            const int32_t pq = skeleton.joints[static_cast<std::size_t>(q)].parent;
            if (pj < 0 && pq < 0) {
                continue; // оба корни
            }
            if (pj < 0 || pq < 0
                || map.partner[static_cast<std::size_t>(pj)] != pq) {
                map.partner[j] = -1;
                changed = true;
            }
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        const int32_t q = map.partner[j];
        if (q < 0) {
            ++map.lone;
        } else if (q == static_cast<int32_t>(j)) {
            ++map.centres;
        } else if (q > static_cast<int32_t>(j)) {
            ++map.pairs;
        }
    }
    return map;
}

void mirror_pose(const skel::Skeleton& skeleton, const MirrorMap& map,
                 std::span<const JointLocal> sample, std::span<JointLocal> out) {
    const std::size_t n = skeleton.size();
    if (!map.valid() || sample.size() < n || out.size() < n) {
        return;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model;
    model_matrices(skeleton, sample, local, model);
    const glm::mat4 R = reflection();
    std::vector<glm::mat4> mirrored(n);
    for (std::size_t j = 0; j < n; ++j) {
        const int32_t q = map.partner[j];
        // СУСТАВ БЕЗ ПАРЫ ОСТАЁТСЯ СОБОЙ, а не отражается: отразить его —
        // значит утверждать, что он на оси тела, чего никто не проверял.
        const std::size_t src = q >= 0 ? static_cast<std::size_t>(q) : j;
        mirrored[j] = q >= 0 ? R * model[src] * R : model[src];
    }
    for (std::size_t j = 0; j < n; ++j) {
        const int32_t par = skeleton.joints[j].parent;
        const glm::mat4 m = par >= 0
                                ? glm::inverse(mirrored[static_cast<std::size_t>(par)])
                                      * mirrored[j]
                                : mirrored[j];
        out[j].translation = glm::vec3{m[3]};
        out[j].rotation = rotation_of(m);
        out[j].scale = sample[j].scale; // масштаб не отражается: он не имеет стороны
    }
}

void mirror_blend(const skel::Skeleton& skeleton, const MirrorMap& map,
                  std::span<const JointLocal> half, float dose,
                  std::span<JointLocal> sample) {
    const float w = std::clamp(dose, 0.0f, 0.5f);
    const std::size_t n = skeleton.size();
    if (w <= 0.0f || !map.valid() || half.size() < n || sample.size() < n) {
        return; // побитовое тождество: контрольная рука приёмки стоит на нём
    }
    std::vector<JointLocal> flipped(n);
    mirror_pose(skeleton, map, half, flipped);
    for (std::size_t j = 0; j < n; ++j) {
        if (map.partner[j] < 0) {
            continue; // ни пары, ни оси — сустав не наш
        }
        JointLocal& a = sample[j];
        const JointLocal& b = flipped[j];
        a.translation = glm::mix(a.translation, b.translation, w);
        a.scale = glm::mix(a.scale, b.scale, w);
        glm::quat qa = glm::normalize(a.rotation);
        glm::quat qb = glm::normalize(b.rotation);
        if (glm::dot(qa, qb) < 0.0f) {
            qb = -qb;
        }
        a.rotation = glm::normalize(glm::slerp(qa, qb, w));
    }
}

float mirror_asymmetry(const skel::Skeleton& skeleton, const MirrorMap& map,
                       std::span<const JointLocal> sample,
                       std::span<const JointLocal> half) {
    const std::size_t n = skeleton.size();
    if (!map.valid() || sample.size() < n || half.size() < n) {
        return 0.0f;
    }
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> model_a;
    std::vector<glm::mat4> model_b;
    model_matrices(skeleton, sample, local, model_a);
    model_matrices(skeleton, half, local, model_b);
    const glm::mat4 R = reflection();
    // ОТСЧЁТ ОТ ТАЗА, А НЕ ОТ НАЧАЛА КООРДИНАТ: корень едет вместе с телом
    // (подъём клипа, решатель стоп), и разность двух мгновений содержала бы
    // этот ход целиком. Тазом здесь служит первый сустав, который сам себе
    // зеркало, — то есть предмет, найденный той же разметкой, а не по имени.
    std::size_t centre = 0;
    bool have_centre = false;
    for (std::size_t j = 0; j < n && !have_centre; ++j) {
        if (map.partner[j] == static_cast<int32_t>(j)) {
            centre = j;
            have_centre = true;
        }
    }
    const glm::vec3 ca = have_centre ? glm::vec3{model_a[centre][3]} : glm::vec3{0.0f};
    const glm::vec3 cb = have_centre ? glm::vec3{model_b[centre][3]} : glm::vec3{0.0f};
    float worst = 0.0f;
    for (std::size_t j = 0; j < n; ++j) {
        const int32_t q = map.partner[j];
        if (q < 0) {
            continue;
        }
        const glm::vec3 a = glm::vec3{model_a[j][3]} - ca;
        const glm::vec3 b =
            glm::vec3{(R * model_b[static_cast<std::size_t>(q)])[3]} - glm::vec3{R * glm::vec4{cb, 1.0f}};
        worst = std::max(worst, glm::length(a - b));
    }
    return worst;
}

} // namespace dfn::anim
