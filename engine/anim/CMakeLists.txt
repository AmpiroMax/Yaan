#
# Created: 10:08:2026 - 01:56:45
# Last updated: 10:08:2026 - 01:56:45
# File: engine/anim/CMakeLists.txt
#
# Responsibility:
# - dfn_anim static library: humanoid rig, procedural clips, rigid-segmented
#   body + mirror puppet. Zone-owned by character (Rule 25, carved 10:08:2026).
#
# Dependencies:
# - Uses: dfn_headers, dfn_core (ecs/components/config). Platform interface
#   headers only (Rule 1); no backend, no engine/render, no engine/gameplay.
# - Used by: engine/app wiring, tests/character.cmake.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. anim sits below gameplay in the DAG:
#   linking dfn_gameplay or dfn_render here is a layering violation.
#
# UPD:
# - 10:08:2026 - 01:56:45: Initial library (rig, pose, clips, body mesh, body).

add_library(dfn_anim STATIC
    sources/Rig.cpp
    sources/Pose.cpp
    sources/Clips.cpp
    sources/BodyMesh.cpp
    sources/Body.cpp)

target_link_libraries(dfn_anim PUBLIC dfn_headers dfn_core)
