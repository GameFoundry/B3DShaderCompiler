// GLSL declares byte address buffers as uint[], so atomic minimum and maximum can't compare signed values.
RWByteAddressBuffer RawOutput : register(u0);

[numthreads(1, 1, 1)]
void CS()
{
    int value = -1;
    RawOutput.InterlockedMin(0, value);
}
