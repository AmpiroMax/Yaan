/*
Module: engine/app
File: engine/app/sources/MenuEmblem.cpp

Responsibility:
- Загрузка герба с полки, его поза от часов меню, раскладка в осях камеры и
  свет экрана меню. Договор и все числа — в MenuEmblem.h.

Dependencies:
- Uses: engine/render (ObjectRegistry, RenderSystem, FirstPersonCamera),
  engine/platform (IRenderer), engine/app AppDoors.
- Used by: engine/app App.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
*/

#include "engine/app/sources/MenuEmblem.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/core/materials/sources/MaterialRegistry.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string_view>

namespace dfn::app {
namespace {

constexpr float TAU = 6.283185307179586f;

// ПРИКОЛОТАЯ ФАЗА (DFN_MENU_OAK_PHASE). Читается ОДИН раз за прогон: доза,
// опрошенная каждый кадр, позволила бы двум кадрам одного прогона разойтись
// в том, что именно проверялось (см. AppDoors.h про Once и EachRead).
struct PinnedPhase {
    bool on = false;
    float seconds = 0.0f;
};

const PinnedPhase& pinned_phase() {
    static const PinnedPhase pinned = [] {
        PinnedPhase p;
        const char* v = door_value("DFN_MENU_OAK_PHASE");
        if (v == nullptr || *v == '\0') {
            return p;
        }
        // strtof, а не from_chars: плавающая перегрузка последней в этой
        // системе появляется только в macOS 26, а сборка обязана идти и на
        // прежних (правило 19, вторая цепочка инструментов).
        char* end = nullptr;
        const float parsed = std::strtof(v, &end);
        if (end != v) {
            p.on = true;
            p.seconds = parsed;
            std::fprintf(stderr,
                         "[меню] DFN_MENU_OAK_PHASE=%.3f: герб приколот к одной "
                         "фазе качания\n",
                         static_cast<double>(parsed));
        }
        return p;
    }();
    return pinned;
}

} // namespace

EmblemPose emblem_pose(float menu_seconds) {
    const PinnedPhase& pin = pinned_phase();
    const float t = pin.on ? pin.seconds : menu_seconds;
    EmblemPose pose;
    pose.yaw_rad = glm::radians(OAK_YAW_DEG)
                   * std::sin(TAU * t / OAK_YAW_PERIOD_S);
    // Сдвиг фазы наклона — чтобы в нуле часов герб не стоял идеально анфас:
    // первый же кадр меню обязан показывать объём, а не плоскую печать.
    pose.tilt_rad = glm::radians(OAK_TILT_DEG)
                    * std::sin(TAU * t / OAK_TILT_PERIOD_S + 1.1f);
    return pose;
}

glm::mat4 emblem_in_camera(const render::FirstPersonCamera& camera,
                           const glm::vec3& local_center, float local_height,
                           float menu_seconds) {
    const float depth = render::RenderSystem::overlay_depth_m(camera) * OAK_DEPTH_FRAC;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float half_w = half_h * camera.aspect_ratio();
    const float scale = local_height > 1e-6f
                            ? (OAK_HEIGHT_FRAC * 2.0f * half_h) / local_height
                            : 1.0f;
    // Доли кадра -> метры в осях камеры. Y холста растёт ВНИЗ, Y камеры вверх.
    const float x = (2.0f * OAK_CENTER_X_FRAC - 1.0f) * half_w;
    const float y = (1.0f - 2.0f * OAK_CENTER_Y_FRAC) * half_h;

    const EmblemPose pose = emblem_pose(menu_seconds);
    // Наклон СНАРУЖИ рыскания: это кивок всей доски вокруг горизонтали кадра,
    // а не вторая ось внутри уже повёрнутого герба.
    return glm::translate(glm::mat4(1.0f), {x, y, -depth})
           * glm::scale(glm::mat4(1.0f), glm::vec3(scale))
           * glm::rotate(glm::mat4(1.0f), pose.tilt_rad, {1.0f, 0.0f, 0.0f})
           * glm::rotate(glm::mat4(1.0f), pose.yaw_rad, {0.0f, 1.0f, 0.0f})
           * glm::translate(glm::mat4(1.0f), -local_center);
}

EmblemDepthSpan emblem_depth_span(const render::FirstPersonCamera& camera,
                                  const glm::vec3& local_center, float local_height,
                                  float local_width, float local_thickness) {
    (void)local_center;
    const float depth = render::RenderSystem::overlay_depth_m(camera) * OAK_DEPTH_FRAC;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float scale = local_height > 1e-6f
                            ? (OAK_HEIGHT_FRAC * 2.0f * half_h) / local_height
                            : 1.0f;
    // Заметаемая по глубине половина — худший случай ОБОИХ поворотов: доска
    // шириной w и толщиной d, повёрнутая на угол a, торчит вдоль оси взгляда
    // на (w/2)|sin a| + (d/2)|cos a| <= (w + d)/2. Берём границу, а не
    // мгновенное значение: утверждение обязано держаться на всей траектории.
    const float yaw = glm::radians(OAK_YAW_DEG);
    const float tilt = glm::radians(OAK_TILT_DEG);
    const float sweep_yaw = 0.5f * local_width * std::fabs(std::sin(yaw))
                            + 0.5f * local_thickness;
    const float sweep_tilt = 0.5f * local_height * std::fabs(std::sin(tilt));
    const float half_span = (sweep_yaw + sweep_tilt) * scale;
    return {depth - half_span, depth + half_span};
}

void light_menu_screen(platform::RenderEnvironment& env,
                       const render::FirstPersonCamera& camera,
                       std::vector<render::RenderSystem::ExtraLight>& lights) {
    const glm::vec3 fwd = camera.forward(0.0f);
    const glm::vec3 right = camera.right(0.0f);
    const glm::vec3 up = glm::normalize(glm::cross(right, fwd));
    const glm::vec3 eye = camera.interpolated_pose(0.0f).position;

    const float depth = render::RenderSystem::overlay_depth_m(camera) * OAK_DEPTH_FRAC;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float half_w = half_h * camera.aspect_ratio();
    const float oak_h = OAK_HEIGHT_FRAC * 2.0f * half_h;
    const glm::vec3 center = eye + fwd * depth
                             + right * ((2.0f * OAK_CENTER_X_FRAC - 1.0f) * half_w)
                             + up * ((1.0f - 2.0f * OAK_CENTER_Y_FRAC) * half_h);

    // СОЛНЦЕ ГАСИТСЯ И ЛОЖИТСЯ ПОЧТИ ГОРИЗОНТАЛЬНО. Цвет ноль — оно не даёт
    // ни люмена; высота 0.03 НИЖЕ SHADOW_MIN_SUN_ELEVATION (0.05), и это
    // единственное, что выключает построение карты теней целиком: иначе
    // 214 тыс. треугольников герба поехали бы ещё и в два каскада.
    // НАПРАВЛЕНИЕ ПРИ ЭТОМ НЕ МУСОР: горизонтальную часть солнца читает
    // направленная составляющая заливки (u_fillSun), поэтому азимут смотрит
    // туда же, откуда бьёт ключ, — небо ярче со стороны света.
    const glm::vec3 key_dir = glm::normalize(right * OAK_KEY_OFFSET.x
                                             + up * OAK_KEY_OFFSET.y
                                             + (-fwd) * OAK_KEY_OFFSET.z);
    glm::vec3 sun_h = key_dir;
    sun_h.y = 0.03f;
    env.sun_direction = glm::normalize(sun_h);
    env.sun_color = glm::vec3{0.0f};
    env.ambient_color = OAK_AMBIENT;
    env.ambient_darkness = 0.0f;
    // Луна выключена НАСМЕРТЬ, и это не уборка: u_moonLight — гейт
    // скотопической ночи в dfn_aerial, а она десатурирует тёмное к люме.
    // Живая луна сделала бы теневую сторону золота СЕРОЙ.
    env.moon_light = 0.0f;
    env.star_intensity = 0.0f;

    lights.clear();
    render::RenderSystem::ExtraLight key;
    key.position = center + right * (OAK_KEY_OFFSET.x * oak_h)
                   + up * (OAK_KEY_OFFSET.y * oak_h)
                   + (-fwd) * (OAK_KEY_OFFSET.z * oak_h);
    key.color = OAK_KEY_COLOR;
    key.radius_m = OAK_KEY_RADIUS_FRAC * oak_h;
    key.casts_shadow = false;
    lights.push_back(key);

    render::RenderSystem::ExtraLight fill;
    fill.position = center + right * (OAK_FILL_OFFSET.x * oak_h)
                    + up * (OAK_FILL_OFFSET.y * oak_h)
                    + (-fwd) * (OAK_FILL_OFFSET.z * oak_h);
    fill.color = OAK_FILL_COLOR;
    fill.radius_m = OAK_FILL_RADIUS_FRAC * oak_h;
    fill.casts_shadow = false;
    lights.push_back(fill);
}

bool MenuEmblem::ensure_loaded(platform::IRenderer& renderer) {
    if (ready()) {
        return true;
    }
    if (attempted_) {
        return false;
    }
    attempted_ = true;

    const auto object = render::read_object(std::filesystem::path(HERALDRY_OAK_DFO));
    if (!object || object->wood.indices.empty()) {
        std::fprintf(stderr,
                     "[меню] герб не прочитан с полки (%s) — главное меню "
                     "останется без эмблемы\n",
                     HERALDRY_OAK_DFO);
        return false;
    }
    const platform::MeshHandle mesh =
        renderer.create_mesh(object->wood.vertices, object->wood.indices);
    if (!mesh.valid()) {
        std::fprintf(stderr, "[меню] герб не залился на видеокарту\n");
        return false;
    }
    const platform::ProgramHandle program = renderer.load_program("prop");
    if (!program.valid()) {
        renderer.destroy_mesh(mesh);
        std::fprintf(stderr, "[меню] программа prop не загрузилась\n");
        return false;
    }
    mesh_ = mesh.id;
    program_ = program.id;
    triangles_ = static_cast<std::uint32_t>(object->wood.triangle_count());

    // ГАБАРИТ БЕРЁТСЯ ОБЩЕЙ МЕРКОЙ (правило 32), а не вторым обходом вершин
    // здесь: перепечённый герб другого размера обязан встать в кадр сам.
    const render::ObjectExtent extent = render::measure_object(*object);
    local_center_ = {0.5f * (extent.lo.x + extent.hi.x),
                     0.5f * (extent.bottom + extent.top),
                     0.5f * (extent.lo.y + extent.hi.y)};
    local_height_ = std::max(1e-4f, extent.top - extent.bottom);
    std::fprintf(stderr,
                 "[меню] герб на полке: %u треугольников, габарит %.3f x %.3f м\n",
                 triangles_, static_cast<double>(extent.hi.x - extent.lo.x),
                 static_cast<double>(local_height_));
    return true;
}

void MenuEmblem::release(platform::IRenderer& renderer) {
    if (mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_});
    }
    mesh_ = 0;
    program_ = 0;
    triangles_ = 0;
    attempted_ = false;
}

