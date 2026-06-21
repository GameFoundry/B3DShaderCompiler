/*
 * TargetsC.h
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_TARGETS_C_H
#define XSC_TARGETS_C_H


#include <Xsc/Export.h>
#include <stdbool.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


//! Shader target enumeration.
enum XscShaderTarget
{
    XscETargetUndefined,                    //!< Undefined shader target.

    XscETargetVertexShader,                 //!< Vertex shader.
    XscETargetTessellationControlShader,    //!< Tessellation-control (also Hull-) shader.
    XscETargetTessellationEvaluationShader, //!< Tessellation-evaluation (also Domain-) shader.
    XscETargetGeometryShader,               //!< Geometry shader.
    XscETargetFragmentShader,               //!< Fragment (also Pixel-) shader.
    XscETargetComputeShader,                //!< Compute shader.
};

//! Output shader version enumeration.
enum XscInputShaderVersion
{
    XscEInputCg     = 2,            //!< Cg (C for graphics) is a slightly extended HLSL3.

    XscEInputHLSL3  = 3,            //!< HLSL Shader Model 3.0 (DirectX 9).
    XscEInputHLSL4  = 4,            //!< HLSL Shader Model 4.0 (DirectX 10).
    XscEInputHLSL5  = 5,            //!< HLSL Shader Model 5.0 (DirectX 11).
    XscEInputHLSL6  = 6,            //!< HLSL Shader Model 6.0 (DirectX 12).

    XscEInputGLSL   = 0x0000ffff,   //!< GLSL (OpenGL).
    XscEInputESSL   = 0x0001ffff,   //!< GLSL (OpenGL ES).
    XscEInputVKSL   = 0x0002ffff,   //!< GLSL (Vulkan).
};

/*
Built-in output target-language identifiers. The output language is selected with
a string (the XscShaderOutput.targetLanguage field) instead of an enumeration, so
optional/proprietary backends can be added without the core naming them. Use
XscIsTargetLanguageSupported() to discover whether a given optional backend is
available in this build.
*/
#define XscTargetGLSL110    "GLSL110"
#define XscTargetGLSL120    "GLSL120"
#define XscTargetGLSL130    "GLSL130"
#define XscTargetGLSL140    "GLSL140"
#define XscTargetGLSL150    "GLSL150"
#define XscTargetGLSL330    "GLSL330"
#define XscTargetGLSL400    "GLSL400"
#define XscTargetGLSL410    "GLSL410"
#define XscTargetGLSL420    "GLSL420"
#define XscTargetGLSL430    "GLSL430"
#define XscTargetGLSL440    "GLSL440"
#define XscTargetGLSL450    "GLSL450"
#define XscTargetGLSL       "GLSL"

#define XscTargetESSL100    "ESSL100"
#define XscTargetESSL300    "ESSL300"
#define XscTargetESSL310    "ESSL310"
#define XscTargetESSL320    "ESSL320"
#define XscTargetESSL       "ESSL"

#define XscTargetVKSL450    "VKSL450"
#define XscTargetVKSL       "VKSL"

#define XscTargetHLSL5      "HLSL5"
#define XscTargetHLSL       "HLSL"


//! Returns the specified shader target as string.
XSC_EXPORT void XscShaderTargetToString(const enum XscShaderTarget target, char* str, size_t maxSize);

//! Returns the specified shader input version as string.
XSC_EXPORT void XscInputShaderVersionToString(const enum XscInputShaderVersion shaderVersion, char* str, size_t maxSize);

//! Returns true if the shader input version specifies HLSL (for DirectX).
XSC_EXPORT bool XscIsInputLanguageHLSL(const enum XscInputShaderVersion shaderVersion);

//! Returns true if the shader input version specifies GLSL (for OpenGL, OpenGL ES, and Vulkan).
XSC_EXPORT bool XscIsInputLanguageGLSL(const enum XscInputShaderVersion shaderVersion);

//! Returns true if the specified output target language is supported by this build (i.e. a backend is registered for it).
XSC_EXPORT bool XscIsTargetLanguageSupported(const char* targetLanguage);

//! Returns the number of output target languages supported by this build.
XSC_EXPORT size_t XscGetSupportedTargetLanguageCount(void);

//! Writes the output target language identifier at the specified index (0 .. count-1) to the given buffer.
XSC_EXPORT void XscGetSupportedTargetLanguage(size_t index, char* targetLanguage, size_t maxSize);

/**
\brief Returns the enumeration of all supported GLSL extensions as a map of extension name and version number.
\param[in] iterator Specifies the iterator. This must be NULL, to get the first element, or the value previously returned by this function.
\param[out] extension Specifies the output string of the extension name.
\param[in] maxSize Specifies the maximal size of the extension name string including the null terminator.
\param[out] version Specifies the output extension version.
\remarks Here is a usage example:
\code
char extension[256];
int version;

// Get first extension
void* iterator = XscGetGLSLExtensionEnumeration(NULL, extension, 256, &version);

while (iterator != NULL)
{
    // Print extension name and version
    printf("%s ( %d )\n", extension, version);
    
    // Get next extension
    iterator = XscGetGLSLExtensionEnumeration(iterator, extension, 256, &version);
}
\endcode
\note This can NOT be used in a multi-threaded environment!
*/
XSC_EXPORT void* XscGetGLSLExtensionEnumeration(void* iterator, char* extension, size_t maxSize, int* version);


#ifdef __cplusplus
} // /extern "C"
#endif


#endif



// ================================================================================
