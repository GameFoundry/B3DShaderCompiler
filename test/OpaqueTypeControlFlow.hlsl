// xsc-args: -Xopaque-struct ON
// Conditional calls remain conditional; loop back-edges and switch fallthrough
// preserve identical static resource bindings.
struct Bundle { Texture2D tex; SamplerState samp; };
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);

Bundle makeA()
{
    Bundle value;
    value.tex = g_tex;
    value.samp = g_samp;
    return value;
}

Bundle makeB()
{
    Bundle value = makeA();
    return value;
}

Bundle selectBundle(bool chooseA)
{
    Bundle value = (chooseA ? makeA() : makeB());
    for (int i = 0; i < 2; ++i)
        value.tex = g_tex;
    switch (int(chooseA))
    {
        case 0:
            value.tex = g_tex;
        case 1:
            value.samp = g_samp;
            break;
        default:
            value = makeA();
            break;
    }
    return value;
}

float mark(inout float value)
{
    value += 1.0;
    return value;
}

float4 consume(float weight, Bundle value, float2 uv)
{
    return value.tex.Sample(value.samp, uv) * weight;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    float weight = 0.0;
    return consume(mark(weight), selectBundle(uv.x > 0.5), uv);
}
