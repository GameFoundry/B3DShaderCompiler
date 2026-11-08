// RayTraceClosest helpers fill only the RayCandidate and RayHit fields that are read. 'Trimmed' reads a few fields
// through helper functions and local copies; 'Escaping' stores the candidate and the hit, so every field is filled.

RaytracingAccelerationStructure gScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);
RWStructuredBuffer<RayHit> gHits : register(u1);

RayDesc MakeRay(float2 position)
{
    RayDesc ray;
    ray.Origin = float3(position, 0.0f);
    ray.TMin = 0.0f;
    ray.Direction = float3(0, 0, 1);
    ray.TMax = 1000.0f;
    return ray;
}

float CandidateDistance(RayCandidate candidate)
{
    return candidate.T;
}

// AnyHit reads T through a function; Intersection reads ObjectRayOrigin through a local copy
struct ReadsFew
{
    float Cutoff;

    bool AnyHit(RayCandidate candidate)
    {
        return CandidateDistance(candidate) > Cutoff;
    }

    bool Intersection(RayCandidate candidate, out float t)
    {
        RayCandidate copy = candidate;
        t = length(copy.ObjectRayOrigin);
        return true;
    }
};

// AnyHit stores the candidate in a member
struct StoresCandidate
{
    RayCandidate Last;

    bool AnyHit(RayCandidate candidate)
    {
        Last = candidate;
        return true;
    }
};

float HitDistance(RayHit hit)
{
    return hit.T;
}

[numthreads(8, 8, 1)]
void Trimmed(uint3 threadId : SV_DispatchThreadID)
{
    ReadsFew callbacks;
    callbacks.Cutoff = 0.5f;
    RayHit hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay((float2)threadId.xy), callbacks);
    RayHit copy = hit;
    gOutput[threadId.xy] = HitDistance(copy);
}

[numthreads(8, 8, 1)]
void Escaping(uint3 threadId : SV_DispatchThreadID)
{
    StoresCandidate callbacks;
    callbacks.Last = (RayCandidate)0;
    gHits[threadId.x] = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay((float2)threadId.xy), callbacks);
    gOutput[threadId.xy] = callbacks.Last.T;
}
