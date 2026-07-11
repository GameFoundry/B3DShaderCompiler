// xsc-args: -Xopaque-struct ON
// Test: a function RETURNS an opaque-bearing struct by value. The callee's return-alias
// summary (tex -> g_tex, samp -> g_samp) must be translated into the caller and seed the
// alias map of the receiving local. Also covers a pass-through function whose returned
// fields resolve to its own (split) parameters -- the summary targets are the synthesized
// opaque params, which the call-site translation maps back to the arguments passed.

struct Bundle { Texture2D tex; SamplerState samp; };

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

Bundle makeBundle()
{
    Bundle b;
    b.tex  = g_tex;
    b.samp = g_samp;
    return b;
}

Bundle passthrough(Bundle b)
{
    return b;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle a = makeBundle();
    Bundle b = passthrough(a);
    return b.tex.Sample(b.samp, uv);
}