render::RenderSystem::ScreenProp
MenuEmblem::screen_prop(const render::FirstPersonCamera& camera,
                        float menu_seconds) const {
    render::RenderSystem::ScreenProp prop;
    if (!ready()) {
        return prop;
    }
    prop.mesh = mesh_;
    prop.program = program_;
    prop.in_camera = emblem_in_camera(camera, local_center_, local_height_,
                                      menu_seconds);
    // ИЗ ЧЕГО СДЕЛАН ГЕРБ (зона МАТЕРИАЛЫ, 28.08, за дозой DFN_MAT).
    //
    // Полка уже несёт золото ЧИСЛОМ: 72039 вершин поля покрашены 0xFF3899C7,
    // то есть rgb(199,153,56). Но число в вершине — это АЛЬБЕДО, и на одном
    // альбедо золото неотличимо от горчичной краски: у обоих ламберт, у обоих
    // нет блика. Материал добавляет ровно недостающее — как поверхность
    // отражает, — и не трогает ни одной вершины полки.
    //
    // ДОЗА 0 (умолчание) — MATERIAL_NONE, то есть кадр меню бит-в-бит прежний.
    // Обе руки замера выходят из одной сборки (правило 47).
    const char* dose = door_value("DFN_MAT");
    if (dose != nullptr && *dose != '\0' && *dose != '0') {
        prop.material = core::material_registry().find("gold-leaf");
    }
    return prop;
}

} // namespace dfn::app
