// Must fail for HLSL6VK: DXC's SPIR-V output packs two-component major vectors into 8 bytes, while the
// push-constant layout gives every one of them 16. Matrices with larger major vectors stay valid.

[pushConstant]
cbuffer DrawConstants
{
    float2x3 columns;
    row_major float3x2 rows;
    float3x2 validColumns;
    row_major float2x3 validRows;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(columns[0][0], rows[0][0], validColumns[0][0], validRows[0][0] + float(vertexId));
}
