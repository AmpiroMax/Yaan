/*
Created: 27:08:2026 - 13:02:00
Last updated: 27:08:2026 - 21:30:00
Module: tests/app
File: tests/app/MenuEmblemTests.cpp

Responsibility:
- Держит три свойства объёмного герба главного меню, КОТОРЫХ КАДР НЕ ДЕРЖИТ:
  * герб не режется ближней плоскостью НА ВСЕЙ траектории качания — кадр
    показывает одну фазу из бесконечности, а срезает бок ровно в крайней;
  * качание — чистая функция часов меню, ограниченная объявленной амплитудой,
    и НИКОГДА не делает полного оборота (иначе полкруга зритель смотрит на
    плоское дно, ради чего оборот и отвергнут);
  * раскладка кладёт герб туда, куда объявлено долями кадра, — на любом
    соотношении сторон и на любой ступени разрешения.

Dependencies:
- Uses: engine/app MenuEmblem (арифметика без видеокарты), engine/render
  FirstPersonCamera, doctest.
- Used by: ctest (app_menu_emblem).

Notes:
- У КАЖДОГО СЛУЧАЯ СВОЯ КОНТРОЛЬНАЯ РУКА (правило 30). Здесь они такие:
  * у запаса до ближней плоскости контроль — ЗАВЕДОМО ПЛОХАЯ раскладка
    (глубина в долю ближней плоскости): утверждение, которое проходит при
    любых числах, не проверяет ничего, и рука это показывает;
  * у амплитуды контроль — вторая, несоизмеримая ось: если бы обе оси
    считались от одного периода, фигура была бы прямой, а не Лиссажу, и
    движение читалось бы метрономом;
  * у раскладки контроль — ДРУГОЕ соотношение сторон: доля центра по X
    обязана остаться той же, а метры — измениться.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
*/
/*
UPD:
- 27:08:2026 - 13:02:00: Создан вместе с 3D-гербом главного меню.
- 27:08:2026 - 13:12:00: Пол амплитуды качания снят с 15° до 6°. Он повторял
  вкусовое число заголовка и покраснел от первой же правки темпа (19° -> 13°)
  — то есть был не утверждением, а копией константы, которую чинят подгонкой.
  Вопрос ЗДЕСЬ один: шевелится ли герб вообще.
- 27:08:2026 - 21:30:00: Тот же пол снят 6° -> 3°: владелец сбавил темп
  качания втрое (13°/11 с -> 7.5°/20 с), и 6° покраснели БЫ второй раз подряд
  — ровно так, как предупреждала запись выше. 3° — ниже половины нынешней
  амплитуды, то есть переживёт и следующее «помедленнее», а на «герб замер»
  ответит по-прежнему. Ни одно другое утверждение рукава не тронуто и ни одно
  не покраснело: запас до ближней плоскости от меньшего угла только вырос.
*/

#include "engine/app/sources/MenuEmblem.h"

#include "engine/core/config/sources/Constants.h"
#include "engine/render/sources/FirstPersonCamera.h"

#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include <cmath>

namespace {

// ГАБАРИТ НАСТОЯЩЕГО ГЕРБА, числами из docs/reports/heraldry-3d.html §6.
// Написан здесь, а не прочитан с полки, НАРОЧНО: рукав обязан идти без
// ассетов и без видеокарты, а вопрос у него не «какой файл лежит на полке»,
// а «правильно ли считается раскладка предмета такого размера». Полку
// проверяет сам читатель — он сверяет хэш содержимого.
constexpr float OAK_W = 0.801f;
constexpr float OAK_H = 0.885f;
constexpr float OAK_T = 0.053f;

dfn::render::FirstPersonCamera menu_camera(float aspect) {
    dfn::render::FirstPersonCamera camera;
    camera.set_projection(glm::radians(dfn::app::OAK_MENU_FOV_DEG), aspect,
                          static_cast<float>(dfn::config::CAMERA_NEAR),
                          static_cast<float>(dfn::config::CAMERA_FAR));
    return camera;
}

} // namespace

