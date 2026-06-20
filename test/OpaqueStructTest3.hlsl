// xsc-args: -Xopaque-struct ON
// Test: struct with both POD and opaque members.

struct TexturedColor
{
    Texture2D    tex;
    SamplerState samp;
    float4       tint;
};

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

float4 sampleAndTint(TexturedColor tc, float2 uv)
{
    return tc.tex.Sample(tc.samp, uv) * tc.tint;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    TexturedColor tc = { g_tex, g_samp, float4(0.5, 1.0, 0.5, 1.0) };
    return sampleAndTint(tc, uv);
}
