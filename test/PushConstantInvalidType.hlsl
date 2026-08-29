// Must fail: boolean storage has no portable cross-backend representation.

[pushConstant]
cbuffer DrawConstants
{
    bool enabled;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(enabled ? 1.0 : 0.0, 0.0, 0.0, float(vertexId));
}
