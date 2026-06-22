/*
 * Backend.h
 */

#ifndef XSC_BACKEND_H
#define XSC_BACKEND_H


#include "Export.h"
#include <string>


namespace Xsc
{


/* ===== Runtime-loadable output backends ===== */

/*
Output backends compiled into this build (e.g. GLSL) are always available. Additional,
optional backends can be shipped as separate DLLs and loaded at runtime with LoadBackend()
below; once loaded they register themselves and their target language becomes selectable
through ShaderOutput::targetLanguage exactly like a built-in one.
*/

/*
Backend ABI contract version. A loadable backend DLL is built against the same internal
headers as the core (Generator/Visitor/AST), so the two must agree on layout.

BUMP this on ANY change to that internal layout, OR on an MSVC toolset / CRT change -- the
whole mechanism assumes both modules share the same toolset and the same dynamic CRT (/MD),
and this integer is the only thing that can catch a mismatch at load time, so it must hard-fail
the load when it differs.
*/
constexpr int BackendAbiVersion = 1;

/*
Loads an output-backend DLL from the given path and lets it self-register. Returns true on
success; on failure returns false and, if 'error' is non-null, sets it to a diagnostic.

A successfully loaded backend is PINNED for the remainder of the process -- it is never unloaded, 
because the registered generator factory points into the DLL's code.
*/
XSC_EXPORT bool LoadBackend(const std::string& dllPath, std::string* error = nullptr);

/*
Loads every backend DLL in 'dir' that matches the platform's backend-DLL naming convention
("xsc_backend_*.dll" / "libxsc_backend_*.so" / "libxsc_backend_*.dylib"). Returns the number
successfully loaded. A non-existent directory is "0 loaded", not an error; 'error' (if non-null)
is only set on a catastrophic failure.
*/
XSC_EXPORT int LoadBackendsInDirectory(const std::string& dir, std::string* error = nullptr);


} // /namespace Xsc


/*
Backend-author helper: stamps the two exported C entry points the loader looks up
(XscBackendAbiVersion and XscRegisterBackend). Use it ONCE in a backend's translation unit,
passing the backend's registrar function:

    XSC_DEFINE_BACKEND(Xsc::RegisterBackend_MyBackend)

Single source of truth for the ABI glue and the exact symbol names, so backends can't drift.
It expands to NOTHING outside the shared-library build: a baked-in (static) backend is reached
through the CMake-generated RegisterBackends() instead, and emitting these exports there would
collide when more than one backend is compiled into the core.
*/
#if defined(_MSC_VER) && defined(XSC_SHARED_LIB)
#   define XSC_DEFINE_BACKEND(registrarFn)                                  \
        extern "C" __declspec(dllexport) int XscBackendAbiVersion()         \
        {                                                                   \
            return ::Xsc::BackendAbiVersion;                                \
        }                                                                   \
        extern "C" __declspec(dllexport) void XscRegisterBackend()          \
        {                                                                   \
            registrarFn();                                                  \
        }
#else
#   define XSC_DEFINE_BACKEND(registrarFn)
#endif


#endif



// ================================================================================
