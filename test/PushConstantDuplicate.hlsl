// Must fail: the portable contract permits one push-constant block per shader.

[pushConstant]
cbuffer FirstConstants
{
    uint firstValue;
};

[pushConstant]
cbuffer SecondConstants
{
    uint secondValue;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return asfloat(firstValue + secondValue + vertexId).xxxx;
}
