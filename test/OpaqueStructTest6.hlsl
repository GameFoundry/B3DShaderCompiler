// xsc-args: -Xopaque-struct ON
// Test: straight-line reassignment of an opaque field.
// The struct's tex is used once before reassignment, and once after.
// Generated GLSL should sample g_texA in the first call and g_texB in the second.

struct Combined
{
    Texture2D    tex;
    SamplerState samp;
};

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

float4 sampleIt(Combined c, float2 uv)
{
    return c.tex.Sample(c.samp, uv);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Combined c;
    c.tex  = g_texA;
    c.samp = g_samp;

    float4 first = sampleIt(c, uv);

    // Re-point the same struct field at a different global texture.
    c.tex = g_texB;

    float4 second = sampleIt(c, uv);

    return first + second;
}
