// xsc-args: -Xopaque-struct ON
// Plain opaque input parameters, including their native array form, are retained.
Texture2D g_tex[2] : register(t0);
SamplerState g_samp : register(s0);
float4 sampleNative(Texture2D textures[2], SamplerState samplerValue, int index, float2 uv)
{
    return textures[index].Sample(samplerValue, uv);
}
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    return sampleNative(g_tex, g_samp, int(uv.x > 0.5), uv);
}
