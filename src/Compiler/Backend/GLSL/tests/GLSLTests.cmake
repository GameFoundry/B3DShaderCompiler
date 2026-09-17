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

# Helper: add_expect_error(<name> <shader> <entry> <stage> <regex>
#                          [EXTRA_FLAGS <flags>]
#                          [LABELS <label>...])
# Registers a shader that must be rejected, pinned to its diagnostic.
# EXTRA_FLAGS supports '@' as intra-flag separator (matching xsc_add_roundtrip_tests).
# LABELS defaults to "glsl-roundtrip;opaque-struct;negative" if omitted.
function(add_expect_error _name _shader _entry _stage _regex)
    cmake_parse_arguments(_ERR "" "" "EXTRA_FLAGS;LABELS" ${ARGN})

    set(_extra_flags "")
    if(_ERR_EXTRA_FLAGS)
        string(REPLACE "@" ";" _extra_flags "${_ERR_EXTRA_FLAGS}")
    endif()

    set(_labels "glsl-roundtrip;opaque-struct;negative")
    if(_ERR_LABELS)
        set(_labels "${_ERR_LABELS}")
    endif()

    add_test(
        NAME    glsl_reject.${_name}
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/${_shader}.hlsl
            -DENTRY=${_entry}
            -DXSC_STAGE=${_stage}
            -DOUT_DIR=${_GLSL_OUT_DIR}
            "-DEXPECT_REGEX=${_regex}"
            "-DXSC_EXTRA_FLAGS=${_extra_flags}"
            -P ${_GLSL_TESTS_DIR}/RunXscExpectError.cmake
    )
    set_tests_properties(glsl_reject.${_name} PROPERTIES LABELS "${_labels}")
endfunction()

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
    "OpaqueStructTest10|main|frag"  # function returns opaque-bearing struct; aliases flow to caller local
    "OpaqueStructTest11|main|frag"  # pure 'out' parameter fills the caller's struct
    "OpaqueStructTest12|main|frag"  # 'inout' parameter rebinds a field inside the callee
    "OpaqueStructNested7|main|frag" # nested struct returned by value; fields resolved from a parameter
)

list(APPEND XSC_GLSL_ROUNDTRIP_CASES
    "OpaqueTypeLocalValues|main|frag"
    "OpaqueTypeLocalTexture|main|frag"
    "OpaqueTypeLocalSampler|main|frag"
    "OpaqueTypeLocalBuffer|main|frag"
    "OpaqueTypeFixedArrays|main|frag"
    "OpaqueTypeAggregateArrays|main|frag"
    "OpaqueTypeNestedArrayAxes|main|frag"
    "OpaqueTypeNativeInputs|main|frag"
    "OpaqueTypeArrayReturn|main|frag"
    "OpaqueTypeRuntimeArray|main|frag"
    "OpaqueTypeReturnsAndOut|main|frag"
    "OpaqueTypeArrayContracts|main|frag"
    "OpaqueTypeDirectCallField|main|frag"
    "OpaqueTypeControlFlow|main|frag"
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

xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;bindless"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    EXTRA_FLAGS -Vin@HLSL6@-Xbindless@ON@--bindless-set@2
    CASES
        "BindlessResources|PS|frag"
        "BindlessResourceClasses|CS|comp"
)

xsc_add_roundtrip_tests(
    PREFIX      glsl_binding_planner
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;auto-binding;binding-planner"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE} "-DEXPECT_REGEX=binding = 3[^\r\n]*autoTexture"
    CASES       "BindingPlanner|PS|frag"
)

# Generated bindings are planned even with source auto-binding disabled. Slots
# 0, 2, and 4 in set 2 are explicit source bindings. Starting generated
# allocation at slot 2 therefore places the first descriptor array at slot 3.
xsc_add_roundtrip_tests(
    PREFIX      glsl_binding_planner_no_auto
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;bindless;binding-planner"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE} -DXSC_AUTO_BINDING=OFF "-DEXPECT_REGEX=set = 2, binding = 3[^\r\n]*xsc_bindless_0"
    EXTRA_FLAGS -Vin@HLSL6@-Xbindless@ON@-EB@--bindless-set@2@--bindless-slot@2
    CASES       "BindlessBindingPlanner|PS|frag"
)

