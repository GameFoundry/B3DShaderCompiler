// xsc-args: -Xopaque-struct ON
Buffer<float4> g_data : register(t0);
float4 main() : SV_Target
{
    Buffer<float4> localData = g_data;
    return localData[0];
}
