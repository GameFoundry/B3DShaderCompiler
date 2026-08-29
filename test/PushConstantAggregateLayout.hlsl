// Portable std140-compatible aggregate layout:
//   DrawData.scalarValue: byte 0
//   DrawData.vectorValue: byte 8
//   drawData:             byte 0,  size 16
//   transform:            byte 16, size 48 (three column vectors)
//   total logical range:  64 bytes

struct DrawData
{
    float  scalarValue;
    float2 vectorValue;
};

[pushConstant]
cbuffer DrawConstants
{
    DrawData drawData;
    float2x3 transform;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    float3 position = mul(drawData.vectorValue, transform);
    return float4(position, drawData.scalarValue + float(vertexId));
}
