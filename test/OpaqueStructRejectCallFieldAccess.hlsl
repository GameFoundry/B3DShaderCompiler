// xsc-args: -Xopaque-struct ON
// Should be rejected: opaque field accessed directly on a function-call result. Resolving
// it would have to discard the call; the user must assign the result to a local first.

struct Bundle { Texture2D tex; SamplerState samp; };

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

Bundle makeBundle()
{
    Bundle b;
    b.tex  = g_tex;
    b.samp = g_samp;
    return b;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    return makeBundle().tex.Sample(g_samp, uv);
}
