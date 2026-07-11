// xsc-args: -Xopaque-struct ON
Texture2D g_tex : register(t0);
void assignTexture(out Texture2D value)
{
    value = g_tex;
}
float4 main() : SV_Target { return float4(1, 1, 1, 1); }
