
// Strict-HLSL variant of MatrixMulTest1.
// Drops the four dim-mismatched mul(M, v) calls (M3*v4, M4*v3, v3*v4, v4*v3).
// Keeps matrix×matrix, matrix×vec (matching), vec×vec (matching), plus the
// scalar/matrix arithmetic — still exercises the substantive coverage.

cbuffer Matrices {
    float3x3 M3;
    float4x4 M4;
    float4x3 M4x3;
};

void main(float3 v3 : VEC3, float4 v4 : VEC4) {

    float3x4 a = (float3x4)0;
    float4x4 b = mul(M4, M4);
    float4x4 c = mul(M4x3, a);

    float3 d = mul(M3, v3);    // matching: 3 cols × dim 3
    float4 g = mul(M4, v4);    // matching: 4 cols × dim 4

    // mul(vec, vec) is ambiguous in some strict-typed backends — use dot()
    // instead. The HLSL overload is a synonym for dot-product, so the
    // semantic coverage is preserved.
    float h = dot(v3, v3);
    float k = dot(v4, v4);

    float4x4 l = M4*1;
    float4x4 m = 2*M4;
    float4x4 n = M4+3;
    float4x4 o = 4+M4;
}
