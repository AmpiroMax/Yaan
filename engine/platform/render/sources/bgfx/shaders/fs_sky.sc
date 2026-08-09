$input v_dir

// Sky fragment (stage 3): horizon->zenith gradient with a haze band at the
// horizon (matching the fog color so distant terrain melts into the sky), plus
// a small sun disc and glow along the environment sun direction.

#include <bgfx_shader.sh>
#include "dfn_env.sh"

void main()
{
    vec3 dir = normalize(v_dir);
    float up = clamp(dir.y, 0.0, 1.0);
    float horizon_band = pow(1.0 - up, 3.0);
    vec3 sky = mix(u_skyZenith, u_skyHorizon, horizon_band);

    float sun_dot = max(dot(dir, u_sunDir), 0.0);
    sky += u_sunColor * (pow(sun_dot, 900.0) * 0.85 + pow(sun_dot, 24.0) * 0.10);

    gl_FragColor = vec4(sky, 1.0);
}
