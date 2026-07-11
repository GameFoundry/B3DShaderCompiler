// xsc-args: -Xopaque-struct ON
struct Bundle
{
    Texture2D tex[];
};
Texture2D g_tex : register(t0);
float4 main() : SV_Target
{
    return float4(1, 1, 1, 1);
}
