/*
 * InlineRayTracingHLSL.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "InlineRayTracingHLSL.h"
#include "HLSLRayTracing.h"
#include "ReportIdents.h"
#include <cstdio>
#include <stdexcept>


namespace Xsc
{

namespace InlineRayTracingHLSL
{


std::string HelperName(const Intrinsic intrinsic, const std::string& callbacksIdent)
{
    std::string name;
    switch (intrinsic)
    {
        case Intrinsic::RayTraceClosest:
            name = "xsc_RayTraceClosest";
            break;
        case Intrinsic::RayTraceAny:
            name = "xsc_RayTraceAny";
            break;
        default:
            throw std::invalid_argument(R_NotInlineRayTracingIntrinsic);
    }

    if (!callbacksIdent.empty())
        name += "_" + callbacksIdent;
    return name;
}

std::string Helper(const Intrinsic intrinsic, const std::string& callbacksIdent, bool hasAnyHit, bool hasIntersection)
{
    /* Also checks that the intrinsic is one of the two built-ins */
    const std::string name = HelperName(intrinsic, callbacksIdent);

    /* RayTraceAny stops at the first accepted hit; without an Intersection callback, procedural primitives are skipped */
    std::string templateFlags;
    if (intrinsic == Intrinsic::RayTraceAny)
        templateFlags = "RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH";
    if (!hasIntersection)
        templateFlags += (templateFlags.empty() ? "" : " | ") + std::string("RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES");
    if (templateFlags.empty())
        templateFlags = "RAY_FLAG_NONE";

    char allowedFlags[16];
    std::snprintf(allowedFlags, sizeof(allowedFlags), "0x%X", HLSLRayTracing::allowedRayFlags);

    std::string s;

    s += "RayHit " + name + "(RaytracingAccelerationStructure scene, uint rayFlags, uint instanceMask, RayDesc ray";
    if (!callbacksIdent.empty())
        s += ", inout " + callbacksIdent + " callbacks";
    s += ")\n";
    s += "{\n";
    s += "    RayQuery<" + templateFlags + "> query;\n";
    s += "    query.TraceRayInline(scene, rayFlags & " + std::string(allowedFlags) + ", instanceMask, ray);\n";
    s += "    while (query.Proceed())\n";
    s += "    {\n";

    // TODO - We should verify that the compiler will optimize away unused entries.
    if (hasAnyHit || hasIntersection)
    {
        s += "        RayCandidate candidate = (RayCandidate)0;\n";
        s += "        candidate.InstanceIndex = query.CandidateInstanceIndex();\n";
        s += "        candidate.InstanceID = query.CandidateInstanceID();\n";
        s += "        candidate.InstanceContributionToHitGroupIndex = query.CandidateInstanceContributionToHitGroupIndex();\n";
        s += "        candidate.GeometryIndex = query.CandidateGeometryIndex();\n";
        s += "        candidate.PrimitiveIndex = query.CandidatePrimitiveIndex();\n";
        s += "        candidate.ObjectRayOrigin = query.CandidateObjectRayOrigin();\n";
        s += "        candidate.ObjectRayDirection = query.CandidateObjectRayDirection();\n";
    }

    s += "        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)\n";
    s += "        {\n";

    if (hasAnyHit)
    {
        s += "            candidate.T = query.CandidateTriangleRayT();\n";
        s += "            candidate.Barycentrics = query.CandidateTriangleBarycentrics();\n";
        s += "            candidate.FrontFace = query.CandidateTriangleFrontFace();\n";
        s += "            if (callbacks.AnyHit(candidate))\n";
        s += "                query.CommitNonOpaqueTriangleHit();\n";
    }
    else
        s += "            query.CommitNonOpaqueTriangleHit();   // No AnyHit: non-opaque triangles are accepted\n";

    s += "        }\n";

    if (hasIntersection)
    {
        s += "        else\n";
        s += "        {\n";
        s += "            float t;\n";
        s += "            if (callbacks.Intersection(candidate, t) && t >= query.RayTMin() && t <= query.CommittedRayT())\n";
        s += "                query.CommitProceduralPrimitiveHit(t);\n";
        s += "        }\n";
    }

    s += "    }\n";
    s += "\n";
    s += "    RayHit hit = (RayHit)0;\n";
    s += "    hit.IsHit = query.CommittedStatus() != COMMITTED_NOTHING;\n";
    s += "    if (hit.IsHit)\n";
    s += "    {\n";
    s += "        hit.IsProcedural = query.CommittedStatus() == COMMITTED_PROCEDURAL_PRIMITIVE_HIT;\n";
    s += "        hit.T = query.CommittedRayT();\n";
    s += "        hit.InstanceIndex = query.CommittedInstanceIndex();\n";
    s += "        hit.InstanceID = query.CommittedInstanceID();\n";
    s += "        hit.InstanceContributionToHitGroupIndex = query.CommittedInstanceContributionToHitGroupIndex();\n";
    s += "        hit.GeometryIndex = query.CommittedGeometryIndex();\n";
    s += "        hit.PrimitiveIndex = query.CommittedPrimitiveIndex();\n";
    s += "        hit.ObjectToWorld = query.CommittedObjectToWorld3x4();\n";
    s += "        hit.WorldToObject = query.CommittedWorldToObject3x4();\n";
    s += "        if (!hit.IsProcedural)\n";
    s += "        {\n";
    s += "            hit.Barycentrics = query.CommittedTriangleBarycentrics();\n";
    s += "            hit.FrontFace = query.CommittedTriangleFrontFace();\n";
    s += "        }\n";
    s += "    }\n";
    s += "    return hit;\n";
    s += "}\n";

    return s;
}


} // /namespace InlineRayTracingHLSL

} // /namespace Xsc



// ================================================================================
