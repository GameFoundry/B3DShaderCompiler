struct Element
{
    float4 value;
};

struct UnusedElement
{
    float4 value;
    float unused;
};

cbuffer Params : register(b0)
{
    uint descriptorIndex;
};

float4 ReadUnusedBindlessResource(uint index)
{
    StructuredBuffer<UnusedElement> resource = ResourceDescriptorHeap[index];
    return resource[index].value;
}

[numthreads(1, 1, 1)]
void CS(uint3 dispatchThread : SV_DispatchThreadID)
{
    uint index = NonUniformResourceIndex(descriptorIndex);

    Buffer<float4> readTexel = ResourceDescriptorHeap[index];
    RWBuffer<float4> writeTexel = ResourceDescriptorHeap[index];
    StructuredBuffer<Element> readStructured = ResourceDescriptorHeap[index];
    RWStructuredBuffer<Element> writeStructured = ResourceDescriptorHeap[index];
    ByteAddressBuffer readBytes = ResourceDescriptorHeap[index];
    RWByteAddressBuffer writeBytes = ResourceDescriptorHeap[index];
    RWTexture2D<float4> writeTexture = ResourceDescriptorHeap[index];

    float4 value = readTexel[dispatchThread.x] + readStructured[dispatchThread.x].value;
    writeTexel[dispatchThread.x] = value;
    writeStructured[dispatchThread.x] = readStructured[dispatchThread.x];
    writeTexture[dispatchThread.xy] = value;
}
