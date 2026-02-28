# Stage a demo "Shipping" folder from an already-staged demo "Binaries" folder.
#
# This variant produces a clean, portable layout:
#   - <Shipping>/<DemoName>.exe
#   - <Shipping>/<DemoName>_Data/...
#
# Where the launcher exe starts the "real" demo executable inside the _Data folder.
# This keeps Shipping tidy while still allowing the inner executable to resolve DLLs
# and other runtime payload from its own directory.
#
# Required cache/script variables:
#   -DDEMO_BIN_DIR=".../Demo/<Name>/Binaries"
#   -DDEMO_SHIP_DIR=".../Demo/<Name>/Shipping"
#   -DDEMO_EXE_NAME="AltinaEngineDemoMinimal.exe" (file name only)
#   -DDEMO_LAUNCHER_EXE=".../AltinaEngineDemoLauncher.exe" (full path)
#
# Copies:
#   - DEMO_BIN_DIR/* -> DEMO_SHIP_DIR/<DemoName>_Data/* (recursive)
#   - DEMO_LAUNCHER_EXE -> DEMO_SHIP_DIR/<DemoName>.exe
#
# Excludes (best-effort):
#   - Debug artifacts: *.pdb, *.ilk, *.iobj, *.ipdb, *.exp, *.lib

if(NOT DEFINED DEMO_BIN_DIR)
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_BIN_DIR is not set")
endif()
if(NOT DEFINED DEMO_SHIP_DIR)
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_SHIP_DIR is not set")
endif()
if(NOT DEFINED DEMO_EXE_NAME)
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_EXE_NAME is not set")
endif()
if(NOT DEFINED DEMO_LAUNCHER_EXE)
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_LAUNCHER_EXE is not set")
endif()

cmake_path(NORMAL_PATH DEMO_BIN_DIR)
cmake_path(NORMAL_PATH DEMO_SHIP_DIR)

if(NOT EXISTS "${DEMO_BIN_DIR}")
    message(FATAL_ERROR "StageDemoShippingRuntime.cmake: DEMO_BIN_DIR does not exist: '${DEMO_BIN_DIR}'")
endif()

file(REMOVE_RECURSE "${DEMO_SHIP_DIR}")
file(MAKE_DIRECTORY "${DEMO_SHIP_DIR}")

cmake_path(GET DEMO_EXE_NAME STEM _demo_stem)
set(_data_dir_name "${_demo_stem}_Data")
set(DEMO_DATA_DIR "${DEMO_SHIP_DIR}/${_data_dir_name}")
file(MAKE_DIRECTORY "${DEMO_DATA_DIR}")

# Copy the runtime payload (recursive) into the data folder, excluding debug artifacts.
file(GLOB_RECURSE _bin_entries
    LIST_DIRECTORIES true
    RELATIVE "${DEMO_BIN_DIR}"
    "${DEMO_BIN_DIR}/*"
)

foreach(_rel IN LISTS _bin_entries)
    # Skip common non-runtime artifacts.
    if(_rel MATCHES "\\.(pdb|ilk|iobj|ipdb|exp|lib)$")
        continue()
    endif()

    set(_src "${DEMO_BIN_DIR}/${_rel}")
    set(_dst "${DEMO_DATA_DIR}/${_rel}")

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

set(_launcher_dst "${DEMO_SHIP_DIR}/${DEMO_EXE_NAME}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${DEMO_LAUNCHER_EXE}" "${_launcher_dst}"
    RESULT_VARIABLE _launcher_copy_result
)
if(NOT _launcher_copy_result EQUAL 0)
    message(FATAL_ERROR "StageDemoShippingRuntime: failed to stage launcher '${DEMO_LAUNCHER_EXE}' -> '${_launcher_dst}' (code=${_launcher_copy_result})")
endif()

# --- Optional external tools -------------------------------------------------
#
# The engine currently launches shader compilers as external processes by default
# (e.g. `dxc.exe`, `slangc.exe`). To keep the demo folder portable, try to stage
# these tools next to the executable by default.
#
# If the tools are not found, we keep staging successful and just print a status
# message so developers can decide whether to install the toolchain or override
# compiler paths at runtime.

function(ae_stage_tool_from_path tool_display_name tool_exe_path)
    if(NOT tool_exe_path)
        message(STATUS "StageDemoShippingRuntime: ${tool_display_name} not found; skipping tool staging.")
        return()
    endif()
    if(NOT EXISTS "${tool_exe_path}")
        message(STATUS "StageDemoShippingRuntime: ${tool_display_name} path does not exist ('${tool_exe_path}'); skipping tool staging.")
        return()
    endif()

    get_filename_component(_tool_dir "${tool_exe_path}" DIRECTORY)

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${tool_exe_path}" "${DEMO_DATA_DIR}"
        RESULT_VARIABLE _copy_exe_result
    )
    if(NOT _copy_exe_result EQUAL 0)
        message(STATUS "StageDemoShippingRuntime: failed to copy ${tool_display_name} exe from '${tool_exe_path}' (code=${_copy_exe_result}).")
        return()
    endif()

    # Copy all DLLs next to the tool (best-effort, excludes debug artifacts by extension).
    file(GLOB _tool_dlls LIST_DIRECTORIES false "${_tool_dir}/*.dll")
    foreach(_dll IN LISTS _tool_dlls)
        if(_dll MATCHES "\\.pdb$")
            continue()
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_dll}" "${DEMO_DATA_DIR}"
            RESULT_VARIABLE _copy_dll_result
        )
        if(NOT _copy_dll_result EQUAL 0)
            message(STATUS "StageDemoShippingRuntime: failed to copy '${_dll}' for ${tool_display_name} (code=${_copy_dll_result}).")
        endif()
    endforeach()

    message(STATUS "StageDemoShippingRuntime: staged ${tool_display_name} from '${_tool_dir}'")
endfunction()

# Default staging for shader compilers used by Runtime/ShaderCompiler.
if(WIN32)
    find_program(AE_DXC_EXE NAMES dxc.exe)
    find_program(AE_SLANGC_EXE NAMES slangc.exe)

    ae_stage_tool_from_path("DXC" "${AE_DXC_EXE}")
    ae_stage_tool_from_path("Slang" "${AE_SLANGC_EXE}")
endif()

message(STATUS "StageDemoShippingRuntime: staged to '${DEMO_SHIP_DIR}' (data='${_data_dir_name}')")
