// xsc-args: -Xopaque-struct ON
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Texture2D localTex = g_tex;
    return localTex.Sample(g_samp, uv);
}
