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
        "ByteAddressBuffer|CS|cs_5_0|comp"
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

    # HLSL returns resources natively; DXC also accepts the runtime resource-array indices FXC rejects.
    xsc_add_roundtrip_tests(
        PREFIX         hlsl6_roundtrip
        DRIVER         ${_HLSL_DRIVER}
        SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
        OUT_DIR        ${_HLSL_OUT_DIR}
        LABELS         "hlsl-roundtrip;opaque-struct"
        DEFINES        -DDXC=${DXC_EXECUTABLE}
        PROFILE_DEFINE FXC_PROFILE
        EXTRA_FLAGS    -Xopaque-struct@ON
        CASES
            "OpaqueTypePlainReturns|main|ps_6_6|frag"
            "OpaqueTypeRuntimeArrayReturn|main|ps_6_6|frag"
    )

    xsc_add_roundtrip_tests(
        PREFIX         hlsl6_roundtrip
        DRIVER         ${_HLSL_DRIVER}
        SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
        OUT_DIR        ${_HLSL_OUT_DIR}
        LABELS         "hlsl-roundtrip;bindless;opaque-struct"
        DEFINES        -DDXC=${DXC_EXECUTABLE}
        PROFILE_DEFINE FXC_PROFILE
        EXTRA_FLAGS    -Xbindless@ON@-Xopaque-struct@ON
        CASES
            "BindlessOpaqueReturns|CS|cs_6_6|comp"
            "BindlessByteAddressBuffer|CS|cs_6_6|comp"
    )
else()
    message(STATUS "dxc not found; HLSL 6 bindless round-trip tests will not be registered.")
endif()

# The HLSL6VK target needs a DXC with SPIR-V code generation, which the Windows SDK copy doesn't have.
if(DXC_EXECUTABLE)
    execute_process(COMMAND "${DXC_EXECUTABLE}" -help OUTPUT_VARIABLE _DXC_HELP ERROR_QUIET)
    if(_DXC_HELP MATCHES "-spirv")
        xsc_add_roundtrip_tests(
            PREFIX         hlsl6vk_roundtrip
            DRIVER         ${_HLSL_DRIVER}
            SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
            OUT_DIR        ${_HLSL_OUT_DIR}/hlsl6vk
            LABELS         "hlsl-roundtrip;hlsl6vk;push-constants"
            DEFINES        -DDXC=${DXC_EXECUTABLE} -DXSC_OUTPUT_TARGET=HLSL6VK
                           -DDXC_EXTRA_FLAGS=-spirv@-fspv-target-env=vulkan1.3@-WX
                           "-DEXPECT_REGEX=vk::image_format..rgba16f[^\r\n]*outputTexture.*vk::image_format..unknown[^\r\n]*inputIndices.*vk::push_constant[^\r\n]*PushConstants"
            PROFILE_DEFINE FXC_PROFILE
            EXTRA_FLAGS    -Xpush-constants@-Xattr-layout
            CASES          "VulkanTargetTest|main|cs_6_6|comp"
        )
    else()
        message(STATUS "HLSL6VK round-trip tests skipped: ${DXC_EXECUTABLE} has no SPIR-V code generation")
    endif()
endif()

