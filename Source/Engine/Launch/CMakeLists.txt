set(TargetName AltinaEngineLaunch)

add_library(${TargetName} SHARED)
add_library(AltinaEngine::Launch ALIAS ${TargetName})

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

if(NOT Target_Private_Sources)
    message(FATAL_ERROR "No private sources found for ${TargetName}")
endif()

if(NOT Target_Public_Headers)
    message(FATAL_ERROR "No public headers found for ${TargetName}")
endif()

target_sources(${TargetName}
    PUBLIC
        FILE_SET public_headers
            TYPE HEADERS
            BASE_DIRS Public
            FILES ${Target_Public_Headers}
    PRIVATE
        ${Target_Private_Sources}
)

target_include_directories(${TargetName}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Public>
        $<INSTALL_INTERFACE:include/AltinaEngine/Launch>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

target_link_libraries(${TargetName}
    PUBLIC
        AltinaEngine::Core
        AltinaEngine::Asset
        AltinaEngine::Application
        AltinaEngine::Input
        AltinaEngine::Rhi::General
)

if(WIN32)
    target_link_libraries(${TargetName}
        PRIVATE
            AltinaEngine::Rhi::D3D11
    )
else()
    target_link_libraries(${TargetName}
        PRIVATE
            AltinaEngine::Rhi::Mock
    )
endif()

target_compile_features(${TargetName}
    PUBLIC
        cxx_std_23
)

target_compile_definitions(${TargetName}
    PRIVATE
        AE_LAUNCH_BUILD
)

set(Warnings
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14826 /w14905 /w14906 /w14928>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(${TargetName}
    PRIVATE
        ${Warnings}
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

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME AltinaEngineLaunch
        FOLDER "Engine/Launch"
)

ae_add_reflection_codegen(TARGET ${TargetName})
