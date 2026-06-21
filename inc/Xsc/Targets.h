/*
 * Targets.h
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_TARGETS_H
#define XSC_TARGETS_H


#include "Export.h"
#include <string>
#include <map>
#include <vector>


namespace Xsc
{


/* ===== Public enumerations ===== */

//! Shader target enumeration.
enum class ShaderTarget
{
    Undefined,                      //!< Undefined shader target.

    VertexShader,                   //!< Vertex shader.
    TessellationControlShader,      //!< Tessellation-control (also Hull-) shader.
    TessellationEvaluationShader,   //!< Tessellation-evaluation (also Domain-) shader.
    GeometryShader,                 //!< Geometry shader.
    FragmentShader,                 //!< Fragment (also Pixel-) shader.
    ComputeShader,                  //!< Compute shader.
};

//! Input shader version enumeration.
enum class InputShaderVersion
{
    Cg      = 2,            //!< Cg (C for graphics) is a slightly extended HLSL3.

    HLSL3   = 3,            //!< HLSL Shader Model 3.0 (DirectX 9).
    HLSL4   = 4,            //!< HLSL Shader Model 4.0 (DirectX 10).
    HLSL5   = 5,            //!< HLSL Shader Model 5.0 (DirectX 11).
    HLSL6   = 6,            //!< HLSL Shader Model 6.0 (DirectX 12).

    GLSL    = 0x0000ffff,   //!< GLSL (OpenGL).
    ESSL    = 0x0001ffff,   //!< GLSL (OpenGL ES).
    VKSL    = 0x0002ffff,   //!< GLSL (Vulkan).
};

//! Intermediate language enumeration.
enum class IntermediateLanguage
{
    SPIRV, //!< SPIR-V.
};


/* ===== Output target languages ===== */

/*
The output shader language is selected with a string identifier instead of an
enumeration, so that optional backends can be added. The built-in identifiers are 
provided as named constants below; additional targets (registered by their own 
backend) are discovered at runtime through GetSupportedTargetLanguages() / IsTargetLanguageSupported().
*/
namespace TargetLanguage
{

XSC_EXPORT extern const char* const GLSL110;    //!< "GLSL110" — GLSL 1.10 (OpenGL 2.0).
XSC_EXPORT extern const char* const GLSL120;    //!< "GLSL120" — GLSL 1.20 (OpenGL 2.1).
XSC_EXPORT extern const char* const GLSL130;    //!< "GLSL130" — GLSL 1.30 (OpenGL 3.0).
XSC_EXPORT extern const char* const GLSL140;    //!< "GLSL140" — GLSL 1.40 (OpenGL 3.1).
XSC_EXPORT extern const char* const GLSL150;    //!< "GLSL150" — GLSL 1.50 (OpenGL 3.2).
XSC_EXPORT extern const char* const GLSL330;    //!< "GLSL330" — GLSL 3.30 (OpenGL 3.3).
XSC_EXPORT extern const char* const GLSL400;    //!< "GLSL400" — GLSL 4.00 (OpenGL 4.0).
XSC_EXPORT extern const char* const GLSL410;    //!< "GLSL410" — GLSL 4.10 (OpenGL 4.1).
XSC_EXPORT extern const char* const GLSL420;    //!< "GLSL420" — GLSL 4.20 (OpenGL 4.2).
XSC_EXPORT extern const char* const GLSL430;    //!< "GLSL430" — GLSL 4.30 (OpenGL 4.3).
XSC_EXPORT extern const char* const GLSL440;    //!< "GLSL440" — GLSL 4.40 (OpenGL 4.4).
XSC_EXPORT extern const char* const GLSL450;    //!< "GLSL450" — GLSL 4.50 (OpenGL 4.5).
XSC_EXPORT extern const char* const GLSL;       //!< "GLSL"    — Auto-detect minimal required GLSL version (for OpenGL 2+).

XSC_EXPORT extern const char* const ESSL100;    //!< "ESSL100" — ESSL 1.00 (OpenGL ES 2.0).
XSC_EXPORT extern const char* const ESSL300;    //!< "ESSL300" — ESSL 3.00 (OpenGL ES 3.0).
XSC_EXPORT extern const char* const ESSL310;    //!< "ESSL310" — ESSL 3.10 (OpenGL ES 3.1).
XSC_EXPORT extern const char* const ESSL320;    //!< "ESSL320" — ESSL 3.20 (OpenGL ES 3.2).
XSC_EXPORT extern const char* const ESSL;       //!< "ESSL"    — Auto-detect minimum required ESSL version (for OpenGL ES 2+).

XSC_EXPORT extern const char* const VKSL450;    //!< "VKSL450" — VKSL 4.50 (Vulkan 1.0).
XSC_EXPORT extern const char* const VKSL;       //!< "VKSL"    — Auto-detect minimum required VKSL version (for Vulkan/SPIR-V).

XSC_EXPORT extern const char* const HLSL5;      //!< "HLSL5"   — HLSL Shader Model 5.0 (DirectX 11). Requires the HLSL backend.
XSC_EXPORT extern const char* const HLSL;       //!< "HLSL"    — Auto-detect HLSL output version. Requires the HLSL backend.

} // /namespace TargetLanguage


/* ===== Public functions ===== */

//! Returns the specified shader target as string.
XSC_EXPORT std::string ToString(const ShaderTarget target);

//! Returns the specified shader input version as string.
XSC_EXPORT std::string ToString(const InputShaderVersion shaderVersion);

//! Returns the specified intermediate language as string.
XSC_EXPORT std::string ToString(const IntermediateLanguage language);

//! Returns true if the shader input version specifies HLSL (for DirectX) or Cg (handled as dialect or HLSL).
XSC_EXPORT bool IsLanguageHLSL(const InputShaderVersion shaderVersion);

//! Returns true if the shader input version specifies GLSL (for OpenGL, OpenGL ES, and Vulkan).
XSC_EXPORT bool IsLanguageGLSL(const InputShaderVersion shaderVersion);

//! Returns the list of output target languages supported by this build (e.g. "GLSL450", "HLSL5", plus any optional backend registered into this build).
XSC_EXPORT std::vector<std::string> GetSupportedTargetLanguages();

//! Returns true if the specified output target language is supported by this build (i.e. a backend is registered for it).
XSC_EXPORT bool IsTargetLanguageSupported(const std::string& targetLanguage);

//! Returns the enumeration of all supported GLSL extensions as a map of extension name and version number.
XSC_EXPORT const std::map<std::string, int>& GetGLSLExtensionEnumeration();


} // /namespace Xsc


#endif



// ================================================================================