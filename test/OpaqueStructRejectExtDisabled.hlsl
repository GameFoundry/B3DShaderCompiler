// Should be rejected by default: opaque resources inside a struct require the
// 'opaque-struct' language extension to be enabled via -Xopaque-struct ON.
// (Intentionally compile with NO extension flag.)

struct Bundle
{
    Texture2D    tex;
    SamplerState samp;
};

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle b = { g_tex, g_samp };
    return b.tex.Sample(b.samp, uv);
}