# --- Ray tracing ---
# Round trips through DXC with the engine's flags, for HLSL6 and, when this DXC generates SPIR-V, for HLSL6VK. A library
# compile keeps [shader] on its own entry only, and each (built-in, callbacks struct) helper is written once.
if(DXC_EXECUTABLE)
    set(_RT_TARGETS HLSL6)
    if(_DXC_HELP MATCHES "-spirv")
        list(APPEND _RT_TARGETS HLSL6VK)
    endif()

    set(_RT_STAGES_REFLECTION
        "Acceleration Structures:[\r\n ]+0: gScene, set 0, usesDynamicOffset false[\r\n ]+Ray Tracing:[\r\n ]+Inline ray tracing: unused[\r\n ]+"
        "RayGen: Ray Generation Shader, payload 0 bytes, attributes 0 bytes[\r\n ]+"
        "Miss: Miss Shader, payload 20 bytes, attributes 0 bytes[\r\n ]+"
        "ClosestHit: Closest-Hit Shader, payload 20 bytes, attributes 8 bytes[\r\n ]+"
        "AnyHit: Any-Hit Shader, payload 20 bytes, attributes 8 bytes[\r\n ]+"
        "Intersection: Intersection Shader, payload 0 bytes, attributes 12 bytes[\r\n ]+"
        "Callable: Callable Shader, payload 0 bytes, attributes 0 bytes")
    string(CONCAT _RT_STAGES_REFLECTION ${_RT_STAGES_REFLECTION})

    # Shared helpers come first, then each struct's helpers right after it, and calls use them
    set(_RT_INLINE_HELPERS
        "RayHit xsc_RayTraceClosest\\(.*RayHit xsc_RayTraceAny\\(.*"
        "struct AlphaTest.*RayHit xsc_RayTraceClosest_AlphaTest\\(.*RayHit xsc_RayTraceAny_AlphaTest\\(.*"
        "struct Spheres.*RayHit xsc_RayTraceClosest_Spheres\\(.*"
        "struct TranslucentSpheres.*RayHit xsc_RayTraceAny_TranslucentSpheres\\(.*"
        "return xsc_RayTraceClosest_AlphaTest\\(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES \\| RAY_FLAG_FORCE_NON_OPAQUE, gInstanceMask, ray, callbacks\\)")
    string(CONCAT _RT_INLINE_HELPERS ${_RT_INLINE_HELPERS})

    # Helpers written twice, or for pairs the source doesn't use
    set(_RT_INLINE_ABSENT
        "RayHit xsc_RayTraceClosest\\(.*RayHit xsc_RayTraceClosest\\(|RayHit xsc_RayTraceAny\\(.*RayHit xsc_RayTraceAny\\(|"
        "RayHit xsc_RayTraceClosest_AlphaTest\\(.*RayHit xsc_RayTraceClosest_AlphaTest\\(|"
        "RayHit xsc_RayTraceAny_AlphaTest\\(.*RayHit xsc_RayTraceAny_AlphaTest\\(|"
        "RayHit xsc_RayTraceClosest_Spheres\\(.*RayHit xsc_RayTraceClosest_Spheres\\(|"
        "RayHit xsc_RayTraceAny_TranslucentSpheres\\(.*RayHit xsc_RayTraceAny_TranslucentSpheres\\(|"
        "xsc_RayTraceAny_Spheres|xsc_RayTraceClosest_TranslucentSpheres")
    string(CONCAT _RT_INLINE_ABSENT ${_RT_INLINE_ABSENT})

    foreach(_target IN LISTS _RT_TARGETS)
        string(TOLOWER "${_target}" _rtPrefix)
        set(_rtPrefix "${_rtPrefix}_raytracing")
        if(_target STREQUAL "HLSL6VK")
            set(_rtOutDir "${_HLSL_OUT_DIR}/hlsl6vk")
            set(_rtDxcFlags "-spirv@-fspv-target-env=vulkan1.3@-Zpc@-Ges@-HV@2016")
        else()
            set(_rtOutDir "${_HLSL_OUT_DIR}")
            set(_rtDxcFlags "-Zpc@-Ges@-HV@2016")
        endif()
        set(_rtDefines -DDXC=${DXC_EXECUTABLE} -DXSC_OUTPUT_TARGET=${_target} -DDXC_EXTRA_FLAGS=${_rtDxcFlags}
                       -DEXPECT_ENTRY_SHADER_ATTRIBUTE=ON)

        # One entry per stage, with every ray tracing function and intrinsic
        xsc_add_roundtrip_tests(
            PREFIX         ${_rtPrefix}
            DRIVER         ${_HLSL_DRIVER}
            SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
            OUT_DIR        ${_rtOutDir}
            LABELS         "hlsl-roundtrip;raytracing"
            DEFINES        ${_rtDefines} "-DREFLECT_REGEX=${_RT_STAGES_REFLECTION}"
            PROFILE_DEFINE FXC_PROFILE
            CASES          "RayTracingStages|RayGen|lib_6_6|rgen"
                           "RayTracingStages|Miss|lib_6_6|rmiss"
                           "RayTracingStages|ClosestHit|lib_6_6|rchit"
                           "RayTracingStages|AnyHit|lib_6_6|rahit"
                           "RayTracingStages|Intersection|lib_6_6|rint"
                           "RayTracingStages|Callable|lib_6_6|rcall"
        )

        # RayTraceClosest and RayTraceAny in compute, pixel and closest hit entries
        xsc_add_roundtrip_tests(
            PREFIX         ${_rtPrefix}
            DRIVER         ${_HLSL_DRIVER}
            SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
            OUT_DIR        ${_rtOutDir}
            LABELS         "hlsl-roundtrip;raytracing"
            DEFINES        ${_rtDefines} "-DEXPECT_REGEX=${_RT_INLINE_HELPERS}" "-DEXPECT_ABSENT_REGEX=${_RT_INLINE_ABSENT}"
                           "-DREFLECT_REGEX=Inline ray tracing: used"
            PROFILE_DEFINE FXC_PROFILE
            CASES          "RayTracingInline|CS|cs_6_6|comp"
                           "RayTracingInline|PS|ps_6_6|frag"
                           "RayTracingInline|ClosestHit|lib_6_6|rchit"
        )

        # Helpers fill only the RayCandidate fields each callback reads, in its own branch, and the RayHit fields the program reads
        set(_RT_FIELDS_TRIMMED
            "\\{[\r\n ]+RayCandidate candidate = \\(RayCandidate\\)0[^\r\n]*[\r\n ]+candidate\\.T = query\\.CandidateTriangleRayT\\(\\)[^\r\n]*[\r\n ]+if \\(callbacks\\.AnyHit"
            ".*else[\r\n ]+\\{[\r\n ]+RayCandidate candidate = \\(RayCandidate\\)0[^\r\n]*[\r\n ]+candidate\\.ObjectRayOrigin = query\\.CandidateObjectRayOrigin\\(\\)[^\r\n]*[\r\n ]+float t"
            ".*hit\\.IsHit = [^\r\n]*[\r\n ]+if \\(hit\\.IsHit\\)[\r\n ]+\\{[\r\n ]+hit\\.T = query\\.CommittedRayT\\(\\)[^\r\n]*[\r\n ]+\\}")
        string(CONCAT _RT_FIELDS_TRIMMED ${_RT_FIELDS_TRIMMED})
        xsc_add_roundtrip_tests(
            PREFIX         ${_rtPrefix}
            DRIVER         ${_HLSL_DRIVER}
            SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
            OUT_DIR        ${_rtOutDir}
            LABELS         "hlsl-roundtrip;raytracing"
            DEFINES        ${_rtDefines} "-DEXPECT_REGEX=${_RT_FIELDS_TRIMMED}"
                           "-DEXPECT_ABSENT_REGEX=IsProcedural =|CommittedInstance|CommittedGeometryIndex|CommittedPrimitiveIndex|CommittedObjectToWorld|CommittedWorldToObject|CommittedTriangle"
            PROFILE_DEFINE FXC_PROFILE
            CASES          "RayTracingInlineFields|Trimmed|cs_6_6|comp"
        )

        # A candidate stored in a member and a hit stored in a buffer fill every field
        set(_RT_FIELDS_ESCAPING
            "xsc_RayTraceClosest_StoresCandidate.*candidate\\.InstanceIndex = query\\.CandidateInstanceIndex\\(\\)"
            ".*candidate\\.FrontFace = query\\.CandidateTriangleFrontFace\\(\\)"
            ".*hit\\.WorldToObject = query\\.CommittedWorldToObject3x4\\(\\)"
            ".*hit\\.FrontFace = query\\.CommittedTriangleFrontFace\\(\\)")
        string(CONCAT _RT_FIELDS_ESCAPING ${_RT_FIELDS_ESCAPING})
        xsc_add_roundtrip_tests(
            PREFIX         ${_rtPrefix}
            DRIVER         ${_HLSL_DRIVER}
            SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
            OUT_DIR        ${_rtOutDir}
            LABELS         "hlsl-roundtrip;raytracing"
            DEFINES        ${_rtDefines} "-DEXPECT_REGEX=${_RT_FIELDS_ESCAPING}"
            PROFILE_DEFINE FXC_PROFILE
            CASES          "RayTracingInlineFields|Escaping|cs_6_6|comp"
        )

        # Acceleration structures through an array and through ResourceDescriptorHeap
        xsc_add_roundtrip_tests(
            PREFIX         ${_rtPrefix}
            DRIVER         ${_HLSL_DRIVER}
            SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
            OUT_DIR        ${_rtOutDir}
            LABELS         "hlsl-roundtrip;raytracing;bindless"
            DEFINES        ${_rtDefines}
                           "-DREFLECT_REGEX=Resource heap: used.*Acceleration Structures:[\r\n ]+0: gScenes, set 1"
            PROFILE_DEFINE FXC_PROFILE
            EXTRA_FLAGS    -Xbindless
            CASES          "RayTracingBindless|RayGen|lib_6_6|rgen"
                           "RayTracingBindless|CS|cs_6_6|comp"
        )

        xsc_add_roundtrip_tests(
            PREFIX         ${_rtPrefix}
            DRIVER         ${_HLSL_DRIVER}
            SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
            OUT_DIR        ${_rtOutDir}
            LABELS         "hlsl-roundtrip;raytracing;push-constants"
            DEFINES        ${_rtDefines} "-DREFLECT_REGEX=TraceConstants \\(8 bytes\\)"
            PROFILE_DEFINE FXC_PROFILE
            EXTRA_FLAGS    -Xpush-constants
            CASES          "RayTracingPushConstants|RayGen|lib_6_6|rgen"
        )
    endforeach()
