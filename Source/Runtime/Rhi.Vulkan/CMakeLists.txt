set(TargetName AltinaEngineRhiVulkan)

add_library(${TargetName} SHARED)
add_library(AltinaEngine::Rhi::Vulkan ALIAS ${TargetName})

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
        AltinaEngine::Rhi::General
    PRIVATE
        AltinaEngine::Shader
)

find_package(Vulkan)
if (Vulkan_FOUND)
    target_link_libraries(${TargetName} PRIVATE Vulkan::Vulkan)
    target_compile_definitions(${TargetName} PRIVATE AE_RHI_VULKAN_AVAILABLE=1)
    if (WIN32)
        target_compile_definitions(${TargetName} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    endif()
else()
    target_compile_definitions(${TargetName} PRIVATE AE_RHI_VULKAN_AVAILABLE=0)
endif()

target_compile_definitions(${TargetName}
    PRIVATE
        AE_RHI_VULKAN_BUILD
)

set_target_properties(${TargetName} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
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

set_target_properties(${TargetName} PROPERTIES
    OUTPUT_NAME AltinaEngine.Rhi.Vulkan
    FOLDER "Engine/Rhi.Vulkan"
)

ae_add_reflection_codegen(TARGET ${TargetName})
