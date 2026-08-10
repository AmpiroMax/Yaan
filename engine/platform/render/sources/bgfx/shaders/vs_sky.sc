$input a_position, a_texcoord0
$output v_dir

// Sky background pass (stage 3): fullscreen clip-space quad; the world-space
// view ray is reconstructed by unprojecting two depths and taking their
// difference. Drawn first in the scene view (sequential mode), no depth
// test/write; terrain covers it.
//
// THE DIFFERENCE HAS A SIGN, and "convention-agnostic" was the wrong claim:
// any two points on the ray give its LINE, not its DIRECTION. Under this
// project's depth convention the z=1 unprojection lands NEARER than the z=0
// one, so far-minus-near came out pointing back down the ray and every
// consumer of v_dir has been reading the ANTIPODE of where it is looking.
// Measured 10:08:2026: dir.y = -0.702 at the top of a frame pitched +0.12 up.
// What that cost, all of it silent:
//   - the gradient's `up` clamped to 0 for the whole visible sky, so
//     u_skyZenith has never been drawn — the sky was a flat horizon colour;
//   - star_fade clamped to 0, so the star field never appeared at all;
//   - the moon disc drew at the mirror of its true direction;
//   - and the W4 cloud sheets, gated on dir.y > 0, could only appear in the
//     narrow band where the inverted ray still read positive — which is
//     exactly the "sheet only materialises near the horizon, mid-sky empty"
//     the first cloud shoot came back with.
// Sign fixed HERE, at the producer, so no consumer has to know about it.

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position.xy, 0.5, 1.0); // depth irrelevant: no test
    vec4 p_far = mul(u_invViewProj, vec4(a_position.xy, 1.0, 1.0));
    vec4 p_near = mul(u_invViewProj, vec4(a_position.xy, 0.0, 1.0));
    v_dir = p_near.xyz / p_near.w - p_far.xyz / p_far.w;
}
