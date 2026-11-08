/*
 * HLSLRayTracing.h
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_HLSL_RAY_TRACING_H
#define XSC_HLSL_RAY_TRACING_H


#include <string>


namespace Xsc
{

namespace HLSLRayTracing
{


// Mask of the ray flags the inline built-ins (RayTraceClosest, RayTraceAny) accept: FORCE_OPAQUE, FORCE_NON_OPAQUE,
// CULL_BACK_FACING_TRIANGLES, CULL_FRONT_FACING_TRIANGLES, CULL_OPAQUE, CULL_NON_OPAQUE and SKIP_PROCEDURAL_PRIMITIVES.
static const unsigned int allowedRayFlags = 0x2F3;

// Returns the predeclared ray tracing declarations as HLSL text.
const std::string& BuiltInDeclarations();

// Returns the declarations of RayCandidate and RayHit as HLSL text, a part of BuiltInDeclarations().
const std::string& InlineTypes();


} // /namespace HLSLRayTracing

} // /namespace Xsc


#endif



// ================================================================================
