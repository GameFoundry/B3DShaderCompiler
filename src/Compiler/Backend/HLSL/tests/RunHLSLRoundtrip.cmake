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

# XSC_EXTRA_FLAGS is optional. Accept it as a CMake-list (";"-delimited) so
# multiple flags can be forwarded; passed straight through to xsc.
if(NOT DEFINED XSC_EXTRA_FLAGS)
    set(XSC_EXTRA_FLAGS "")
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
    COMMAND "${XSC}" -o "${XSC_OUTPUT}" -Vin "${XSC_HLSL_VERSION}" -Vout "${XSC_HLSL_VERSION}" -E "${ENTRY}" -T "${XSC_STAGE}" ${XSC_EXTRA_FLAGS} "${SHADER}"
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

if(DEFINED EXPECT_REGEX)
    file(READ "${XSC_OUTPUT}" GENERATED_HLSL)
    if(NOT GENERATED_HLSL MATCHES "${EXPECT_REGEX}")
        message(FATAL_ERROR "generated HLSL did not match /${EXPECT_REGEX}/:\n${GENERATED_HLSL}")
    endif()
endif()

if(DXC)
    execute_process(
        COMMAND "${DXC}" -T "${FXC_PROFILE}" -E "${ENTRY}" -Fo "${XSC_OUTPUT}.dxil" "${XSC_OUTPUT}"
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
