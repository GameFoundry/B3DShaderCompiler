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


namespace
{

// A field of RayCandidate or RayHit, and the RayQuery method that returns its value.
struct FieldQuery
{
    const char* field;
    const char* query;
};

// RayCandidate fields of every candidate.
const FieldQuery candidateFields[] =
{
    { "InstanceIndex",                       "CandidateInstanceIndex"                       },
    { "InstanceID",                          "CandidateInstanceID"                          },
    { "InstanceContributionToHitGroupIndex", "CandidateInstanceContributionToHitGroupIndex" },
    { "GeometryIndex",                       "CandidateGeometryIndex"                       },
    { "PrimitiveIndex",                      "CandidatePrimitiveIndex"                      },
    { "ObjectRayOrigin",                     "CandidateObjectRayOrigin"                     },
    { "ObjectRayDirection",                  "CandidateObjectRayDirection"                  },
};

// RayCandidate fields of triangle candidates only.
const FieldQuery candidateTriangleFields[] =
{
    { "T",                                   "CandidateTriangleRayT"                        },
    { "Barycentrics",                        "CandidateTriangleBarycentrics"                },
    { "FrontFace",                           "CandidateTriangleFrontFace"                   },
};

// RayHit fields of every committed hit (besides 'IsHit' and 'IsProcedural').
const FieldQuery hitFieldQueries[] =
{
    { "T",                                   "CommittedRayT"                                },
    { "InstanceIndex",                       "CommittedInstanceIndex"                       },
    { "InstanceID",                          "CommittedInstanceID"                          },
    { "InstanceContributionToHitGroupIndex", "CommittedInstanceContributionToHitGroupIndex" },
    { "GeometryIndex",                       "CommittedGeometryIndex"                       },
    { "PrimitiveIndex",                      "CommittedPrimitiveIndex"                      },
    { "ObjectToWorld",                       "CommittedObjectToWorld3x4"                    },
    { "WorldToObject",                       "CommittedWorldToObject3x4"                    },
};

// RayHit fields of committed triangle hits only.
const FieldQuery hitTriangleFieldQueries[] =
{
    { "Barycentrics",                        "CommittedTriangleBarycentrics"                },
    { "FrontFace",                           "CommittedTriangleFrontFace"                   },
};

// Appends an assignment for each of the specified fields that is read.
template <std::size_t N>
void AppendFieldQueries(std::string& s, const std::string& indent, const char* ident, const FieldQuery (&queries)[N], const FieldUsage& usage)
{
    for (const auto& query : queries)
    {
        if (usage.Reads(query.field))
            s += indent + ident + "." + query.field + " = query." + query.query + "();\n";
    }
}

template <std::size_t N>
bool ReadsAny(const FieldQuery (&queries)[N], const FieldUsage& usage)
{
    for (const auto& query : queries)
    {
        if (usage.Reads(query.field))
            return true;
    }
    return false;
}

// Appends the declaration of the candidate and the assignments of the fields that are read.
void AppendCandidate(std::string& s, const FieldUsage& usage, bool isTriangle)
{
    const std::string indent = "            ";
    s += indent + "RayCandidate candidate = (RayCandidate)0;\n";
    AppendFieldQueries(s, indent, "candidate", candidateFields, usage);
    if (isTriangle)
        AppendFieldQueries(s, indent, "candidate", candidateTriangleFields, usage);
}

} // /namespace


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

std::string Helper(
    const Intrinsic     intrinsic,
    const std::string&  callbacksIdent,
    const FieldUsage*   anyHitFields,
    const FieldUsage*   intersectionFields,
    const FieldUsage&   hitFields)
{
    /* Also checks that the intrinsic is one of the two built-ins */
    const std::string name = HelperName(intrinsic, callbacksIdent);

    const bool hasIntersection = (intersectionFields != nullptr);

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

    /* Each branch fills only the candidate fields its callback reads (DXC's SPIR-V output keeps unused queries) */
    s += "        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)\n";
    s += "        {\n";

    if (anyHitFields)
    {
        AppendCandidate(s, *anyHitFields, true);
        s += "            if (callbacks.AnyHit(candidate))\n";
        s += "                query.CommitNonOpaqueTriangleHit();\n";
    }
    else
        s += "            query.CommitNonOpaqueTriangleHit();   // No AnyHit: non-opaque triangles are accepted\n";

    s += "        }\n";

    if (intersectionFields)
    {
        s += "        else\n";
        s += "        {\n";
        AppendCandidate(s, *intersectionFields, false);
        s += "            float t;\n";
        s += "            if (callbacks.Intersection(candidate, t) && t >= query.RayTMin() && t <= query.CommittedRayT())\n";
        s += "                query.CommitProceduralPrimitiveHit(t);\n";
        s += "        }\n";
    }

    s += "    }\n";
    s += "\n";
    /* Fill only the hit fields the program reads; 'IsHit' and 'IsProcedural' also guard the fields that depend on them */
    const bool readsTriangleFields  = ReadsAny(hitTriangleFieldQueries, hitFields);
    const bool readsProcedural      = (readsTriangleFields || hitFields.Reads("IsProcedural"));
    const bool readsCommittedFields = (readsProcedural || ReadsAny(hitFieldQueries, hitFields));

    s += "    RayHit hit = (RayHit)0;\n";

    if (readsCommittedFields || hitFields.Reads("IsHit"))
        s += "    hit.IsHit = query.CommittedStatus() != COMMITTED_NOTHING;\n";

    if (readsCommittedFields)
    {
        s += "    if (hit.IsHit)\n";
        s += "    {\n";

        if (readsProcedural)
            s += "        hit.IsProcedural = query.CommittedStatus() == COMMITTED_PROCEDURAL_PRIMITIVE_HIT;\n";

        AppendFieldQueries(s, "        ", "hit", hitFieldQueries, hitFields);

        if (readsTriangleFields)
        {
            s += "        if (!hit.IsProcedural)\n";
            s += "        {\n";
            AppendFieldQueries(s, "            ", "hit", hitTriangleFieldQueries, hitFields);
            s += "        }\n";
        }

        s += "    }\n";
    }

    s += "    return hit;\n";
    s += "}\n";

    return s;
}


} // /namespace InlineRayTracingHLSL

} // /namespace Xsc



// ================================================================================
