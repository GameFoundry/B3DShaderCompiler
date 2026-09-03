struct Element
{
    float4 value;
};

[numthreads(1, 1, 1)]
void CS(uint descriptorIndex : SV_GroupIndex)
{
    AppendStructuredBuffer<Element> output = ResourceDescriptorHeap[descriptorIndex];
}
