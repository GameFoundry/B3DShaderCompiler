// xsc-args: -Xopaque-struct ON
// Test: a sub-struct used as the DESTINATION of a copy. `m.albedo = src;` copies src's alias
// entries into m's "albedo.*" entries, so the later reads of m.albedo.tex/samp resolve to the
// globals that were assigned to src. Exercises the sub-struct copy-assignment path
// (CopyAliasSubtree from VisitExprStmnt); without it the reads would be "uninitialized".

struct TexBundle { Texture2D tex; SamplerState samp; };
struct Material  { TexBundle albedo; float4 tint; };

Texture2D    g_tex  : register(t0);
SamplerState g_samp : register(s0);

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    TexBundle src;
    src.tex  = g_tex;
    src.samp = g_samp;

    Material m;
    m.albedo = src;                  // sub-struct as the copy DESTINATION
    m.tint   = float4(1, 1, 1, 1);

    return m.albedo.tex.Sample(m.albedo.samp, uv) * m.tint;
}
