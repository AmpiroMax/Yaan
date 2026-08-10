#
# Created: 09:08:2026 - 00:45:00
# Last updated: 10:08:2026 - 03:14:30
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
#
# UPD:
# - 09:08:2026 - 00:45:00: Stage 2 — mesher, camera, tour, null backend tests.
# - 09:08:2026 - 11:14:00: Stage 3 — proc texture, palette, render system
#   (water/env) tests.
# - 09:08:2026 - 11:57:20: Stage 3b — proc mesh, scatter batcher, water mesher.
# - 09:08:2026 - 17:55:00: Map screen tests (explored chunks, markers, compose).
# - 09:08:2026 - 19:53:00: Sky model tests (sun/moon geometry, night invariants).
# - 09:08:2026 - 19:46:00: Flora generator tests (flora agent's suite).
# - 09:08:2026 - 20:58:00: Terrain LOD tests (ladder, selection, fade window).
# - 09:08:2026 - 22:12:57: LodTerrain tests (the LOD drawing half over the null
#   renderer: no draw before delivery, fade values, release on disable).
# - 09:08:2026 - 23:32:07: Bitmap font tests (glyph coverage + the unmappable
#   control, UTF-8 decoding, alias identity, bitmap uniqueness, clipping).
# - 10:08:2026 - 03:14:30: Cloud model tests (downwind drift, becalmed control).

add_dfn_test(render_bitmap_font render/BitmapFontTests.cpp dfn_render)
add_dfn_test(render_terrain_mesher render/TerrainMesherTests.cpp dfn_render)
add_dfn_test(render_proc_mesh render/ProcMeshTests.cpp dfn_render)
add_dfn_test(render_scatter_batcher render/ScatterBatcherTests.cpp dfn_render)
add_dfn_test(render_water_mesher render/WaterMesherTests.cpp dfn_render)
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
