// RayTraceClosest and RayTraceAny in compute, pixel and closest hit entries: without callbacks, with AnyHit only,
// with Intersection only, with both, one struct used by both kinds, and a callbacks struct passed through a helper.
// Each (kind, struct) pair gets one helper, written right after its struct.

RaytracingAccelerationStructure gScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);
Texture2D gAlpha : register(t1);
StructuredBuffer<float> gOpacity : register(t2);
SamplerState gSampler : register(s0);

cbuffer TraceParams : register(b0)
{
    uint gRuntimeFlags;
    uint gInstanceMask;
};

// AnyHit only, used by both RayTraceClosest and RayTraceAny
struct AlphaTest
{
    float Cutoff;
    uint Tested;

    bool AnyHit(RayCandidate candidate)
    {
        Tested++;
        float alpha = gAlpha.SampleLevel(gSampler, candidate.Barycentrics, 0).a;
        return alpha >= Cutoff && candidate.T > 0.0f;
    }
};

// Intersection only
struct Spheres
{
    float Radius;

    bool Intersection(RayCandidate candidate, out float t)
    {
        float3 origin = candidate.ObjectRayOrigin;
        float3 direction = candidate.ObjectRayDirection;

        float a = dot(direction, direction);
        float b = dot(origin, direction);
        float h = b * b - a * (dot(origin, origin) - Radius * Radius);

        t = (-b - sqrt(max(h, 0.0f))) / a;
        return h >= 0.0f;
    }
};

// Both methods, plus a method the built-ins don't use
struct TranslucentSpheres
{
    float Transmittance;

    float Opacity(uint instance)
    {
        return gOpacity[instance];
    }

    bool AnyHit(RayCandidate candidate)
    {
        Transmittance *= 1.0f - Opacity(candidate.InstanceID);
        return Transmittance < 0.01f;
    }

    bool Intersection(RayCandidate candidate, out float t)
    {
        t = length(candidate.ObjectRayOrigin) - 1.0f;
        return candidate.FrontFace || t > 0.0f;
    }
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

// Callbacks passed through a helper's inout parameter
RayHit TraceAlphaTested(RayDesc ray, inout AlphaTest callbacks)
{
    return RayTraceClosest(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_FORCE_NON_OPAQUE, gInstanceMask, ray, callbacks);
}

float Shade(RayHit hit)
{
    // Members named like intrinsics
    float value = hit.T + hit.InstanceID + hit.InstanceIndex + hit.PrimitiveIndex + hit.GeometryIndex;
    value += hit.InstanceContributionToHitGroupIndex + hit.ObjectToWorld[0][3] + hit.WorldToObject[2][3];
    return hit.IsHit ? value + (hit.IsProcedural ? 1.0f : 0.0f) + (hit.FrontFace ? hit.Barycentrics.x : 0.0f) : 0.0f;
}

float TraceAll(RayDesc ray)
{
    RayHit plain = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, ray);
    bool occluded = RayTraceAny(gScene, RAY_FLAG_CULL_OPAQUE, 0xFF, ray).IsHit;

    AlphaTest alpha;
    alpha.Cutoff = 0.5f;
    alpha.Tested = 0;
    RayHit alphaClosest = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, ray, alpha);
    RayHit alphaAny = RayTraceAny(gScene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, 0xFF, ray, alpha);
    RayHit alphaHelper = TraceAlphaTested(ray, alpha);

    Spheres spheres;
    spheres.Radius = 2.0f;
    RayHit sphereHit = RayTraceClosest(gScene, gRuntimeFlags, gInstanceMask, ray, spheres);

    TranslucentSpheres translucent;
    translucent.Transmittance = 1.0f;
    RayHit translucentHit = RayTraceAny(gScene, gRuntimeFlags | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, ray, translucent);

    float result = Shade(plain) + Shade(alphaClosest) + Shade(alphaAny) + Shade(alphaHelper) + Shade(sphereHit);
    result += Shade(translucentHit) * translucent.Transmittance + alpha.Tested;
    return occluded ? 0.0f : result;
}

[numthreads(8, 8, 1)]
void CS(uint3 threadId : SV_DispatchThreadID)
{
    gOutput[threadId.xy] = TraceAll(MakeRay((float2)threadId.xy));
}

float4 PS(float4 position : SV_Position) : SV_Target0
{
    return TraceAll(MakeRay(position.xy));
}

// Targets without ray tracing pipelines compile only the inline tracing entries
#ifndef INLINE_RAY_TRACING_ONLY

struct Payload
{
    float4 color;
};

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    RayDesc ray;
    ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    ray.TMin = 0.001f;
    ray.Direction = float3(0, 1, 0);
    ray.TMax = 100.0f;

    payload.color = TraceAll(ray) * attribs.barycentrics.x;
}

#endif
