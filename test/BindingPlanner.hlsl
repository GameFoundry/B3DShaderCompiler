// Explicit bindings intentionally follow resources that need auto-binding.
// A one-pass allocator assigns the first three resources to slots 0, 1, and 2,
// colliding with the later declarations. The coordinated planner must reserve
// those explicit slots first and assign the implicit resources to 3, 4, and 5.

Texture2D<float4> autoTexture;
SamplerState autoSampler;

cbuffer AutoConstants
{
    float4 autoTint;
};

Texture2D<float4> explicitTexture : register(t0);
SamplerState explicitSampler : register(s1);

cbuffer ExplicitConstants : register(b2)
{
    float4 explicitTint;
};

struct PixelInput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

float4 PS(PixelInput input) : SV_Target
{
    return autoTexture.Sample(autoSampler, input.texcoord) * autoTint +
        explicitTexture.Sample(explicitSampler, input.texcoord) * explicitTint;
}
