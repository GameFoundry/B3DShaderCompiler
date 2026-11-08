# RunHLSLRoundtrip.cmake
#
# Single round-trip test driver, invoked per test by CTest:
#   cmake -DXSC=... -DFXC=... -DSHADER=... -DENTRY=... -DFXC_PROFILE=... \
#         -DXSC_STAGE=... -DOUT_DIR=... -P RunHLSLRoundtrip.cmake
#
# Steps:
#   1. Run xsc to emit an HLSL "round-trip" copy of the input shader into OUT_DIR.
#   2. Run fxc on the round-trip copy and check it compiles cleanly.
# The test fails (non-zero exit) on any step's failure.

foreach(_v XSC SHADER ENTRY FXC_PROFILE XSC_STAGE OUT_DIR)
    if(NOT ${_v})
        message(FATAL_ERROR "Required variable not set: ${_v}")
    endif()
endforeach()

if(NOT DXC AND NOT FXC)
    message(FATAL_ERROR "Either FXC or DXC must be set")
endif()

if(DXC)
    set(XSC_HLSL_VERSION HLSL6)
else()
    set(XSC_HLSL_VERSION HLSL5)
endif()

# XSC_OUTPUT_TARGET is optional, and selects an output target other than the input
# version. DXC_EXTRA_FLAGS is optional, with '@' as its flag separator.
if(NOT DEFINED XSC_OUTPUT_TARGET)
    set(XSC_OUTPUT_TARGET "${XSC_HLSL_VERSION}")
endif()
if(DEFINED DXC_EXTRA_FLAGS)
    string(REPLACE "@" ";" DXC_EXTRA_FLAGS "${DXC_EXTRA_FLAGS}")
endif()

# XSC_EXTRA_FLAGS is optional. Accept it as a CMake-list (";"-delimited) so
# multiple flags can be forwarded; passed straight through to xsc.
if(NOT DEFINED XSC_EXTRA_FLAGS)
    set(XSC_EXTRA_FLAGS "")
endif()

# Optional checks of the generated source and of the printed reflection:
#   EXPECT_REGEX                   must match the generated source
#   EXPECT_ABSENT_REGEX            must not match the generated source
#   EXPECT_ENTRY_SHADER_ATTRIBUTE  ON: a ray tracing entry point has the only [shader("...")] attribute in the source,
#                                  and other entry points leave none
#   REFLECT_REGEX                  xsc runs with --reflect, and its output must match
if(DEFINED REFLECT_REGEX)
    list(APPEND XSC_EXTRA_FLAGS --reflect)
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

get_filename_component(SHADER_NAME "${SHADER}" NAME_WE)

# xsc derives the output filename from the input + entry + stage extension, so
# we feed it an "-o" path with the full target name to land in OUT_DIR.
if(XSC_STAGE STREQUAL "vert")
    set(XSC_EXT "vert")
elseif(XSC_STAGE STREQUAL "frag")
    set(XSC_EXT "frag")
elseif(XSC_STAGE STREQUAL "geom")
    set(XSC_EXT "geom")
elseif(XSC_STAGE STREQUAL "comp")
    set(XSC_EXT "comp")
else()
    set(XSC_EXT "${XSC_STAGE}")
endif()

set(XSC_OUTPUT "${OUT_DIR}/${SHADER_NAME}.${ENTRY}.${XSC_EXT}")

execute_process(
    COMMAND "${XSC}" -o "${XSC_OUTPUT}" -Vin "${XSC_HLSL_VERSION}" -Vout "${XSC_OUTPUT_TARGET}" -E "${ENTRY}" -T "${XSC_STAGE}" ${XSC_EXTRA_FLAGS} "${SHADER}"
    RESULT_VARIABLE XSC_RESULT
    OUTPUT_VARIABLE XSC_OUT
    ERROR_VARIABLE  XSC_ERR
)

