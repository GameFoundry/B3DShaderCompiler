/*
 * InlineRayTracingHLSL.h
 *
 * HLSL text of the helper functions that implement the inline ray tracing built-ins (RayTraceClosest,
 * RayTraceAny) with DXR's RayQuery.
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_INLINE_RAY_TRACING_HLSL_H
#define XSC_INLINE_RAY_TRACING_HLSL_H


#include "ASTEnums.h"
#include <set>
#include <string>


namespace Xsc
{

namespace InlineRayTracingHLSL
{


// Fields of a built-in ray tracing structure (RayCandidate or RayHit) that a program reads.
struct FieldUsage
{
    // Returns true if the field with the specified identifier is read.
    bool Reads(const std::string& field) const
    {
        return (allFields || fields.find(field) != fields.end());
    }

    bool                    allFields = false;  // Every field is read.
    std::set<std::string>   fields;             // Identifiers of the fields read by member access.
};

/*
Returns the name of the helper for the specified built-in and optional callbacks struct. 'intrinsic' must be
Intrinsic::RayTraceClosest or Intrinsic::RayTraceAny; any other intrinsic throws std::invalid_argument.
*/
XSC_EXPORT std::string HelperName(const Intrinsic intrinsic, const std::string& callbacksIdent);

/*
Returns the helper function that implements the specified built-in with a RayQuery loop. 'intrinsic' must be
Intrinsic::RayTraceClosest or Intrinsic::RayTraceAny; any other intrinsic throws std::invalid_argument. 'callbacksIdent'
is empty for calls without callbacks. 'anyHitFields' and 'intersectionFields' are the RayCandidate fields that the
struct's AnyHit and Intersection methods read, or null if the struct has no such method. 'hitFields' are the RayHit
fields that the program reads. The helper fills only the fields that are read.
*/
std::string Helper(
    const Intrinsic             intrinsic,
    const std::string&          callbacksIdent,
    const FieldUsage*           anyHitFields,
    const FieldUsage*           intersectionFields,
    const FieldUsage&           hitFields
);


} // /namespace InlineRayTracingHLSL

} // /namespace Xsc


#endif



// ================================================================================
