# RunXscExpectError.cmake
#
# Negative-test driver, invoked per test by CTest:
#   cmake -DXSC=... -DSHADER=... -DENTRY=... -DXSC_STAGE=... -DOUT_DIR=... \
#         -DEXPECT_REGEX=... [-DXSC_EXTRA_FLAGS=...] -P RunXscExpectError.cmake
#
# Runs xsc on a shader that is expected to be REJECTED, and asserts that xsc's
# combined output matches EXPECT_REGEX (the specific diagnostic). This pins each
# negative case to its intended error. The test fails if that diagnostic does
# not appear -- e.g. if the shader is no longer rejected (a regression) or is
# rejected with a different message than expected.
#
# Note: xsc returns exit code 0 even when a compilation reports errors, so the
# diagnostic text -- not the exit code -- is the signal we assert on. (The
# positive round-trip drivers likewise rely on the produced output file +
# downstream validator rather than xsc's exit code.)

foreach(_v XSC SHADER ENTRY XSC_STAGE OUT_DIR EXPECT_REGEX)
    if(NOT ${_v})
        message(FATAL_ERROR "Required variable not set: ${_v}")
    endif()
endforeach()

if(NOT DEFINED XSC_EXTRA_FLAGS)
    set(XSC_EXTRA_FLAGS "")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

get_filename_component(SHADER_NAME "${SHADER}" NAME_WE)
set(XSC_OUTPUT "${OUT_DIR}/${SHADER_NAME}.${ENTRY}.out")

execute_process(
    COMMAND "${XSC}" -o "${XSC_OUTPUT}" -Vout VKSL450
            -E "${ENTRY}" -T "${XSC_STAGE}" ${XSC_EXTRA_FLAGS} "${SHADER}"
    RESULT_VARIABLE XSC_RESULT
    OUTPUT_VARIABLE XSC_OUT
    ERROR_VARIABLE  XSC_ERR
)

set(XSC_ALL "${XSC_OUT}${XSC_ERR}")

if(NOT XSC_ALL MATCHES "${EXPECT_REGEX}")
    message(FATAL_ERROR "expected xsc to reject ${SHADER} with a diagnostic matching:\n  ${EXPECT_REGEX}\nbut it did not. Actual output:\n${XSC_ALL}")
endif()

message(STATUS "Expected-error OK: ${SHADER} entry=${ENTRY} matched /${EXPECT_REGEX}/")
