/*
Created: 17:08:2026 - 09:48:46
Last updated: 17:08:2026 - 09:48:46
Module: engine/render
File: engine/render/sources/FloraFireflies.cpp

Responsibility:
- Реализация поля светлячков: медленный wander по сглаженному хэш-шуму,
  мягкий возврат в границы карты и в полосу высот, личный пульс вспышки с
  Курамото-связью к соседям, билборды и до трёх точек света.

Dependencies:
- Uses: FloraFireflies.h, ProcMesh.h.
- Used by: RenderSystem (проводка лида), tests.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ДЕТЕРМИНИЗМ: hash-шум от (seed, особь, квант времени); никакого rand().
*/
/*
UPD:
- 17:08:2026 - 09:48:46: Created — см. заголовок.
*/

#include "engine/render/sources/FloraFireflies.h"

#include <algorithm>
#include <cmath>

namespace dfn::render {
namespace {

constexpr float TAU = 6.2831853f;

/// SplitMix64 — та же семья, что в кузнице: детерминизм от seed.
uint64_t mix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

float unit_of(uint64_t x) {
    return static_cast<float>(mix(x) >> 40) / 16777216.0f; // [0,1)
}

/// Сглаженный 1D шум по целым квантам: узлы — хэш, между ними smoothstep.
/// Это «когерентный шум курса» из дизайна (§4): плавно, без дёрганья.
float smooth_noise(uint64_t id, float t) {
    const float ft = std::floor(t);
    const auto k = static_cast<int64_t>(ft);
    const float a = unit_of(id * 0x10001ull + static_cast<uint64_t>(k));
    const float b = unit_of(id * 0x10001ull + static_cast<uint64_t>(k + 1));
    float f = t - ft;
    f = f * f * (3.0f - 2.0f * f);
    return (a + (b - a) * f) * 2.0f - 1.0f; // [-1,1]
}

} // namespace

void FireflyField::init(const FireflyParams& params) {
    p_ = params;
    time_ = 0.0f;
    night_ = 0.0f;
    flies_.clear();
    flies_.reserve(static_cast<size_t>(p_.count));
    for (int i = 0; i < p_.count; ++i) {
        const auto id = static_cast<uint64_t>(i) + p_.seed * 0x100000001ull;
        Fly fly;
        // Равномерно по всей карте — «повсюду», не по зонам.
        fly.pos.x = 4.0f + unit_of(id * 5 + 1) * (p_.world_span - 8.0f);
        fly.pos.z = 4.0f + unit_of(id * 5 + 2) * (p_.world_span - 8.0f);
        fly.pos.y = 0.0f; // сядет на полосу высот первым же update()
        fly.yaw = unit_of(id * 5 + 3) * TAU;
        fly.speed = p_.speed_min
                  + unit_of(id * 5 + 4) * (p_.speed_max - p_.speed_min);
        fly.period = p_.blink_period * (0.6f + 0.8f * unit_of(id * 5 + 5));
        fly.phase = unit_of(id * 5 + 6) * TAU;
        fly.noise_seed = unit_of(id * 5 + 7) * 64.0f;
        flies_.push_back(fly);
    }
}

void FireflyField::update(float dt, float night01,
                          const std::function<float(float, float)>& ground_at) {
    time_ += dt;
    night_ = std::clamp(night01, 0.0f, 1.0f);
    const float lo = 4.0f;
    const float hi = p_.world_span - 4.0f;
    // Фазы: личный ход + Курамото-подтяжка к соседям. O(n^2) на полутора
    // сотнях мушек — копейки, и никакой структуры данных, которая могла бы
    // разойтись между двумя руками приёмки.
    std::vector<float> pull(flies_.size(), 0.0f);
    const float r2 = p_.sync_radius * p_.sync_radius;
    for (size_t i = 0; i < flies_.size(); ++i) {
        float acc = 0.0f;
        int n = 0;
        for (size_t j = 0; j < flies_.size(); ++j) {
            if (i == j) continue;
            const glm::vec3 d = flies_[j].pos - flies_[i].pos;
            if (d.x * d.x + d.z * d.z > r2) continue;
            acc += std::sin(flies_[j].phase - flies_[i].phase);
            ++n;
        }
        pull[i] = (n > 0) ? acc / static_cast<float>(n) : 0.0f;
    }
    for (size_t i = 0; i < flies_.size(); ++i) {
        Fly& fly = flies_[i];
        const auto id = static_cast<uint64_t>(i) + p_.seed * 0x100000001ull;
        // КУРС: медленный дрейф по сглаженному шуму (≈8-секундные волны) —
        // «медленно, чтобы красиво ощутить атмосферу пространства».
        fly.yaw += smooth_noise(id, time_ * 0.125f + fly.noise_seed) * 0.9f * dt;
        // Мягкий разворот от края карты: не стенка, а нежелание уходить.
        const float margin = 12.0f;
        glm::vec3 steer{0.0f};
        if (fly.pos.x < lo + margin) steer.x += 1.0f;
        if (fly.pos.x > hi - margin) steer.x -= 1.0f;
        if (fly.pos.z < lo + margin) steer.z += 1.0f;
        if (fly.pos.z > hi - margin) steer.z -= 1.0f;
        if (steer.x != 0.0f || steer.z != 0.0f) {
            const float want = std::atan2(steer.x, -steer.z);
            float diff = want - fly.yaw;
            while (diff > 3.14159f) diff -= TAU;
            while (diff < -3.14159f) diff += TAU;
            fly.yaw += diff * std::min(1.0f, 1.6f * dt);
        }
        const glm::vec3 dir{std::sin(fly.yaw), 0.0f, -std::cos(fly.yaw)};
        fly.pos += dir * (fly.speed * dt);
        fly.pos.x = std::clamp(fly.pos.x, lo, hi);
        fly.pos.z = std::clamp(fly.pos.z, lo, hi);
        // ВЫСОТА: дыхание внутри полосы над землёй, ease к цели.
        const float ground = ground_at ? ground_at(fly.pos.x, fly.pos.z) : 0.0f;
        const float band = p_.h_min
            + (p_.h_max - p_.h_min)
                  * (0.5f + 0.5f * smooth_noise(id * 3 + 1,
                                                time_ * 0.09f + fly.noise_seed));
        const float target_y = ground + band;
        fly.pos.y += (target_y - fly.pos.y) * std::min(1.0f, 1.2f * dt);
        // ВСПЫШКА: личный ход + связь.
        fly.phase += (TAU / fly.period + p_.sync_gain * pull[i]) * dt;
        if (fly.phase > TAU) fly.phase -= TAU;
    }
}

float FireflyField::brightness(int i) const {
    const Fly& fly = flies_[static_cast<size_t>(i)];
    // Пульс: узкая тёплая вспышка, долгое тление — не синус-лампочка.
    const float s = 0.5f + 0.5f * std::sin(fly.phase);
    return night_ * (0.08f + 0.92f * s * s * s);
}

void FireflyField::build_mesh(MeshData& out, glm::vec3 cam_right,
                              glm::vec3 cam_up) const {
    if (night_ <= 0.001f) return; // день: мушки спят, меша нет
    const glm::vec3 warm{0.62f, 0.95f, 0.45f}; // тёплая зелень светляка
    for (size_t i = 0; i < flies_.size(); ++i) {
        const float b = brightness(static_cast<int>(i));
        if (b <= 0.02f) continue;
        const Fly& fly = flies_[i];
        const float s = p_.size * (0.8f + 0.6f * b);
        const glm::vec3 r = cam_right * s;
        const glm::vec3 u = cam_up * s;
        const uint32_t c = pack(warm * b);
        const auto base = static_cast<uint32_t>(out.vertices.size());
        const glm::vec3 n = glm::vec3{0.0f, 1.0f, 0.0f};
        out.vertices.push_back({fly.pos - r - u, n, {0.0f, 0.0f}, c});
        out.vertices.push_back({fly.pos - r + u, n, {0.0f, 1.0f}, c});
        out.vertices.push_back({fly.pos + r + u, n, {1.0f, 1.0f}, c});
        out.vertices.push_back({fly.pos + r - u, n, {1.0f, 0.0f}, c});
        out.indices.insert(out.indices.end(),
                           {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

std::vector<FireflyField::Light> FireflyField::lights(int max_count) const {
    std::vector<Light> out;
    if (night_ <= 0.001f || flies_.empty()) return out;
    // Кластерная яркость: сумма вспышек соседей в радиусе синхронизации —
    // столб света стоит там, где рой, а не где одинокая мушка.
    const float r2 = p_.sync_radius * p_.sync_radius;
    std::vector<float> local(flies_.size(), 0.0f);
    for (size_t i = 0; i < flies_.size(); ++i) {
        float acc = 0.0f;
        for (size_t j = 0; j < flies_.size(); ++j) {
            const glm::vec3 d = flies_[j].pos - flies_[i].pos;
            if (d.x * d.x + d.z * d.z <= r2)
                acc += brightness(static_cast<int>(j));
        }
        local[i] = acc;
    }
    std::vector<size_t> order(flies_.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return local[a] > local[b]; });
    constexpr float MIN_APART = 25.0f;
    for (size_t oi : order) {
        if (static_cast<int>(out.size()) >= max_count) break;
        bool far_enough = true;
        for (const Light& l : out) {
            const glm::vec3 d = flies_[oi].pos - l.pos;
            if (d.x * d.x + d.z * d.z < MIN_APART * MIN_APART) {
                far_enough = false;
                break;
            }
        }
        if (!far_enough) continue;
        out.push_back({flies_[oi].pos,
                       std::min(1.0f, local[oi] * 0.25f) * night_});
    }
    return out;
}

std::vector<FireflyField::RankedLight>
FireflyField::lights_ranked(glm::vec3 viewer, int max_count) const {
    std::vector<RankedLight> out;
    for (const Light& l : lights(max_count)) {
        const glm::vec3 d = l.pos - viewer;
        const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        out.push_back({l.pos, l.intensity,
                       l.intensity / (1.0f + dist / 20.0f)});
    }
    return out;
}

} // namespace dfn::render
