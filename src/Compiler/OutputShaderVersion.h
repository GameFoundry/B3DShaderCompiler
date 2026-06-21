/*
 * OutputShaderVersion.h
 *
 * Internal numeric representation of a built-in output shader version.
 *
 * This enum used to live in the public <Xsc/Targets.h> header and double as the
 * caller-facing output-language selector. The public selector is now a string
 * (see Xsc::TargetLanguage in <Xsc/Targets.h>); this enum is kept purely as an
 * internal implementation detail so the GLSL/HLSL emitters can keep reasoning
 * about the concrete language family and version number. It must not be exported
 * or referenced from any public header, and it deliberately contains no
 * optional/proprietary-backend values — those backends register themselves by
 * string through the BackendRegistry.
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_OUTPUT_SHADER_VERSION_H
#define XSC_OUTPUT_SHADER_VERSION_H


#include <string>


namespace Xsc
{


//! Internal output shader version enumeration (GLSL/ESSL/VKSL/HLSL families only).
enum class OutputShaderVersion
{
    GLSL110 = 110,                  //!< GLSL 1.10 (OpenGL 2.0).
    GLSL120 = 120,                  //!< GLSL 1.20 (OpenGL 2.1).
    GLSL130 = 130,                  //!< GLSL 1.30 (OpenGL 3.0).
    GLSL140 = 140,                  //!< GLSL 1.40 (OpenGL 3.1).
    GLSL150 = 150,                  //!< GLSL 1.50 (OpenGL 3.2).
    GLSL330 = 330,                  //!< GLSL 3.30 (OpenGL 3.3).
    GLSL400 = 400,                  //!< GLSL 4.00 (OpenGL 4.0).
    GLSL410 = 410,                  //!< GLSL 4.10 (OpenGL 4.1).
    GLSL420 = 420,                  //!< GLSL 4.20 (OpenGL 4.2).
    GLSL430 = 430,                  //!< GLSL 4.30 (OpenGL 4.3).
    GLSL440 = 440,                  //!< GLSL 4.40 (OpenGL 4.4).
    GLSL450 = 450,                  //!< GLSL 4.50 (OpenGL 4.5).
    GLSL    = 0x0000ffff,           //!< Auto-detect minimal required GLSL version (for OpenGL 2+).

    ESSL100 = (0x00010000 + 100),   //!< ESSL 1.00 (OpenGL ES 2.0). \note Currently not supported!
    ESSL300 = (0x00010000 + 300),   //!< ESSL 3.00 (OpenGL ES 3.0). \note Currently not supported!
    ESSL310 = (0x00010000 + 310),   //!< ESSL 3.10 (OpenGL ES 3.1). \note Currently not supported!
    ESSL320 = (0x00010000 + 320),   //!< ESSL 3.20 (OpenGL ES 3.2). \note Currently not supported!
    ESSL    = 0x0001ffff,           //!< Auto-detect minimum required ESSL version (for OpenGL ES 2+). \note Currently not supported!

    VKSL450 = (0x00020000 + 450),   //!< VKSL 4.50 (Vulkan 1.0).
    VKSL    = 0x0002ffff,           //!< Auto-detect minimum required VKSL version (for Vulkan/SPIR-V).

    HLSL5   = (0x00030000 + 500),   //!< HLSL Shader Model 5.0 (DirectX 11). Used for HLSL-to-HLSL "round-trip" output.
    HLSL    = 0x0003ffff,           //!< Auto-detect HLSL output version. Currently aliased to HLSL5.
};


//! Returns the specified output shader version as string (e.g. "GLSL 4.50").
std::string ToString(const OutputShaderVersion shaderVersion);

//! Returns true if the output shader version specifies GLSL (for OpenGL 2+).
bool IsLanguageGLSL(const OutputShaderVersion shaderVersion);

//! Returns true if the output shader version specifies ESSL (for OpenGL ES 2+).
bool IsLanguageESSL(const OutputShaderVersion shaderVersion);

//! Returns true if the output shader version specifies VKSL (for Vulkan).
bool IsLanguageVKSL(const OutputShaderVersion shaderVersion);

//! Returns true if the output shader version specifies HLSL (DirectX).
bool IsLanguageHLSL(const OutputShaderVersion shaderVersion);


} // /namespace Xsc


#endif



// ================================================================================
