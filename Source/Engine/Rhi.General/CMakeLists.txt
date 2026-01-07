set(TargetName AltinaEngineRhiGeneral)

add_library(${TargetName} SHARED)
add_library(AltinaEngine::Rhi::General ALIAS ${TargetName})

file(GLOB_RECURSE Target_Public_Headers
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h
)

file(GLOB_RECURSE Target_Private_Sources
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.mm
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
    PUBLIC
        FILE_SET public_headers TYPE HEADERS BASE_DIRS Public FILES
            ${Target_Public_Headers}
)

target_include_directories(${TargetName}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Public>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

target_link_libraries(${TargetName}
    PUBLIC
        AltinaEngine::Core
)

target_compile_definitions(${TargetName}
    PRIVATE
        AE_RHI_GENERAL_BUILD
)

set_target_properties(${TargetName} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

set(AE_DEMO_MINIMAL_BIN_DIR ${CMAKE_SOURCE_DIR}/Demo/Minimal/Binaries)
set(AE_TEST_BIN_DIR ${CMAKE_BINARY_DIR}/Source/Tests)

add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${AE_DEMO_MINIMAL_BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${AE_DEMO_MINIMAL_BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${AE_TEST_BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${AE_TEST_BIN_DIR}
    COMMENT "Copying ${TargetName} to consumer binaries"
)

set_target_properties(${TargetName} PROPERTIES
    OUTPUT_NAME AltinaEngineRhiGeneral
    FOLDER "Engine/Rhi.General"
)