add_expect_error(UnsupportedResource BindlessUnsupportedResource CS comp
    "bindless append/consume buffers require an explicit counter handle"
    EXTRA_FLAGS -Vin@HLSL6@-Xbindless@ON
    LABELS "glsl-roundtrip;bindless;negative")
add_expect_error(ExtensionDisabled BindlessExtensionDisabled PS frag
    "ResourceDescriptorHeap' requires the 'bindless' language extension"
    EXTRA_FLAGS -Vin@HLSL6
    LABELS "glsl-roundtrip;bindless;negative")
add_expect_error(KindMismatch BindlessKindMismatch PS frag
    "ResourceDescriptorHeap' cannot provide a resource of type 'SamplerState'"
    EXTRA_FLAGS -Vin@HLSL6@-Xbindless@ON
    LABELS "glsl-roundtrip;bindless;negative")
add_expect_error(MissingContext BindlessMissingContext PS frag
    "ResourceDescriptorHeap' must initialize or be assigned to a resource variable"
    EXTRA_FLAGS -Vin@HLSL6@-Xbindless@ON
    LABELS "glsl-roundtrip;bindless;negative")

xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;hlsl-templates"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    EXTRA_FLAGS -Xhlsl-templates@ON
    CASES       "TemplateTest1|VS|vert"
)

# Intrinsics whose translation depends on the argument's type. The bit-cast case guards against the
# result of 'asint'/'asuint'/'asfloat' being typed as its argument instead of as the intrinsic's own
# type, which turns the surrounding integer operations into invalid float ones (rejected here).
xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;intrinsics"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    CASES       "BitCastIntrinsics|main|frag"
)

xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;wave-intrinsics"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE} -DGLSLANG_TARGET_ENV=vulkan1.1
    EXTRA_FLAGS -Vin@HLSL6
    CASES       "WaveIntrinsics|main|comp"
)

xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;push-constants"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    EXTRA_FLAGS -Xpush-constants
    CASES       "PushConstantLayoutTest|main|vert"
)

xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;push-constants"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    EXTRA_FLAGS -Xpush-constants@--max-push-constant-buffer-size@4
    CASES       "PushConstantScalarRange|main|vert"
)

xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;push-constants;aggregates"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    EXTRA_FLAGS -Xpush-constants@--max-push-constant-buffer-size@64
    CASES       "PushConstantAggregateLayout|main|vert"
)

# Dynamic bindings keep ordinary native buffer declarations and packing.
xsc_add_roundtrip_tests(
    PREFIX      glsl_roundtrip
    DRIVER      ${_GLSL_TESTS_DIR}/RunGLSLRoundtrip.cmake
    SHADER_DIR  ${PROJECT_SOURCE_DIR}/test
    OUT_DIR     ${_GLSL_OUT_DIR}
    LABELS      "glsl-roundtrip;dynamic-offsets"
    DEFINES     -DGLSLANG=${GLSLANG_VALIDATOR_EXECUTABLE}
    EXTRA_FLAGS -Xdynamic-offsets
    CASES       "DynamicOffsets|main|vert"
                "DynamicOffsetsRenamed|main|frag"
                "DynamicOffsetsAutomatic|main|frag"
)

# --- Negative (expect-error) cases -----------------------------------------
add_expect_error(DynamicOffsetExtension DynamicOffsets main vert "requires language extension 'dynamic-offsets'"
    LABELS "glsl-roundtrip;dynamic-offsets;negative")
add_expect_error(DynamicOffsetPushOnly DynamicOffsets main vert "requires language extension 'dynamic-offsets'"
    EXTRA_FLAGS -Xpush-constants
    LABELS "glsl-roundtrip;dynamic-offsets;negative")
add_expect_error(DynamicOffsetLegacyGLSL DynamicOffsetsAutomatic main frag "preserves uniform buffers"
    EXTRA_FLAGS -Xdynamic-offsets@-Vout@GLSL120@--extension@ON
    LABELS "glsl-roundtrip;dynamic-offsets;negative")

set(_DYNAMIC_OFFSET_INVALID_TARGETS
    StructuredBuffer RWStructuredBuffer StructuredBufferArray RWStructuredBufferArray
    ByteAddressBuffer RWByteAddressBuffer Buffer RWBuffer Texture TextureArray Sampler
    Value Function TBuffer CBufferField StructField Parameter Local Statement Struct)
