// The float2 must start at byte 8 in the portable std140-compatible layout.

[pushConstant]
cbuffer LayoutConstants
{
    float  scalarValue;
    float2 vectorValue;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(vectorValue, scalarValue, float(vertexId));
}
