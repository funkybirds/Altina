# Stage a demo "Shipping" folder from an already-staged demo "Binaries" folder.
#
# Required cache/script variables:
#   -DDEMO_BIN_DIR=".../Demo/<Name>/Binaries"
#   -DDEMO_SHIP_DIR=".../Demo/<Name>/Shipping"
#
# Copies:
#   - DEMO_BIN_DIR/Assets  -> DEMO_SHIP_DIR/Assets
#   - DEMO_BIN_DIR/*.exe   -> DEMO_SHIP_DIR
#   - DEMO_BIN_DIR/*.dll   -> DEMO_SHIP_DIR
#
# Intentionally ignores .pdb and other non-runtime artifacts.

if(NOT DEFINED DEMO_BIN_DIR)
    message(FATAL_ERROR "StageDemoShipping.cmake: DEMO_BIN_DIR is not set")
endif()
if(NOT DEFINED DEMO_SHIP_DIR)
    message(FATAL_ERROR "StageDemoShipping.cmake: DEMO_SHIP_DIR is not set")
endif()

cmake_path(NORMAL_PATH DEMO_BIN_DIR)
cmake_path(NORMAL_PATH DEMO_SHIP_DIR)

file(MAKE_DIRECTORY "${DEMO_SHIP_DIR}")

# Assets
if(EXISTS "${DEMO_BIN_DIR}/Assets")
    # Avoid stale assets when the source tree changes.
    file(REMOVE_RECURSE "${DEMO_SHIP_DIR}/Assets")
    file(COPY "${DEMO_BIN_DIR}/Assets" DESTINATION "${DEMO_SHIP_DIR}")
else()
    message(STATUS "StageDemoShipping: '${DEMO_BIN_DIR}/Assets' not found; skipping asset copy")
endif()

# Runtime binaries (Windows: .exe + .dll)
file(GLOB _shipping_files
    LIST_DIRECTORIES false
    "${DEMO_BIN_DIR}/*.exe"
    "${DEMO_BIN_DIR}/*.dll"
)

foreach(_f IN LISTS _shipping_files)
    file(COPY "${_f}" DESTINATION "${DEMO_SHIP_DIR}")
endforeach()

message(STATUS "StageDemoShipping: staged to '${DEMO_SHIP_DIR}'")

