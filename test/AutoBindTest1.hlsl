// Auto-binding fixture (-AB): no resource carries a complete register.
// The generator must assign sequential per-space slots (synthesized registers
// go to space0), reserve the explicitly numbered slot against auto-assignment,
// and normalize the DX9-style register(c0, space1) cbuffer spelling (used by
// host engines to carry a slot + space pair) to a proper b-register binding.
// Requires a *_5_1 fxc profile since the normalized binding keeps its space.
//
// Also carries language-extension attributes ([color], [hideInInspector], ...)
// — host-engine metadata the generator must suppress: fxc rejects attributes
// on variable declarations, so any leak fails the round-trip.

cbuffer PerObject : register(c0, space1)
{
	float4x4 world;
};

cbuffer PerFrame
{
	float4x4 vp;
	[color] [hdr] float4 tint;
};

[hideInInspector] [name("AlbedoTexture")]
Texture2D albedo;
Texture2D detail : register(t3); // explicit slot: must survive and be skipped by the counter
[internal] SamplerState samp;

struct PixelIn
{
	float4 pos : SV_Position;
	float2 uv  : TEXCOORD0;
};

float4 PS(PixelIn inp) : SV_Target
{
	float4 baseColor   = albedo.Sample(samp, inp.uv);
	float4 detailColor = detail.Sample(samp, inp.uv * 4.0);
	float4 worldPos    = mul(world, float4(inp.uv, 0.0, 1.0));
	float4 clipPos     = mul(vp, worldPos);
	return baseColor * detailColor * tint + clipPos * 0.0;
}
