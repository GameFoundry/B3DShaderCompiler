// Compiler-generated bindless bindings share space 2 with ordinary source
// resources. Explicit source bindings occupy slots 0, 1, and 4, so the bindless
// bindings start at slot 2: the sampler heap at 2 and the resource heap at 3.

Texture2D<float4> fixedTexture : register(t0, space2);
SamplerState fixedSampler : register(s1, space2);

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
