// Byte address buffer members: loads, stores, atomics and sizes, on read-only and read-write buffers.
ByteAddressBuffer RawInput : register(t0);
RWByteAddressBuffer RawOutput : register(u0);

cbuffer Constants : register(b0)
{
    uint BaseAddress;
    int SignedAddress;
};

// Functions sharing their names with byte address buffer members.
uint Store(uint value) { return value * 2; }
uint Load2(uint value) { return value + 1; }

uint ComputeAddress(uint index)
{
    return BaseAddress + index * 16;
}

[numthreads(64, 1, 1)]
void CS(uint3 threadId : SV_DispatchThreadID)
{
    uint address = BaseAddress + threadId.x * 16;

    uint first = RawInput.Load(address);
    uint2 pair = RawInput.Load2(address + 4);
    uint3 triple = RawInput.Load3(SignedAddress);
    uint4 wide = RawInput.Load4(address);
    float asFloat = asfloat(RawInput.Load(0));

    uint size;
    RawInput.GetDimensions(size);

    RawOutput.Store(address, first + size + Store(3) + Load2(4));
    RawOutput.Store2(address + 4, pair);
    RawOutput.Store3(address + 12, triple);
    RawOutput.Store4(address + 32, wide + uint4(1, 2, 3, (uint)asFloat));

    // Stores that are a whole statement evaluate their address and value once, also as an unbraced branch
    if (threadId.x < 32)
        RawOutput.Store2(ComputeAddress(threadId.x), uint2(threadId.x, size));

    uint original;
    int signedOriginal;
    RawOutput.InterlockedAdd(0, 1);
    RawOutput.InterlockedAdd(4, 1, original);
    RawOutput.InterlockedAnd(8, 0xFF, signedOriginal);
    RawOutput.InterlockedOr(12, original);
    RawOutput.InterlockedXor(16, 3);
    RawOutput.InterlockedMin(20, original);
    RawOutput.InterlockedMax(24, original, original);
    RawOutput.InterlockedExchange(28, 5, original);
    RawOutput.InterlockedCompareExchange(32, 0, 1, original);
    RawOutput.InterlockedCompareStore(36, 0, 2);
    RawOutput.InterlockedAdd(40, -1);

    uint outputSize;
    RawOutput.GetDimensions(outputSize);
    if (threadId.x == 0)
        RawOutput.Store(44, outputSize + RawOutput.Load(48) + (uint)signedOriginal);
}
