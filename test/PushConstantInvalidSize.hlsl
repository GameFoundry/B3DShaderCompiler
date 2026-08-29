// Must fail with the default 16-byte limit.

[pushConstant]
cbuffer DrawConstants
{
    uint4 values;
    uint  overflow;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return asfloat(values + overflow + vertexId);
}
