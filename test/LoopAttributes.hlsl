// Loop control attribute test ('[unroll]', '[loop]')
//
// Vulkan GLSL output writes these as GL_EXT_control_flow_attributes ('[[unroll]]', '[[dont_unroll]]'), which glslang
// turns into SPIR-V loop controls (hints, which drivers may ignore). '[unroll(N)]' caps the unroll count, which the
// extension cannot express, so it is dropped.

cbuffer Params : register(b0)
{
	uint count;
};

float4 main() : SV_Target
{
	uint sum = 0;

	[unroll]
	for (uint unrolled = 0; unrolled < 4; ++unrolled)
		sum += unrolled;

	[loop]
	for (uint kept = 0; kept < count; ++kept)
		sum ^= kept;

	[unroll(2)]
	for (uint capped = 0; capped < count; ++capped)
		sum += capped;

	uint index = 0;
	[loop]
	while (index < count)
	{
		sum += index;
		++index;
	}

	[unroll]
	do
	{
		sum ^= 3;
	}
	while (sum < 2);

	return float4((float)sum, 0, 0, 1);
}
