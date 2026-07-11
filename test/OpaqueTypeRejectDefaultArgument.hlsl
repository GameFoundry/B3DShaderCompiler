// xsc-args: -Xopaque-struct ON
struct Bundle { Texture2D tex; SamplerState samp; };
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 sampleBundle(Bundle value = { g_tex, g_samp })
{
    return float4(1, 1, 1, 1);
}
float4 main() : SV_Target
{
    return sampleBundle();
}
