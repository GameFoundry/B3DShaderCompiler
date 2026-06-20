// xsc-args: -Xopaque-struct ON
// Test: chained function calls passing opaque-bearing struct through.

struct Bundle { Texture2D tex; SamplerState samp; };

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

float4 deepSample(Bundle b, float2 uv)
{
    return b.tex.Sample(b.samp, uv);
}

float4 helper(Bundle b, float2 uv)
{
    // Pass our parameter through to another opaque-bearing function.
    return deepSample(b, uv) * 0.5;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle b = { g_tex, g_samp };
    return helper(b, uv);
}
