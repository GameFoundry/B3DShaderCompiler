// xsc-args: -Xopaque-struct ON
// Test: nested opaque-bearing struct. The inner bundle is fully opaque; the outer
// struct adds a POD member. Opaque fields are assigned individually and read via a
// dotted access path inside the called function.

struct TexBundle
{
    Texture2D    tex;
    SamplerState samp;
};

struct Material
{
    TexBundle albedo;
    float4    tint;
};

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

float4 shade(Material m, float2 uv)
{
    return m.albedo.tex.Sample(m.albedo.samp, uv) * m.tint;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Material m;
    m.albedo.tex  = g_tex;
    m.albedo.samp = g_samp;
    m.tint        = float4(1, 1, 1, 1);
    return shade(m, uv);
}
