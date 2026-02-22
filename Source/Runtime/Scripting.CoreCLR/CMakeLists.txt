set(TargetName AltinaEngineScriptingCoreCLR)

add_library(${TargetName} SHARED)
add_library(AltinaEngine::Scripting::CoreCLR ALIAS ${TargetName})

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
        $<INSTALL_INTERFACE:include/AltinaEngine/Scripting.CoreCLR>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

target_link_libraries(${TargetName}
    PUBLIC
        AltinaEngine::Core
        AltinaEngine::Scripting
    PRIVATE
        AltinaEngine::Input
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${TargetName}
        PRIVATE
            dl
    )
endif()

target_compile_features(${TargetName}
    PUBLIC
        cxx_std_23
)

target_compile_definitions(${TargetName}
    PRIVATE
        AE_SCRIPTING_CORECLR_BUILD
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

find_program(AE_DOTNET_HOST dotnet)
if(AE_DOTNET_HOST)
    set(AE_MANAGED_ROOT ${CMAKE_SOURCE_DIR}/Source/Managed/AltinaEngine.Managed)
    set(AE_MANAGED_PROJECT ${AE_MANAGED_ROOT}/AltinaEngine.Managed.csproj)
    set(AE_MANAGED_OUT_DIR ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed)
    set(AE_MANAGED_DLL ${AE_MANAGED_OUT_DIR}/AltinaEngine.Managed.dll)
    set(AE_MANAGED_STAMP ${AE_MANAGED_OUT_DIR}/AltinaEngine.Managed.build.stamp)

    add_custom_command(
        OUTPUT ${AE_MANAGED_STAMP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${AE_MANAGED_OUT_DIR}
        COMMAND ${AE_DOTNET_HOST} build ${AE_MANAGED_PROJECT}
            -o ${AE_MANAGED_OUT_DIR}
            -c $<IF:$<CONFIG:Debug>,Debug,Release>
            /t:Rebuild
            /p:NoIncremental=true
            /p:MSBuildProjectExtensionsPath=${AE_MANAGED_OUT_DIR}/obj/
            /p:BaseIntermediateOutputPath=${AE_MANAGED_OUT_DIR}/obj/
            /p:IntermediateOutputPath=${AE_MANAGED_OUT_DIR}/obj/$<IF:$<CONFIG:Debug>,Debug,Release>/
        COMMAND ${CMAKE_COMMAND} -E touch ${AE_MANAGED_STAMP}
        DEPENDS
            ${AE_MANAGED_PROJECT}
            ${AE_MANAGED_ROOT}/ManagedBootstrap.cs
            ${AE_MANAGED_ROOT}/ManagedApi.cs
            ${AE_MANAGED_ROOT}/NativeApi.cs
            ${AE_MANAGED_ROOT}/ManagedLog.cs
            ${AE_MANAGED_ROOT}/ScriptComponent.cs
            ${AE_MANAGED_ROOT}/Input.cs
        BYPRODUCTS
            ${AE_MANAGED_OUT_DIR}/AltinaEngine.Managed.dll
            ${AE_MANAGED_OUT_DIR}/AltinaEngine.Managed.runtimeconfig.json
            ${AE_MANAGED_OUT_DIR}/AltinaEngine.Managed.deps.json
            ${AE_MANAGED_OUT_DIR}/AltinaEngine.Managed.pdb
        COMMENT "Building managed scripting runtime assembly"
        VERBATIM
    )

    add_custom_target(AltinaEngineManagedBuild
        DEPENDS ${AE_MANAGED_STAMP}
    )
    set_property(TARGET AltinaEngineManagedBuild PROPERTY BUILD_ALWAYS TRUE)

    add_dependencies(${TargetName} AltinaEngineManagedBuild)
else()
    message(STATUS "dotnet not found; skipping managed scripting runtime build.")
endif()

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
        OUTPUT_NAME AltinaEngine.Scripting.CoreCLR
        FOLDER "Engine/Scripting.CoreCLR"
)
