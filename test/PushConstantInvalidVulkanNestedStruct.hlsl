// Must fail for HLSL6VK: the push-constant layout rounds 'inner' up to 16 bytes and places 'after' at byte 16, but
// DXC's SPIR-V output places it at byte 4. A structure that is a direct member of the buffer stays valid.

struct Inner
{
    float value;
};

struct Outer
{
    Inner inner;
    float after;
};

[pushConstant]
cbuffer DrawConstants
{
    Inner direct;
    Outer outer;
};

float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    return float4(direct.value, outer.inner.value, outer.after, float(vertexId));
}
