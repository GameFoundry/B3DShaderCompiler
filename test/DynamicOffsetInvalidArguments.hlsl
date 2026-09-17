// Copyright 2026 Marko Pintera.
#if defined(TEST_Multiple)
[dynamicOffset(1, 2)]
#else
[dynamicOffset(1)]
#endif
cbuffer Values { float4 color; };

float4 main() : SV_Target0
{
    return color;
}
