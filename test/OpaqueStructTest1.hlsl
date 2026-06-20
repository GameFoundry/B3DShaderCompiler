// xsc-args: -Xopaque-struct ON
// Test: opaque-bearing struct passed to a function. FXAA-style bundle pattern.

struct CombinedTexture
{
    Texture2D    tex;
    SamplerState samp;
};

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

float4 sampleHelper(CombinedTexture ct, float2 uv)
{
    return ct.tex.Sample(ct.samp, uv);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    CombinedTexture ct;
    ct.tex  = g_tex;
    ct.samp = g_samp;
    return sampleHelper(ct, uv);
}
