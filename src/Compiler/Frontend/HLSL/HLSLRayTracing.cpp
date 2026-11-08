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
        "\n"
        "static const uint RAY_FLAG_NONE = 0;\n"
        "static const uint RAY_FLAG_FORCE_OPAQUE = 1;\n"
        "static const uint RAY_FLAG_FORCE_NON_OPAQUE = 2;\n"
        "static const uint RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH = 4;\n"
        "static const uint RAY_FLAG_SKIP_CLOSEST_HIT_SHADER = 8;\n"
        "static const uint RAY_FLAG_CULL_BACK_FACING_TRIANGLES = 16;\n"
        "static const uint RAY_FLAG_CULL_FRONT_FACING_TRIANGLES = 32;\n"
        "static const uint RAY_FLAG_CULL_OPAQUE = 64;\n"
        "static const uint RAY_FLAG_CULL_NON_OPAQUE = 128;\n"
        "static const uint RAY_FLAG_SKIP_TRIANGLES = 256;\n"
        "static const uint RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES = 512;\n"
        "\n"
        "static const uint HIT_KIND_TRIANGLE_FRONT_FACE = 254;\n"
        "static const uint HIT_KIND_TRIANGLE_BACK_FACE = 255;\n";
    return text;
}


} // /namespace HLSLRayTracing

} // /namespace Xsc



// ================================================================================
