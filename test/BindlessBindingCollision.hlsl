// The default bindless layout reserves slots 0 (sampler heap) and 1 (resource
// heap) in space 0. An explicit source binding at slot 1 must be rejected.

Texture2D<float4> fixedTexture : register(t1);

cbuffer Params : register(b2)
{
    uint resourceIndex;
    float2 texcoord;
};

float4 PS() : SV_Target
{
    Texture2D<float4> textureObject = ResourceDescriptorHeap[resourceIndex];
    return textureObject.Load(int3(0, 0, 0)) + fixedTexture.Load(int3(0, 0, 0));
}
