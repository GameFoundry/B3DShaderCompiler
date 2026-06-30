// xsc-args: -Xopaque-struct ON
// Test: passing a nested opaque-bearing sub-struct (`m.albedo`) directly to a function
// that takes the inner struct type. Two different bundles are passed through the same
// helper, each resolving to its own globals.

struct TexBundle
{
    Texture2D    tex;
    SamplerState samp;
};

struct Material
{
    TexBundle albedo;
    TexBundle normal;
    float4    tint;
};

Texture2D    g_albedoTex : register(t0);
Texture2D    g_normalTex : register(t1);
SamplerState g_samp      : register(s0);

float4 sampleBundle(TexBundle b, float2 uv)
{
    return b.tex.Sample(b.samp, uv);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Material m;
    m.albedo.tex  = g_albedoTex;
    m.albedo.samp = g_samp;
    m.normal.tex  = g_normalTex;
    m.normal.samp = g_samp;
    m.tint        = float4(1, 1, 1, 1);

    float4 a = sampleBundle(m.albedo, uv);
    float4 n = sampleBundle(m.normal, uv);
    return (a + n) * m.tint;
}
