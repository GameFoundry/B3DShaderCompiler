# RunGLSLRoundtrip.cmake
#
# Single GLSL/SPIR-V round-trip test driver, invoked per test by CTest:
#   cmake -DXSC=... -DGLSLANG=... -DSHADER=... -DENTRY=... -DXSC_STAGE=... \
#         -DOUT_DIR=... [-DXSC_EXTRA_FLAGS=...] -P RunGLSLRoundtrip.cmake
#
# Steps:
#   1. Run xsc with `-Vout VKSL450` (Vulkan GLSL) to emit a GLSL copy of the
#      HLSL input. `--extension ON` and `-AB ON` are forced on so the output is
#      self-contained Vulkan GLSL (explicit binding/location qualifiers, plus
#      any required GLSL extension pragmas).
#   2. Run glslangValidator with `-V` on the result; this compiles it all the
#      way to SPIR-V and runs SPIR-V validation, so the test fails if the output
#      violates a SPIR-V rule -- e.g. an opaque type left inside a struct, which
#      Vulkan/SPIR-V forbids (this is what gives the opaque-struct tests teeth;
#      desktop OpenGL GLSL would accept sampler-in-struct, Vulkan does not).
# The test fails (non-zero exit) on any step's failure.

foreach(_v XSC GLSLANG SHADER ENTRY XSC_STAGE OUT_DIR)
    if(NOT ${_v})
        message(FATAL_ERROR "Required variable not set: ${_v}")
    endif()
endforeach()

# XSC_EXTRA_FLAGS is optional. Accept it as a CMake-list (";"-delimited) so
# multiple flags can be forwarded; passed straight through to xsc.
if(NOT DEFINED XSC_EXTRA_FLAGS)
    set(XSC_EXTRA_FLAGS "")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

get_filename_component(SHADER_NAME "${SHADER}" NAME_WE)

# glslangValidator infers the shader stage from the file extension.
if(XSC_STAGE STREQUAL "vert")
    set(XSC_EXT "vert")
elseif(XSC_STAGE STREQUAL "frag")
    set(XSC_EXT "frag")
elseif(XSC_STAGE STREQUAL "geom")
    set(XSC_EXT "geom")
elseif(XSC_STAGE STREQUAL "tesc")
    set(XSC_EXT "tesc")
elseif(XSC_STAGE STREQUAL "tese")
    set(XSC_EXT "tese")
elseif(XSC_STAGE STREQUAL "comp")
    set(XSC_EXT "comp")
else()
    set(XSC_EXT "${XSC_STAGE}")
endif()

set(XSC_OUTPUT "${OUT_DIR}/${SHADER_NAME}.${ENTRY}.${XSC_EXT}")

execute_process(
    COMMAND "${XSC}" -o "${XSC_OUTPUT}" -Vout VKSL450 --extension ON -AB ON
            -E "${ENTRY}" -T "${XSC_STAGE}" ${XSC_EXTRA_FLAGS} "${SHADER}"
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

execute_process(
    COMMAND "${GLSLANG}" -V "${XSC_OUTPUT}" -o "${XSC_OUTPUT}.spv"
    RESULT_VARIABLE GLSLANG_RESULT
    OUTPUT_VARIABLE GLSLANG_OUT
    ERROR_VARIABLE  GLSLANG_ERR
)

if(NOT GLSLANG_RESULT EQUAL 0)
    message(FATAL_ERROR "glslangValidator rejected the round-trip output ${XSC_OUTPUT}:\n${GLSLANG_OUT}\n${GLSLANG_ERR}")
endif()

message(STATUS "GLSL round-trip OK: ${SHADER} entry=${ENTRY}")
