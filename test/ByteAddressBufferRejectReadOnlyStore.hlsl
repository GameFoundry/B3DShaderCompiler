// Read-only byte address buffers have no stores or atomics.
ByteAddressBuffer RawInput : register(t0);

[numthreads(1, 1, 1)]
void CS()
{
    RawInput.Store(0, 1);
}
