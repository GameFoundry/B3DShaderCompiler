// xsc-args: -Xopaque-struct ON
// Runtime indexing is legal when every fixed element is a slice of one resource array.

struct Bundle
{
    Texture2D tex;
};

Texture2D    g_tex[2] : register(t0);
SamplerState g_samp   : register(s0);

float4 sampleRuntime(Bundle bundles[2], int index, float2 uv)
{
    return bundles[index].tex.Sample(g_samp, uv);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle bundles[2];
    bundles[0].tex = g_tex[0];
    bundles[1].tex = g_tex[1];
    return sampleRuntime(bundles, int(uv.x > 0.5), uv);
}
