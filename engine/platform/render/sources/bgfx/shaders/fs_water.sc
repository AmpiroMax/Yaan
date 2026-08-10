$input v_texcoord0, v_wpos

// Water fragment (stage 3): semi-transparent animated surface — two scrolled
// samples of the procedural water texture over the environment water color,
// sun/ambient lit, fog-aware (fades toward the sky like terrain). Rendered
// with alpha blend + no depth write (backend state for the "water" program).

#include <bgfx_shader.sh>
#include "dfn_env.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_params;

void main()
{
    vec2 uv1 = v_texcoord0 + u_waterScroll * u_envTime;
    vec2 uv2 = v_texcoord0 * 1.7 - u_waterScroll.yx * (u_envTime * 0.8);
    vec3 t1 = texture2D(s_texColor, uv1).rgb;
    vec3 t2 = texture2D(s_texColor, uv2).rgb;
    vec3 waves = t1 * 0.6 + t2 * 0.4;

    vec3 base = mix(u_waterColor.rgb, waves, step(0.5, u_params.x) * 0.65);
    // Cloud shadow (W4): the same coverage field the terrain darkens under.
    // Without this a crawling shadow SKIPS every lake and river — a bright
    // hole in the middle of the moving patch, which is the two-samplers
    // disagreement the one-field rule exists to kill.
    vec3 lit = base * (u_ambientColor
                       + u_sunColor * (max(u_sunDir.y, 0.0)
                                       * dfn_cloud_sun_vis(v_wpos)));

    float fog = dfn_fog_factor(v_wpos);
    float alpha = u_waterColor.a * (1.0 - fog * 0.6);
    gl_FragColor = vec4(mix(lit, u_fogColor, fog), alpha);
}
