// Must fail for a push-constant marker configured as b3, space7.

cbuffer ConflictingConstants : register(b3, space7)
{
    float4 value;
};

[pushConstant]
cbuffer DrawConstants
{
    uint objectId;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return value + float(objectId + vertexId);
}
