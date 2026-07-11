// xsc-args: -Xopaque-struct ON
struct Bundle { Texture2D tex; };
Texture2D g_texA : register(t0);
Texture2D g_texB : register(t1);
SamplerState g_samp : register(s0);
float4 sampleRuntime(Bundle values[2], int index, float2 uv)
{
    return values[index].tex.Sample(g_samp, uv);
}
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle values[2];
    values[0].tex = g_texA;
    values[1].tex = g_texB;
    return sampleRuntime(values, int(uv.x > 0.5), uv);
}