if(NOT XSC_RESULT EQUAL 0)
    message(FATAL_ERROR "xsc failed (exit=${XSC_RESULT}):\n${XSC_OUT}\n${XSC_ERR}")
endif()

if(NOT EXISTS "${XSC_OUTPUT}")
    message(FATAL_ERROR "xsc did not produce expected file: ${XSC_OUTPUT}")
endif()

file(READ "${XSC_OUTPUT}" GENERATED_HLSL)
if(DEFINED EXPECT_REGEX AND NOT GENERATED_HLSL MATCHES "${EXPECT_REGEX}")
    message(FATAL_ERROR "generated HLSL did not match /${EXPECT_REGEX}/:\n${GENERATED_HLSL}")
endif()

if(DEFINED EXPECT_ABSENT_REGEX AND GENERATED_HLSL MATCHES "${EXPECT_ABSENT_REGEX}")
    message(FATAL_ERROR "generated HLSL unexpectedly matched /${EXPECT_ABSENT_REGEX}/:\n${GENERATED_HLSL}")
endif()

if(EXPECT_ENTRY_SHADER_ATTRIBUTE)
    # Counted by length: a list of "[shader(" matches is one element, since '[' stops ';' from separating them
    string(REPLACE "[shader(" "" _withoutShaderAttributes "${GENERATED_HLSL}")
    string(LENGTH "${GENERATED_HLSL}" _sourceLength)
    string(LENGTH "${_withoutShaderAttributes}" _sourceLengthWithout)
    math(EXPR _shaderAttributeCount "(${_sourceLength} - ${_sourceLengthWithout}) / 8")
    if(XSC_STAGE MATCHES "^(rgen|rint|rahit|rchit|rmiss|rcall)$")
        if(NOT _shaderAttributeCount EQUAL 1 OR NOT GENERATED_HLSL MATCHES "\\[shader\\(\"[a-z]+\"\\)\\][\r\n\t ]*void ${ENTRY}\\(")
            message(FATAL_ERROR "expected exactly one [shader] attribute, on '${ENTRY}', but found ${_shaderAttributeCount}:\n${GENERATED_HLSL}")
        endif()
    elseif(NOT _shaderAttributeCount EQUAL 0)
        message(FATAL_ERROR "expected no [shader] attribute for stage '${XSC_STAGE}', but found ${_shaderAttributeCount}:\n${GENERATED_HLSL}")
    endif()
endif()

if(DEFINED REFLECT_REGEX AND NOT XSC_OUT MATCHES "${REFLECT_REGEX}")
    message(FATAL_ERROR "xsc reflection did not match /${REFLECT_REGEX}/:\n${XSC_OUT}")
endif()

if(DXC)
    execute_process(
        COMMAND "${DXC}" ${DXC_EXTRA_FLAGS} -T "${FXC_PROFILE}" -E "${ENTRY}" -Fo "${XSC_OUTPUT}.dxil" "${XSC_OUTPUT}"
        RESULT_VARIABLE COMPILER_RESULT
        OUTPUT_VARIABLE COMPILER_OUT
        ERROR_VARIABLE  COMPILER_ERR
    )
    set(_compiler_name "dxc")
else()
    execute_process(
        COMMAND "${FXC}" /nologo /T "${FXC_PROFILE}" /E "${ENTRY}" /Fo nul "${XSC_OUTPUT}"
        RESULT_VARIABLE COMPILER_RESULT
        OUTPUT_VARIABLE COMPILER_OUT
        ERROR_VARIABLE  COMPILER_ERR
    )
    set(_compiler_name "fxc")
endif()

if(NOT COMPILER_RESULT EQUAL 0)
    message(FATAL_ERROR "${_compiler_name} rejected the round-trip output ${XSC_OUTPUT}:\n${COMPILER_OUT}\n${COMPILER_ERR}")
endif()

message(STATUS "Round-trip OK: ${SHADER} entry=${ENTRY} profile=${FXC_PROFILE}")
