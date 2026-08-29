// Must fail: D3D12 root constants cannot expose array members portably.

[pushConstant]
cbuffer DrawConstants
{
    float values[2];
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(values[vertexId & 1], 0.0, 0.0, 1.0);
}
