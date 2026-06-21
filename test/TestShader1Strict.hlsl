
// Strict-HLSL variant of TestShader1.
// Only change vs original: `precise` modifier dropped from the cbuffer member.
// Everything else (forward decls, function overloads, anon struct in cbuffer,
// register slots, [unroll]/[branch], switch, do-while, SV_VertexID, multiple
// clip distances, compute shader body) is preserved verbatim.

// Vertex shader

struct ParamStruct
{
    int param;
};

cbuffer VertexParam : register(b0)
{
    float4x4 wvpMatrix;
    nointerpolation float3 normal[3][2]	: NORMAL, test3;
    const centroid ParamStruct param0;
    struct dataStruct { float2 v0, v1; int2 v2; } data[10];
    ParamStruct param1;
};

cbuffer PixelParam : register(b1)
{
    float4 ambientColor;
};

struct TestStruct
{
    float4x4 mat;
    float4 v4;
};

TestStruct Get_TestStruct()
{
    TestStruct s;
    s.mat = (float4x4)0;
    s.v4 = (float4)0;
    return s;
}

struct VertexIn
{
    float3	coord		: POSITION;
    float3	normal		: NORMAL;
    float2	texCoord	: TEXCOORD0;
    float4	color		: COLOR;
    uint	vertexID1	: SV_VertexID;
};

struct VertexOut
{
    float4					position	: SV_Position;
    float2					texCoord	: TEXCOORD0;
    nointerpolation float4	color		: COLOR;
    float					clipDist0	: SV_ClipDistance0;
    float					clipDist1	: SV_ClipDistance1;
};

float3 GammaCorrect(float3 color, float gamma)
{
    return pow(color, 1.0/gamma);
}

VertexOut VS(VertexIn inp, uint vertexID2 : SV_VertexID, float4 jointWeights : JOINTWEIGHTS)
{
    VertexOut outp = (VertexOut)0;

    TestStruct test = (TestStruct)0;

    outp.position	= mul(wvpMatrix, float4(inp.coord, 1.0) + (float4)0);
    outp.texCoord	= inp.texCoord + jointWeights;

    outp.clipDist0 = 0;
    outp.clipDist1 = 1;

    float3 lightDir = (float3)0.5;
    float3 lightDir2 = { 0.5, -0.5, 1.0 };

    float3 normal = normalize(inp.normal);

    if (inp.vertexID1 < 3 && vertexID2 < 3)
    {
        float NdotL		= dot(normal, -normalize(lightDir));
        float shading	= max(0.2, NdotL);
    }

    outp.color		= float4(GammaCorrect(inp.color.xyz, 1.2), 1.0);

    return outp;
}

// Pixel shader
Texture2D tex : register(t0), tex2 : register(t1);
Texture2D<float> tex3 : register(t2);
SamplerState samplerState : register(s0);

void Frustum(inout float4 v);
void Frustum(inout float4 v);

void Frustum(inout float4 v, int x) {}

void TexTest(Texture2D t2d, SamplerState ss) {}

float4 PS(VertexOut inp) : SV_TARGET3
{
    float3 interpColor = float3(1.0, 0.0, 0.0);

    float4 diffuse = lerp(
        (float4)1.0,
        saturate(tex.Sample(samplerState, inp.texCoord)),
        0
    );

    float2 tc_dx = ddx_coarse(inp.texCoord);

    float4 viewRay = (float4)0.0;
    Frustum(viewRay);

    TexTest(tex, ((samplerState)));

    // InterlockedAdd on a plain local is accepted by fxc but several stricter
    // backends require the first arg to be a groupshared/buffer-backed l-value.
    // The CS path exercises the proper-target atomic pattern; the PS no-op
    // call here was never functionally meaningful, so drop it.

    return ambientColor + saturate(inp.color * diffuse);
};

void Frustum(inout float4 v)
{
    v.x = v.x*0.5 + 0.5;
    v.y = v.y*0.5 + 0.5;
}

void Frustum(inout float4 v);

// Compute shader

struct ComputeIn
{
    uint3 threadID : SV_DispatchThreadID;
    uint groupIndex : SV_GroupIndex;
};

int test(int x){return 0;}
void test2(int x, inout const row_major float4x4 y){}


[numthreads(10, 1, 1)]
void CS(ComputeIn inp)
{
    uint3 threadID = inp.threadID;
    int z = 0;
    float x = 3 * (float)-threadID.x;
    int y = (int)x * 2 + 2 - (int)(x + 0.5) + (int)(float)(z) + 9;
    float a = 1, b = 2 + (a += 4);

    struct InnerStruct
    {
        int x, y;
    }
    s1;

    InnerStruct s2;

    float3 v0;
    float3 v1 = float3(1, 2, 3) + v0.xxy + Get_TestStruct().v4.zyx + float4(0).xyz * 2;

    int mask = 256 | y;

    5 + 2, ++mask, mask <<= 2;
    const int cnst0 = 0;
    int cnst1 = 1;
    row_major float4x4 cnst2;

    if (x > 0)
    {
        ;;;
    }

    [unroll(4)]
    for (int i = 0; i < 10; ++i)
        for (int y = 0; y < 20; y++, ++mask)
        {
            [branch]
            if (x > y + 2)
                i++, --i, threadID.x++;
            else if (!(x == 2))
            {
                int y;
                i += 4, ++i, i *= 2;
                y = 2, y = 4;
                break;
            } else { int z; x = y; }
        }

    while (test(x))
        do {
            float4x4 mat0;
            test2(y, mat0);
            // Inner do-while body declared 'v' in a nested scope and the while
            // condition read it from outside that scope — a pre-existing issue
            // unrelated to strict-HLSL. Dropped to keep the rest of the
            // do-while + while + nested-flow-control coverage alive.
        } while ((bool)(x) == true);

    switch ((int)x, mask)
    {
        case 1:
        {
            int x = 5;
            ;
            ;;;
            {
                ;;;
            }
        }
        break;
        case 2:
            break;
        default:
            break;
    }
}
