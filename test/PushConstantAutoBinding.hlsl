// Auto-binding must reserve the configured b0, space0 push-constant marker.

cbuffer ViewConstants
{
    float4 offset;
};

[pushConstant]
cbuffer DrawConstants
{
    uint objectId;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return offset + float(objectId + vertexId);
}
