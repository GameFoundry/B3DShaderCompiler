# HLSLTests.cmake
#
# Self-contained CTest registration for the HLSL round-trip suite. Included
# from the root CMakeLists.txt when XSC_BUILD_HLSL=ON and
# XSC_BUILD_HLSL_ROUNDTRIP_TESTS=ON. Locates fxc, registers per-case tests that
# run `xsc -Vout HLSL5` on shaders from test/ and re-compile the output with
# fxc to verify the result is valid HLSL.

if(NOT WIN32)
    message(WARNING "XSC_BUILD_HLSL_ROUNDTRIP_TESTS requires fxc.exe; only Windows is supported.")
    return()
endif()

find_program(FXC_EXECUTABLE fxc
    HINTS
        "$ENV{WindowsSdkVerBinPath}x64"
        "$ENV{WindowsSdkDir}bin/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.18362.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/x64"
)

if(NOT FXC_EXECUTABLE)
    message(WARNING "fxc.exe not found; HLSL round-trip tests will not be registered.")
    return()
endif()

if(NOT TARGET xsc)
    message(FATAL_ERROR "XSC_BUILD_HLSL_ROUNDTRIP_TESTS requires XSC_BUILD_SHELL=ON.")
endif()

message(STATUS "HLSL round-trip tests: using fxc at ${FXC_EXECUTABLE}")
enable_testing()

set(XSC_BIN "$<TARGET_FILE:xsc>")

# Pipe-delimited (CMake lists use ';' internally): shader|entry|fxc_profile|xsc_stage
# MVP coverage: VS/PS, arithmetic & control flow, intrinsics, function calls,
# semantics, struct outputs. Each entry has been confirmed to compile cleanly
# with fxc both before and after the xsc HLSL round-trip.
set(XSC_HLSL_ROUNDTRIP_CASES
    "ArrayTest3|main|vs_5_0|vert"
    "ExprTest3|VS|vs_5_0|vert"
    "ExprTest4|VS|vs_5_0|vert"
    "ExprTest5|VS|vs_5_0|vert"
    "FloatTest2|VS|vs_5_0|vert"
    "FormattingTest1|VS|vs_5_0|vert"
    "FuncOverloadTest1|PS|ps_5_0|frag"
    "FunctionCallTest1|VS|vs_5_0|vert"
    "ScopeTest1|VS|vs_5_0|vert"
    "SemanticTest3|VS|vs_5_0|vert"
)

foreach(case IN LISTS XSC_HLSL_ROUNDTRIP_CASES)
    string(REPLACE "|" ";" _parts "${case}")
    list(GET _parts 0 _shader)
    list(GET _parts 1 _entry)
    list(GET _parts 2 _profile)
    list(GET _parts 3 _stage)

    add_test(
        NAME    hlsl_roundtrip.${_shader}.${_entry}
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DFXC=${FXC_EXECUTABLE}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/${_shader}.hlsl
            -DENTRY=${_entry}
            -DFXC_PROFILE=${_profile}
            -DXSC_STAGE=${_stage}
            -DOUT_DIR=${CMAKE_BINARY_DIR}/hlsl_roundtrip
            -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/HLSL/tests/RunHLSLRoundtrip.cmake
    )
    set_tests_properties(hlsl_roundtrip.${_shader}.${_entry} PROPERTIES LABELS "hlsl-roundtrip")
endforeach()

# --- Opaque-struct pass-through cases ---
# HLSL natively supports opaque types inside structs (the FXAA bundle pattern),
# so the backend emits the struct unchanged; fxc must still accept the result.
# These require the OpaqueStructTypes language extension to be enabled in xsc.
set(XSC_HLSL_OPAQUE_CASES
    "OpaqueStructTest1|main|ps_5_0|frag"
    "OpaqueStructTest2|main|ps_5_0|frag"
    "OpaqueStructTest3|main|ps_5_0|frag"
    "OpaqueStructTest4|main|ps_5_0|frag"
    "OpaqueStructTest5|main|ps_5_0|frag"
    "OpaqueStructTest6|main|ps_5_0|frag"
    "OpaqueStructTest7|main|ps_5_0|frag"
)

foreach(case IN LISTS XSC_HLSL_OPAQUE_CASES)
    string(REPLACE "|" ";" _parts "${case}")
    list(GET _parts 0 _shader)
    list(GET _parts 1 _entry)
    list(GET _parts 2 _profile)
    list(GET _parts 3 _stage)

    add_test(
        NAME    hlsl_roundtrip.${_shader}.${_entry}
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DFXC=${FXC_EXECUTABLE}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/${_shader}.hlsl
            -DENTRY=${_entry}
            -DFXC_PROFILE=${_profile}
            -DXSC_STAGE=${_stage}
            -DOUT_DIR=${CMAKE_BINARY_DIR}/hlsl_roundtrip
            "-DXSC_EXTRA_FLAGS=-Xopaque-struct;ON"
            -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/HLSL/tests/RunHLSLRoundtrip.cmake
    )
    set_tests_properties(hlsl_roundtrip.${_shader}.${_entry} PROPERTIES LABELS "hlsl-roundtrip;opaque-struct")
endforeach()