TEST_CASE("герб не режется ближней плоскостью ни в одной фазе качания") {
    const dfn::render::FirstPersonCamera camera = menu_camera(16.0f / 9.0f);
    const dfn::app::EmblemDepthSpan span =
        dfn::app::emblem_depth_span(camera, glm::vec3{0.0f}, OAK_H, OAK_W, OAK_T);

    // Граница берётся по ХУДШЕМУ повороту, поэтому одного утверждения хватает
    // на всю траекторию; отдельная проверка по секундам была бы выборкой из
    // непрерывного множества, то есть слабее.
    CHECK(span.near_m > camera.near_plane());
    // И запас не микроскопический: 20 % ближней плоскости — это 2 см, меньше
    // которых любая правка композиции стала бы игрой в рулетку.
    CHECK(span.near_m > camera.near_plane() * 1.2f);
    CHECK(span.far_m > span.near_m);

    // КОНТРОЛЬ: то же утверждение о ЗАВЕДОМО ПЛОХОЙ раскладке обязано падать.
    // Без него «near > near_plane» могло бы держаться просто потому, что
    // формула всегда возвращает большое число.
    const float bad_depth = camera.near_plane() * 0.5f;
    const float bad_half_h = bad_depth * std::tan(camera.fov_y() * 0.5f);
    const float bad_scale = dfn::app::OAK_HEIGHT_FRAC * 2.0f * bad_half_h / OAK_H;
    CHECK(bad_depth - 0.5f * OAK_W * bad_scale < camera.near_plane());
}

TEST_CASE("качание ограничено амплитудой и никогда не делает оборота") {
    float max_yaw = 0.0f;
    float max_tilt = 0.0f;
    // Проходим НЕСКОЛЬКО периодов обеих осей: 20 и 29 с взаимно просты, и
    // короткая выборка застала бы только одну их комбинацию.
    for (int i = 0; i <= 20000; ++i) {
        const float t = static_cast<float>(i) * 0.01f;
        const dfn::app::EmblemPose pose = dfn::app::emblem_pose(t);
        max_yaw = std::max(max_yaw, std::fabs(pose.yaw_rad));
        max_tilt = std::max(max_tilt, std::fabs(pose.tilt_rad));
    }
    CHECK(max_yaw <= glm::radians(dfn::app::OAK_YAW_DEG) + 1e-4f);
    CHECK(max_tilt <= glm::radians(dfn::app::OAK_TILT_DEG) + 1e-4f);
    // ЛИЦО НИКОГДА НЕ УХОДИТ ОТ ЗРИТЕЛЯ. Дно герба — плоская плита без
    // рельефа, поэтому четверть оборота (90°) — граница, за которой предмет
    // перестаёт быть гербом; держим её с большим запасом.
    CHECK(max_yaw < glm::radians(45.0f));
    // ...и амплитуда всё же НЕ НОЛЬ: качание, выродившееся в неподвижность,
    // прошло бы оба утверждения выше. Пол — 3°, а не сегодняшние 7.5: пол
    // ЗДЕСЬ обязан отвечать на «шевелится ли герб вообще», а не повторять
    // вкусовое число из заголовка. Повтор превратил бы всякую правку темпа
    // в красный тест, который чинят подгонкой константы, — то есть в тест,
    // который ничего не держит. Он и покраснел БЫ второй раз подряд: 6°
    // пережили правку 19 -> 13, но не пережили бы 13 -> 7.5.
    CHECK(max_yaw > glm::radians(3.0f));

    // ЧИСТАЯ ФУНКЦИЯ: одна и та же секунда — одна и та же поза.
    CHECK(dfn::app::emblem_pose(3.5f).yaw_rad
          == doctest::Approx(dfn::app::emblem_pose(3.5f).yaw_rad));

    // КОНТРОЛЬ НЕСОИЗМЕРИМОСТИ: у совпадающих периодов наклон был бы
    // постоянной долей рыскания, то есть фигура выродилась бы в отрезок.
    // Ищем две секунды с одинаковым рысканием и РАЗНЫМ наклоном.
    const dfn::app::EmblemPose a = dfn::app::emblem_pose(0.0f);
    const dfn::app::EmblemPose b = dfn::app::emblem_pose(dfn::app::OAK_YAW_PERIOD_S);
    CHECK(a.yaw_rad == doctest::Approx(b.yaw_rad).epsilon(0.01));
    CHECK(std::fabs(a.tilt_rad - b.tilt_rad) > glm::radians(1.0f));
}

