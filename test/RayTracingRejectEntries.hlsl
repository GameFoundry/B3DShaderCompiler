// The [shader] attribute and ray tracing entry point rules. Each case macro declares one 'Entry' that breaks one rule.

struct Payload
{
    float4 color;
};

struct LargeAttributes
{
    float3x3 basis;     // 36 bytes, over the 32 byte limit
};

#ifdef PAYLOAD_WITH_RESOURCE
struct ResourcePayload
{
    float4 color;
    Texture2D colorTexture;
};
#endif

#ifdef UNKNOWN_STAGE
[shader("raygen")]
void Entry() { }
#endif

#ifdef DUPLICATE_ATTRIBUTE
[shader("miss")]
[shader("miss")]
void Entry(inout Payload payload) { payload.color = 0.0f; }
#endif

#ifdef ATTRIBUTE_ON_MEMBER_FUNCTION
struct Methods
{
    [shader("miss")]
    void Method(inout Payload payload) { payload.color = 0.0f; }
};

[shader("raygeneration")]
void Entry() { }
#endif

#ifdef ATTRIBUTE_ON_STRUCT
[shader("miss")]
struct Marked
{
    float value;
};

[shader("raygeneration")]
void Entry() { }
#endif

#ifdef ATTRIBUTE_ON_STATEMENT
[shader("raygeneration")]
void Entry()
{
    [shader("miss")]
    for (uint i = 0; i < 2; ++i) { }
}
#endif

#if defined(STAGE_MISMATCH) || defined(NOT_RAY_TRACING_TARGET)
[shader("miss")]
void Entry(inout Payload payload) { payload.color = 0.0f; }
#endif

#ifdef MISSING_ATTRIBUTE
void Entry() { }
#endif

#ifdef RETURNS_VALUE
[shader("raygeneration")]
float Entry() { return 0.0f; }
#endif

#ifdef RAYGEN_PARAMETER
[shader("raygeneration")]
void Entry(uint index) { }
#endif

#ifdef MISS_IN_PAYLOAD
[shader("miss")]
void Entry(in Payload payload) { }
#endif

#ifdef CALLABLE_TWO_PARAMETERS
[shader("callable")]
void Entry(inout Payload payload, inout Payload other) { }
#endif

#ifdef CLOSESTHIT_ONE_PARAMETER
[shader("closesthit")]
void Entry(inout Payload payload) { payload.color = 0.0f; }
#endif

#ifdef ANYHIT_INOUT_ATTRIBUTES
[shader("anyhit")]
void Entry(inout Payload payload, inout BuiltInTriangleIntersectionAttributes attribs) { payload.color.xy = attribs.barycentrics; }
#endif

#ifdef PAYLOAD_NOT_STRUCT
[shader("miss")]
void Entry(inout float4 payload) { payload = 0.0f; }
#endif

#ifdef PAYLOAD_WITH_RESOURCE
[shader("miss")]
void Entry(inout ResourcePayload payload) { payload.color = 0.0f; }
#endif

#ifdef ATTRIBUTES_TOO_LARGE
[shader("closesthit")]
void Entry(inout Payload payload, in LargeAttributes attribs) { payload.color.xyz = attribs.basis[0]; }
#endif

#ifdef REPORT_HIT_TOO_LARGE
[shader("intersection")]
void Entry()
{
    LargeAttributes attribs = (LargeAttributes)0;
    ReportHit(1.0f, 0, attribs);
}
#endif

#ifdef REPORT_HIT_NOT_STRUCT
[shader("intersection")]
void Entry()
{
    ReportHit(1.0f, 0, float2(0.0f, 1.0f));
}
#endif
