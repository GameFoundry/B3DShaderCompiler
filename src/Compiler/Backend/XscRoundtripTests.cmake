# XscRoundtripTests.cmake
#
# Shared CTest registration helper for the output-backend round-trip suites.
#
# Every backend (HLSL, GLSL, ...) registers batches of round-trip cases
# that share the same skeleton: split a pipe-delimited
#   "<shader>|<entry>|[<profile>|]<stage>[|<extra>]"
# row, then emit one `add_test` that runs the backend's per-case driver script
# via `cmake -P`, then tag the test with LABELS. The only things that actually
# differ between batches are the test-name prefix, the driver, the validator
# executable, the shader folder, the labels, and any extra xsc flags. This
# function captures the skeleton so each backend test file only declares its
# case list plus those few values.
#
# Multi-token xsc flags (e.g. `-Xopaque-struct ON`, which is two argv tokens)
# are written with '@' as the intra-flag separator everywhere a flag is passed
# (EXTRA_FLAGS and the per-case trailing field). '@' is converted to ';' before
# the flag reaches xsc. This avoids CMake's list separator ';' splitting the
# value apart while it travels through cmake_parse_arguments / the case row.

include_guard(GLOBAL)

# xsc_add_roundtrip_tests(
#     PREFIX         <name>          (required) test-name prefix, e.g. hlsl_roundtrip
#     DRIVER         <path>          (required) the `-P` driver script (Run*.cmake)
#     SHADER_DIR     <dir>           (required) folder holding <shader>.hlsl
#     OUT_DIR        <dir>           (required) per-suite scratch output dir
#     LABELS         <labels>        (required) CTest LABELS applied to every case
#     CASES          <row> [<row>..] (required) pipe-delimited case rows
#     DEFINES        <-Dk=v> [...]   (optional) extra `-D` args forwarded to every
#                                    case verbatim (e.g. the validator exe path)
#     PROFILE_DEFINE <name>          (optional) when set, each row carries a profile
#                                    field at index 2, forwarded as -D<name>=<profile>;
#                                    the stage is then field 3 instead of field 2
#     EXTRA_FLAGS    <flags>         (optional) constant xsc flags for every case
#                                    ('@' -> ';', see header)
#     PER_CASE_FLAGS                 (optional) the row's trailing field carries
#                                    per-case xsc flags ('@' -> ';'); when present
#                                    and non-empty it overrides EXTRA_FLAGS
#
# Reads XSC_BIN (the xsc target file) from the caller's scope -- every suite sets
# it to "$<TARGET_FILE:xsc>".
function(xsc_add_roundtrip_tests)
    cmake_parse_arguments(_RT
        "PER_CASE_FLAGS"
        "PREFIX;DRIVER;SHADER_DIR;OUT_DIR;LABELS;PROFILE_DEFINE;EXTRA_FLAGS"
        "CASES;DEFINES"
        ${ARGN})

    foreach(_req PREFIX DRIVER SHADER_DIR OUT_DIR LABELS CASES)
        if(NOT _RT_${_req})
            message(FATAL_ERROR "xsc_add_roundtrip_tests: missing required ${_req}")
        endif()
    endforeach()

    foreach(_case IN LISTS _RT_CASES)
        string(REPLACE "|" ";" _parts "${_case}")
        list(GET _parts 0 _shader)
        list(GET _parts 1 _entry)

        # The profile is an optional field; when present it shifts the stage
        # (and the per-case flag field) one slot to the right.
        set(_profileArg "")
        if(_RT_PROFILE_DEFINE)
            list(GET _parts 2 _profile)
            list(GET _parts 3 _stage)
            set(_profileArg "-D${_RT_PROFILE_DEFINE}=${_profile}")
            set(_flagsIdx 4)
        else()
            list(GET _parts 2 _stage)
            set(_flagsIdx 3)
        endif()

        # Resolve this case's xsc flags: a non-empty per-case field wins over the
        # constant EXTRA_FLAGS. '@' -> ';' restores multi-token flags.
        string(REPLACE "@" ";" _flags "${_RT_EXTRA_FLAGS}")
        if(_RT_PER_CASE_FLAGS)
            list(LENGTH _parts _np)
            if(_np GREATER _flagsIdx)
                list(GET _parts ${_flagsIdx} _pcf)
                string(REPLACE "@" ";" _pcf "${_pcf}")
                if(NOT _pcf STREQUAL "")
                    set(_flags "${_pcf}")
                endif()
            endif()
        endif()

        set(_name "${_RT_PREFIX}.${_shader}.${_entry}")
        add_test(
            NAME    ${_name}
            COMMAND ${CMAKE_COMMAND}
                -DXSC=${XSC_BIN}
                ${_RT_DEFINES}
                -DSHADER=${_RT_SHADER_DIR}/${_shader}.hlsl
                -DENTRY=${_entry}
                ${_profileArg}
                -DXSC_STAGE=${_stage}
                -DOUT_DIR=${_RT_OUT_DIR}
                "-DXSC_EXTRA_FLAGS=${_flags}"
                -P ${_RT_DRIVER}
        )
        set_tests_properties(${_name} PROPERTIES LABELS "${_RT_LABELS}")
    endforeach()
endfunction()
