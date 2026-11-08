// Stage legality: each case macro adds one call that its entry's stage doesn't allow. The checks also cover calls whose
// results are unused, and calls reached through helper functions.

struct Payload
{
    float4 color;
};

struct Attributes
{
    float2 uv;
};

RaytracingAccelerationStructure gScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);
Texture2D gTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerComparisonState gComparison : register(s1);

RayDesc MakeRay()
{
    RayDesc ray;
    ray.Origin = 0.0f;
    ray.TMin = 0.0f;
    ray.Direction = float3(0, 0, 1);
    ray.TMax = 1.0f;
    return ray;
}

void IgnoreFromHelper()
{
    IgnoreHit();
}

[shader("raygeneration")]
void RayGen()
{
    Payload payload = (Payload)0;
#ifdef RAYGEN_WORLD_RAY_ORIGIN
    payload.color.xyz = WorldRayOrigin();
#endif
#ifdef RAYGEN_DDX
    payload.color.x = ddx(payload.color.y);
#endif
#ifdef RAYGEN_ALL_MEMORY_BARRIER
    AllMemoryBarrier();
#endif
    gOutput[DispatchRaysIndex().xy] = payload.color;
}

[shader("miss")]
void Miss(inout Payload payload)
{
#ifdef MISS_INSTANCE_ID
    payload.color.x = InstanceID();
#endif
#ifdef MISS_HIT_KIND_UNUSED
    HitKind();
#endif
#ifdef MISS_ACCEPT_HIT
    AcceptHitAndEndSearch();
#endif
#ifdef MISS_SAMPLE
    payload.color = gTexture.Sample(gSampler, payload.color.xy);
#endif
#ifdef MISS_SAMPLE_CMP
    payload.color.x = gTexture.SampleCmp(gComparison, payload.color.xy, 0.5f);
#endif
#ifdef MISS_LEVEL_OF_DETAIL
    payload.color.x = gTexture.CalculateLevelOfDetail(gSampler, payload.color.xy);
#endif
    payload.color.w = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
#ifdef CLOSESTHIT_IGNORE_HIT
    IgnoreHit();
#endif
#ifdef CLOSESTHIT_IGNORE_HIT_HELPER
    IgnoreFromHelper();
#endif
#ifdef CLOSESTHIT_CLIP
    clip(attribs.barycentrics.x - 0.5f);
#endif
#ifdef CLOSESTHIT_OBJECT_TO_WORLD
    payload.color.x = ObjectToWorld()[0][0];
#endif
#ifdef CLOSESTHIT_WORLD_TO_OBJECT
    payload.color.x = WorldToObject()[0][0];
#endif
    payload.color.xy = attribs.barycentrics;
}

[shader("anyhit")]
void AnyHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
#ifdef ANYHIT_TRACE_RAY
    TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, MakeRay(), payload);
#endif
#ifdef ANYHIT_REPORT_HIT
    Attributes hitAttribs;
    hitAttribs.uv = attribs.barycentrics;
    ReportHit(1.0f, 0, hitAttribs);
#endif
#ifdef ANYHIT_DISCARD
    if (attribs.barycentrics.x < 0.5f)
        discard;
#endif
    payload.color.xy = attribs.barycentrics;
}

[shader("intersection")]
void Intersection()
{
    Attributes attribs;
    attribs.uv = 0.0f;
#ifdef INTERSECTION_CALL_SHADER
    Payload payload = (Payload)0;
    CallShader(0, payload);
#endif
#ifdef INTERSECTION_HIT_KIND
    attribs.uv.x = HitKind();
#endif
    ReportHit(1.0f, 0, attribs);
}

[shader("callable")]
void Callable(inout Payload payload)
{
#ifdef CALLABLE_RAY_T_CURRENT
    payload.color.x = RayTCurrent();
#endif
#ifdef CALLABLE_GROUP_BARRIER
    GroupMemoryBarrierWithGroupSync();
#endif
    payload.color.w = 1.0f;
}

[numthreads(8, 8, 1)]
void CS(uint3 threadId : SV_DispatchThreadID)
{
    float4 color = 0.0f;
#ifdef COMPUTE_DISPATCH_RAYS_INDEX
    color.xy = (float2)DispatchRaysIndex().xy;
#endif
#ifdef COMPUTE_TRACE_RAY
    Payload payload = (Payload)0;
    TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, MakeRay(), payload);
#endif
    gOutput[threadId.xy] = color;
}

float4 PS(float4 position : SV_Position) : SV_Target0
{
    float4 color = position;
#ifdef PIXEL_PRIMITIVE_INDEX
    color.x = PrimitiveIndex();
#endif
    return color;
}
