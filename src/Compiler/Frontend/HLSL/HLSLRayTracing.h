/*
 * HLSLRayTracing.h
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_HLSL_RAY_TRACING_H
#define XSC_HLSL_RAY_TRACING_H


#include <Xsc/Export.h>
#include <string>


namespace Xsc
{

namespace HLSLRayTracing
{


// Values of the RAY_FLAG_* constants, which match DXR.
struct RayFlags
{
    enum : unsigned int
    {
        None                        = 0,
        ForceOpaque                 = (1u << 0),
        ForceNonOpaque              = (1u << 1),
        AcceptFirstHitAndEndSearch  = (1u << 2),
        SkipClosestHitShader        = (1u << 3),
        CullBackFacingTriangles     = (1u << 4),
        CullFrontFacingTriangles    = (1u << 5),
        CullOpaque                  = (1u << 6),
        CullNonOpaque               = (1u << 7),
        SkipTriangles               = (1u << 8),
        SkipProceduralPrimitives    = (1u << 9),
    };
};

// Mask of the ray flags the inline built-ins (RayTraceClosest, RayTraceAny) accept.
static const unsigned int allowedRayFlags =
(
    RayFlags::ForceOpaque               |
    RayFlags::ForceNonOpaque            |
    RayFlags::CullBackFacingTriangles   |
    RayFlags::CullFrontFacingTriangles  |
    RayFlags::CullOpaque                |
    RayFlags::CullNonOpaque             |
    RayFlags::SkipProceduralPrimitives
);

// Returns the predeclared ray tracing declarations as HLSL text.
XSC_EXPORT const std::string& BuiltInDeclarations();

// Returns the declarations of RayCandidate and RayHit as HLSL text, a part of BuiltInDeclarations().
const std::string& InlineTypes();


} // /namespace HLSLRayTracing

} // /namespace Xsc


#endif



// ================================================================================
