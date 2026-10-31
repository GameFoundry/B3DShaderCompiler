// xsc-args: -Xopaque-struct ON
// The callee indexes its array parameter at runtime and returns the element; the caller sees the argument's resource
// array indexed by the value the callee computed.
struct Bundle { Texture2D tex; };

Texture2D    g_texArray[2] : register(t0);
SamplerState g_samp        : register(s0);

Texture2D Pick(Bundle bundles[2], int index) { return bundles[index].tex; }

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle bundles[2];
    bundles[0].tex = g_texArray[0];
    bundles[1].tex = g_texArray[1];
    return Pick(bundles, int(uv.x > 0.5)).Sample(g_samp, uv);
}
