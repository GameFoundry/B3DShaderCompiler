// A single scalar occupies a four-byte logical push-constant range even though
// the generated HLSL cbuffer carrier is reflected as one 16-byte row.

[pushConstant]
cbuffer DrawConstants
{
    float scale;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(scale, float(vertexId), 0.0, 1.0);
}