endif()

# Ray tracing rejections, one diagnostic per compile. Pipe-delimited: shader|entry|stage|case macro|expected diagnostic
# regex[|extra xsc flags, '@'-separated]. The regex can't contain '|' or ';'.
set(XSC_HLSL_RAYTRACING_REJECT_CASES
    # Stage table: one disallowed call per row, also when its result is unused or it's reached through a helper
    "RayTracingRejectStages|RayGen|rgen|RAYGEN_WORLD_RAY_ORIGIN|'WorldRayOrigin' isn't available in stage 'Ray Generation Shader'"
    "RayTracingRejectStages|RayGen|rgen|RAYGEN_DDX|'ddx' isn't available in stage 'Ray Generation Shader'"
    "RayTracingRejectStages|RayGen|rgen|RAYGEN_ALL_MEMORY_BARRIER|'AllMemoryBarrier' isn't available in stage 'Ray Generation Shader'"
    "RayTracingRejectStages|Miss|rmiss|MISS_INSTANCE_ID|'InstanceID' isn't available in stage 'Miss Shader'"
    "RayTracingRejectStages|Miss|rmiss|MISS_HIT_KIND_UNUSED|'HitKind' isn't available in stage 'Miss Shader'"
    "RayTracingRejectStages|Miss|rmiss|MISS_ACCEPT_HIT|'AcceptHitAndEndSearch' isn't available in stage 'Miss Shader'"
    "RayTracingRejectStages|Miss|rmiss|MISS_SAMPLE|'Sample' isn't available in stage 'Miss Shader'"
    "RayTracingRejectStages|Miss|rmiss|MISS_SAMPLE_CMP|'SampleCmp' isn't available in stage 'Miss Shader'"
    "RayTracingRejectStages|Miss|rmiss|MISS_LEVEL_OF_DETAIL|'CalculateLevelOfDetail' isn't available in stage 'Miss Shader'"
    "RayTracingRejectStages|ClosestHit|rchit|CLOSESTHIT_IGNORE_HIT|'IgnoreHit' isn't available in stage 'Closest-Hit Shader'"
    "RayTracingRejectStages|ClosestHit|rchit|CLOSESTHIT_IGNORE_HIT_HELPER|'IgnoreHit' isn't available in stage 'Closest-Hit Shader'"
    "RayTracingRejectStages|ClosestHit|rchit|CLOSESTHIT_CLIP|'clip' isn't available in stage 'Closest-Hit Shader'"
    "RayTracingRejectStages|ClosestHit|rchit|CLOSESTHIT_OBJECT_TO_WORLD|'ObjectToWorld' isn't supported.*use 'ObjectToWorld3x4' or 'ObjectToWorld4x3'"
    "RayTracingRejectStages|ClosestHit|rchit|CLOSESTHIT_WORLD_TO_OBJECT|'WorldToObject' isn't supported.*use 'WorldToObject3x4' or 'WorldToObject4x3'"
    "RayTracingRejectStages|AnyHit|rahit|ANYHIT_TRACE_RAY|'TraceRay' isn't available in stage 'Any-Hit Shader'"
    "RayTracingRejectStages|AnyHit|rahit|ANYHIT_REPORT_HIT|'ReportHit' isn't available in stage 'Any-Hit Shader'"
    "RayTracingRejectStages|AnyHit|rahit|ANYHIT_DISCARD|'discard' isn't available in stage 'Any-Hit Shader'"
    "RayTracingRejectStages|Intersection|rint|INTERSECTION_CALL_SHADER|'CallShader' isn't available in stage 'Intersection Shader'"
    "RayTracingRejectStages|Intersection|rint|INTERSECTION_HIT_KIND|'HitKind' isn't available in stage 'Intersection Shader'"
    "RayTracingRejectStages|Callable|rcall|CALLABLE_RAY_T_CURRENT|'RayTCurrent' isn't available in stage 'Callable Shader'"
    "RayTracingRejectStages|Callable|rcall|CALLABLE_GROUP_BARRIER|'GroupMemoryBarrierWithGroupSync' isn't available in stage 'Callable Shader'"
    "RayTracingRejectStages|CS|comp|COMPUTE_DISPATCH_RAYS_INDEX|'DispatchRaysIndex' isn't available in stage 'Compute Shader'"
    "RayTracingRejectStages|CS|comp|COMPUTE_TRACE_RAY|'TraceRay' isn't available in stage 'Compute Shader'"
    "RayTracingRejectStages|PS|frag|PIXEL_PRIMITIVE_INDEX|'PrimitiveIndex' isn't available in stage 'Fragment Shader'"

    # Inline built-ins: RayQuery, callbacks arguments and structs, constant flags, arguments, and callback nesting
    "RayTracingRejectInline|CS|comp|RAY_QUERY|'RayQuery' isn't supported. use RayTraceClosest or RayTraceAny"
    "RayTracingRejectInline|CS|comp|CALLBACKS_GLOBAL|callbacks argument of 'RayTraceClosest' must be a local variable or a parameter"
    "RayTracingRejectInline|CS|comp|CALLBACKS_MEMBER|callbacks argument of 'RayTraceClosest' must be a local variable or a parameter"
    "RayTracingRejectInline|CS|comp|CALLBACKS_TEMPORARY|callbacks argument of 'RayTraceClosest' must be a local variable or a parameter"
    "RayTracingRejectInline|CS|comp|CALLBACKS_ARRAY_ELEMENT|callbacks argument of 'RayTraceClosest' must be a local variable or a parameter"
    "RayTracingRejectInline|CS|comp|CALLBACKS_STATIC_LOCAL|callbacks argument of 'RayTraceClosest' must be a local variable or a parameter"
    "RayTracingRejectInline|CS|comp|CALLBACKS_LOCAL_STRUCT|callbacks argument of 'RayTraceClosest' must have a struct type declared at global scope, but it is 'struct LocalCallbacks'"
    "RayTracingRejectInline|CS|comp|CALLBACKS_TEMPLATE|callbacks argument of 'RayTraceClosest' must have a struct type declared at global scope, but it is 'struct TemplateCallbacks'|-Xhlsl-templates"
    "RayTracingRejectInline|CS|comp|CALLBACKS_NOT_STRUCT|callbacks argument of 'RayTraceClosest' must have a struct type declared at global scope, but it is 'float'"
    "RayTracingRejectInline|CS|comp|CALLBACKS_NO_METHODS|callbacks struct 'Typo' has neither an 'AnyHit' nor an 'Intersection' method"
    "RayTracingRejectInline|CS|comp|ANYHIT_EXTRA_PARAMETER|'BadAnyHit::AnyHit' must be declared as 'bool AnyHit\\(RayCandidate candidate\\)'"
    "RayTracingRejectInline|CS|comp|ANYHIT_RETURN_TYPE|'BadAnyHit::AnyHit' must be declared as 'bool AnyHit\\(RayCandidate candidate\\)'"
    "RayTracingRejectInline|CS|comp|INTERSECTION_NOT_OUT|'BadIntersection::Intersection' must be declared as 'bool Intersection\\(RayCandidate candidate, out float t\\)'"
    "RayTracingRejectInline|CS|comp|INTERSECTION_WRONG_TYPE|'BadIntersection::Intersection' must be declared as 'bool Intersection\\(RayCandidate candidate, out float t\\)'"
    "RayTracingRejectInline|CS|comp|FLAG_ACCEPT_FIRST_HIT|ray flag RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH can't be used with 'RayTraceClosest'. use RayTraceAny"
    "RayTracingRejectInline|CS|comp|FLAG_SKIP_CLOSEST_HIT|ray flag RAY_FLAG_SKIP_CLOSEST_HIT_SHADER can't be used with 'RayTraceAny'"
    "RayTracingRejectInline|CS|comp|FLAG_SKIP_TRIANGLES|ray flag RAY_FLAG_SKIP_TRIANGLES can't be used with 'RayTraceClosest'. not supported on all backends"
    "RayTracingRejectInline|CS|comp|FLAG_UNKNOWN|ray flag 0x400 can't be used with 'RayTraceClosest'"
    "RayTracingRejectInline|CS|comp|ARG_COUNT|'RayTraceClosest' takes 4 or 5 arguments.*but got 3"
    "RayTracingRejectInline|CS|comp|ARG_TYPE|argument 4 of 'RayTraceAny' must be 'RayDesc', but it is 'float3'"
    "RayTracingRejectInline|CS|comp|NEST_TRACE_DIRECT|'RayTraceAny' can't be used inside the ray tracing callback 'Callbacks::AnyHit'"
    "RayTracingRejectInline|CS|comp|NEST_TRACE_HELPER|'RayTraceClosest' can't be used inside the ray tracing callback 'Callbacks::AnyHit'"
    "RayTracingRejectInline|ClosestHit|rchit|NEST_TRACE_RAY|'TraceRay' can't be used inside the ray tracing callback 'Callbacks::AnyHit'"
    "RayTracingRejectInline|ClosestHit|rchit|NEST_CALL_SHADER|'CallShader' can't be used inside the ray tracing callback 'Callbacks::Intersection'"
    "RayTracingRejectInline|PS|frag|NEST_DISCARD|'discard' can't be used inside the ray tracing callback 'Callbacks::AnyHit'"
    "RayTracingRejectInline|PS|frag|NEST_CLIP_HELPER|'clip' can't be used inside the ray tracing callback 'Callbacks::AnyHit'"

    # The [shader] attribute and entry point signatures
    "RayTracingRejectEntries|Entry|rgen|UNKNOWN_STAGE|the 'shader' attribute takes one of"
    "RayTracingRejectEntries|Entry|rmiss|DUPLICATE_ATTRIBUTE|function 'Entry' has more than one 'shader' attribute"
    "RayTracingRejectEntries|Entry|rgen|ATTRIBUTE_ON_MEMBER_FUNCTION|the 'shader' attribute is only allowed on global functions"
    "RayTracingRejectEntries|Entry|rgen|ATTRIBUTE_ON_STRUCT|the 'shader' attribute is only allowed on global functions"
    "RayTracingRejectEntries|Entry|rgen|ATTRIBUTE_ON_STATEMENT|the 'shader' attribute is only allowed on global functions"
    "RayTracingRejectEntries|Entry|rchit|STAGE_MISMATCH|entry point 'Entry' is marked .shader..miss..., which doesn't match target 'Closest-Hit Shader'"
    "RayTracingRejectEntries|Entry|comp|NOT_RAY_TRACING_TARGET|entry point 'Entry' is marked .shader..miss..., but target 'Compute Shader' isn't a ray tracing stage"
    "RayTracingRejectEntries|Entry|rgen|MISSING_ATTRIBUTE|entry point 'Entry' needs the attribute .shader..raygeneration... for target 'Ray Generation Shader'"
    "RayTracingRejectEntries|Entry|rgen|RETURNS_VALUE|ray tracing entry point 'Entry' must return void"
    "RayTracingRejectEntries|Entry|rgen|RAYGEN_PARAMETER|raygeneration entry point 'Entry' must take no parameters"
    "RayTracingRejectEntries|Entry|rmiss|MISS_IN_PAYLOAD|miss entry point 'Entry' must take one 'inout' payload struct"
    "RayTracingRejectEntries|Entry|rcall|CALLABLE_TWO_PARAMETERS|callable entry point 'Entry' must take one 'inout' parameter struct"
    "RayTracingRejectEntries|Entry|rchit|CLOSESTHIT_ONE_PARAMETER|closesthit entry point 'Entry' must take an 'inout' payload struct, then an 'in' attribute struct"
    "RayTracingRejectEntries|Entry|rahit|ANYHIT_INOUT_ATTRIBUTES|anyhit entry point 'Entry' must take an 'inout' payload struct, then an 'in' attribute struct"
    "RayTracingRejectEntries|Entry|rmiss|PAYLOAD_NOT_STRUCT|miss entry point 'Entry' must take one 'inout' payload struct"
    "RayTracingRejectEntries|Entry|rmiss|PAYLOAD_WITH_RESOURCE|payload 'ResourcePayload' must be a struct that holds only numeric data|-Xopaque-struct"
    "RayTracingRejectEntries|Entry|rchit|ATTRIBUTES_TOO_LARGE|attribute struct 'LargeAttributes' is 36 bytes. ray tracing attributes are limited to 32 bytes"
    "RayTracingRejectEntries|Entry|rint|REPORT_HIT_TOO_LARGE|attribute struct 'LargeAttributes' is 36 bytes. ray tracing attributes are limited to 32 bytes"
    "RayTracingRejectEntries|Entry|rint|REPORT_HIT_NOT_STRUCT|attribute struct 'float2' must be a struct that holds only numeric data"
)

