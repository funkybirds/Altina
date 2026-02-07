set(TargetName AltinaEngineShaderCompiler)

add_library(${TargetName} SHARED)
add_library(AltinaEngine::ShaderCompiler ALIAS ${TargetName})

if(WIN32)
    option(AE_ENABLE_DXC "Enable DXC shader compiler backend" ON)
else()
    option(AE_ENABLE_DXC "Enable DXC shader compiler backend" OFF)
endif()
option(AE_ENABLE_SLANG "Enable Slang shader compiler backend" ON)

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
        $<INSTALL_INTERFACE:include/AltinaEngine/ShaderCompiler>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

target_link_libraries(${TargetName}
    PUBLIC
        AltinaEngine::Core
        AltinaEngine::Rhi::General
)

if(WIN32)
    target_link_libraries(${TargetName}
        PRIVATE
            d3dcompiler
            dxcompiler
            dxguid
    )
endif()

target_compile_definitions(${TargetName}
    PRIVATE
        AE_SHADER_COMPILER_BUILD
        AE_SHADER_COMPILER_ENABLE_DXC=$<BOOL:${AE_ENABLE_DXC}>
        AE_SHADER_COMPILER_ENABLE_SLANG=$<BOOL:${AE_ENABLE_SLANG}>
)

set_target_properties(${TargetName} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
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
        OUTPUT_NAME AltinaEngineShaderCompiler
        FOLDER "Engine/ShaderCompiler"
)
