// Selection control attribute test ('[flatten]', '[branch]')
//
// Vulkan GLSL output writes these as GL_EXT_control_flow_attributes ('[[flatten]]', '[[dont_flatten]]'), which glslang
// turns into SPIR-V selection controls. '[forcecase]' and '[call]' have no equivalent, so they are dropped.

cbuffer Params : register(b0)
{
	uint count;
	float threshold;
};

float4 main() : SV_Target
{
	float value = 0;

	[flatten]
	if (threshold > 0.5f)
		value = 1;

	[branch]
	if (count > 4)
		value += 2;
	else [flatten] if (count > 2)
		value += 3;

	[branch]
	switch (count)
	{
		case 0: value += 4; break;
		default: value += 5; break;
	}

	[forcecase]
	switch (count + 1)
	{
		case 1: value *= 2; break;
		default: value *= 3; break;
	}

	return float4(value, 0, 0, 1);
}
