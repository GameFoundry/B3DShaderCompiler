// Copyright 2026 Marko Pintera.
// Neither array cbuffer syntax nor ConstantBuffer<T> objects are supported.
#if defined(TEST_CBufferArray)
[dynamicOffset] cbuffer Values[1] { float4 color; };
#elif defined(TEST_ConstantBuffer)
struct Data { float4 color; };
[dynamicOffset] ConstantBuffer<Data> values;
#elif defined(TEST_ConstantBufferArray)
struct Data { float4 color; };
[dynamicOffset] ConstantBuffer<Data> values[1];
#else
#error Select a TEST_* declaration.
#endif

float4 main() : SV_Target0
{
    return 0;
}
