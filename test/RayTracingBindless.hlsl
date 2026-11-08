// Acceleration structures through an array and through ResourceDescriptorHeap, in a ray generation and a compute entry.

struct Payload
{
    float4 color;
};

RaytracingAccelerationStructure gScenes[2] : register(t0, space1);
RWTexture2D<float4> gOutput : register(u0);

cbuffer SceneParams : register(b0)
{
    uint gSceneIndex;
    uint gHeapSceneIndex;
};

RayDesc MakeRay(float2 position)
{
    RayDesc ray;
    ray.Origin = float3(position, 0.0f);
    ray.TMin = 0.0f;
    ray.Direction = float3(0, 0, 1);
    ray.TMax = 1000.0f;
    return ray;
}

[shader("raygeneration")]
void RayGen()
{
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[gHeapSceneIndex];

    Payload payload = (Payload)0;
    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, MakeRay((float2)DispatchRaysIndex().xy), payload);
    TraceRay(gScenes[gSceneIndex], RAY_FLAG_NONE, 0xFF, 0, 1, 0, MakeRay((float2)DispatchRaysIndex().xy), payload);
    gOutput[DispatchRaysIndex().xy] = payload.color;
}

[numthreads(8, 8, 1)]
void CS(uint3 threadId : SV_DispatchThreadID)
{
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[gHeapSceneIndex];

    RayHit fromHeap = RayTraceClosest(scene, RAY_FLAG_NONE, 0xFF, MakeRay((float2)threadId.xy));
    RayHit fromArray = RayTraceAny(gScenes[1], RAY_FLAG_NONE, 0xFF, MakeRay((float2)threadId.xy));
    gOutput[threadId.xy] = float4(fromHeap.T, fromArray.T, 0.0f, 1.0f);
}
