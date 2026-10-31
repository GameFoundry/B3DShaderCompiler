// xsc-args: -Xopaque-struct ON
// Bare textures and samplers returned from functions, including runtime-indexed resource arrays, forwarded parameters,
// and nested calls.

cbuffer Params : register(b0) { int g_index; };

Texture2D    g_texA        : register(t0);
Texture2D    g_texArray[2] : register(t1);
SamplerState g_samp        : register(s0);

Texture2D GetTexture() { return g_texA; }
SamplerState GetSampler() { return g_samp; }
SamplerState Forward(SamplerState value) { return value; }

Texture2D GetIndexed(int index) { int local = index + 1; return g_texArray[local - 1]; }
Texture2D GetIndexedNested(int index) { return GetIndexed(index * 2); }

Texture2D Choose(bool first)
{
    if (first)
        return g_texA;
    return g_texA;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    int index = g_index;
    float4 result = GetTexture().Sample(GetSampler(), uv);
    result += GetIndexed(index).Sample(Forward(g_samp), uv);
    result += GetIndexedNested(index).Sample(Forward(GetSampler()), uv);

    result += Choose(uv.y > 0.5).Sample(g_samp, uv);

    SamplerState localSampler = GetSampler();
    return result + g_texA.Sample(localSampler, uv);
}
