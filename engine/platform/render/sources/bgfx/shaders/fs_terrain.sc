$input v_color0, v_normal, v_texcoord0

// Terrain fragment shader: simple directional lambert over the vertex ground
// tint — must read as "ground" in the stage-2 screenshots (Q51).

#include <bgfx_shader.sh>

void main()
{
    vec3 n = normalize(v_normal);
    // Fixed sun from high in the south-east; look-dev value, not gameplay.
    vec3 to_sun = normalize(vec3(0.35, 0.8, 0.45));
    float ndotl = max(dot(n, to_sun), 0.0);
    float light = 0.35 + 0.65 * ndotl;
    gl_FragColor = vec4(v_color0.rgb * light, 1.0);
}
