// xsc-args: -Xopaque-struct ON
// Test: a function returns a NESTED opaque-bearing struct whose inner bundle comes from a
// parameter. The return summary carries dotted keys ("albedo.tex") whose targets are the
// callee's synthesized opaque parameters; the call-site translation must map them to the
// globals the caller passed, and the caller then samples through the returned value.

struct TexBundle { Texture2D tex; SamplerState samp; };
struct Material  { TexBundle albedo; float4 tint; };

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

Material makeMaterial(TexBundle b)
{
    Material m;
    m.albedo = b;                    // sub-struct copy: dotted aliases from the parameter
    m.tint   = float4(0.5, 0.5, 0.5, 1);
    return m;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    TexBundle b;
    b.tex  = g_tex;
    b.samp = g_samp;

    Material m = makeMaterial(b);
    return m.albedo.tex.Sample(m.albedo.samp, uv) * m.tint;
}
