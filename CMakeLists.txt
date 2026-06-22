
# === CMake lists for the XShaderCompiler - (09/10/2014) ===

cmake_minimum_required(VERSION 3.15)
project(XShaderCompiler)

# Pin the MSVC runtime to the dynamic CRT (/MD, /MDd for Debug) for ALL targets. xsc_core
# and any separately-loaded backend DLL (xsc_backend_*) must share one CRT so that
# std::string/vector/function/unique_ptr can cross the DLL boundary on a single heap. Requires
# CMP0091 (NEW since CMake 3.15, implied by the minimum above).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")


# === Build path ===

set(BUILD_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/build")
set(EXECUTABLE_OUTPUT_PATH ${BUILD_OUTPUT_PATH} CACHE PATH "Build directory" FORCE)
set(LIBRARY_OUTPUT_PATH ${BUILD_OUTPUT_PATH} CACHE PATH "Build directory" FORCE)
set(INSTALL_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/install" CACHE PATH "Installation directory")


# === Options ===

# Main
option(XSC_BUILD_SHELL "Build XShaderCompiler shell 'xsc'" OFF)
option(XSC_BUILD_DEBUGGER "Build XShaderCompiler debugger 'xsc_debugger' (requires wxWidgets library)" OFF)
option(XSC_BUILD_TESTS "Build all test applications" OFF)
option(XSC_BUILD_HLSL "Build the HLSL output backend" OFF)
option(XSC_BUILD_HLSL_ROUNDTRIP_TESTS "Register CTest cases that round-trip HLSL test shaders through xsc and validate the output with fxc.exe (Windows only)" OFF)
option(XSC_BUILD_GLSL_ROUNDTRIP_TESTS "Register CTest cases that round-trip test shaders through xsc -Vout VKSL450 and validate the output to SPIR-V with glslangValidator" OFF)
option(XSC_SHARED_LIB "Build XShaderCompiler as a shared library instead of a static library" OFF)

# Wrappers
option(XSC_BUILD_WRAPPER_C "Build C wrapper (ISO C99)" OFF)
option(XSC_BUILD_WRAPPER_CSHARP "Build C# wrapper" OFF)

# Specials
option(XSC_ENABLE_EASTER_EGGS "Enables little easter eggs" OFF)
option(XSC_ENABLE_POST_VALIDATION "Enables the post validation in presettings with 'glslangValidator'" OFF)
option(XSC_ENABLE_LANGUAGE_EXT "Enables a few language extensions (e.g. 'space' attribute for stronger type system)" ON)
option(XSC_ENABLE_SPIRV "Enables all SPIR-V related features (experimental; requires submodule in 'external/SPIR-V')" OFF)

#set(XSC_REPORT_LANGUAGE "EN" CACHE STRING "Language of the report output (supported values: 'EN')")


# === Preprocessor definitions ===

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if(XSC_SHARED_LIB)
	add_definitions(-DXSC_SHARED_LIB)
endif()

if(XSC_ENABLE_EASTER_EGGS)
	add_definitions(-DXSC_ENABLE_EASTER_EGGS)
endif()

if(XSC_ENABLE_POST_VALIDATION)
	add_definitions(-DXSC_ENABLE_POST_VALIDATION)
endif()

if(XSC_ENABLE_LANGUAGE_EXT)
    add_definitions(-DXSC_ENABLE_LANGUAGE_EXT)
endif()

if(XSC_ENABLE_SPIRV)
	add_definitions(-DXSC_ENABLE_SPIRV)
endif()


# === Global files ===

file(GLOB FilesInc ${PROJECT_SOURCE_DIR}/inc/Xsc/*.*)
file(GLOB FilesIncC ${PROJECT_SOURCE_DIR}/inc/XscC/*.*)
file(GLOB FilesSrcCompiler ${PROJECT_SOURCE_DIR}/src/Compiler/*.*)
file(GLOB FilesSrcCompilerAST ${PROJECT_SOURCE_DIR}/src/Compiler/AST/*.*)
file(GLOB FilesSrcCompilerASTVisitor ${PROJECT_SOURCE_DIR}/src/Compiler/AST/Visitor/*.*)
file(GLOB FilesSrcCompilerFrontend ${PROJECT_SOURCE_DIR}/src/Compiler/Frontend/*.*)
file(GLOB FilesSrcCompilerFrontendHLSL ${PROJECT_SOURCE_DIR}/src/Compiler/Frontend/HLSL/*.*)
file(GLOB FilesSrcCompilerFrontendGLSL ${PROJECT_SOURCE_DIR}/src/Compiler/Frontend/GLSL/*.*)
file(GLOB FilesSrcCompilerBackend ${PROJECT_SOURCE_DIR}/src/Compiler/Backend/*.*)
file(GLOB FilesSrcCompilerReport ${PROJECT_SOURCE_DIR}/src/Compiler/Report/*.*)
file(GLOB FilesSrcCompilerCFG ${PROJECT_SOURCE_DIR}/src/Compiler/CFG/*.*)
file(GLOB FilesSrcShell ${PROJECT_SOURCE_DIR}/src/Shell/*.*)
file(GLOB FilesSrcDebugger ${PROJECT_SOURCE_DIR}/src/Debugger/*.*)
file(GLOB FilesSrcWrapperC ${PROJECT_SOURCE_DIR}/src/Wrapper/C/*.*)
#file(GLOB FilesSrcWrapperCSharp ${PROJECT_SOURCE_DIR}/src/Wrapper/CSharp/*.*)

set(FilesTest ${PROJECT_SOURCE_DIR}/test)

if(WIN32)
	file(GLOB FilesSrcCompilerPlatform ${PROJECT_SOURCE_DIR}/src/Compiler/Platform/Win32/*.*)
else()
	file(GLOB FilesSrcCompilerPlatform ${PROJECT_SOURCE_DIR}/src/Compiler/Platform/Unix/*.*)
endif()

set(
	FilesCompilerAll
	${FilesInc}
	${FilesSrcCompiler}
	${FilesSrcCompilerAST}
	${FilesSrcCompilerASTVisitor}
	${FilesSrcCompilerPlatform}
	${FilesSrcCompilerFrontend}
	${FilesSrcCompilerFrontendHLSL}
	${FilesSrcCompilerFrontendGLSL}
	${FilesSrcCompilerBackend}
	${FilesSrcCompilerReport}
)

if(XSC_ENABLE_SPIRV)
	set(FilesCompilerAll ${FilesCompilerAll} ${FilesSrcCompilerCFG})
endif()


# === Source group folders ===

source_group("inc\\Xsc" FILES ${FilesInc})
source_group("inc\\XscC" FILES ${FilesIncC})
source_group("src" FILES ${FilesSrcCompiler} ${FilesSrcShell} ${FilesSrcDebugger})
source_group("src\\AST" FILES ${FilesSrcCompilerAST})
source_group("src\\AST\\Visitor" FILES ${FilesSrcCompilerASTVisitor})
source_group("src\\Platform" FILES ${FilesSrcCompilerPlatform})
source_group("src\\Frontend" FILES ${FilesSrcCompilerFrontend})
source_group("src\\Frontend\\HLSL" FILES ${FilesSrcCompilerFrontendHLSL})
source_group("src\\Frontend\\GLSL" FILES ${FilesSrcCompilerFrontendGLSL})
source_group("src\\Backend" FILES ${FilesSrcCompilerBackend})
source_group("src\\Report" FILES ${FilesSrcCompilerReport})

if(XSC_ENABLE_SPIRV)
	source_group("src\\CFG" FILES ${FilesSrcCompilerCFG})
endif()


# === Include directories ===

include_directories("${PROJECT_SOURCE_DIR}/inc")

include_directories("${PROJECT_SOURCE_DIR}/src/Compiler")
include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/AST")
include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/AST/Visitor")
include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/Frontend")
include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/Frontend/HLSL")
include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/Frontend/GLSL")
include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/Backend")
include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/Report")

# Per-backend include directories (Backend/GLSL, Backend/HLSL, ...) are added to
# xsc_core by each backend's own CMakeLists.txt.

if(XSC_ENABLE_SPIRV)
	include_directories("${PROJECT_SOURCE_DIR}/src/Compiler/CFG")
	include_directories("${PROJECT_SOURCE_DIR}/external/SPIR-V/include")
endif()

# === Compile flags ===
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# === Executable ===

# Library core
if(XSC_SHARED_LIB)
	add_library(xsc_core SHARED ${FilesCompilerAll})
	# Marks this TU set as "building the core DLL" so the XSC_EXPORT/XSC_DATA macros dllexport rather than dllimport (see inc/Xsc/Export.h).
	target_compile_definitions(xsc_core PRIVATE XSC_BUILDING_CORE)
	if(MSVC)
		# The exported emitter classes hold non-exported members (std::vector/map, CodeWriter,
		# ReportHandler, ...) and, internally, sit atop non-exported helper bases; C4251/C4275
		# ("needs dll-interface") are benign here because every consumer is first-party and
		# shares these headers + the one dynamic CRT. Silence the noise.
		target_compile_options(xsc_core PRIVATE /wd4251 /wd4275)
	endif()
else()
	add_library(xsc_core STATIC ${FilesCompilerAll})
endif()

set_target_properties(xsc_core PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(xsc_core PRIVATE cxx_range_for)

# Dynamic loader, for Xsc::LoadBackend (BackendLoader.cpp). Empty on Windows, '-ldl' on Unix.
target_link_libraries(xsc_core ${CMAKE_DL_LIBS})

set(XSC_INSTALL_TARGETS "xsc_core")

# Shell application
if(XSC_BUILD_SHELL)
	add_executable(xsc ${FilesSrcShell})
	set_target_properties(xsc PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(xsc xsc_core)
	target_compile_features(xsc PRIVATE cxx_range_for)
	set(XSC_INSTALL_TARGETS ${XSC_INSTALL_TARGETS} "xsc")
endif()

# Debugger UI aplication
if(XSC_BUILD_DEBUGGER)
	if(WIN32)
		add_executable(xsc_debugger WIN32 ${FilesSrcDebugger} "${PROJECT_SOURCE_DIR}/src/Debugger/Resources.rc")
	else()
		add_executable(xsc_debugger ${FilesSrcDebugger})
	endif()
	
	set_target_properties(xsc_debugger PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(xsc_debugger xsc_core)
	target_compile_features(xsc_debugger PRIVATE cxx_range_for)
	
	# wxWidgets Libs
	find_package(wxWidgets REQUIRED adv aui base core stc propgrid richtext html xml scintilla)
	if(wxWidgets_FOUND)
		message("Found wxWidgets library")
		include_directories(${wxWidgets_INCLUDE_DIRS})
		target_link_libraries(xsc_debugger ${wxWidgets_LIBRARIES})
	else(wxWidgets_FOUND)
		message(SEND_ERROR "Missing wxWidgets library")
	endif(wxWidgets_FOUND)
	
	set(XSC_INSTALL_TARGETS ${XSC_INSTALL_TARGETS} "xsc_debugger")
endif()

# C wrapper
if(XSC_BUILD_WRAPPER_C)
	if(XSC_SHARED_LIB)
		add_library(xsc_core_c SHARED ${FilesSrcWrapperC})
	else()
		add_library(xsc_core_c STATIC ${FilesSrcWrapperC})
	endif()
	
	set_target_properties(xsc_core_c PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(xsc_core_c xsc_core)
	target_compile_features(xsc_core_c PRIVATE cxx_range_for)

	# The C wrapper exports ITS OWN C API (XSCC_EXPORT). Target-private so consumers of the
	# wrapper headers import rather than export (see inc/XscC/ExportC.h).
	if(XSC_SHARED_LIB)
		target_compile_definitions(xsc_core_c PRIVATE XSCC_BUILDING_WRAPPER)
	endif()
	
	set(XSC_INSTALL_TARGETS ${XSC_INSTALL_TARGETS} "xsc_core_c")
endif()

# C# wrapper
if(WIN32 AND XSC_BUILD_WRAPPER_CSHARP)
	add_subdirectory(src/Wrapper/CSharp)
endif()

if(XSC_BUILD_TESTS)
	# Test C wrapper
	if(XSC_BUILD_WRAPPER_C)
		add_executable(XscTest_CWrapper "${FilesTest}/XscTest_CWrapper.c")
		set_target_properties(XscTest_CWrapper PROPERTIES LINKER_LANGUAGE C)
		target_link_libraries(XscTest_CWrapper xsc_core_c)
	endif()

	# Enable CTest at the root, before the backend folders below register their round-trip
	# tests. enable_testing() propagates to all subdirectories, so a single top-level `ctest`
	# (or the IDE's RUN_TESTS target) discovers every backend's tests. Without a root-level
	# call, testing is only enabled inside each leaf backend dir and `ctest` at the build root
	# finds nothing -- reporting success while silently running zero tests.
	enable_testing()
endif()

# === Output backends ===
# Each output backend lives in its own folder under src/Compiler/Backend/ and owns
# its build logic in a local CMakeLists.txt (sources, include dir, defines, and its
# round-trip tests). The core never names a backend — it just adds every backend
# folder that provides a CMakeLists.txt. This is how optional backends
# plug in without touching the core build. Added here, after the shell and
# wrappers, so each backend can wire its round-trip tests against the 'xsc' target.
file(GLOB XSC_BACKEND_DIRS LIST_DIRECTORIES true "${PROJECT_SOURCE_DIR}/src/Compiler/Backend/*")
list(SORT XSC_BACKEND_DIRS)
foreach(_backendDir ${XSC_BACKEND_DIRS})
	if(EXISTS "${_backendDir}/CMakeLists.txt")
		add_subdirectory(${_backendDir})
	endif()
endforeach()


# === Generated backend registration ===
#
# Each backend folder enrolled its registrar in the XSC_BACKEND_REGISTRARS global
# property. Generate the body of RegisterBackends() -- the single, generically named
# entry point the core calls once on first use -- so it invokes exactly the backends
# compiled into this build. The core only ever references RegisterBackends(); this
# generated file is the only place the concrete backends are named, produced fresh
# from whatever folders are present (an open checkout without an optional backend
# names nothing of it). The function body is empty when no backend enrolled.
get_property(_xscRegistrars GLOBAL PROPERTY XSC_BACKEND_REGISTRARS)

set(_xscRegDecls "")
set(_xscRegCalls "")
foreach(_reg IN LISTS _xscRegistrars)
	string(APPEND _xscRegDecls "void ${_reg}();\n")
	string(APPEND _xscRegCalls "    ${_reg}();\n")
endforeach()

set(_xscGeneratedFile "${CMAKE_CURRENT_BINARY_DIR}/generated/BackendRegistrations.generated.cpp")
file(GENERATE
	OUTPUT  "${_xscGeneratedFile}"
	CONTENT "// Generated by CMake -- do not edit.\n//\n// Defines Xsc::RegisterBackends(), which registers every output backend compiled\n// into this build. Assembled from the backend folders present at configure time.\n\nnamespace Xsc\n{\n\n${_xscRegDecls}\nvoid RegisterBackends()\n{\n${_xscRegCalls}}\n\n} // /namespace Xsc\n"
)

target_sources(xsc_core PRIVATE "${_xscGeneratedFile}")
source_group("src\\Backend\\Generated" FILES "${_xscGeneratedFile}")


# === Show summary ===

message("~~~ Build Summary ~~~")

if(XSC_SHARED_LIB)
	message("Build Core: xsc_core (SHARED)")
else()
	message("Build Core: xsc_core (STATIC)")
endif()

if(XSC_BUILD_SHELL)
	message("Build Shell: xsc")
endif()

if(XSC_BUILD_DEBUGGER)
	message("Build Debugger: xsc_debugger")
endif()

if(XSC_BUILD_WRAPPER_C)
	message("Build C Wrapper: xsc_core_c")
endif()

if(XSC_BUILD_WRAPPER_CSHARP)
	message("Build C# Wrapper: xsc_core_csharp")
endif()

if(XSC_ENABLE_SPIRV)
	message("Include SPIR-V (Experimental)")
endif()


# === Installation Rules ===

install(
	TARGETS ${XSC_INSTALL_TARGETS}
	DESTINATION ${INSTALL_OUTPUT_PATH}
)

install(
	DIRECTORY inc/Xsc
	DESTINATION ${INSTALL_OUTPUT_PATH}/include
)

IF(MSVC)
	install(
		FILES ${BUILD_OUTPUT_PATH}/RelWithDebInfo/xsc_core.pdb
		DESTINATION ${INSTALL_OUTPUT_PATH}
		CONFIGURATIONS RelWithDebInfo
	)

	install(
		FILES ${BUILD_OUTPUT_PATH}/Debug/xsc_core.pdb
		DESTINATION ${INSTALL_OUTPUT_PATH}
		CONFIGURATIONS Debug
	)
ENDIF(MSVC)
