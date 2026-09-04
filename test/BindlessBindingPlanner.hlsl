// Compiler-generated bindless bindings share space 2 with ordinary source
// resources. With the default generated start slot, occupied slots 0, 2, and 4
// force the resource and sampler descriptor bindings to slots 1 and 3.

Texture2D<float4> fixedTexture : register(t0, space2);
SamplerState fixedSampler : register(s2, space2);

cbuffer Params : register(b4, space2)
{
    uint resourceIndex;
    uint samplerIndex;
    float2 texcoord;
};

float4 PS() : SV_Target
{
    Texture2D<float4> textureObject =
        ResourceDescriptorHeap[NonUniformResourceIndex(resourceIndex)];
    SamplerState samplerObject =
        SamplerDescriptorHeap[NonUniformResourceIndex(samplerIndex)];

    return textureObject.Sample(samplerObject, texcoord) +
        fixedTexture.Sample(fixedSampler, texcoord);
}
