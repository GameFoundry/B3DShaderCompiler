// xsc-args: -Xopaque-struct ON
// Test: an opaque field is reassigned inside a function that RECEIVED the struct as a
// parameter -- a different function than main, where the struct was created. The use before
// the reassignment must resolve to the passed-in texture; the use after (here, forwarding the
// modified struct to a deeper call) must resolve to the reassigned global. This verifies that
// straight-line reassignment updates a parameter-seeded alias map, and that the call-site
// decomposition picks up the updated alias.

struct Combined { Texture2D tex; SamplerState samp; };

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

float4 deepSample(Combined c, float2 uv)
{
    return c.tex.Sample(c.samp, uv);
}

float4 helper(Combined c, float2 uv)
{
    float4 first = c.tex.Sample(c.samp, uv);   // resolves to the passed-in texture (g_texA)
    c.tex = g_texB;                            // field changed in this (non-creating) method
    float4 second = deepSample(c, uv);         // forwarded after the change -> g_texB
    return first + second;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Combined c;
    c.tex  = g_texA;
    c.samp = g_samp;
    return helper(c, uv);
}
