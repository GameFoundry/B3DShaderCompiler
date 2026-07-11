// xsc-args: -Xopaque-struct ON
// Nested aggregate braces follow the cached source-to-residual slot mapping.

struct Bundle
{
    Texture2D    tex[2];
    SamplerState samp;
    float4       tint;
};

Texture2D    g_tex[2] : register(t0);
SamplerState g_samp   : register(s0);

float4 shade(Bundle values[2], float2 uv)
{
    return values[1].tex[1].Sample(values[1].samp, uv) * values[1].tint;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle values[2] =
    {
        { { g_tex[0], g_tex[1] }, g_samp, float4(0.5, 0.5, 0.5, 1.0) },
        { { g_tex[1], g_tex[0] }, g_samp, float4(1.0, 0.5, 0.5, 1.0) }
    };
    return shade(values, uv);
}
