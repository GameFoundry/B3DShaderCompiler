// xsc-args: -Xopaque-struct ON
// Test: a sub-struct used as the SOURCE of a copy. `TexBundle b = m.albedo;` imports the
// dotted alias entries from m ("albedo.tex" -> "tex", "albedo.samp" -> "samp") into the
// new local b, which is then passed to a function expecting a TexBundle. Exercises the
// sub-struct copy-init path (InitAliasFromInitializer Case A with a non-empty source path).

struct TexBundle { Texture2D tex; SamplerState samp; };
struct Material  { TexBundle albedo; float4 tint; };

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

float4 sampleBundle(TexBundle b, float2 uv)
{
    return b.tex.Sample(b.samp, uv);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Material m;
    m.albedo.tex  = g_tex;
    m.albedo.samp = g_samp;
    m.tint        = float4(1, 1, 1, 1);

    TexBundle b = m.albedo;          // sub-struct as the copy SOURCE
    return sampleBundle(b, uv) * m.tint;
}
