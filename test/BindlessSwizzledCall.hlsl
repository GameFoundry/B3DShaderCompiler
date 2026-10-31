// A swizzle on the result of a method call on a heap local. The opaque lowering removes the local's declaration, so the
// local used as the call's prefix must still be rewritten into a heap access.

cbuffer Indices
{
    uint TextureIndex;
};

RWStructuredBuffer<float> Output : register(u0);

[numthreads(1, 1, 1)]
void CS()
{
    Texture2D<float4> heapTexture = ResourceDescriptorHeap[TextureIndex];
    Output[0] = heapTexture.Load(int3(1, 0, 0)).r;
    Output[1] = heapTexture.Load(int3(0, 0, 0)).gb.y;
}
