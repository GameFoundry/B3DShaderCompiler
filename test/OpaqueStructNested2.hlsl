// xsc-args: -Xopaque-struct ON
// Test: nested opaque-bearing struct where the inner struct ALSO has a POD member, so
// the inner residual survives stripping. Opaque fields and the inner POD field are set
// via field assignment; the inner POD field is read back through the dotted path (and
// must survive on the residual struct).

struct TexBundle
{
    Texture2D    tex;
    SamplerState samp;
    float        weight;
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
    return m.albedo.tex.Sample(m.albedo.samp, uv) * m.albedo.weight * m.tint;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Material m;
    m.albedo.tex    = g_tex;
    m.albedo.samp   = g_samp;
    m.albedo.weight = 0.5;
    m.tint          = float4(1, 1, 1, 1);
    return shade(m, uv);
}
