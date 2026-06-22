/*
 * ExportC.h
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_EXPORT_C_H
#define XSC_EXPORT_C_H


/*
Symbol visibility for the C wrapper DLL (xsc_core_c).

This is intentionally SEPARATE from <Xsc/Export.h>'s XSC_EXPORT: the C functions declared in
the XscC headers are exported by the C WRAPPER's own DLL, not by xsc_core. Reusing XSC_EXPORT
here would make them dllimport when building the wrapper (since XSC_BUILDING_CORE is not set
for the wrapper), which is wrong. XSCC_BUILDING_WRAPPER is defined target-private on
xsc_core_c.
*/
#if defined(_MSC_VER) && defined(XSC_SHARED_LIB)
#   if defined(XSCC_BUILDING_WRAPPER)
#       define XSCC_EXPORT __declspec(dllexport)
#   else
#       define XSCC_EXPORT __declspec(dllimport)
#   endif
#else
#   define XSCC_EXPORT
#endif


#endif



// ================================================================================
