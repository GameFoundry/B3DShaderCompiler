cbuffer Draw : register(b0)
{
    uint textureIndex;
};

SamplerState samplerObject : register(s0);

float4 PS(float2 texcoord : TEXCOORD0) : SV_Target
{
    return ResourceDescriptorHeap[textureIndex].Sample(samplerObject, texcoord);
}
