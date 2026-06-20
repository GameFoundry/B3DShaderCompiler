// xsc-args: -Xopaque-struct ON
// Should be rejected: global of opaque-bearing struct.
struct Bundle { Texture2D t; SamplerState s; };
Bundle g_bundle;
float4 main(float2 uv : TEXCOORD0) : SV_Target { return float4(0,0,0,1); }
