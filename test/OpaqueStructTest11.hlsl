// xsc-args: -Xopaque-struct ON
// Test: a pure 'out' parameter of opaque-bearing struct type. The parameter is not split
// (nothing is passed in); the callee's writes are tracked against an all-Unset alias map
// and its exit state is copied back into the caller's variable after the call.

struct Bundle { Texture2D tex; SamplerState samp; };

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

void fillBundle(out Bundle b)
{
    b.tex  = g_tex;
    b.samp = g_samp;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle b;
    fillBundle(b);
    return b.tex.Sample(b.samp, uv);
}
