#
# File: tests/render.cmake
#
# Responsibility:
# - Registers the render zone's tests (Rule 25: zone-owned; included by the
#   lead's tests/CMakeLists scaffold via add_dfn_test).
#
# Dependencies:
# - Uses: add_dfn_test(), dfn_render, dfn_platform_{window,input,render}.
# - Used by: tests/CMakeLists.txt.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. GPU-free tests only (null backends).
add_dfn_test(render_object_registry_house render/ObjectRegistryHouseTests.cpp
    dfn_render dfn_core)
add_dfn_test(render_bitmap_font render/BitmapFontTests.cpp dfn_render)
add_dfn_test(render_terrain_mesher render/TerrainMesherTests.cpp dfn_render)
add_dfn_test(render_proc_mesh render/ProcMeshTests.cpp dfn_render)
add_dfn_test(render_scatter_batcher render/ScatterBatcherTests.cpp dfn_render)
add_dfn_test(render_ground_tufts render/GroundTuftsTests.cpp dfn_render)
add_dfn_test(render_water_mesher render/WaterMesherTests.cpp dfn_render)
add_dfn_test(render_path_mesher render/PathMesherTests.cpp dfn_render dfn_core)
add_dfn_test(render_map_screen render/MapScreenTests.cpp dfn_render)
add_dfn_test(render_sky_model render/SkyModelTests.cpp dfn_render)
add_dfn_test(render_cloud_model render/CloudModelTests.cpp dfn_render)
add_dfn_test(render_terrain_lod render/TerrainLodTests.cpp dfn_render)
add_dfn_test(render_lod_terrain render/LodTerrainTests.cpp
    dfn_render dfn_platform_render dfn_core)
add_dfn_test(render_proc_flora render/ProcFloraTests.cpp dfn_render)
add_dfn_test(render_camera render/CameraTests.cpp dfn_render)
add_dfn_test(render_tour render/TourTests.cpp dfn_render dfn_platform_render)
add_dfn_test(render_null_backends render/NullBackendTests.cpp
    dfn_platform_window dfn_platform_input dfn_platform_render)
add_dfn_test(render_proc_texture render/ProcTextureTests.cpp dfn_render)
add_dfn_test(render_palette render/PaletteTests.cpp dfn_platform_render)
add_dfn_test(render_system render/RenderSystemTests.cpp
    dfn_render dfn_platform_render dfn_core)
add_dfn_test(render_part_forge_joints render/PartForgeJointTests.cpp
    dfn_render dfn_core)
add_dfn_test(render_part_forge_walls render/PartForgeWallTests.cpp
    dfn_render dfn_world dfn_core)
add_dfn_test(render_part_forge_roofs render/PartForgeRoofTests.cpp
    dfn_render dfn_core)
add_dfn_test(render_part_forge_stairs render/PartForgeStairTests.cpp
    dfn_render dfn_core)
add_dfn_test(render_parts_atlas render/PartsAtlasTests.cpp dfn_render dfn_core)
# ПОНЯТИЕ МАТЕРИАЛ (зона МАТЕРИАЛЫ, 28.08): полнота отображения 36 пар листа
# набора (обратная совместимость полки числом), умолчание == ламберт (контрольная
# рука кадра) и разделение золота от штукатурки блином fs_prop.
add_dfn_test(render_material_registry render/MaterialRegistryTests.cpp
    dfn_render dfn_core)
add_dfn_test(render_sign_forge render/SignForgeTests.cpp dfn_render dfn_core)
# ТРЕТИЙ АУДИТ: фаза мерцания факела (жалоба «свет мигает» — сеялась от размера
# пула кандидатов, то есть от светляков) и ревизия листа в ключе кэша плитки.
# Обе функции были в безымянных пространствах и не проверялись ничем; у обоих
# случаев контроль — дофиксная формула, выписанная дословно (правило 39).
add_dfn_test(render_flame_phase render/FlamePhaseTests.cpp
    dfn_render dfn_platform_render dfn_core)

# ВТОРАЯ ИТЕРАЦИЯ ДЕРЕВЬЕВ (28.08): пять разниц записки docs/reports/trees-g3,
# каждая мерится по ПОСТРОЕННОЙ геометрии, а не по полю рецепта, плюс две руки
# неприкосновенности первой итерации (её листва обязана оставаться в зелёной
# полосе атласа; ряды v2 обязаны быть непустыми).
add_dfn_test(render_tree_forge_v2 render/TreeForgeV2Tests.cpp dfn_render dfn_core)

# ЭКРАН ЗАГРУЗКИ (И15 волна А): модель этапов и разность против контрольной
# руки, рисующей тот же экран без слов (правило 47).
add_dfn_test(render_loading_screen render/LoadingScreenTests.cpp dfn_render)
