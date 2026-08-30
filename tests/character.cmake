#
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

add_dfn_test(character_rig_pose character/RigPoseTests.cpp dfn_anim)

add_dfn_test(character_clips character/ClipTests.cpp dfn_anim)

add_dfn_test(character_body character/BodyTests.cpp dfn_anim dfn_core)

add_dfn_test(character_posture character/PostureTests.cpp dfn_anim dfn_core)

# ИМПОРТ И СКИННИНГ (волна 30.08). Единственный набор зоны, который читает
# ФАЙЛ: эталонные числа — это факты о чужом ассете (Khronos RiggedFigure), а не
# о нашей выпечке, и потому линкуется dfn_render ради .dfo-читателя.
add_dfn_test(character_skinning character/SkinningTests.cpp dfn_anim dfn_render dfn_core)
if(TARGET dfn_characters)
    add_dependencies(character_skinning dfn_characters)
endif()
