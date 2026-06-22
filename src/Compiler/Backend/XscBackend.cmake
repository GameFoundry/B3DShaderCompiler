# XscBackend.cmake
#
# Shared helper for output backends that can build EITHER baked into xsc_core (static build)
# OR as a separately loadable backend DLL (shared build). Every backend folder calls
# xsc_add_backend(...) instead of hand-writing the dual-mode block, so the boundary details
# (which symbols to export, where the DLL lands, how it installs, the HLSL include path) live
# in exactly one place and the next backend is a few lines with no copy-paste.
#
#   xsc_add_backend(
#       NAME         <name>            (required) lower-case id; the DLL is xsc_backend_<name>
#       SOURCES      <files>           (required) the backend's *.cpp/*.h
#       REGISTRAR    <fn>              (required) the void RegisterBackend_*() free function
#       INCLUDE_DIRS <dirs>            (optional) extra private include dirs
#   )
#
# Shared build: builds MODULE xsc_backend_<name>, linked to xsc_core, output next to xsc.exe /
#   xsc_core.dll, and installed. The backend exposes itself only through its exported C entry
#   points (XSC_DEFINE_BACKEND), so it is NEVER enrolled in XSC_BACKEND_REGISTRARS here.
# Static build: compiles SOURCES into xsc_core and enrolls REGISTRAR with the CMake-generated
#   RegisterBackends() dispatcher (the baked-in path).

include_guard(GLOBAL)

function(xsc_add_backend)
    cmake_parse_arguments(_B "" "NAME;REGISTRAR" "SOURCES;INCLUDE_DIRS" ${ARGN})

    foreach(_req NAME SOURCES REGISTRAR)
        if(NOT _B_${_req})
            message(FATAL_ERROR "xsc_add_backend: missing required ${_req}")
        endif()
    endforeach()

    if(XSC_SHARED_LIB)
        add_library(xsc_backend_${_B_NAME} MODULE ${_B_SOURCES})
        target_link_libraries(xsc_backend_${_B_NAME} PRIVATE xsc_core)
        target_compile_features(xsc_backend_${_B_NAME} PRIVATE cxx_range_for)

        # HLSLGenerator.h lives in Backend/HLSL, which HLSL's CMakeLists adds PRIVATE to
        # xsc_core (so it is not inherited here). Backends deriving from the HLSL emitter need
        # it explicitly. Other core include dirs (inc, Backend, AST, ...) ARE inherited from
        # the root include_directories().
        target_include_directories(xsc_backend_${_B_NAME} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/HLSL
            ${_B_INCLUDE_DIRS})

        if(MSVC)
            # A backend derives from the dllimport'd HLSLGenerator/Generator (classes that hold
            # non-dll-interface STL members), so it sees the same benign C4251/C4275 the core
            # suppresses on its side of the boundary. Silence them here too for a clean build.
            target_compile_options(xsc_backend_${_B_NAME} PRIVATE /wd4251 /wd4275)
        endif()

        # Land next to xsc.exe / xsc_core.dll so the shell, tests, and host can find it.
        set_target_properties(xsc_backend_${_B_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUTPUT_PATH}"
            LIBRARY_OUTPUT_DIRECTORY "${BUILD_OUTPUT_PATH}")

        # A backend installs ONLY through its own dedicated install component. A host can then pull 
        # exactly one backend with
        #     cmake --install <build> --component backend_<folder> --prefix <dest>
        # dropping just this DLL (+PDB) into <dest>
        get_filename_component(_backendFolder ${CMAKE_CURRENT_SOURCE_DIR} NAME)

        install(TARGETS xsc_backend_${_B_NAME}
            RUNTIME DESTINATION .
            LIBRARY DESTINATION .
            COMPONENT backend_${_backendFolder}
            EXCLUDE_FROM_ALL)

        # Ship the backend's debug symbols alongside it (configs that emit a PDB).
        if(MSVC)
            install(FILES $<TARGET_PDB_FILE:xsc_backend_${_B_NAME}>
                DESTINATION .
                CONFIGURATIONS RelWithDebInfo Debug
                COMPONENT backend_${_backendFolder}
                EXCLUDE_FROM_ALL
                OPTIONAL)
        endif()
    else()
        target_sources(xsc_core PRIVATE ${_B_SOURCES})
        target_include_directories(xsc_core PRIVATE ${_B_INCLUDE_DIRS})
        set_property(GLOBAL APPEND PROPERTY XSC_BACKEND_REGISTRARS ${_B_REGISTRAR})
    endif()
endfunction()
