// xsc-args: -Xopaque-struct ON
Texture2D g_tex : register(t0);
Texture2D returnTexture()
{
    return g_tex;
}
float4 main() : SV_Target { return float4(1, 1, 1, 1); }
