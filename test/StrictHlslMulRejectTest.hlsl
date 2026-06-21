
// Strict-HLSL negative fixture: mul(matrix, vector) with dim mismatch.
// Without -Xstrict-hlsl, fxc/xsc silently zero-pad the vector — accepted.
// With -Xstrict-hlsl, the analyzer must reject this implicit promotion.

cbuffer Settings : register(b0)
{
    float4x4 wvpMatrix;
};

float4 VS(float3 pos : POSITION) : SV_Position
{
    // wvpMatrix has 4 columns; pos has 3 elements — strict-hlsl forbids this.
    return mul(wvpMatrix, pos);
}
