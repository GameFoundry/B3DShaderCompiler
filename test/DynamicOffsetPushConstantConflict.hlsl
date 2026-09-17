// Copyright 2026 Marko Pintera.
#if defined(TEST_PushFirst)
[pushConstant][dynamicOffset]
#else
[dynamicOffset][pushConstant]
#endif
cbuffer Values { float4 color; };

float4 main() : SV_Target0
{
    return color;
}