foreach(_target IN LISTS _DYNAMIC_OFFSET_INVALID_TARGETS)
    add_expect_error(DynamicOffset${_target} DynamicOffsetInvalidTarget main frag "only valid on a non-array cbuffer"
        EXTRA_FLAGS -Xdynamic-offsets@-DTEST_${_target}
        LABELS "glsl-roundtrip;dynamic-offsets;negative")
endforeach()

foreach(_arguments Single Multiple)
    add_expect_error(DynamicOffset${_arguments}Argument DynamicOffsetInvalidArguments main frag "takes no arguments"
        EXTRA_FLAGS -Xdynamic-offsets@-DTEST_${_arguments}
        LABELS "glsl-roundtrip;dynamic-offsets;negative")
endforeach()
foreach(_order PushFirst DynamicFirst)
    add_expect_error(DynamicOffsetConflict${_order} DynamicOffsetPushConstantConflict main frag "cannot be combined"
        EXTRA_FLAGS -Xdynamic-offsets@-Xpush-constants@-DTEST_${_order}
        LABELS "glsl-roundtrip;dynamic-offsets;negative")
endforeach()
foreach(_buffer CBufferArray ConstantBuffer ConstantBufferArray)
    add_expect_error(DynamicOffset${_buffer} DynamicOffsetUnsupportedBuffer main frag "syntax error"
        EXTRA_FLAGS -Xdynamic-offsets@-DTEST_${_buffer}
        LABELS "glsl-roundtrip;dynamic-offsets;negative")
endforeach()

