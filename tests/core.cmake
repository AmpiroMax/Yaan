#
# File: tests/core.cmake
#
# Responsibility:
# - Test registration for the core zone (engine/core + engine/world). Included
#   by tests/CMakeLists.txt (lead scaffold); uses add_dfn_test().
#
# Dependencies:
# - Uses: add_dfn_test (tests/CMakeLists.txt), dfn_core, dfn_world.
# - Used by: ctest.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Zone-owned (core agent, Rule 25).

add_dfn_test(test_ecs core/EcsTests.cpp dfn_core)
add_dfn_test(test_house_passability core/HousePassabilityTests.cpp dfn_world)
add_dfn_test(test_json core/JsonTests.cpp dfn_core)
add_dfn_test(test_time core/TimeTests.cpp dfn_core)
add_dfn_test(test_events core/EventBusTests.cpp dfn_core)
add_dfn_test(test_math core/MathTests.cpp dfn_core)
add_dfn_test(test_worldgen_determinism core/WorldgenTests.cpp dfn_world)
add_dfn_test(test_worldgen_v2 core/WorldgenV2Tests.cpp dfn_world)
add_dfn_test(test_forest_stand core/ForestStandTests.cpp dfn_world)
add_dfn_test(test_chunk_streaming core/ChunkManagerTests.cpp dfn_world)
add_dfn_test(test_voxel core/VoxelTests.cpp dfn_world)
add_dfn_test(test_world_format core/WorldFormatTests.cpp dfn_world)
add_dfn_test(test_scene core/SceneTests.cpp dfn_world)

# ПОСТРОЙКА КАК ГИПЕРГРАФ, первый срез. Держит то, ради чего модель затевалась:
# связи существуют и их видно (выбрал вершину — вот её элементы), а занятую
# вершину нельзя удалить молча. Гиперребро проверяется отдельно от мультиребра:
# это разные вещи, и путать их — значит однажды разложить пол на пары и потерять
# «выбрал пол — подсветились его вершины».
add_dfn_test(test_house_graph core/HouseGraphTests.cpp dfn_world)

# ГЕОМЕТРИЯ ПОСТРОЙКИ. Отдельный рукав от test_house_graph, и разделение по
# предмету, а не по размеру: тот держит СВЯЗИ (кто на ком висит), этот — ТЕЛА
# (что нарисовано и во что упирается игрок). Одна модель может быть верной, а
# геометрия из неё неверной, и наоборот; общий рукав скрыл бы, которая из двух.
add_dfn_test(test_house_mesh core/HouseMeshTests.cpp dfn_world)
# ПЛЕЧО НЕНУЛЕВОЙ ДОЗЫ ФАСКИ (критерий К4 ТЗ материалов). Отдельным файлом от
# test_house_mesh нарочно: тот меряет прежнюю острую геометрию числами и
# зовётся с 0.0f — у двух плеч разные поводы к правке, и склеенные в один файл
# они правились бы вместе, то есть плечо нулевой дозы поехало бы за фаской.
add_dfn_test(test_house_bevel core/HouseBevelTests.cpp dfn_world)
# ЛЕСТНИЦА ДАЛЬНИХ ФОРМ (И13). Отдельным рукавом от test_house_bevel и по той
# же причине, по которой тот отделён от test_house_mesh: у выбора ступени и у
# геометрии ступени разные поводы к правке. Здесь же живёт отрицательное плечо
# всей волны — полная форма обязана остаться прежней ПОБАЙТОВО.
add_dfn_test(test_house_lod core/HouseLodTests.cpp dfn_world)
add_dfn_test(test_house_style core/HouseStyleTests.cpp dfn_world)
add_dfn_test(test_layout_load core/LayoutLoadTests.cpp dfn_world)
add_dfn_test(test_ground_relief core/GroundReliefTests.cpp dfn_world)
add_dfn_test(test_mountain_horizon core/MountainHorizonTests.cpp dfn_world)
add_dfn_test(test_great_oak core/GreatOakTests.cpp dfn_world)
add_dfn_test(test_coarse_lod core/CoarseLodTests.cpp dfn_world)
add_dfn_test(test_height_lattice core/HeightLatticeTests.cpp dfn_world)
add_dfn_test(test_lod_seam core/LodSeamTests.cpp dfn_world dfn_render)
# Links dfn_render for the SAME reason test_lod_seam does: it checks two zones
# agree about one geometry (the occluder discs are sized from render's own
# species table, never from literals copied into the test).
add_dfn_test(test_find_occlusion core/FindOcclusionTests.cpp dfn_world dfn_render)
add_dfn_test(core_scene_joint_rules core/SceneJointRuleTests.cpp dfn_world)
add_dfn_test(core_scene_house_rules core/SceneHouseRuleTests.cpp dfn_world)
add_dfn_test(core_scene_stair_rules core/SceneStairRuleTests.cpp dfn_world)

# РЕЕСТР ВЕЩЕСТВ КАК ДАННЫЕ (зона МАТЕРИАЛЫ, волна 3, 28.08). Рукав ядра, а не
# рисовальщика, и разделение по предмету: здесь проверяется САМ РЕЕСТР —
# загрузка файла, отказ вслух на неизвестном имени и битой строке, личность
# записи по содержимому и сверка с таблицей физики (ворота против того, чтобы
# два списка веществ снова разъехались). Как это вещество ВЫГЛЯДИТ — вопрос
# render_material_registry, и склеенные в один файл они правились бы вместе.
add_dfn_test(core_material_table core/MaterialTableTests.cpp dfn_core)

# РЕЦЕПТ ДОМА НАЗЫВАЕТ ВЕЩЕСТВО ИМЕНЕМ (зона МАТЕРИАЛЫ, волна 3, 28.08).
# Отдельный рукав от test_house_mesh: тот меряет ГЕОМЕТРИЮ (что построено),
# этот — СЛОВАРЬ (из чего сказано, что оно сделано). Главное утверждение —
# «имя и координаты дают одну клетку»: разойдись эти два пути, перевод 194
# рецептов на имена стал бы перекраской города, а не переименованием.
add_dfn_test(core_house_material core/HouseMaterialTests.cpp dfn_world)
