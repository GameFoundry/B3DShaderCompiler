// Copyright 2026 Marko Pintera.
[dynamicOffset]
[internal]
[hideInInspector]
cbuffer PerDraw : register(b2, space1)
{
    float4 drawColors[2];
    row_major float4x4 transform;
};

cbuffer StaticParameters : register(b1, space0)
{
    float4 tint;
};

float4 main(float4 position : POSITION) : SV_Position
{
    return mul(transform, position) + drawColors[0] * drawColors[1] * tint;
}
