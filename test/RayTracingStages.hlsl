// One entry point per ray tracing stage. Each compile keeps [shader] only on its own entry, and reflection reports
// every stage with its payload and attribute sizes.

struct Payload
{
    float4 color;
    uint depth;
};

struct SphereAttributes
{
    float3 normal;
};

// Members named like intrinsics resolve to the members
struct Hit
{
    uint InstanceID;
    float HitKind;
};

RaytracingAccelerationStructure gScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);
Texture2D gTexture : register(t1);
SamplerState gSampler : register(s0);

[shader("raygeneration")]
void RayGen()
{
    uint2 index = DispatchRaysIndex().xy;
    float2 size = (float2)DispatchRaysDimensions().xy;

    RayDesc ray;
    ray.Origin = float3(index / size, 0.0f);
    ray.TMin = 0.001f;
    ray.Direction = float3(0, 0, 1);
    ray.TMax = 1000.0f;

    Payload payload = (Payload)0;
    TraceRay(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 1, 0, ray, payload);
    CallShader(0, payload);
    gOutput[index] = payload.color;
}

[shader("miss")]
void Miss(inout Payload payload)
{
    payload.color = gTexture.SampleLevel(gSampler, WorldRayDirection().xy, 0) * RayTCurrent();
    if (payload.depth < 2)
    {
        RayDesc ray;
        ray.Origin = WorldRayOrigin();
        ray.TMin = RayTMin();
        ray.Direction = -WorldRayDirection();
        ray.TMax = 10.0f;
        TraceRay(gScene, RayFlags(), 0xFF, 0, 1, 0, ray, payload);
    }
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 bary = float3(1.0f - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics);

    Hit hit;
    hit.InstanceID = InstanceID() + InstanceIndex();
    hit.HitKind = (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE ? 1.0f : 0.0f);

    float3x4 objectToWorld = ObjectToWorld3x4();
    float4x3 worldToObject = WorldToObject4x3();
    payload.color = float4(bary * hit.HitKind, objectToWorld[0][0] + worldToObject[0][0]);
    payload.depth = hit.InstanceID + PrimitiveIndex() + GeometryIndex();
}

[shader("anyhit")]
void AnyHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    if (attribs.barycentrics.x < 0.1f)
        IgnoreHit();
    if (payload.depth > 4 || HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
        AcceptHitAndEndSearch();
}

[shader("intersection")]
void Intersection()
{
    float3 origin = ObjectRayOrigin();
    float3 direction = ObjectRayDirection();

    SphereAttributes attribs;
    attribs.normal = normalize(origin + direction);
    ReportHit(RayTMin() + length(origin), 0, attribs);
}

[shader("callable")]
void Callable(inout Payload payload)
{
    payload.depth += DispatchRaysIndex().x;
}
