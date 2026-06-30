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

# Shared round-trip registration helper (xsc_add_roundtrip_tests).
include("${PROJECT_SOURCE_DIR}/src/Compiler/Backend/XscRoundtripTests.cmake")

set(XSC_BIN "$<TARGET_FILE:xsc>")
set(_HLSL_DRIVER  "${PROJECT_SOURCE_DIR}/src/Compiler/Backend/HLSL/tests/RunHLSLRoundtrip.cmake")
set(_HLSL_OUT_DIR "${CMAKE_BINARY_DIR}/hlsl_roundtrip")

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

xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    CASES          ${XSC_HLSL_ROUNDTRIP_CASES}
)

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
    "OpaqueStructTest8|main|ps_5_0|frag"     # field reassigned inside a callee, then forwarded to a deeper call
    "OpaqueStructTest9|main|ps_5_0|frag"     # if/else both arms rebind to the same global -> join stays resolved
    "OpaqueStructNested1|main|ps_5_0|frag"   # nested bundle, field-assignment init
    "OpaqueStructNested2|main|ps_5_0|frag"   # nested bundle w/ inner POD, field-assignment (inner POD survives stripping)
    "OpaqueStructNested3|main|ps_5_0|frag"   # pass nested sub-struct to a function
    "OpaqueStructNested4|main|ps_5_0|frag"   # nested fully-opaque bundle, copy-init propagates dotted alias map
    "OpaqueStructNested5|main|ps_5_0|frag"   # sub-struct as copy source (TexBundle b = m.albedo)
    "OpaqueStructNested6|main|ps_5_0|frag"   # sub-struct as copy destination (m.albedo = src)
)

xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;opaque-struct"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -Xopaque-struct@ON
    CASES          ${XSC_HLSL_OPAQUE_CASES}
)

# --- Strict-HLSL negative-compile fixtures ---
# Each fixture has two CTest entries:
#   *.no_flag    — xsc must succeed through the HLSL round-trip (analyzer
#                  accepts the fxc-permissive construct without the extension).
#   *.with_flag  — xsc must reject with a specific diagnostic. We reuse the
#                  GLSL negative-test driver (RunXscExpectError) which asserts
#                  on the diagnostic text rather than xsc's exit code (xsc
#                  returns 0 even on a reported error — see the driver comment).
# Pipe-delimited: shader|entry|fxc_profile|xsc_stage|expected_diagnostic_regex
set(XSC_STRICT_HLSL_CASES
    "StrictHlslMulRejectTest|VS|vs_5_0|vert|strict-hlsl forbids implicit promotion in mul"
    "StrictHlslPreciseRejectTest|VS|vs_5_0|vert|strict-hlsl forbids the 'precise' type modifier"
)

foreach(case IN LISTS XSC_STRICT_HLSL_CASES)
    string(REPLACE "|" ";" _parts "${case}")
    list(GET _parts 0 _shader)
    list(GET _parts 1 _entry)
    list(GET _parts 2 _profile)
    list(GET _parts 3 _stage)
    list(GET _parts 4 _expect)

    # Positive (no flag) — must still compile through the HLSL round-trip.
    add_test(
        NAME    hlsl_roundtrip.${_shader}.${_entry}.no_flag
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
    set_tests_properties(hlsl_roundtrip.${_shader}.${_entry}.no_flag
        PROPERTIES LABELS "hlsl-roundtrip;strict-hlsl")

    # Negative (with flag) — xsc analyzer must emit the expected diagnostic.
    # Reuse the GLSL backend's negative-test driver: it asserts on output text
    # regardless of exit code, which is the right signal here since xsc returns
    # 0 even on analyzer errors.
    add_test(
        NAME    hlsl_roundtrip.${_shader}.${_entry}.with_flag
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/${_shader}.hlsl
            -DENTRY=${_entry}
            -DXSC_STAGE=${_stage}
            -DOUT_DIR=${CMAKE_BINARY_DIR}/hlsl_roundtrip
            -DEXPECT_REGEX=${_expect}
            "-DXSC_EXTRA_FLAGS=-Xstrict-hlsl"
            -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests/RunXscExpectError.cmake
    )
    set_tests_properties(hlsl_roundtrip.${_shader}.${_entry}.with_flag
        PROPERTIES LABELS "strict-hlsl;negative")
endforeach()
