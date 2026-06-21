# === GLSL output backend ===
#
# GLSL is the always-on core backend (it produces GLSL, ESSL, and VKSL output).
# It contributes its sources and include directory to the xsc_core library and
# registers its own round-trip tests.

file(GLOB FilesBackendGLSL
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.h)

target_sources(xsc_core PRIVATE ${FilesBackendGLSL})
target_include_directories(xsc_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

source_group("src\\Backend\\GLSL" FILES ${FilesBackendGLSL})

# Enroll this backend's registrar with the CMake-generated RegisterBackends()
# dispatcher (offers the GLSL/ESSL/VKSL output targets).
set_property(GLOBAL APPEND PROPERTY XSC_BACKEND_REGISTRARS RegisterBackend_GLSL)

# GLSL round-trip tests (xsc -Vout VKSL450 -> SPIR-V via glslangValidator).
if(XSC_BUILD_GLSL_ROUNDTRIP_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/GLSLTests.cmake")
	include("${CMAKE_CURRENT_SOURCE_DIR}/tests/GLSLTests.cmake")
endif()
