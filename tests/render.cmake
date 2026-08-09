#
# Created: 09:08:2026 - 00:45:00
# Last updated: 09:08:2026 - 00:45:00
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

add_dfn_test(render_terrain_mesher render/TerrainMesherTests.cpp dfn_render)
add_dfn_test(render_camera render/CameraTests.cpp dfn_render)
add_dfn_test(render_tour render/TourTests.cpp dfn_render dfn_platform_render)
add_dfn_test(render_null_backends render/NullBackendTests.cpp
    dfn_platform_window dfn_platform_input dfn_platform_render)