foreach(_case IN LISTS XSC_HLSL_RAYTRACING_REJECT_CASES)
    string(REPLACE "|" ";" _parts "${_case}")
    list(GET _parts 0 _shader)
    list(GET _parts 1 _entry)
    list(GET _parts 2 _stage)
    list(GET _parts 3 _macro)
    list(GET _parts 4 _expect)
    set(_flags "-Vin;HLSL6;-D${_macro}")
    list(LENGTH _parts _partCount)
    if(_partCount GREATER 5)
        list(GET _parts 5 _extraFlags)
        string(REPLACE "@" ";" _extraFlags "${_extraFlags}")
        list(APPEND _flags ${_extraFlags})
    endif()

    add_test(
        NAME hlsl_reject.${_shader}.${_macro}
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/${_shader}.hlsl
            -DENTRY=${_entry}
            -DXSC_STAGE=${_stage}
            -DOUT_DIR=${_HLSL_OUT_DIR}
            -DXSC_VOUT=HLSL6
            "-DEXPECT_REGEX=${_expect}"
            "-DXSC_EXTRA_FLAGS=${_flags}"
            -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests/RunXscExpectError.cmake
    )
    set_tests_properties(hlsl_reject.${_shader}.${_macro} PROPERTIES LABELS "raytracing;negative")
