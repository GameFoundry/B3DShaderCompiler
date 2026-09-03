cbuffer Params : register(b0)
{
    uint   textureIndex;
    uint   samplerIndex;
    uint   shadowTextureIndex;
    uint   shadowSamplerIndex;
    float2 texcoord;
};

float4 PS() : SV_Target
{
    Texture2D<float4> textureObject = ResourceDescriptorHeap[NonUniformResourceIndex(textureIndex)];
    SamplerState samplerObject = SamplerDescriptorHeap[NonUniformResourceIndex(samplerIndex)];
    Texture2D<float> shadowTexture = ResourceDescriptorHeap[NonUniformResourceIndex(shadowTextureIndex)];
    SamplerComparisonState shadowSampler = SamplerDescriptorHeap[NonUniformResourceIndex(shadowSamplerIndex)];
    return textureObject.Sample(samplerObject, texcoord) + shadowTexture.SampleCmp(shadowSampler, texcoord, 0.5f);
}
