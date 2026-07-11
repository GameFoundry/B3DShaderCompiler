// xsc-args: -Xopaque-struct ON
struct Bundle { Texture2D tex; SamplerState samp; };
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
Bundle makeBundle()
{
    Bundle value;
    value.tex = g_tex;
    value.samp = g_samp;
    return value;
}
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    return makeBundle().tex.Sample(g_samp, uv);
}