endforeach()

# Only HLSL6, HLSL6VK and HLSL support ray tracing; other targets reject it for the whole source
foreach(_target HLSL5 VKSL450)
    add_test(
        NAME hlsl_reject.RayTracingStages.${_target}
        COMMAND ${CMAKE_COMMAND}
            -DXSC=${XSC_BIN}
            -DSHADER=${PROJECT_SOURCE_DIR}/test/RayTracingStages.hlsl
            -DENTRY=RayGen
            -DXSC_STAGE=rgen
            -DOUT_DIR=${_HLSL_OUT_DIR}
            -DXSC_VOUT=${_target}
            "-DEXPECT_REGEX=ray tracing is not supported by output target '${_target}'"
            "-DXSC_EXTRA_FLAGS=-Vin;HLSL6"
            -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests/RunXscExpectError.cmake
    )
    set_tests_properties(hlsl_reject.RayTracingStages.${_target} PROPERTIES LABELS "raytracing;negative")
endforeach()

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

# Dynamic bindings keep ordinary native buffer declarations and packing.
xsc_add_roundtrip_tests(
    PREFIX         hlsl_roundtrip
    DRIVER         ${_HLSL_DRIVER}
    SHADER_DIR     ${PROJECT_SOURCE_DIR}/test
    OUT_DIR        ${_HLSL_OUT_DIR}
    LABELS         "hlsl-roundtrip;dynamic-offsets"
    DEFINES        -DFXC=${FXC_EXECUTABLE}
    PROFILE_DEFINE FXC_PROFILE
    EXTRA_FLAGS    -Xdynamic-offsets
    PER_CASE_FLAGS
    CASES          "DynamicOffsets|main|vs_5_1|vert"
                   "DynamicOffsetsRenamed|main|ps_5_1|frag"
                   "DynamicOffsetsAutomatic|main|ps_5_1|frag|-Xdynamic-offsets@-AB"
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
    EXTRA_FLAGS    -Xpush-constants@-push-constant-register@3@-push-constant-space@7
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
    EXTRA_FLAGS    -Xpush-constants@--max-push-constant-buffer-size@4
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
    EXTRA_FLAGS    -Xpush-constants@--max-push-constant-buffer-size@64
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
    EXTRA_FLAGS    -Xpush-constants@-AB@-push-constant-register@0@-push-constant-space@0
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
        "-DXSC_EXTRA_FLAGS=-Xpush-constants;-push-constant-register;3;-push-constant-space;7"
        -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests/RunXscExpectError.cmake
)
set_tests_properties(hlsl_reject.PushConstantBindingCollision
    PROPERTIES LABELS "hlsl-roundtrip;push-constants;negative")

