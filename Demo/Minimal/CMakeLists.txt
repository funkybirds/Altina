set(TargetName AltinaEngineDemoMinimal)

add_executable(${TargetName})

option(AE_DEMO_ENABLE_SHIPPING "Stage Demo/<Name>/Shipping from Demo/<Name>/Binaries (copies Assets + *.exe/*.dll only)" ON)

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
set(DemoShippingDir ${CMAKE_CURRENT_SOURCE_DIR}/Shipping)
set(DemoAssetCookOutput ${DemoBinaryDir}/Assets/Registry/AssetRegistry.json)
set(DemoAssetsRoot ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(DemoConfigRoot ${CMAKE_CURRENT_SOURCE_DIR}/Config)
set(DemoCookedRoot ${DemoBinaryDir}/Assets)
set(EngineShaderRoot ${CMAKE_SOURCE_DIR}/Source/Shader)
set(DemoStagedShaderRoot ${DemoBinaryDir}/Assets/Shader)
set(AE_DEMO_STAGE_SHIPPING_SCRIPT ${CMAKE_SOURCE_DIR}/Scripts/StageDemoShipping.cmake)
file(GLOB_RECURSE DemoAssetSources CONFIGURE_DEPENDS
    ${DemoAssetsRoot}/*
)
file(GLOB_RECURSE EngineShaderSources CONFIGURE_DEPENDS
    ${EngineShaderRoot}/*
)

# Clean demo output folders at the very start of a demo build (ignore if missing).
# This avoids stale files in Demo/<Name>/Binaries and Demo/<Name>/Shipping.
set(DemoPreCleanTarget ${TargetName}PreClean)
add_custom_target(${DemoPreCleanTarget}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DemoBinaryDir}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DemoShippingDir}"
    # Ensure the linker has a valid output directory even if other staging steps are up-to-date.
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DemoBinaryDir}"
    COMMENT "Cleaning Minimal demo output folders (Binaries/Shipping)"
    VERBATIM
)
set_property(TARGET ${DemoPreCleanTarget} PROPERTY FOLDER "Demo/Minimal")

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

set(DemoShaderStageStamp ${CMAKE_BINARY_DIR}/Demo/Minimal/StageShaders.stamp)
add_custom_command(
    OUTPUT ${DemoShaderStageStamp}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${EngineShaderRoot} ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E touch ${DemoShaderStageStamp}
    DEPENDS ${EngineShaderSources}
    COMMENT "Staging engine shaders for Minimal demo"
    VERBATIM
)
add_custom_target(AltinaEngineDemoMinimalStageShaders ALL
    DEPENDS ${DemoShaderStageStamp}
)
set_property(TARGET AltinaEngineDemoMinimalStageShaders PROPERTY FOLDER "Demo/Minimal")
add_dependencies(${TargetName} AltinaEngineDemoMinimalStageShaders)

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

if(AE_DEMO_ENABLE_SHIPPING)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging Minimal demo Shipping folder"
        VERBATIM
    )

    # Keep Shipping in sync even when only assets/shaders change (the executable may not relink,
    # so its POST_BUILD step would not run).
    add_custom_command(
        TARGET AltinaEngineDemoMinimalStageShaders
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging Minimal demo Shipping folder (shader changes)"
        VERBATIM
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

if(AE_DEMO_ENABLE_SHIPPING)
    add_custom_command(
        TARGET AltinaEngineDemoMinimalCookAssets
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging Minimal demo Shipping folder (asset changes)"
        VERBATIM
    )
endif()

add_dependencies(${TargetName} AltinaEngineDemoMinimalCookAssets)
