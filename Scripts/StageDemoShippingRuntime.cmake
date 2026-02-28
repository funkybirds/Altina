# Stage a demo "Shipping" folder from an already-staged demo "Binaries" folder.
#
# This variant copies runtime files recursively (including subdirectories) so that
# self-contained .NET publish outputs (e.g. `runtimes/`, data files) are preserved.
#
# Required cache/script variables:
#   -DDEMO_BIN_DIR=".../Demo/<Name>/Binaries"
#   -DDEMO_SHIP_DIR=".../Demo/<Name>/Shipping"
#
# Copies:
#   - DEMO_BIN_DIR/Assets -> DEMO_SHIP_DIR/Assets
#   - All other files/dirs under DEMO_BIN_DIR -> DEMO_SHIP_DIR (recursive)
#
# Excludes (best-effort):
#   - Debug artifacts: *.pdb, *.ilk, *.iobj, *.ipdb, *.exp, *.lib

if(NOT DEFINED DEMO_BIN_DIR)
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_BIN_DIR is not set")
endif()
if(NOT DEFINED DEMO_SHIP_DIR)
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_SHIP_DIR is not set")
endif()

cmake_path(NORMAL_PATH DEMO_BIN_DIR)
cmake_path(NORMAL_PATH DEMO_SHIP_DIR)

if(NOT EXISTS "${DEMO_BIN_DIR}")
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_BIN_DIR does not exist: '${DEMO_BIN_DIR}'")
endif()

file(MAKE_DIRECTORY "${DEMO_SHIP_DIR}")

# Assets (replace fully to avoid stale assets when the source tree changes).
if(EXISTS "${DEMO_BIN_DIR}/Assets")
    file(REMOVE_RECURSE "${DEMO_SHIP_DIR}/Assets")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEMO_SHIP_DIR}/Assets")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${DEMO_BIN_DIR}/Assets" "${DEMO_SHIP_DIR}/Assets"
    )
else()
    message(STATUS "StageDemoShippingRuntime: '${DEMO_BIN_DIR}/Assets' not found; skipping asset copy")
endif()

# Copy the rest of the runtime payload (recursive), excluding debug artifacts.
file(GLOB_RECURSE _bin_entries
    LIST_DIRECTORIES true
    RELATIVE "${DEMO_BIN_DIR}"
    "${DEMO_BIN_DIR}/*"
)

foreach(_rel IN LISTS _bin_entries)
    if(_rel STREQUAL "Assets" OR _rel MATCHES "^Assets/")
        continue()
    endif()

    # Skip common non-runtime artifacts.
    if(_rel MATCHES "\\.(pdb|ilk|iobj|ipdb|exp|lib)$")
        continue()
    endif()

    set(_src "${DEMO_BIN_DIR}/${_rel}")
    set(_dst "${DEMO_SHIP_DIR}/${_rel}")

    if(IS_DIRECTORY "${_src}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${_dst}")
    else()
        cmake_path(GET _dst PARENT_PATH _dst_parent)
        if(NOT _dst_parent STREQUAL "")
            execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${_dst_parent}")
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_src}" "${_dst}")
    endif()
endforeach()

message(STATUS "StageDemoShippingRuntime: staged to '${DEMO_SHIP_DIR}'")

