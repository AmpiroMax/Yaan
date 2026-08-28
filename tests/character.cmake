#
# Created: 10:08:2026 - 01:56:45
# Last updated: 28:08:2026 - 11:16:40
# File: tests/character.cmake
#
# Responsibility:
# - Registers the character zone's test executables (Rule 25 / Q34: zone-owned
#   test registration; the tests/ scaffold itself is lead-owned).
#
# Dependencies:
# - Uses: add_dfn_test() from tests/CMakeLists.txt; dfn_anim, dfn_core.
# - Used by: tests/CMakeLists.txt (conditional include), ctest.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. Character-owned; other zones
#   register in their own <zone>.cmake.
#
# UPD:
# - 10:08:2026 - 01:56:45: Rig/pose math, the gait footfall contract (with the
#                          shifted-clip control), body/puppet ECS suites.
# - 28:08:2026 - 11:16:40: character_posture — позы сидя/лёжа замером (углы
#   суставов числами, стопы на полу с контролем недосягаемого сиденья, глаз
#   позы против стоячей камеры sim).

add_dfn_test(character_rig_pose character/RigPoseTests.cpp dfn_anim)

add_dfn_test(character_clips character/ClipTests.cpp dfn_anim)

add_dfn_test(character_body character/BodyTests.cpp dfn_anim dfn_core)

add_dfn_test(character_posture character/PostureTests.cpp dfn_anim dfn_core)
