// xsc-args: -Xopaque-struct ON
// More than one fixed array axis around an opaque-bearing element type.
struct Bundle { Texture2D tex; SamplerState samp; };
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 shade(Bundle grid[2][2], float2 uv)
{
    return grid[1][0].tex.Sample(grid[1][0].samp, uv);
}
float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    Bundle grid[2][2];
    grid[0][0].tex = g_tex; grid[0][0].samp = g_samp;
    grid[0][1] = grid[0][0];
    grid[1][0] = grid[0][0];
    grid[1][1] = grid[0][0];
    return shade(grid, uv);
}
