// GLSL can't pass storage buffer blocks to functions.
StructuredBuffer<uint> Input : register(t0);
RWStructuredBuffer<uint> Output : register(u0);

uint Read(StructuredBuffer<uint> source, uint index)
{
    return source[index];
}

[numthreads(1, 1, 1)]
void CS()
{
    Output[0] = Read(Input, 2);
}
