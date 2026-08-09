$input v_color0, v_normal, v_texcoord0, v_wpos

// Foliage fragment shader: ALPHA CUTOUT (discard), never blending — cutout
// needs no back-to-front sorting and does not fight the palette post.
// Two-sided lighting: a leaf card seen from behind must be lit, not black.

#include <bgfx_shader.sh>
#include "dfn_env.sh"
#include "dfn_shadow.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_params; // x: texture bound

#define FOLIAGE_ALPHA_CUTOFF 0.5

// --- Leaf translucency (в: the user's three reference photos are ALL shot
// into the light, and in every one the back-lit leaves are BRIGHTER than the
// sky-facing ones — the crown reads as luminous, not as a dark mass, with the
// branches as dark silhouettes against it).
//
// Forward-scattering approximation: light that passed THROUGH a leaf keeps
// going roughly the way it was travelling, so the glow peaks when the viewer
// looks along the sun's direction of travel.
//
// EXAGGERATED ON PURPOSE, and this is the interesting constraint: the palette
// post has 8 shades per ramp, so a "subtle" +10% glow lands on the SAME index
// and is wasted work. Worse, sub-step differences come out as Bayer dither,
// which on few-pixel leaf cards reads as noise rather than light. The value
// therefore moves ~1.6x, and the tint pushes the albedo WARM — a ramp change
// (green -> gold) is a far stronger signal in a 64-colour palette than any
// shade step within one ramp, and it is exactly what the photos show.
#define FOLIAGE_TRANSMIT_STRENGTH 1.60
#define FOLIAGE_TRANSMIT_SHARPNESS 3.0
#define FOLIAGE_TRANSMIT_TINT vec3(1.35, 1.05, 0.55)
// Back-lit leaves INSIDE the canopy's own shadow still glow: the light that
// reaches them has passed through one or two other leaves and is still largely
// transmitted, which is why a whole crown glows and not just its outer shell.
// Our 0.156 m shadow texel cannot resolve which interior leaf is lit, so
// multiplying transmission by shadow visibility would kill the effect for
// exactly the leaves that make the crown luminous. Deliberate physical fudge.
#define FOLIAGE_TRANSMIT_SHADOW_FLOOR 0.45

void main()
{
    dfn_screen_door(u_params.y, gl_FragCoord.xy); // LOD cross-fade / per-draw dissolve
    vec4 tex = texture2D(s_texColor, v_texcoord0);
    // Untextured fallback (mask not resident): draw the card solid rather than
    // discarding everything, so a missing texture reads as a bug, not as a
    // vanished forest.
    float alpha = mix(1.0, tex.a, step(0.5, u_params.x));
    if (alpha < FOLIAGE_ALPHA_CUTOFF) {
        discard;
    }
    vec3 albedo = mix(vec3(0.28, 0.40, 0.18), tex.rgb, step(0.5, u_params.x));
    // Per-card value jitter (vertex colour BLUE). The photos show one crown
    // carrying bright rim, mid tones and deep shade at once, and neighbouring
    // trees differing strongly; a single flat tone per species would throw
    // that away. Flora jitters this per card.
    albedo *= mix(0.78, 1.26, v_color0.b);

    vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 n = normalize(v_normal);
    // Flip toward the viewer: a flat card has no meaningful back face.
    n = dot(n, eye - v_wpos) < 0.0 ? -n : n;

    float vis = dfn_shadow_factor(v_wpos, n);
    vec3 lit = albedo * dfn_surface_light(v_wpos, n, vis, v_color0.a);

    // Translucency: peaks looking INTO the sun, and only where the sun is on
    // the far side of the card (a front-lit leaf does not glow, it is just lit).
    vec3 view_dir = normalize(v_wpos - eye);
    float forward = pow(max(dot(view_dir, -u_sunDir), 0.0), FOLIAGE_TRANSMIT_SHARPNESS);
    float back_lit = max(-dot(n, u_sunDir), 0.0);
    float transmit = forward * back_lit
                   * mix(FOLIAGE_TRANSMIT_SHADOW_FLOOR, 1.0, vis);
    lit += albedo * FOLIAGE_TRANSMIT_TINT * u_sunColor
           * (transmit * FOLIAGE_TRANSMIT_STRENGTH);

    float fog = dfn_fog_factor(v_wpos);
    gl_FragColor = vec4(mix(lit, u_fogColor, fog), 1.0);
}
