// xsc-args: -Xopaque-struct ON
// Negative test: a function whose return paths bind an opaque field to DIFFERENT globals.
// The joined return summary is ambiguous for that field, so reading it through the
// returned value in the caller must be rejected with a clear error.

struct Bundle { Texture2D tex; SamplerState samp; };

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

Bundle pick(float2 uv)
{
    Bundle b;
    b.samp = g_samp;
    if (uv.x > 0.5)
    {
        b.tex = g_texA;
        return b;
    }
    b.tex = g_texB;
    return b;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle b = pick(uv);
    return b.tex.Sample(b.samp, uv);   // ambiguous: g_texA or g_texB
}
