// Valid portable BSL push-constant block: exactly four DWORDs.

[pushConstant]
cbuffer DrawConstants
{
    uint   objectId;
    float  lodBias;
    float2 viewportScale;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(viewportScale, lodBias, float(objectId + vertexId));
}
