// RayTraceClosest and RayTraceAny rejections: RayQuery, callbacks arguments and structs, constant flags, arguments, and
// what callback methods may call. Each case macro selects one error.

RaytracingAccelerationStructure gScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

struct Payload
{
    float4 color;
};

RayDesc MakeRay()
{
    RayDesc ray;
    ray.Origin = 0.0f;
    ray.TMin = 0.0f;
    ray.Direction = float3(0, 0, 1);
    ray.TMax = 1.0f;
    return ray;
}

float NestedTrace()
{
    return RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay()).T;
}

void ClipFromHelper(float value)
{
    clip(value);
}

struct Callbacks
{
    float Value;

    bool AnyHit(RayCandidate candidate)
    {
#ifdef NEST_TRACE_DIRECT
        Value += RayTraceAny(gScene, RAY_FLAG_NONE, 0xFF, MakeRay()).T;
#endif
#ifdef NEST_TRACE_HELPER
        Value += NestedTrace();
#endif
#ifdef NEST_TRACE_RAY
        Payload payload = (Payload)0;
        TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, MakeRay(), payload);
#endif
#ifdef NEST_DISCARD
        if (candidate.T > 1.0f)
            discard;
#endif
#ifdef NEST_CLIP_HELPER
        ClipFromHelper(candidate.T);
#endif
        return candidate.T > Value;
    }

    bool Intersection(RayCandidate candidate, out float t)
    {
#ifdef NEST_CALL_SHADER
        Payload payload = (Payload)0;
        CallShader(0, payload);
#endif
        t = Value;
        return true;
    }
};

struct Holder
{
    Callbacks callbacks;
};

#ifdef CALLBACKS_GLOBAL
static Callbacks gCallbacks;
#endif

#ifdef CALLBACKS_NO_METHODS
struct Typo
{
    bool Anyhit(RayCandidate candidate) { return true; }
};
#endif

#ifdef ANYHIT_EXTRA_PARAMETER
struct BadAnyHit
{
    bool AnyHit(RayCandidate candidate, float cutoff) { return candidate.T > cutoff; }
};
#endif

#ifdef ANYHIT_RETURN_TYPE
struct BadAnyHit
{
    float AnyHit(RayCandidate candidate) { return candidate.T; }
};
#endif

#ifdef INTERSECTION_NOT_OUT
struct BadIntersection
{
    bool Intersection(RayCandidate candidate, float t) { return t > 0.0f; }
};
#endif

#ifdef INTERSECTION_WRONG_TYPE
struct BadIntersection
{
    bool Intersection(RayCandidate candidate, out uint t) { t = 0; return true; }
};
#endif

float4 Trace()
{
    Callbacks callbacks;
    callbacks.Value = 0.5f;
    RayHit hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), callbacks);

#ifdef RAY_QUERY
    RayQuery<RAY_FLAG_NONE> query;
#endif
#ifdef CALLBACKS_GLOBAL
    hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), gCallbacks);
#endif
#ifdef CALLBACKS_MEMBER
    Holder holder;
    holder.callbacks.Value = 0.5f;
    hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), holder.callbacks);
#endif
#ifdef CALLBACKS_TEMPORARY
    hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), (Callbacks)0);
#endif
#ifdef CALLBACKS_ARRAY_ELEMENT
    Callbacks list[2];
    list[0].Value = 0.5f;
    hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), list[0]);
#endif
#ifdef CALLBACKS_NOT_STRUCT
    float notStruct = 0.0f;
    hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), notStruct);
#endif
#ifdef CALLBACKS_NO_METHODS
    Typo typo;
    hit = RayTraceAny(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), typo);
#endif
#if defined(ANYHIT_EXTRA_PARAMETER) || defined(ANYHIT_RETURN_TYPE)
    BadAnyHit badAnyHit;
    hit = RayTraceAny(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), badAnyHit);
#endif
#if defined(INTERSECTION_NOT_OUT) || defined(INTERSECTION_WRONG_TYPE)
    BadIntersection badIntersection;
    hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, MakeRay(), badIntersection);
#endif
#ifdef FLAG_ACCEPT_FIRST_HIT
    hit = RayTraceClosest(gScene, RAY_FLAG_CULL_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, MakeRay());
#endif
#ifdef FLAG_SKIP_CLOSEST_HIT
    hit = RayTraceAny(gScene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, MakeRay());
#endif
#ifdef FLAG_SKIP_TRIANGLES
    hit = RayTraceClosest(gScene, RAY_FLAG_SKIP_TRIANGLES, 0xFF, MakeRay(), callbacks);
#endif
#ifdef FLAG_UNKNOWN
    hit = RayTraceClosest(gScene, 0x400, 0xFF, MakeRay());
#endif
#ifdef ARG_COUNT
    hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF);
#endif
#ifdef ARG_TYPE
    hit = RayTraceAny(gScene, RAY_FLAG_NONE, 0xFF, float3(0, 0, 1));
#endif
    return float4(hit.T, callbacks.Value, 0.0f, 1.0f);
}

[numthreads(8, 8, 1)]
void CS(uint3 threadId : SV_DispatchThreadID)
{
    gOutput[threadId.xy] = Trace();
}

float4 PS(float4 position : SV_Position) : SV_Target0
{
    return Trace() * position;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.color = Trace() * attribs.barycentrics.x;
}
