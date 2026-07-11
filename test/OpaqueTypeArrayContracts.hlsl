// xsc-args: -Xopaque-struct ON
// Fixed arrays in out/inout contracts and inside a returned aggregate.

struct Bundle
{
    Texture2D    tex;
    SamplerState samp;
};

struct BundleSet
{
    Bundle values[2];
    float4 gain;
};

Texture2D    g_texA : register(t0);
Texture2D    g_texB : register(t1);
SamplerState g_samp : register(s0);

void initialize(out Bundle values[2])
{
    values[0].tex  = g_texA;
    values[0].samp = g_samp;
    values[1].tex  = g_texB;
    values[1].samp = g_samp;
}

void redirect(inout Bundle values[2])
{
    values[0].tex = g_texB;
}

BundleSet makeSet()
{
    BundleSet result;
    initialize(result.values);
    result.gain = float4(0.5, 0.5, 0.5, 1.0);
    return result;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle values[2];
    initialize(values);
    redirect(values);
    float4 direct = makeSet().values[1].tex.Sample(g_samp, uv);
    return direct + values[0].tex.Sample(values[0].samp, uv);
}