# Each registers a shader that must be rejected, pinned to its diagnostic.
# With the extension enabled, these specific unsupported patterns are rejected.
add_expect_error(Global       OpaqueStructRejectGlobal       main frag "cannot be declared as globals"          EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(Entry        OpaqueStructRejectEntry        main frag "entry-point parameters or return types" EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(CBuffer      OpaqueStructRejectCBuffer      main frag "constant-buffer members"                 EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(CondReassign OpaqueStructRejectCondReassign main frag "cannot be resolved to a single global"    EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(LoopReassign OpaqueStructRejectLoopReassign main frag "cannot be resolved to a single global"    EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(ReturnAmbiguous OpaqueStructRejectReturnAmbiguous main frag "cannot be resolved to a single global" EXTRA_FLAGS -Xopaque-struct@ON)
# Without the extension, the struct is rejected outright (no extra flags).
add_expect_error(ExtDisabled  OpaqueStructRejectExtDisabled  main frag "opaque-struct' language extension is enabled")

add_expect_error(UnrelatedRuntime OpaqueTypeRejectUnrelatedRuntime main frag "runtime indexing requires one cohesive" EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(DynamicWrite     OpaqueTypeRejectDynamicWrite     main frag "runtime-indexed writes to opaque values" EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(UnsizedArray     OpaqueTypeRejectUnsizedArray     main frag "require a fixed, positive compile-time extent" EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(DefaultArgument  OpaqueTypeRejectDefaultArgument  main frag "cannot use default arguments" EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(PlainReturn      OpaqueTypeRejectPlainReturn      main frag "cannot return a structure containing opaque resources" EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(PlainOut         OpaqueTypeRejectPlainOut         main frag "no 'out'/'inout'" EXTRA_FLAGS -Xopaque-struct@ON)
add_expect_error(LocalExtDisabled OpaqueTypeLocalTexture           main frag "opaque-struct' language extension is enabled")

add_expect_error(PushConstantExtension PushConstantTest1 main vert "attribute 'pushConstant' requires language extension 'push-constants'"
    LABELS "glsl-roundtrip;push-constants;negative")
add_expect_error(PushConstantType      PushConstantInvalidType main vert "must be a non-array scalar"
    EXTRA_FLAGS -Xpush-constants
    LABELS "glsl-roundtrip;push-constants;negative")
add_expect_error(PushConstantArray     PushConstantInvalidArray main vert "must be a non-array scalar"
    EXTRA_FLAGS -Xpush-constants
    LABELS "glsl-roundtrip;push-constants;negative")
add_expect_error(PushConstantSize      PushConstantInvalidSize main vert "exceeding the configured maximum of 16 bytes"
    EXTRA_FLAGS -Xpush-constants
    LABELS "glsl-roundtrip;push-constants;negative")
add_expect_error(PushConstantDuplicate PushConstantDuplicate   main vert "only one push-constant buffer"
    EXTRA_FLAGS -Xpush-constants
    LABELS "glsl-roundtrip;push-constants;negative")

add_expect_error(TemplateExtensionDisabled TemplateTest1 VS vert "hlsl-templates' language extension"
    LABELS "glsl-roundtrip;hlsl-templates;negative")

# add_generated_glsl_assert(<name> <shader> <expect regex> <reject regex> [<labels>])
function(add_generated_glsl_assert _name _shader _expect _reject)
    set(_labels "glsl-roundtrip;opaque-type;generated")
    if(ARGN)
        set(_labels "${ARGN}")
    endif()

    set(_output "${_GLSL_OUT_DIR}/assert_${_name}.frag")
    add_test(
        NAME glsl_generated.${_name}
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/${_shader}.hlsl
            -DENTRY=main
            -DXSC_STAGE=frag
            -DOUTPUT=${_output}
            "-DEXPECT_REGEX=${_expect}"
            "-DREJECT_REGEX=${_reject}"
            -P ${_GLSL_TESTS_DIR}/RunGeneratedGLSLAssert.cmake
    )
    set_tests_properties(glsl_generated.${_name} PROPERTIES LABELS "${_labels}")
endfunction()

add_generated_glsl_assert(runtime_array OpaqueTypeRuntimeArray
    "texture2D bundles_opaque_array_.*\\[2\\]"
    "struct Bundle")
add_generated_glsl_assert(residual_return OpaqueTypeReturnsAndOut
    "void makeBundle\\(vec4 tint, out Bundle"
    "struct OpaqueOnly")
add_generated_glsl_assert(plain_locals OpaqueTypeLocalValues
    "texelFetch\\(g_data, 0\\)"
    "localTex|localData|localSampler")
add_generated_glsl_assert(no_dummy OpaqueTypeArrayContracts
    "void makeSet\\(out BundleSet"
    "dummy|struct Bundle[ \\t\\r\\n]*\\{")

# 'asint'/'asuint'/'asfloat' reinterpret the bit pattern of their argument, so the GLSL function they
# map to has to be chosen from that argument's type: GLSL only converts between a real and an integral
# type, and reinterpreting between int and uint is a plain constructor cast. Writing the wrong one
# still produces *valid* GLSL -- it just makes GLSL insert an implicit value conversion on the
# argument, silently destroying the bit pattern -- so the round-trip test cannot catch it and these
# assertions pin the emitted function per source type instead.
set(_BITCAST_LABELS "glsl-roundtrip;intrinsics;generated")

add_generated_glsl_assert(bitcast_from_float BitCastIntrinsics
    "int f2i = floatBitsToInt\\(fVal\\.x\\)"
    "[fu]intBitsToFloat\\(fVal" ${_BITCAST_LABELS})
add_generated_glsl_assert(bitcast_from_uint BitCastIntrinsics
    "float u2f = uintBitsToFloat\\(uVal\\.y\\)"
    "floatBitsTo(Int|Uint)\\(uVal" ${_BITCAST_LABELS})
add_generated_glsl_assert(bitcast_from_int BitCastIntrinsics
    "float i2f = intBitsToFloat\\(iVal\\.y\\)"
    "floatBitsTo(Int|Uint)\\(iVal" ${_BITCAST_LABELS})
# int <-> uint must stay a constructor cast; this is the case that decoded packed data incorrectly
add_generated_glsl_assert(bitcast_int_uint BitCastIntrinsics
    "int u2i = int\\(uVal\\.x\\)"
    "uintBitsToFloat\\(iVal" ${_BITCAST_LABELS})
# The result must carry the intrinsic's type, not the argument's, or the surrounding integer
# operations degrade into float ones (e.g. a shift by '16.f')
add_generated_glsl_assert(bitcast_result_type BitCastIntrinsics
    "uint shifted = floatBitsToUint\\(fVal\\.w\\) >> 16"
    "16\\.f|255\\.f" ${_BITCAST_LABELS})
