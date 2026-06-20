// xsc-args: -Xopaque-struct ON
// Test: function takes two opaque-bearing structs.

struct CombinedTexture
{
    Texture2D    tex;
    SamplerState samp;
};

Texture2D    g_diffuse  : register(t0);
SamplerState g_dsamp    : register(s0);
Texture2D    g_normal   : register(t1);
SamplerState g_nsamp    : register(s1);

float4 mix2(CombinedTexture a, CombinedTexture b, float2 uv)
{
    return a.tex.Sample(a.samp, uv) + b.tex.Sample(b.samp, uv);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    CombinedTexture d = { g_diffuse, g_dsamp };
    CombinedTexture n = { g_normal,  g_nsamp };
    return mix2(d, n, uv);
}
