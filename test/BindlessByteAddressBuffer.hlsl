// Byte address buffers read from the resource heap, directly and through a function returning the heap access.
cbuffer Indices : register(b0)
{
    uint RawIndex;
    uint OutputIndex;
};

ByteAddressBuffer GetRawBuffer(uint index)
{
    return ResourceDescriptorHeap[index];
}

[numthreads(64, 1, 1)]
void CS(uint3 threadId : SV_DispatchThreadID)
{
    ByteAddressBuffer raw = ResourceDescriptorHeap[NonUniformResourceIndex(RawIndex + threadId.x)];
    RWByteAddressBuffer output = ResourceDescriptorHeap[OutputIndex];

    uint size;
    raw.GetDimensions(size);

    uint4 wide = raw.Load4(threadId.x * 16);
    uint2 pair = GetRawBuffer(RawIndex + 1).Load2(8);

    output.Store4(32 + threadId.x * 16, wide);

    uint original;
    output.InterlockedAdd(12, 1, original);

    if (threadId.x == 0)
    {
        output.Store2(0, pair);
        output.Store(8, size);
        output.Store(16, original);
    }
}
