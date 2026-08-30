RWStructuredBuffer<uint> g_output : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // WithWarpSync calls must remain in control flow that is uniform across the wave.
    WarpGroupMemoryBarrier();
    WarpGroupMemoryBarrierWithWarpSync();
    WarpDeviceMemoryBarrier();
    WarpDeviceMemoryBarrierWithWarpSync();
    WarpAllMemoryBarrier();
    WarpAllMemoryBarrierWithWarpSync();

    const uint laneIndex = WaveGetLaneIndex();
    const uint laneCount = WaveGetLaneCount();
    const bool predicate = (dispatchThreadId.x & 1u) != 0u;
    const float2 floatValue = float2(dispatchThreadId.xy) + 1.0f;
    const uint2 uintValue = dispatchThreadId.xy + uint2(1u, 2u);

    const bool firstLane = WaveIsFirstLane();
    const bool anyTrue = WaveActiveAnyTrue(predicate);
    const bool allTrue = WaveActiveAllTrue(predicate);
    const bool2 allEqual = WaveActiveAllEqual(floatValue);
    const uint4 ballotValue = WaveActiveBallot(predicate);

    const float2 laneValue = WaveReadLaneAt(floatValue, 0u);
    const float2 firstValue = WaveReadLaneFirst(floatValue);
    const uint activeCount = WaveActiveCountBits(predicate);

    const float2 sumValue = WaveActiveSum(floatValue);
    const float2 productValue = WaveActiveProduct(floatValue);
    const uint2 andValue = WaveActiveBitAnd(uintValue);
    const uint2 orValue = WaveActiveBitOr(uintValue);
    const uint2 xorValue = WaveActiveBitXor(uintValue);
    const float2 minValue = WaveActiveMin(floatValue);
    const float2 maxValue = WaveActiveMax(floatValue);

    const uint prefixCount = WavePrefixCountBits(predicate);
    const float2 prefixSum = WavePrefixSum(floatValue);
    const float2 prefixProduct = WavePrefixProduct(floatValue);

    const float2 quadLane = QuadReadLaneAt(floatValue, laneIndex & 3u);
    const float2 quadX = QuadReadAcrossX(floatValue);
    const float2 quadY = QuadReadAcrossY(floatValue);
    const float2 quadDiagonal = QuadReadAcrossDiagonal(floatValue);

    uint result = laneIndex + laneCount + activeCount + prefixCount;
    result += uint(firstLane) + uint(anyTrue) + uint(allTrue) + uint(allEqual.x) + uint(allEqual.y);
    result += ballotValue.x + ballotValue.y + ballotValue.z + ballotValue.w;
    result += asuint(laneValue.x + firstValue.y + sumValue.x + productValue.y + minValue.x + maxValue.y);
    result += andValue.x + orValue.y + xorValue.x;
    result += asuint(prefixSum.x + prefixProduct.y + quadLane.x + quadX.y + quadY.x + quadDiagonal.y);

    g_output[dispatchThreadId.x] = result;
}
