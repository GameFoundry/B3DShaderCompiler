// xsc-args: -Xopaque-struct ON
// Test: an 'inout' parameter of opaque-bearing struct type. The callee reads the current
// binding (seeded from the split parameters), rebinds one field, and the caller must see
// the updated binding after the call (exit-state copy-back), while the untouched field
// still maps to what was passed in.

struct Combined { Texture2D tex; SamplerState samp; };

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

float4 sampleAndSwap(inout Combined c, float2 uv)
{
    float4 before = c.tex.Sample(c.samp, uv);   // resolves to the passed-in g_texA
    c.tex = g_texB;                             // rebinding flows back to the caller
    return before;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Combined c;
    c.tex  = g_texA;
    c.samp = g_samp;
    float4 first  = sampleAndSwap(c, uv);
    float4 second = c.tex.Sample(c.samp, uv);   // resolves to g_texB after the call
    return first + second;
}
