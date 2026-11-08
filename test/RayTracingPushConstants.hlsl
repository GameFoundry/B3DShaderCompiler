// A push constant block read by a ray generation entry.

struct Payload
{
    float4 color;
};

RaytracingAccelerationStructure gScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[pushConstant]
cbuffer TraceConstants
{
    float  maxDistance;
    uint   instanceMask;
};

[shader("raygeneration")]
void RayGen()
{
    RayDesc ray;
    ray.Origin = float3((float2)DispatchRaysIndex().xy, 0.0f);
    ray.TMin = 0.0f;
    ray.Direction = float3(0, 0, 1);
    ray.TMax = maxDistance;

    Payload payload = (Payload)0;
    TraceRay(gScene, RAY_FLAG_NONE, instanceMask, 0, 1, 0, ray, payload);
    gOutput[DispatchRaysIndex().xy] = payload.color;
}
