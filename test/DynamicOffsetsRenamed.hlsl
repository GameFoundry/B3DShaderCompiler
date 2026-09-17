// Copyright 2026 Marko Pintera.
[dynamicOffset]
cbuffer input : register(b3, space2)
{
    float4 color;
};

float4 main() : SV_Target0
{
    return color;
}
