// xsc-args: -Xopaque-struct ON
// Test: same as OpaqueStructTest6 but using direct member-access (no helper function),
// so we exercise the in-place ObjectExpr rewrite path for the post-reassign use.

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

    float4 first = c.tex.Sample(c.samp, uv);

    c.tex = g_texB;

    float4 second = c.tex.Sample(c.samp, uv);

    return first + second;
}
