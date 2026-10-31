// Geometry shader streams are stage outputs, not opaque resources. The opaque
// lowering enabled by the bindless extension must accept them as inout parameters.

struct VSOutput
{
    float4 position : SV_Position;
};

struct GSOutput
{
    float4 position : SV_Position;
    uint layer : SV_RenderTargetArrayIndex;
};

[maxvertexcount(6)]
void GS(triangle VSOutput inputs[3], inout TriangleStream<GSOutput> stream)
{
    Texture2D<float4> offsets = ResourceDescriptorHeap[1u];

    for (uint layer = 0; layer < 2; ++layer)
    {
        for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
        {
            GSOutput output;
            output.position = inputs[vertexIndex].position + offsets.Load(int3(layer, 0, 0));
            output.layer = layer;
            stream.Append(output);
        }

        stream.RestartStrip();
    }
}
