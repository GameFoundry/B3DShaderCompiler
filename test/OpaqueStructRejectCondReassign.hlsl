// xsc-args: -Xopaque-struct ON
// Negative test: conditional reassignment of an opaque field must be rejected
// with a clear error. The use after the if/else is ambiguous because the
// field could refer to either g_texA or g_texB.

struct Combined
{
    Texture2D    tex;
    SamplerState samp;
};

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Combined c;
    c.tex  = g_texA;
    c.samp = g_samp;

    if (uv.x > 0.5)
        c.tex = g_texB;

    // Ambiguous: could be g_texA or g_texB depending on branch taken.
    return c.tex.Sample(c.samp, uv);
}
