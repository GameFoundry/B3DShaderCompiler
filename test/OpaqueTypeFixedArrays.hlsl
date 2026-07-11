// xsc-args: -Xopaque-struct ON
// Arrays of opaque-bearing structs, nested axes, and direct opaque array members.

struct Bundle
{
    Texture2D    tex[2];
    SamplerState samp;
    float4       tint;
};

Texture2D    g_tex[2] : register(t0);
SamplerState g_samp   : register(s0);

float4 shade(Bundle bundles[2], float2 uv)
{
    return bundles[1].tex[0].Sample(bundles[1].samp, uv) * bundles[1].tint;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle bundles[2];
    bundles[0].tex[0] = g_tex[0];
    bundles[0].tex[1] = g_tex[1];
    bundles[0].samp   = g_samp;
    bundles[0].tint   = float4(0.5, 0.5, 0.5, 1.0);
    bundles[1]        = bundles[0];
    return shade(bundles, uv);
}
