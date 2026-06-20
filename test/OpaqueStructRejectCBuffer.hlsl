// xsc-args: -Xopaque-struct ON
// Should be rejected: opaque-bearing struct inside cbuffer.
struct Bundle { Texture2D t; SamplerState s; };
cbuffer Cb { Bundle b; };
float4 main(float2 uv : TEXCOORD0) : SV_Target { return float4(0,0,0,1); }
