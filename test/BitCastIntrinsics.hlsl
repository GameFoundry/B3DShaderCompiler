// Bit-cast intrinsic test ('asint', 'asuint', 'asfloat')
//
// These intrinsics reinterpret the bit pattern of their argument, so both their result type and the
// GLSL function they map to depend on the type of that argument. Covers all nine source/destination
// pairs, plus expression contexts that are only valid if the result carries the type the intrinsic
// dictates rather than the argument's type.

cbuffer Params : register(b0)
{
	float4 fVal;
	uint4  uVal;
	int4   iVal;
};

float4 main() : SV_Target
{
	// float -> int/uint, plus the identity cast
	int   f2i = asint(fVal.x);
	uint  f2u = asuint(fVal.y);
	float f2f = asfloat(fVal.z);

	// uint -> int/float, plus the identity cast
	int   u2i = asint(uVal.x);
	float u2f = asfloat(uVal.y);
	uint  u2u = asuint(uVal.z);

	// int -> uint/float, plus the identity cast
	uint  i2u = asuint(iVal.x);
	float i2f = asfloat(iVal.y);
	int   i2i = asint(iVal.z);

	// Vector forms keep the dimension of their argument
	int3   v2i = asint(fVal.xyz);
	float2 v2f = asfloat(uVal.xy);

	// Integer operations on the result: only valid if 'asuint'/'asint' are typed as uint/int here,
	// rather than as the float/uint type of their argument
	uint shifted = asuint(fVal.w) >> 16;
	uint masked  = asuint(fVal.x) & 0xFFu;
	int  signExt = asint(uVal.w) >> 3;

	// ... and conversely, 'asfloat' must be typed as float so this stays a floating-point multiply
	float scaled = asfloat(iVal.w) * 2.0f;

	return float4(
		float(f2i + u2i + i2i + signExt + v2i.x),
		float(f2u + u2u + i2u + shifted + masked),
		f2f + u2f + i2f + scaled + v2f.x,
		1.0f
	);
}