TEST_CASE("раскладка ставит герб в объявленные доли кадра") {
    for (const float aspect : {16.0f / 9.0f, 4.0f / 3.0f, 21.0f / 9.0f}) {
        const dfn::render::FirstPersonCamera camera = menu_camera(aspect);
        // Фаза нуля рыскания и нуля наклона: раскладку меряем без поворота,
        // иначе утверждение смешивало бы два разных вопроса.
        const glm::mat4 m = dfn::app::emblem_in_camera(camera, glm::vec3{0.0f},
                                                       OAK_H, 0.0f);
        const glm::vec4 center = m * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
        const float depth = -center.z;
        const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
        const float half_w = half_h * aspect;

        const float x_frac = 0.5f * (center.x / half_w + 1.0f);
        const float y_frac = 0.5f * (1.0f - center.y / half_h);
        CHECK(x_frac == doctest::Approx(dfn::app::OAK_CENTER_X_FRAC).epsilon(0.002));
        CHECK(y_frac == doctest::Approx(dfn::app::OAK_CENTER_Y_FRAC).epsilon(0.002));

        // Высота: верх и низ габарита в долях кадра дают объявленную долю.
        const glm::vec4 top = m * glm::vec4{0.0f, 0.5f * OAK_H, 0.0f, 1.0f};
        const glm::vec4 bottom = m * glm::vec4{0.0f, -0.5f * OAK_H, 0.0f, 1.0f};
        const float h_frac = (top.y - bottom.y) / (2.0f * half_h);
        CHECK(h_frac == doctest::Approx(dfn::app::OAK_HEIGHT_FRAC).epsilon(0.002));

        // ПОДОШВА СТВОЛА УХОДИТ ЗА НИЖНИЙ КРАЙ КАДРА — то самое кадрирование,
        // которым закрыт срез основания (заказ владельца 27.08). Это
        // утверждение о КОМПОЗИЦИИ, и без него пара чисел центра и высоты
        // могла бы разъехаться в любую сторону при первой же правке.
        CHECK(dfn::app::OAK_CENTER_Y_FRAC + 0.5f * dfn::app::OAK_HEIGHT_FRAC > 1.0f);
        // ...а крона в кадр ВХОДИТ: срез снизу — не повод потерять верх.
        CHECK(dfn::app::OAK_CENTER_Y_FRAC - 0.5f * dfn::app::OAK_HEIGHT_FRAC > 0.05f);
    }
}

TEST_CASE("свет экрана меню не строит теней и не гасит цвет золота") {
    dfn::render::FirstPersonCamera camera = menu_camera(16.0f / 9.0f);
    dfn::platform::RenderEnvironment env{};
    std::vector<dfn::render::RenderSystem::ExtraLight> lights;
    dfn::app::light_menu_screen(env, camera, lights);

    // ГЛАВНОЕ ЧИСЛО ЭТОГО СВЕТА. Выше SHADOW_MIN_SUN_ELEVATION (0.05) бэкенд
    // строит карту теней и шлёт КАЖДЫЙ непрозрачный меш ещё и в два каскада —
    // 214 тыс. треугольников герба стали бы 642 тыс., а сам он занял бы в
    // ближней карте пару текселей и затенял бы себя как попало.
    CHECK(env.sun_direction.y < 0.05f);
    CHECK(env.sun_color == glm::vec3{0.0f});
    // Луна выключена: u_moonLight — гейт скотопической десатурации, и живая
    // луна увела бы теневую сторону золота в серое.
    CHECK(env.moon_light == doctest::Approx(0.0f));
    CHECK(env.ambient_darkness == doctest::Approx(0.0f));

    // Два источника, оба БЕЗ теневого слота (их два на весь кадр, и герб не
    // тот предмет, который стоит их занимать).
    REQUIRE(lights.size() == 2);
    for (const auto& l : lights) {
        CHECK_FALSE(l.casts_shadow);
        CHECK(l.radius_m > 0.0f);
    }
    // Ключ ТЕПЛЕЕ заливки, заливка ХОЛОДНЕЕ ключа — иначе «тёплое золото на
    // холодном небе» осталось бы словами в комментарии.
    CHECK(lights[0].color.r > lights[0].color.b);
    CHECK(lights[1].color.b > lights[1].color.r);
    // ...и ключ сильно ярче заливки: две одинаковые лампы дали бы плоскую
    // печать, ради ухода от которой всё и делалось.
    CHECK(lights[0].color.r > lights[1].color.r * 3.0f);

    // КОНТРОЛЬ РАДИУСА: источник обязан ДОСТАВАТЬ до герба. Затухание
    // линейно-оконное, поэтому лампа, стоящая дальше своего радиуса, не даёт
    // ровно ничего — и кадр в этом случае выглядит как «свет не назначили».
    const glm::vec3 eye = camera.interpolated_pose(0.0f).position;
    const glm::vec3 fwd = camera.forward(0.0f);
    const float depth = dfn::render::RenderSystem::overlay_depth_m(camera)
                        * dfn::app::OAK_DEPTH_FRAC;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float half_w = half_h * camera.aspect_ratio();
    const glm::vec3 right = camera.right(0.0f);
    const glm::vec3 up = glm::normalize(glm::cross(right, fwd));
    const glm::vec3 center =
        eye + fwd * depth
        + right * ((2.0f * dfn::app::OAK_CENTER_X_FRAC - 1.0f) * half_w)
        + up * ((1.0f - 2.0f * dfn::app::OAK_CENTER_Y_FRAC) * half_h);
    for (const auto& l : lights) {
        CHECK(glm::length(l.position - center) < l.radius_m);
    }
}
