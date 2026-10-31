// GLSL writes the address of multi-word loads once per word, so it can't change state.
RWByteAddressBuffer RawOutput : register(u0);

uint NextAddress()
{
    return 4;
}

[numthreads(1, 1, 1)]
void CS()
{
    RawOutput.Store(0, RawOutput.Load2(NextAddress()).y);
}
