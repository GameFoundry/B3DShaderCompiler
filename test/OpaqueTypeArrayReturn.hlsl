// xsc-args: -Xopaque-struct ON
// A fixed array aggregate returned through a typedef exercises the residual-out ABI.
struct Bundle { Texture2D tex; SamplerState samp; float4 tint; };
typedef Bundle BundleArray[2];
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
BundleArray makeArray()
{
    BundleArray values;
    values[0].tex = g_tex; values[0].samp = g_samp; values[0].tint = float4(0.5, 1, 1, 1);
    values[1] = values[0];
    return values;
}
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    BundleArray values = makeArray();
    float4 direct = makeArray()[0].tex.Sample(g_samp, uv);
    return direct + values[1].tex.Sample(values[1].samp, uv) * values[1].tint;
}
