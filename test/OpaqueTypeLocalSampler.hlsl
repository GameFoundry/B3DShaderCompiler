// xsc-args: -Xopaque-struct ON
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    SamplerState localSampler = g_samp;
    return g_tex.Sample(localSampler, uv);
}
