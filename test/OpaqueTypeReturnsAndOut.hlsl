// xsc-args: -Xopaque-struct ON
// Mixed and fully opaque returns, forward declarations, direct call-field access,
// and static out/inout resource contracts.

struct Bundle
{
    Texture2D    tex;
    SamplerState samp;
    float4       tint;
};

struct OpaqueOnly
{
    Texture2D    tex;
    SamplerState samp;
};

typedef OpaqueOnly OpaqueAlias;

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

Bundle makeBundle(float4 tint);

OpaqueAlias makeOpaque()
{
    OpaqueAlias value;
    value.tex  = g_texA;
    value.samp = g_samp;
    return value;
}

void initialize(out Bundle value)
{
    value.tex  = g_texA;
    value.samp = g_samp;
    value.tint = float4(1, 1, 1, 1);
}

void redirect(inout Bundle value)
{
    value.tex = g_texB;
}

Bundle makeBundle(float4 tint)
{
    Bundle value;
    value.tex  = g_texA;
    value.samp = g_samp;
    value.tint = tint;
    return value;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle value;
    initialize(value);
    redirect(value);
    float4 a = makeBundle(float4(0.5, 1, 1, 1)).tex.Sample(g_samp, uv);
    float4 b = makeOpaque().tex.Sample(g_samp, uv);
    return a + b + value.tex.Sample(value.samp, uv) * value.tint;
}
