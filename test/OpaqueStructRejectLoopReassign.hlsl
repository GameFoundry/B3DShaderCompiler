// xsc-args: -Xopaque-struct ON
// Negative test: an opaque field reassigned inside a loop body cannot be resolved to a single
// global at a use after the loop (the loop may run zero or more times, so the binding is not
// statically known). The resolver conservatively marks loop-reassigned opaque fields ambiguous
// at the join; reading 'tex' afterward must be rejected with a clear error.

struct Combined { Texture2D tex; SamplerState samp; };

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Combined c;
    c.tex  = g_texA;
    c.samp = g_samp;

    for (int i = 0; i < 4; ++i)
        c.tex = g_texB;     // reassigned in a loop -> ambiguous after the loop

    return c.tex.Sample(c.samp, uv);
}
