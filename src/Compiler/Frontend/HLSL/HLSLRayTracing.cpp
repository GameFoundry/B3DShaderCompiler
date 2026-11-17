/*
 * HLSLRayTracing.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "HLSLRayTracing.h"


namespace Xsc
{

namespace HLSLRayTracing
{


const std::string& InlineTypes()
{
    static const std::string text =
        "struct RayCandidate\n"
        "{\n"
        "    float T;\n"
        "    float2 Barycentrics;\n"
        "    bool FrontFace;\n"
        "    uint InstanceIndex;\n"
        "    uint InstanceID;\n"
        "    uint InstanceContributionToHitGroupIndex;\n"
        "    uint GeometryIndex;\n"
        "    uint PrimitiveIndex;\n"
        "    float3 ObjectRayOrigin;\n"
        "    float3 ObjectRayDirection;\n"
        "};\n"
        "\n"
        "struct RayHit\n"
        "{\n"
        "    bool IsHit;\n"
        "    bool IsProcedural;\n"
        "    float T;\n"
        "    float2 Barycentrics;\n"
        "    bool FrontFace;\n"
        "    uint InstanceIndex;\n"
        "    uint InstanceID;\n"
        "    uint InstanceContributionToHitGroupIndex;\n"
        "    uint GeometryIndex;\n"
        "    uint PrimitiveIndex;\n"
        "    float3x4 ObjectToWorld;\n"
        "    float3x4 WorldToObject;\n"
        "};\n";
    return text;
}

// Returns the declarations of the RAY_FLAG_* constants as HLSL text.
static std::string RayFlagDeclarations()
{
    struct RayFlag
    {
        const char*     name;
        unsigned int    value;
    };

    static const RayFlag rayFlags[] =
    {
        { "RAY_FLAG_NONE",                              RayFlags::None                       },
        { "RAY_FLAG_FORCE_OPAQUE",                      RayFlags::ForceOpaque                },
        { "RAY_FLAG_FORCE_NON_OPAQUE",                  RayFlags::ForceNonOpaque             },
        { "RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH",   RayFlags::AcceptFirstHitAndEndSearch },
        { "RAY_FLAG_SKIP_CLOSEST_HIT_SHADER",           RayFlags::SkipClosestHitShader       },
        { "RAY_FLAG_CULL_BACK_FACING_TRIANGLES",        RayFlags::CullBackFacingTriangles    },
        { "RAY_FLAG_CULL_FRONT_FACING_TRIANGLES",       RayFlags::CullFrontFacingTriangles   },
        { "RAY_FLAG_CULL_OPAQUE",                       RayFlags::CullOpaque                 },
        { "RAY_FLAG_CULL_NON_OPAQUE",                   RayFlags::CullNonOpaque              },
        { "RAY_FLAG_SKIP_TRIANGLES",                    RayFlags::SkipTriangles              },
        { "RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES",        RayFlags::SkipProceduralPrimitives   },
    };

    std::string s;
    for (const auto& flag : rayFlags)
        s += std::string("static const uint ") + flag.name + " = " + std::to_string(flag.value) + ";\n";

    return s;
}

const std::string& BuiltInDeclarations()
{
    /* Values match DXC's built-in declarations */
    static const std::string text =
        "struct RayDesc\n"
        "{\n"
        "    float3 Origin;\n"
        "    float TMin;\n"
        "    float3 Direction;\n"
        "    float TMax;\n"
        "};\n"
        "\n"
        "struct BuiltInTriangleIntersectionAttributes\n"
        "{\n"
        "    float2 barycentrics;\n"
        "};\n"
        "\n" +
        InlineTypes() +
        "\n" +
        RayFlagDeclarations() +
        "\n"
        "static const uint HIT_KIND_TRIANGLE_FRONT_FACE = 254;\n"
        "static const uint HIT_KIND_TRIANGLE_BACK_FACE = 255;\n";
    return text;
}


} // /namespace HLSLRayTracing

} // /namespace Xsc



// ================================================================================
