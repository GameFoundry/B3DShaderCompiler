// Covers what the HLSL6VK target adds on top of HLSL6: DXC's SPIR-V code generator rejects push-constant cbuffers,
// needs the BSL image layout as an attribute, derives a format for typed buffers that their views may not match, and
// warns about default values of cbuffer members.

[layout(rgba16f)]
RWTexture2D<float4> outputTexture : register(u0);

Buffer<uint2> inputIndices : register(t2);

cbuffer Parameters : register(b1)
{
    float scale = 2.0f;
};

[pushConstant]
cbuffer PushConstants
{
    float  scalarValue;
    float2 vectorValue;
};

[numthreads(1, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    outputTexture[threadId.xy] = float4(vectorValue * scale, scalarValue, inputIndices[threadId.x].x);
}
