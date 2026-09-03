cbuffer Draw : register(b0)
{
    uint samplerIndex;
};

Texture2D<float4> textureObject : register(t0);

float4 PS(float2 texcoord : TEXCOORD0) : SV_Target
{
    SamplerState samplerObject = ResourceDescriptorHeap[samplerIndex];
    return textureObject.Sample(samplerObject, texcoord);
}
