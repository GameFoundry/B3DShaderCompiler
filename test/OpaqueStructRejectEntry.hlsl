// xsc-args: -Xopaque-struct ON
// Should be rejected: opaque-bearing struct as entry-point parameter.
struct Bundle { Texture2D t; SamplerState s; };
float4 main(Bundle b, float2 uv : TEXCOORD0) : SV_Target { return float4(0,0,0,1); }
