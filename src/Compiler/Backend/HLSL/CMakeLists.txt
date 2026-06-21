# === HLSL output backend ===
#
# The HLSL emitter doubles as the base class that derived output backends extend,
# so its sources are always compiled into xsc_core. Whether "HLSL5"/"HLSL" are
# actually offered as output targets is gated by XSC_BUILD_HLSL, which enrolls the
# HLSL registrar with the CMake-generated RegisterBackends() dispatcher.

file(GLOB FilesBackendHLSL
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.h)

target_sources(xsc_core PRIVATE ${FilesBackendHLSL})
target_include_directories(xsc_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

source_group("src\\Backend\\HLSL" FILES ${FilesBackendHLSL})

if(XSC_BUILD_HLSL)
	# Offer HLSL5/HLSL as output targets by enrolling the HLSL registrar with the
	# generated RegisterBackends() dispatcher. (The emitter sources themselves are
	# always compiled above because derived backends extend them.)
	set_property(GLOBAL APPEND PROPERTY XSC_BACKEND_REGISTRARS RegisterBackend_HLSL)

	# HLSL round-trip tests (xsc HLSL->HLSL validated with fxc.exe; Windows only).
	if(XSC_BUILD_HLSL_ROUNDTRIP_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/HLSLTests.cmake")
		include("${CMAKE_CURRENT_SOURCE_DIR}/tests/HLSLTests.cmake")
	endif()
endif()
