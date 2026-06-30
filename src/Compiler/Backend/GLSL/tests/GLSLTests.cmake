# GLSLTests.cmake
#
# Self-contained CTest registration for the GLSL/SPIR-V round-trip suite.
# Included from the root CMakeLists.txt when XSC_BUILD_GLSL_ROUNDTRIP_TESTS=ON.
# Locates glslangValidator (Vulkan SDK) and registers per-case tests that run
# `xsc -Vout VKSL450` on shaders from test/ and validate the result all the way
# to SPIR-V with glslangValidator -V.
#
# Two kinds of cases:
#   * round-trip (positive): xsc must succeed and the output must pass SPIR-V
#     validation (RunGLSLRoundtrip.cmake).
#   * expect-error (negative): xsc must REJECT the shader with a specific
#     diagnostic (RunXscExpectError.cmake).

find_program(GLSLANG_VALIDATOR_EXECUTABLE
    NAMES glslangValidator
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/Bin32"
)

if(NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(STATUS "glslangValidator not found; GLSL round-trip tests will not be registered.")
    return()
endif()

if(NOT TARGET xsc)
    message(FATAL_ERROR "XSC_BUILD_GLSL_ROUNDTRIP_TESTS requires XSC_BUILD_SHELL=ON.")
endif()

message(STATUS "GLSL round-trip tests: using glslangValidator at ${GLSLANG_VALIDATOR_EXECUTABLE}")
enable_testing()

# Shared round-trip registration helper (xsc_add_roundtrip_tests).
include("${PROJECT_SOURCE_DIR}/src/Compiler/Backend/XscRoundtripTests.cmake")

set(XSC_BIN "$<TARGET_FILE:xsc>")
set(_GLSL_TESTS_DIR "${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests")
set(_GLSL_OUT_DIR   "${CMAKE_BINARY_DIR}/glsl_roundtrip")

# --- Positive round-trip cases ---------------------------------------------
# Pipe-delimited: shader|entry|stage. All opaque-struct cases compile with the
# OpaqueStructTypes language extension enabled.
set(XSC_GLSL_ROUNDTRIP_CASES
    "OpaqueStructTest1|main|frag"   # struct passed to function (assignment form)
    "OpaqueStructTest2|main|frag"   # aggregate-initializer form
    "OpaqueStructTest3|main|frag"   # mixed POD + opaque members
    "OpaqueStructTest4|main|frag"   # two opaque-bearing struct params
    "OpaqueStructTest5|main|frag"   # chained calls passing the struct through
    "OpaqueStructTest6|main|frag"   # straight-line reassignment (via helper)
    "OpaqueStructTest7|main|frag"   # straight-line reassignment (inline access)
    "OpaqueStructTest8|main|frag"   # field reassigned inside a callee, then forwarded to a deeper call
    "OpaqueStructTest9|main|frag"   # if/else both arms rebind to the same global -> join stays resolved
    "OpaqueStructNested1|main|frag" # nested bundle, field-assignment init
    "OpaqueStructNested2|main|frag" # nested bundle w/ inner POD, field-assignment (inner POD survives stripping)
    "OpaqueStructNested3|main|frag" # pass nested sub-struct to a function
    "OpaqueStructNested4|main|frag" # nested fully-opaque bundle, copy-init propagates dotted alias map
    "OpaqueStructNested5|main|frag" # sub-struct as copy source (TexBundle b = m.albedo)
    "OpaqueStructNested6|main|frag" # sub-struct as copy destination (m.albedo = src)
)

# No PROFILE_DEFINE: glslangValidator infers the stage from the file extension,
# so these rows carry no profile field (shader|entry|stage).
xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;opaque-struct"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    EXTRA_FLAGS -Xopaque-struct@ON
    CASES       ${XSC_GLSL_ROUNDTRIP_CASES}
)

# --- Negative (expect-error) cases -----------------------------------------
# Each registers a shader that must be rejected, pinned to its diagnostic.
# Helper: add_expect_error(<name> <shader> <entry> <stage> <regex> <extra flags...>)
function(add_expect_error _name _shader _entry _stage _regex)
    add_test(
        NAME    glsl_reject.${_name}
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/${_shader}.hlsl
            -DENTRY=${_entry}
            -DXSC_STAGE=${_stage}
            -DOUT_DIR=${_GLSL_OUT_DIR}
            "-DEXPECT_REGEX=${_regex}"
            ${ARGN}
            -P ${_GLSL_TESTS_DIR}/RunXscExpectError.cmake
    )
    set_tests_properties(glsl_reject.${_name} PROPERTIES LABELS "glsl-roundtrip;opaque-struct;negative")
endfunction()

# With the extension enabled, these specific unsupported patterns are rejected.
add_expect_error(Global       OpaqueStructRejectGlobal       main frag "cannot be declared as globals"        "-DXSC_EXTRA_FLAGS=-Xopaque-struct;ON")
add_expect_error(Entry        OpaqueStructRejectEntry        main frag "entry-point parameters or return types" "-DXSC_EXTRA_FLAGS=-Xopaque-struct;ON")
add_expect_error(CBuffer      OpaqueStructRejectCBuffer      main frag "constant-buffer members"               "-DXSC_EXTRA_FLAGS=-Xopaque-struct;ON")
add_expect_error(CondReassign OpaqueStructRejectCondReassign main frag "cannot be resolved to a single global"  "-DXSC_EXTRA_FLAGS=-Xopaque-struct;ON")
add_expect_error(LoopReassign OpaqueStructRejectLoopReassign main frag "cannot be resolved to a single global"  "-DXSC_EXTRA_FLAGS=-Xopaque-struct;ON")
# Without the extension, the struct is rejected outright (no extra flags).
add_expect_error(ExtDisabled  OpaqueStructRejectExtDisabled  main frag "opaque-struct' language extension is enabled")
