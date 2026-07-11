# Compile one opaque-type fixture and assert on the generated GLSL text.

foreach(_v XSC SHADER ENTRY XSC_STAGE OUTPUT)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "Required variable not set: ${_v}")
    endif()
endforeach()

execute_process(
    COMMAND "${XSC}" -o "${OUTPUT}" -Vout VKSL450 --extension ON -AB ON
            -E "${ENTRY}" -T "${XSC_STAGE}" -Xopaque-struct ON "${SHADER}"
    RESULT_VARIABLE XSC_RESULT
    OUTPUT_VARIABLE XSC_OUT
    ERROR_VARIABLE XSC_ERR
)

if(NOT XSC_RESULT EQUAL 0 OR NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "xsc failed to generate assertion input:\n${XSC_OUT}\n${XSC_ERR}")
endif()

file(READ "${OUTPUT}" GENERATED_GLSL)

if(DEFINED EXPECT_REGEX AND NOT GENERATED_GLSL MATCHES "${EXPECT_REGEX}")
    message(FATAL_ERROR "generated GLSL did not match /${EXPECT_REGEX}/:\n${GENERATED_GLSL}")
endif()

if(DEFINED REJECT_REGEX AND GENERATED_GLSL MATCHES "${REJECT_REGEX}")
    message(FATAL_ERROR "generated GLSL unexpectedly matched /${REJECT_REGEX}/:\n${GENERATED_GLSL}")
endif()

message(STATUS "Generated GLSL assertion OK: ${SHADER}")
