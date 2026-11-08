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
#include <string>


namespace Xsc
{

namespace InlineRayTracingHLSL
{


/*
Returns the name of the helper for the specified built-in and optional callbacks struct. 'intrinsic' must be
Intrinsic::RayTraceClosest or Intrinsic::RayTraceAny; any other intrinsic throws std::invalid_argument.
*/
std::string HelperName(const Intrinsic intrinsic, const std::string& callbacksIdent);

/*
Returns the helper function that implements the specified built-in with a RayQuery loop. 'intrinsic' must be
Intrinsic::RayTraceClosest or Intrinsic::RayTraceAny; any other intrinsic throws std::invalid_argument. 'callbacksIdent'
is empty for calls without callbacks; 'hasAnyHit' and 'hasIntersection' say which callback methods the struct has.
*/
std::string Helper(const Intrinsic intrinsic, const std::string& callbacksIdent, bool hasAnyHit, bool hasIntersection);


} // /namespace InlineRayTracingHLSL

} // /namespace Xsc


#endif



// ================================================================================
