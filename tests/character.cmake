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

# КЛИПЫ, КОТОРЫЕ ПРОИГРЫВАЮТСЯ (волна 31.08). Тот же довод, что у набора
# выше — читает ФАЙЛ, потому что мерить скольжение ноги можно только на
# настоящем клипе настоящей модели, и потому линкует dfn_render ради читателя
# .dfo. Контрольная рука («без подгонки шага») живёт ВНУТРИ набора: судья,
# у которого обе руки зелёные, неотличим от судьи, который ничего не мерит.
add_dfn_test(character_clips_played character/ClipPlayerTests.cpp
             dfn_anim dfn_render dfn_core)
if(TARGET dfn_characters)
    add_dependencies(character_clips_played dfn_characters)
endif()

# СНОС ОПОРНОЙ СТОПЫ — ИЗВЕСТНЫЙ ДЕФЕКТ НА ТЕЛЕ MPFB, И ОН КРАСНЫЙ НАРОЧНО.
# Подгонка шага (stride scale) и выбор клипа по сносу уходят по заказу
# владельца 02.09 («ноги твёрдо стоят на земле — искоренить на глубоком
# уровне»): их заменяет заземлённая локомоция (root motion из клипа, запертая
# опорная стопа, FootIk — docs/design/LOCOMOTION_GROUNDED.md), отдельная
# волна. Прибор вынесен в свой рукав и помечен как sim_great_oak_stair, чтобы
# прогон мог спросить состояние дерева БЕЗ него (`ctest -LE known-defect`);
# чего быть не должно — ослабления полос ради зелёного свода: зелёный здесь —
# событие, о котором этот набор и существует, чтобы объявить.
add_dfn_test(character_clips_slide character/ClipSlideTests.cpp
             dfn_anim dfn_render dfn_core)
set_tests_properties(character_clips_slide PROPERTIES LABELS "known-defect")

# ПРИБОРЫ ЛОКОМОЦИИ НА СИНТЕТИКЕ (владелец 04.09: «нужны инструменты внутри
# игры, что числами расскажут о проблемах»): каждый детектор LocoTelemetry
# получает тик с заложенным дефектом и обязан его засчитать сверх порога реестра.
add_dfn_test(character_loco_telemetry character/LocoTelemetryTests.cpp
             dfn_anim dfn_core)
if(TARGET dfn_characters)
    add_dependencies(character_clips_slide dfn_characters)
endif()

# ХИТБОКСЫ ЧАСТЕЙ ТЕЛА (волна «стойка, оружие, стопы, хитбоксы»). Тот же довод,
# что у двух наборов выше: читает ФАЙЛ, потому что покрытие силуэта — это
# утверждение о НАСТОЯЩЕМ скине настоящей модели, и потому линкует dfn_render
# ради читателя .dfo.
add_dfn_test(character_hitboxes character/HitboxTests.cpp
             dfn_anim dfn_render dfn_core)
if(TARGET dfn_characters)
    add_dependencies(character_hitboxes dfn_characters)
endif()

# СТОЙКА ПО РЕФЕРЕНСУ (волна сверки со Skyrim). Тот же довод, что у наборов
# выше: читает ФАЙЛ, потому что стойка — это утверждение о НАСТОЯЩЕМ клипе
# настоящей модели. Контрольная рука («тот же клип без слоёв») живёт ВНУТРИ
# набора и обязана вылетать из тех же вилок: приёмка, которую нечем провалить,
# ничего не меряет.
add_dfn_test(character_stance character/StanceTests.cpp
             dfn_anim dfn_render dfn_core)
if(TARGET dfn_characters)
    add_dependencies(character_stance dfn_characters)
endif()

# РЕЕСТР ПОЗ И ГРАФ ПЕРЕХОДОВ (волна «позы и переходы»). Файла не читает:
# реестр обязан считаться без единого ассета, и набор это доказывает тем, что
# линкует только dfn_anim и dfn_core.
add_dfn_test(character_pose_library character/PoseLibraryTests.cpp dfn_anim dfn_core)
