set(TargetName AltinaEngineAsset)

add_library(${TargetName} SHARED)
add_library(AltinaEngine::Asset ALIAS ${TargetName})

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
        $<INSTALL_INTERFACE:include/AltinaEngine/Asset>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

target_link_libraries(${TargetName}
    PUBLIC
        AltinaEngine::Core
)

target_compile_definitions(${TargetName}
    PRIVATE
        AE_ASSET_BUILD
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

set(AE_TEST_BIN_DIR ${CMAKE_BINARY_DIR}/Source/Tests)

add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${AE_TEST_BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${AE_TEST_BIN_DIR}
    COMMENT "Copying ${TargetName} to test binaries"
)

set_target_properties(${TargetName}
    PROPERTIES
        OUTPUT_NAME AltinaEngine.Asset
        FOLDER "Engine/Asset"
)

ae_add_reflection_codegen(TARGET ${TargetName})
