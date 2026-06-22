/*
 * Export.h
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_EXPORT_H
#define XSC_EXPORT_H


/*
Symbol visibility for the shared-library (DLL) build.

Two groups carry the macros:
  - The PUBLIC API in inc/Xsc
  - The INTERNAL emitter surface a separately-built backend DLL derives from

XSC_EXPORT covers code; XSC_DATA covers data (the TargetLanguage string constants). They are
identical and exist as two names only to document intent at the declaration. XSC_BUILDING_CORE
is defined ONLY on the xsc_core target (target-private), never globally -- that is what
distinguishes "building the core DLL" from "consuming it".
*/
#if defined(_MSC_VER) && defined(XSC_SHARED_LIB)
#   if defined(XSC_BUILDING_CORE)
#       define XSC_EXPORT __declspec(dllexport)
#       define XSC_DATA   __declspec(dllexport)
#   else
#       define XSC_EXPORT __declspec(dllimport)
#       define XSC_DATA   __declspec(dllimport)
#   endif
#else
#   define XSC_EXPORT
#   define XSC_DATA
#endif


#endif



// ================================================================================