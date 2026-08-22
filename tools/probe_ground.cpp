/*
Created: 23:08:2026 - 21:30:00
Last updated: 23:08:2026 - 23:55:00
Module: tools
File: tools/probe_ground.cpp

Responsibility:
- ПОСТРОЕННАЯ ЗЕМЛЯ СЦЕНЫ, В ЛЮБОЙ ТОЧКЕ И ПАЧКАМИ. Читает x z со stdin, пишет
  terrain_height на stdout по строке на пробу.

  ЗАЧЕМ ОТДЕЛЬНЫЙ ПРИБОР, КОГДА ЕСТЬ `dfn_scene_check --ground`. Тот кладёт в
  мир ТОЛЬКО [pad] и [river] сцены и НЕ ЧИТАЕТ .relief вовсе. Для стенда с
  падами это верно, а для Вайтрана — другой город: его форма почти вся живёт в
  слое relief (943 КБ дельт против одного terrain-блока), и ответ отличается на
  метры. Разница не косметическая: именно на ней приёмка города три волны
  подряд говорила «дома сидят верно», пока владелец смотрел на дома, утопленные
  по треть этажа.

  ...И ВТОРОЕ: ПАЧКАМИ. Приёмка меряет пятна двадцати шести домов сеткой шагом
  метр — это тысячи проб. Отдельный запуск на пробу стоил бы построения мира на
  каждую, то есть минуты вместо секунды, и прибор бы не звали.

Usage:
    probe_ground <scene.scene>   < пробы "x z" (по паре на строку)
  Не регистрируется в CMake НАМЕРЕННО — как tools/dump_heightmap.cpp и
  tools/measure_shadow_jitter.cpp: это измерительный прибор, а не часть игры.
  Сборка поверх настроенного дерева:

    clang++ -std=c++23 -O2 -I . -I build_lead/dfn_generated \
        -isystem build_lead/_deps/glm-src tools/probe_ground.cpp \
        build_lead/engine/world/libdfn_world.a build_lead/engine/core/libdfn_core.a \
        -o <scratch>/probe_ground

  Запускать ИЗ КОРНЯ репозитория: стенд грузится относительным путём, как в
  тестах, поэтому измеряемый мир — тот же самый.

  Стенд взят Gallery — тот, что объявляют карты города (`source = stand:Gallery`).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПРИБОР НЕ ЗНАЕТ НИ ОДНОГО ЧИСЛА ГОРОДА. Он отвечает «какая здесь земля» и
  ничего не считает сам: пороги, пятна и вердикты живут у того, кто спрашивает
  (tools/gen_whiterun.py). Прибор, знающий ожидаемый ответ, перестаёт быть
  второй независимой дорогой к числу.
*/
/*
UPD:
- 23:08:2026 - 21:30:00: Создан для приёмки «утопленности» домов Вайтрана —
  нужна была земля, КОТОРУЮ ПОСТРОИЛ ДВИЖОК, а не поле высот генератора.
- 23:08:2026 - 23:55:00: Им же снимается НАТУРАЛЬНАЯ земля под дельты relief
  (выпуск WHITERUN_BARE=1, сетка 256x256): файл /tmp/whiterun_natural.txt до
  сих пор был снимком разовой руки, которого в дереве не писал никто, и его
  расхождение со свежим замером доходило до 0.481 м — то есть ровно на столько
  врал весь рельеф города.
*/
#include "engine/core/config/sources/Constants.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/ReliefLayer.h"
#include "engine/world/sources/Scene.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/world/sources/WorldgenForest.h"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace dfn;
using namespace dfn::world;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <scene>  < \"x z\" pairs on stdin\n", argv[0]);
        return 2;
    }
    const std::filesystem::path scene_path = argv[1];
    SceneDoc doc;
    std::string error;
    if (!read_scene(scene_path, doc, error)) {
        std::fprintf(stderr, "[scene] %s\n", error.c_str());
        return 1;
    }
    WorldGenParams params;
    params.seed = 1;
    params.min_chunk = {0, 0};
    params.max_chunk = {7, 7};
    const auto lr = load_layout_file(
        "games/daggerfall_n/assets/world/testbed_layout.json", params.layout);
    if (!lr.ok) {
        std::fprintf(stderr, "[layout] %s (run from the repo root)\n", lr.error.c_str());
        return 1;
    }
    params.layout = gallery_stand_layout();
    for (const ScenePad& P : doc.pads) {
        BuildingPad pad;
        pad.center = P.center;
        pad.half_extents = P.half_extents;
        pad.radius = P.radius;
        pad.blend = P.blend;
        pad.height = P.height;
        params.composed_pads.push_back(pad);
    }
    for (const SceneRiver& R : doc.rivers) {
        RiverChannel ch;
        ch.points = R.points;
        ch.width_m = R.width_m;
        ch.depth_m = R.depth_m;
        ch.bank_m = R.bank_m;
        params.composed_rivers.push_back(std::move(ch));
    }
    // THE SIDECAR IS NOT OPTIONAL WHEN THE SCENE NAMES ONE. A missing file is
    // an error in read_relief for the same reason it is one here: answering
    // "no edits" would make a lost terrain layer look like a map that moved by
    // itself, and the measurement would be confidently wrong rather than loud.
    if (!doc.relief.empty()) {
        const std::filesystem::path rp = scene_path.parent_path() / doc.relief;
        if (!read_relief(rp, params.composed_relief, error)) {
            std::fprintf(stderr, "[relief] %s: %s\n", rp.string().c_str(),
                         error.c_str());
            return 1;
        }
        std::fprintf(stderr, "[relief] %s loaded\n", rp.string().c_str());
    } else {
        std::fprintf(stderr, "[relief] the scene declares none\n");
    }
    const WorldGenContext gen = build_world_context(params);
    double x = 0.0;
    double z = 0.0;
    while (std::scanf("%lf %lf", &x, &z) == 2) {
        std::printf("%.4f\n",
                    static_cast<double>(terrain_height(
                        gen, {static_cast<float>(x), static_cast<float>(z)})));
    }
    return 0;
}
