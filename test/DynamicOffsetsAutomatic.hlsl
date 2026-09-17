// Copyright 2026 Marko Pintera.
[dynamicOffset]
cbuffer PerDraw
{
    float4 color;
};

cbuffer StaticParameters : register(b0)
{
    float4 tint;
};

float4 main() : SV_Target0
{
    return color * tint;
}
