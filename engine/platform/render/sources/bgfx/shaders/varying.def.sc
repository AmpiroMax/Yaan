// Daggerfall N — shared varying/attribute definitions for all shaders.
// Compiled by shaderc (CMake step, Q50); see engine/platform/render/CMakeLists.txt.
// Stage 3: added v_wpos (world position for splat/fog) and v_dir (sky ray).

vec4 v_color0    : COLOR0    = vec4(1.0, 1.0, 1.0, 1.0);
vec3 v_normal    : NORMAL    = vec3(0.0, 1.0, 0.0);
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
vec3 v_wpos      : TEXCOORD1 = vec3(0.0, 0.0, 0.0);
vec3 v_dir       : TEXCOORD2 = vec3(0.0, 0.0, 1.0);

vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec4 a_color0    : COLOR0;

// Skinning attributes (character wave, 30.08): the SkinnedVertex layout's
// palette slots and weights. Declared here with everything else because the
// varying def is shared by every program; a shader that does not name them in
// its $input line neither sees nor pays for them.
vec4 a_indices   : BLENDINDICES;
vec4 a_weight    : BLENDWEIGHT;
