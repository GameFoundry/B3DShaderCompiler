// xsc-args: -Xopaque-struct ON
// Test: positive control-flow join. Both arms of an if/else rebind the same opaque field to
// the SAME global, so the join is unambiguous and the later read resolves cleanly. Verifies
// that JoinAliasMaps keeps an agreed alias resolved (rather than over-conservatively marking
// it ambiguous), and that an opaque-field assignment used as an unbraced branch body degrades
// to a valid empty statement after the field is hoisted.

struct Combined { Texture2D tex; SamplerState samp; };

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Combined c;
    c.samp = g_samp;

    if (uv.x > 0.5)
        c.tex = g_texA;
    else
        c.tex = g_texA;     // same global on both arms -> join stays resolved

    return c.tex.Sample(c.samp, uv);
}
