/*
 * BackendLoader.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 *
 * Runtime loading of optional output-backend DLLs. This is the ONLY place the core touches
 * the OS dynamic loader, and it names no backend -- it merely loads the module it is given,
 * validates the backend ABI version, and invokes the backend's self-registration entry point.
 */

#include <Xsc/Backend.h>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <Windows.h>
#else
#   include <dlfcn.h>
#   include <dirent.h>
#endif

#include "BackendRegistry.h"


namespace Xsc
{


/* ----- Entry-point signatures (must match XSC_DEFINE_BACKEND in <Xsc/Backend.h>) ----- */

using BackendAbiVersionProc = int  (*)();
using BackendRegisterProc   = void (*)();

static const char* g_abiVersionSymbol = "XscBackendAbiVersion";
static const char* g_registerSymbol   = "XscRegisterBackend";

static void SetError(std::string* error, const std::string& msg)
{
    if (error)
        *error = msg;
}


#if defined(_WIN32)

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty())
        return std::wstring();
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0)
        return std::wstring(); /* malformed UTF-8 -- caller's load fails cleanly downstream */
    std::wstring result(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &result[0], len);
    return result;
}

static std::string WideToUtf8(const std::wstring& s)
{
    if (s.empty())
        return std::string();
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return std::string();
    std::string result(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &result[0], len, nullptr, nullptr);
    return result;
}

bool LoadBackend(const std::string& dllPath, std::string* error)
{
    HMODULE module = LoadLibraryW(Utf8ToWide(dllPath).c_str());
    if (!module)
    {
        SetError(error, "failed to load backend library '" + dllPath + "'");
        return false;
    }

    auto abiVersionProc = reinterpret_cast<BackendAbiVersionProc>(GetProcAddress(module, g_abiVersionSymbol));
    auto registerProc   = reinterpret_cast<BackendRegisterProc  >(GetProcAddress(module, g_registerSymbol  ));

    if (!abiVersionProc || !registerProc)
    {
        SetError(error, "'" + dllPath + "' is not a valid XShaderCompiler backend (missing entry points)");
        FreeLibrary(module);
        return false;
    }

    if (abiVersionProc() != BackendAbiVersion)
    {
        SetError(error, "backend '" + dllPath + "' was built against an incompatible backend ABI version");
        FreeLibrary(module);
        return false;
    }

    /* Make sure the backends baked into this build are registered first, so a runtime backend
       that overrides a built-in of the same target language deterministically wins regardless
       of whether the registry has been read yet. */
    BackendRegistry::Instance().EnsureBuiltinsRegistered();

    /* Register the backend. The module is intentionally left loaded (pinned for process
       lifetime) because the registered generator factory points into its code. */
    registerProc();
    return true;
}

int LoadBackendsInDirectory(const std::string& dir, std::string* error)
{
    const std::string pattern = dir + "\\xsc_backend_*.dll";

    WIN32_FIND_DATAW findData;
    HANDLE handle = FindFirstFileW(Utf8ToWide(pattern).c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE)
        return 0; /* nothing matched / directory absent -- not an error */

    int count = 0;
    std::string failures;
    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            const std::string name = WideToUtf8(findData.cFileName);
            std::string loadError;
            if (LoadBackend(dir + "\\" + name, &loadError))
                ++count;
            else
            {
                if (!failures.empty())
                    failures += "; ";
                failures += loadError;
            }
        }
    }
    while (FindNextFileW(handle, &findData));

    FindClose(handle);

    /* Best-effort: one bad DLL does not fail the whole scan, but each rejected module's reason
       is reported so the caller can surface what was skipped. */
    if (!failures.empty())
        SetError(error, failures);

    return count;
}

#else // POSIX

bool LoadBackend(const std::string& dllPath, std::string* error)
{
    void* module = dlopen(dllPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module)
    {
        SetError(error, "failed to load backend library '" + dllPath + "'");
        return false;
    }

    auto abiVersionProc = reinterpret_cast<BackendAbiVersionProc>(dlsym(module, g_abiVersionSymbol));
    auto registerProc   = reinterpret_cast<BackendRegisterProc  >(dlsym(module, g_registerSymbol  ));

    if (!abiVersionProc || !registerProc)
    {
        SetError(error, "'" + dllPath + "' is not a valid XShaderCompiler backend (missing entry points)");
        dlclose(module);
        return false;
    }

    if (abiVersionProc() != BackendAbiVersion)
    {
        SetError(error, "backend '" + dllPath + "' was built against an incompatible backend ABI version");
        dlclose(module);
        return false;
    }

    /* See Windows branch: register the baked-in backends first so a loaded override wins. */
    BackendRegistry::Instance().EnsureBuiltinsRegistered();

    /* Pinned for process lifetime (see Windows branch). */
    registerProc();
    return true;
}

int LoadBackendsInDirectory(const std::string& dir, std::string* error)
{
    DIR* handle = opendir(dir.c_str());
    if (!handle)
        return 0; /* directory absent -- not an error */

    const std::string prefix = "libxsc_backend_";

    int count = 0;
    std::string failures;
    while (dirent* entry = readdir(handle))
    {
        const std::string name = entry->d_name;
        const bool hasPrefix = (name.compare(0, prefix.size(), prefix) == 0);
        const bool hasSuffix =
            (name.size() > 3 && name.compare(name.size() - 3, 3, ".so")    == 0) ||
            (name.size() > 6 && name.compare(name.size() - 6, 6, ".dylib") == 0);

        if (hasPrefix && hasSuffix)
        {
            std::string loadError;
            if (LoadBackend(dir + "/" + name, &loadError))
                ++count;
            else
            {
                if (!failures.empty())
                    failures += "; ";
                failures += loadError;
            }
        }
    }

    closedir(handle);

    /* Best-effort (see Windows branch): report each rejected module's reason. */
    if (!failures.empty())
        SetError(error, failures);

    return count;
}

#endif


} // /namespace Xsc



// ================================================================================
