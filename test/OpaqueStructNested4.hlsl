// xsc-args: -Xopaque-struct ON
// Test: copy-initialization of a nested opaque-bearing struct. The whole alias map
// (with dotted keys like "albedo.tex") must be propagated to the destination variable,
// which is then decomposed at the call site.

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
    Material src;
    src.albedo.tex  = g_tex;
    src.albedo.samp = g_samp;
    src.tint        = float4(1, 1, 1, 1);

    Material dst = src;          // copy: dotted alias map propagates to dst
    return shade(dst, uv);
}
