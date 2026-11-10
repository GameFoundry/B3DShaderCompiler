// Only the functions the compiled entry point reaches are written. Global variables are all written, so the
// functions their initializers call are written too.

cbuffer Parameters
{
    float4 tint;
};

float Scale(float value)
{
    return value * 2.0;
}

// No entry point calls this, the initializer below does
float InitializerValue()
{
    return 0.5;
}

static const float initialized = InitializerValue();

float4 OtherHelper(float4 value)
{
    return value * tint;
}

float4 PSOther(float4 position : SV_Position) : SV_Target
{
    return OtherHelper(position);
}

float4 PS(float4 position : SV_Position) : SV_Target
{
    return tint * Scale(position.x);
}
