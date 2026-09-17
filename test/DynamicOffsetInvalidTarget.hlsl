// Copyright 2026 Marko Pintera.
// Each TEST_* macro selects one declaration for the existing expected-error driver.
#if defined(TEST_StructuredBuffer)
[dynamicOffset] StructuredBuffer<float4> values;
#elif defined(TEST_RWStructuredBuffer)
[dynamicOffset] RWStructuredBuffer<float4> values;
#elif defined(TEST_StructuredBufferArray)
[dynamicOffset] StructuredBuffer<float4> values[1];
#elif defined(TEST_RWStructuredBufferArray)
[dynamicOffset] RWStructuredBuffer<float4> values[2];
#elif defined(TEST_ByteAddressBuffer)
[dynamicOffset] ByteAddressBuffer values;
#elif defined(TEST_RWByteAddressBuffer)
[dynamicOffset] RWByteAddressBuffer values;
#elif defined(TEST_Buffer)
[dynamicOffset] Buffer<float4> values;
#elif defined(TEST_RWBuffer)
[dynamicOffset] RWBuffer<float4> values;
#elif defined(TEST_Texture)
[dynamicOffset] Texture2D values;
#elif defined(TEST_TextureArray)
[dynamicOffset] Texture2D values[1];
#elif defined(TEST_Sampler)
[dynamicOffset] SamplerState samplerValue;
#elif defined(TEST_Value)
[dynamicOffset] float value;
#elif defined(TEST_Function)
[dynamicOffset] float helper() { return 0; }
#elif defined(TEST_TBuffer)
[dynamicOffset] tbuffer Values { float value; };
#elif defined(TEST_CBufferField)
cbuffer Values { [dynamicOffset] float value; };
#elif defined(TEST_StructField)
struct Values { [dynamicOffset] float value; };
#elif defined(TEST_Parameter)
float helper([dynamicOffset] float value) { return value; }
#elif defined(TEST_Local)
float helper() { [dynamicOffset] float value; return 0; }
#elif defined(TEST_Statement)
float helper() { [dynamicOffset] if (true) return 1; return 0; }
#elif defined(TEST_Struct)
[dynamicOffset] struct Values { float value; };
#else
#error Select a TEST_* declaration.
#endif

float4 main() : SV_Target0
{
    return 0;
}
