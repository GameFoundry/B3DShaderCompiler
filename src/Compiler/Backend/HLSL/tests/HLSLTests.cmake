# HLSLTests.cmake
#
# Self-contained CTest registration for the HLSL round-trip suite. Included
# from the root CMakeLists.txt when XSC_BUILD_HLSL=ON and
# XSC_BUILD_HLSL_ROUNDTRIP_TESTS=ON. Locates fxc and dxc, registers per-case
# tests that round-trip shaders through the matching HLSL target, and compiles
# the result with the vendor compiler.

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

find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
        "C:/Program Files (x86)/Windows Kits/10/bin/x64"
)

if(NOT FXC_EXECUTABLE AND NOT DXC_EXECUTABLE)
    message(WARNING "Neither fxc.exe nor dxc.exe was found; HLSL round-trip tests will not be registered.")
    return()
endif()

if(NOT TARGET xsc)
    message(FATAL_ERROR "XSC_BUILD_HLSL_ROUNDTRIP_TESTS requires XSC_BUILD_SHELL=ON.")
endif()

if(FXC_EXECUTABLE)
    message(STATUS "HLSL 5 round-trip tests: using fxc at ${FXC_EXECUTABLE}")
endif()
if(DXC_EXECUTABLE)
    message(STATUS "HLSL 6 round-trip tests: using dxc at ${DXC_EXECUTABLE}")
endif()
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
if(FXC_EXECUTABLE)
    set(XSC_HLSL_ROUNDTRIP_CASES
        "ArrayTest3|main|vs_5_0|vert"
        "ExprTest3|VS|vs_5_0|vert"
        "ExprTest4|VS|vs_5_0|vert"
        "ExprTest5|VS|vs_5_0|vert"
        "FloatTest2|VS|vs_5_0|vert"
        "FormattingTest1|VS|vs_5_0|vert"
        "FuncOverloadTest1|PS|ps_5_0|frag"
        "FunctionCallTest1|VS|vs_5_0|vert"
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
endif()

if(DXC_EXECUTABLE)
    xsc_add_roundtrip_tests(
        PREFIX         hlsl6_roundtrip
        DRIVER         ${_HLSL_DRIVER}
        SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
        OUT_DIR        ${_HLSL_OUT_DIR}
        LABELS         "hlsl-roundtrip;bindless"
        DEFINES        -DDXC=${DXC_EXECUTABLE}
        PROFILE_DEFINE FXC_PROFILE
        EXTRA_FLAGS    -Xbindless@ON
        CASES
            "BindlessResources|PS|ps_6_6|frag"
            "BindlessResourceClasses|CS|cs_6_6|comp"
            "BindlessBindingPlanner|PS|ps_6_6|frag"
    )
else()
    message(STATUS "dxc not found; HLSL 6 bindless round-trip tests will not be registered.")
endif()

if(NOT FXC_EXECUTABLE)
    return()
endif()

# HLSL 2021-style templates are specialized by the front end, so the HLSL5
# round-trip remains valid for FXC as well as DX12 consumers.
xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;hlsl-templates"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -Xhlsl-templates@ON
    CASES          "TemplateTest1|VS|vs_5_0|vert"
)

# --- Auto-binding case ---
# Driven with -AB: unregistered resources must receive sequential per-space
# slots, an explicit slot must be reserved against the counter, and the
# DX9-style register(c0, space1) cbuffer spelling must be normalized to a
# b-register binding. ps_5_1 because the normalized binding keeps its space.
set(XSC_HLSL_AUTOBIND_CASES
    "AutoBindTest1|PS|ps_5_1|frag"
)

xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;auto-binding"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -AB
    CASES          ${XSC_HLSL_AUTOBIND_CASES}
)

# Explicit declarations may follow resources that need automatic bindings. The
# planner must reserve the later slots before assigning the earlier resources.
xsc_add_roundtrip_tests(
    PREFIX         hlsl_binding_planner
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;auto-binding;binding-planner"
    DEFINES        -DFXC=${FXC_EXECUTABLE} "-DEXPECT_REGEX=autoTexture[^\r\n]*register\\(t3\\)"
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -AB
    CASES          "BindingPlanner|PS|ps_5_0|frag"
)

# Push constants use a reserved SM5.1 register space and explicit packoffset
# annotations so HLSL bytecode reflection retains both identity and layout.
xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;push-constants"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -Xall@-push-constant-register@3@-push-constant-space@7
    CASES          "PushConstantLayoutTest|main|vs_5_1|vert"
)

xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;push-constants"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -Xall@--max-push-constant-buffer-size@4
    CASES          "PushConstantScalarRange|main|vs_5_1|vert"
)

xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;push-constants;aggregates"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -Xall@--max-push-constant-buffer-size@64
    CASES          "PushConstantAggregateLayout|main|vs_5_1|vert"
)

xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;push-constants;auto-binding"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -Xall@-AB@-push-constant-register@0@-push-constant-space@0
    CASES          "PushConstantAutoBinding|main|vs_5_1|vert"
)

add_test(
    NAME hlsl_reject.PushConstantBindingCollision
    COMMAND ${CMAKE_COMMAND}
        -DXSC=${XSC_BIN}
        -DSHADER=${PROJECT_SOURCE_DIR}/test/PushConstantBindingCollision.hlsl
        -DENTRY=main
        -DXSC_STAGE=vert
        -DOUT_DIR=${_HLSL_OUT_DIR}
        -DXSC_VOUT=HLSL5
        "-DEXPECT_REGEX=push-constant HLSL binding b3, space7 conflicts"
        "-DXSC_EXTRA_FLAGS=-Xall;-push-constant-register;3;-push-constant-space;7"
        -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests/RunXscExpectError.cmake
)
set_tests_properties(hlsl_reject.PushConstantBindingCollision
    PROPERTIES LABELS "hlsl-roundtrip;push-constants;negative")

# --- Opaque-struct pass-through cases ---
# HLSL natively supports opaque types inside structs (the FXAA bundle pattern),
# so the backend emits the struct unchanged; fxc must still accept the result.
# These require the OpaqueStructTypes language extension to be enabled in xsc.
set(XSC_HLSL_OPAQUE_CASES
    "ScopeTest1|VS|vs_5_0|vert"              # local SamplerState declarations require the opaque extension
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
    "OpaqueStructTest10|main|ps_5_0|frag"    # function returns opaque-bearing struct by value
    "OpaqueStructTest11|main|ps_5_0|frag"    # pure 'out' parameter of opaque-bearing struct type
    "OpaqueStructTest12|main|ps_5_0|frag"    # 'inout' parameter rebinds a field inside the callee
    "OpaqueStructNested7|main|ps_5_0|frag"   # nested struct returned by value; fields from a parameter
)

list(APPEND XSC_HLSL_OPAQUE_CASES
    "OpaqueTypeLocalValues|main|ps_5_0|frag"
    "OpaqueTypeFixedArrays|main|ps_5_0|frag"
    "OpaqueTypeAggregateArrays|main|ps_5_0|frag"
    "OpaqueTypeNestedArrayAxes|main|ps_5_0|frag"
    "OpaqueTypeReturnsAndOut|main|ps_5_0|frag"
    "OpaqueTypeArrayContracts|main|ps_5_0|frag"
    "OpaqueTypeDirectCallField|main|ps_5_0|frag"
    # FXC itself rejects runtime sampler-array indexing and opaque-struct
    # ternaries, so those two GLSL-lowering fixtures are intentionally not
    # part of the HLSL backend round-trip list.
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