add_test(
    NAME hlsl_reject.PushConstantInvalidVulkanMatrix
    COMMAND ${CMAKE_COMMAND}
        -DXSC=${XSC_BIN}
        -DSHADER=${PROJECT_SOURCE_DIR}/test/PushConstantInvalidVulkanMatrix.hlsl
        -DENTRY=main
        -DXSC_STAGE=vert
        -DOUT_DIR=${_HLSL_OUT_DIR}
        -DXSC_VOUT=HLSL6VK
        "-DEXPECT_REGEX=push-constant matrix 'columns' has two-component columns or rows.*push-constant matrix 'rows' has two-component columns or rows"
        "-DXSC_EXTRA_FLAGS=-Xpush-constants;--max-push-constant-buffer-size;256"
        -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests/RunXscExpectError.cmake
)
set_tests_properties(hlsl_reject.PushConstantInvalidVulkanMatrix
    PROPERTIES LABELS "hlsl-roundtrip;hlsl6vk;push-constants;negative")

add_test(
    NAME hlsl_reject.PushConstantInvalidVulkanNestedStruct
    COMMAND ${CMAKE_COMMAND}
        -DXSC=${XSC_BIN}
        -DSHADER=${PROJECT_SOURCE_DIR}/test/PushConstantInvalidVulkanNestedStruct.hlsl
        -DENTRY=main
        -DXSC_STAGE=vert
        -DOUT_DIR=${_HLSL_OUT_DIR}
        -DXSC_VOUT=HLSL6VK
        "-DEXPECT_REGEX=2 ERRORS.*push-constant member 'inner' is a structure inside another structure"
        "-DXSC_EXTRA_FLAGS=-Xpush-constants;--max-push-constant-buffer-size;256"
        -P ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/GLSL/tests/RunXscExpectError.cmake
)
set_tests_properties(hlsl_reject.PushConstantInvalidVulkanNestedStruct
    PROPERTIES LABELS "hlsl-roundtrip;hlsl6vk;push-constants;negative")

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
