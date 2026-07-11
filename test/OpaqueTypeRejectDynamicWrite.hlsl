// xsc-args: -Xopaque-struct ON
struct Bundle { Texture2D tex; };
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle values[2];
    int index = int(uv.x > 0.5);
    values[index].tex = g_tex;
    return values[0].tex.Sample(g_samp, uv);
}
