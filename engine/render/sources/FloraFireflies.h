/*
Created: 17:08:2026 - 09:47:53
Last updated: 17:08:2026 - 11:16:13
Module: engine/render
File: engine/render/sources/FloraFireflies.h

Responsibility:
- СВЕТЛЯЧКИ (этап 3 полянки, заказ пользователя 17.08): ночное поле мушек,
  летающих ПО ВСЕЙ КАРТЕ («они должны летать повсюду, а не только в какой-то
  зоне»), медленно («чтобы красиво ощутить атмосферу пространства»), с
  пульсирующим свечением и медленной взаимной синхронизацией вспышек.
- Дизайн-источники: docs/SKYRIM_FAUNA_RESEARCH.md §4 (Reynolds wander по
  когерентному шуму, Minecraft-гейт по темноте, boids-светляки с фазовой
  синхронизацией).

Key items:
- FireflyParams / FireflyField: init() -> update(dt, night01, ground_at) ->
  build_mesh(камера) + lights() — 2-3 настоящих точки света на всё поле
  (бюджет рендера: 8 point lights в кадре, 2 теневых — светлякам тени НЕ
  положены, слоты нужны факелу игрока и очагу).

Contract:
- ДЕТЕРМИНИЗМ: одинаковые seed и последовательность dt дают побитово равное
  состояние — приёмка двумя руками обязана сходиться (правило 30).
- Модуль НЕ знает движка: высота земли приходит колбэком, «ночность» —
  числом 0..1 (0 = день, мушки спят и меш пуст), камера — тремя векторами.
  Проводку в кадр делает зона app/render (лид) — см. заявку от 17.08.

Dependencies:
- Uses: ProcMesh.h (MeshData, pack), glm.
- Used by: RenderSystem (проводка лида), tests/render/ProcFloraTests.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Никаких стенных часов и rand(): вся случайность — от seed, всё время — от
  суммы dt. Иначе ломается резюме и приёмка.
*/
/*
UPD:
- 17:08:2026 - 09:47:53: Created — поле светлячков: wander по сглаженному хэш-шуму,
  Курамото-синхронизация вспышек, ночной гейт, билборды в MeshData, lights().
- 17:08:2026 - 11:16:13: Мушка 5.5 см (3 см не читались с двух метров — замечание лида при проводке).
*/

#pragma once

#include "engine/render/sources/ProcMesh.h"

#include <cstdint>
#include <functional>
#include <glm/vec3.hpp>
#include <vector>

namespace dfn::render {

struct FireflyParams {
    uint64_t seed = 779;
    int count = 140;             ///< мушек на карту 256x256 — «повсюду»
    float world_span = 256.0f;
    float speed_min = 0.4f;      ///< м/с — медленно, атмосфера важнее
    float speed_max = 0.8f;
    float h_min = 0.3f;          ///< полоса высот над землёй, м
    float h_max = 2.4f;
    float blink_period = 3.0f;   ///< с; на особь ±40%
    float sync_gain = 0.15f;     ///< связь Курамото: рой сходится МЕДЛЕННО
    float sync_radius = 7.0f;    ///< соседи ближе этого тянут фазу, м
    /// Полуразмер билборда, м. 3 см не читались с двух метров (замечание
    /// лида при проводке) — мушка-огонёк крупнее жизни, зато видима.
    float size = 0.055f;
};

/// Одно поле на карту. Обновляется каждый кадр, строит меш заново (сотни
/// треугольников — дешевле любого кеша) и отдаёт до трёх точек света.
class FireflyField {
public:
    void init(const FireflyParams& params);

    /// ground_at(x, z) — мировая высота земли; night01: 0 день .. 1 глухая
    /// ночь (гейт и общая яркость).
    void update(float dt, float night01,
                const std::function<float(float, float)>& ground_at);

    /// Билборды к камере; пустой меш днём. Цвет — тёплая зелень, яркость —
    /// пульс особи x night01.
    void build_mesh(MeshData& out, glm::vec3 cam_right, glm::vec3 cam_up) const;

    struct Light {
        glm::vec3 pos;
        float intensity;         ///< 0..1, дышит суммой вспышек кластера
    };
    /// До max_count самых ярких кластеров, разнесённых минимум на 25 м.
    /// significance = яркость кластера / (1 + дистанция до viewer/20 м) —
    /// заказ лида: общий бюджет 8 огней делится ЧЕСТНО между факелами и
    /// роями, отбор по значимости, не «кто первый в списке».
    struct RankedLight {
        glm::vec3 pos;
        float intensity;
        float significance;
    };
    [[nodiscard]] std::vector<Light> lights(int max_count = 3) const;
    [[nodiscard]] std::vector<RankedLight> lights_ranked(glm::vec3 viewer,
                                                         int max_count = 3) const;

    [[nodiscard]] int count() const { return static_cast<int>(flies_.size()); }
    [[nodiscard]] glm::vec3 position(int i) const { return flies_[static_cast<size_t>(i)].pos; }
    [[nodiscard]] float phase(int i) const { return flies_[static_cast<size_t>(i)].phase; }
    [[nodiscard]] float brightness(int i) const;

private:
    struct Fly {
        glm::vec3 pos{0.0f};
        float yaw = 0.0f;
        float speed = 0.5f;
        float phase = 0.0f;      ///< фаза вспышки, рад
        float period = 3.0f;     ///< личный период, с
        float noise_seed = 0.0f; ///< смещение шума курса
    };
    std::vector<Fly> flies_;
    FireflyParams p_;
    float time_ = 0.0f;
    float night_ = 0.0f;
};

} // namespace dfn::render
