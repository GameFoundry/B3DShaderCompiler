// xsc-args: -Xopaque-struct ON
// Plain opaque locals use the same symbolic binding machinery as aggregate lanes.

Texture2D      g_tex  : register(t0);
Buffer<float4> g_data : register(t1);
SamplerState   g_samp : register(s0);

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Texture2D      localTex = g_tex;
    Buffer<float4> localData;
    SamplerState   localSampler;
    localData    = g_data;
    localSampler = g_samp;
    return localTex.Sample(localSampler, uv) + localData[0];
}
