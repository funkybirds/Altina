set(TargetName AltinaEngineDemoMinimal)

add_executable(${TargetName})

set(Target_Private_Sources
    Source/Main.cpp
)

target_sources(${TargetName}
    PRIVATE
        ${Target_Private_Sources}
)

target_link_libraries(${TargetName}
    PRIVATE
        AltinaEngine::Launch
        AltinaEngine::ShaderCompiler
        AltinaEngine::Gameplay
)

if(WIN32)
    target_link_libraries(${TargetName}
        PRIVATE
            AltinaEngine::Rhi::D3D11
            d3dcompiler
            dxguid
    )
endif()

target_compile_features(${TargetName}
    PRIVATE
        cxx_std_23
)

set(DemoBinaryDir ${CMAKE_SOURCE_DIR}/Demo/Minimal/Binaries)
set(DemoAssetCookOutput ${DemoBinaryDir}/Assets/Registry/AssetRegistry.json)
set(DemoAssetsRoot ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(DemoConfigRoot ${CMAKE_CURRENT_SOURCE_DIR}/Config)
set(DemoCookedRoot ${DemoBinaryDir}/Assets)
file(GLOB_RECURSE DemoAssetSources CONFIGURE_DEPENDS
    ${DemoAssetsRoot}/*
)

set_target_properties(${TargetName}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${DemoBinaryDir}
        FOLDER "Demo/Minimal"
)

add_custom_command(
    TARGET ${TargetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${TargetName}>
        ${DemoBinaryDir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}/Assets/Config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${DemoConfigRoot} ${DemoBinaryDir}/Assets/Config
    COMMENT "Staging Minimal demo executable"
)

if(WIN32)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${TargetName}>
            ${DemoBinaryDir}
        COMMAND_EXPAND_LISTS
        COMMENT "Staging Minimal demo runtime DLLs"
    )
endif()

find_program(AE_DOTNET_HOST dotnet)
if(AE_DOTNET_HOST)
    set(AE_DEMO_MANAGED_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Source/Managed/AltinaEngine.Demo.Minimal)
    set(AE_DEMO_MANAGED_PROJECT ${AE_DEMO_MANAGED_ROOT}/AltinaEngine.Demo.Minimal.csproj)
    set(AE_DEMO_MANAGED_OUT_DIR ${CMAKE_BINARY_DIR}/Demo/Minimal/Managed)
    set(AE_DEMO_MANAGED_STAMP ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.build.stamp)

    add_custom_command(
        OUTPUT ${AE_DEMO_MANAGED_STAMP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${AE_DEMO_MANAGED_OUT_DIR}
        COMMAND ${AE_DOTNET_HOST} build ${AE_DEMO_MANAGED_PROJECT}
            -o ${AE_DEMO_MANAGED_OUT_DIR}
            -c $<IF:$<CONFIG:Debug>,Debug,Release>
            /t:Rebuild
            /p:NoIncremental=true
            /p:MSBuildProjectExtensionsPath=${AE_DEMO_MANAGED_OUT_DIR}/obj/
            /p:BaseIntermediateOutputPath=${AE_DEMO_MANAGED_OUT_DIR}/obj/
            /p:IntermediateOutputPath=${AE_DEMO_MANAGED_OUT_DIR}/obj/$<IF:$<CONFIG:Debug>,Debug,Release>/
        COMMAND ${CMAKE_COMMAND} -E touch ${AE_DEMO_MANAGED_STAMP}
        DEPENDS
            ${AE_DEMO_MANAGED_PROJECT}
            ${AE_DEMO_MANAGED_ROOT}/DemoScript.cs
            ${CMAKE_SOURCE_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.csproj
            ${CMAKE_SOURCE_DIR}/Source/Managed/AltinaEngine.Managed/ManagedLog.cs
        BYPRODUCTS
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.dll
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.runtimeconfig.json
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.deps.json
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.pdb
        COMMENT "Building Minimal demo managed scripting assembly"
        VERBATIM
    )

    add_custom_target(AltinaEngineDemoMinimalManagedBuild
        DEPENDS ${AE_DEMO_MANAGED_STAMP}
    )
    set_property(TARGET AltinaEngineDemoMinimalManagedBuild PROPERTY BUILD_ALWAYS TRUE)
    add_dependencies(${TargetName} AltinaEngineDemoMinimalManagedBuild)
    if(TARGET AltinaEngineManagedBuild)
        add_dependencies(AltinaEngineDemoMinimalManagedBuild AltinaEngineManagedBuild)
    endif()

    add_custom_command(
        TARGET AltinaEngineDemoMinimalManagedBuild
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.dll
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.runtimeconfig.json
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.deps.json
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.Minimal.pdb
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.dll
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.runtimeconfig.json
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.deps.json
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.pdb
            ${DemoBinaryDir}
        COMMENT "Staging Minimal demo managed scripting assembly"
    )
else()
    message(STATUS "dotnet not found; skipping Minimal demo managed build.")
endif()

add_custom_command(
    OUTPUT ${DemoAssetCookOutput}
    COMMAND $<TARGET_FILE:AltinaEngineAssetTool> cook
        --root ${CMAKE_SOURCE_DIR}
        --platform Win64
        --demo Minimal
        --build-root ${CMAKE_BINARY_DIR}
        --cook-root ${DemoCookedRoot}
    DEPENDS
        AltinaEngineAssetTool
        ${DemoAssetSources}
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Scripts/DemoScript.script
    COMMENT "Cooking Minimal demo assets"
    VERBATIM
)

add_custom_target(AltinaEngineDemoMinimalCookAssets
    DEPENDS ${DemoAssetCookOutput}
)

add_dependencies(${TargetName} AltinaEngineDemoMinimalCookAssets)
