set(TargetName AltinaEngineDemoSpaceshipGame)

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

set(DemoBinaryDir ${CMAKE_SOURCE_DIR}/Demo/SpaceshipGame/Binaries)
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
    COMMENT "Cleaning SpaceshipGame demo output folders (Binaries/Shipping)"
    VERBATIM
)
set_property(TARGET ${DemoPreCleanTarget} PROPERTY FOLDER "Demo/SpaceshipGame")

set_target_properties(${TargetName}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${DemoBinaryDir}
        FOLDER "Demo/SpaceshipGame"
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
    COMMENT "Staging SpaceshipGame demo executable"
)

# Put the stamp under the demo output tree so the PreClean target removes it, forcing restaging.
set(DemoShaderStageStamp ${DemoBinaryDir}/Assets/Shader/StageShaders.stamp)
add_custom_command(
    OUTPUT ${DemoShaderStageStamp}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${EngineShaderRoot} ${DemoStagedShaderRoot}
    COMMAND ${CMAKE_COMMAND} -E touch ${DemoShaderStageStamp}
    DEPENDS ${EngineShaderSources}
    COMMENT "Staging engine shaders for SpaceshipGame demo"
    VERBATIM
)
add_custom_target(AltinaEngineDemoSpaceshipGameStageShaders ALL
    DEPENDS ${DemoShaderStageStamp}
)
set_property(TARGET AltinaEngineDemoSpaceshipGameStageShaders PROPERTY FOLDER "Demo/SpaceshipGame")
add_dependencies(${TargetName} AltinaEngineDemoSpaceshipGameStageShaders)

if(WIN32)
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${TargetName}>
            ${DemoBinaryDir}
        COMMAND_EXPAND_LISTS
        COMMENT "Staging SpaceshipGame demo runtime DLLs"
    )
endif()

if(TARGET AltinaEngineManagedBuild)
    add_dependencies(${TargetName} AltinaEngineManagedBuild)

    # The engine initializes CoreCLR by default (when enabled). Stage the managed runtime artifacts
    # next to the demo executable so hostfxr can find them.
    add_custom_command(
        TARGET ${TargetName}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
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
        COMMENT "Staging managed scripting runtime for SpaceshipGame demo"
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
        COMMENT "Staging SpaceshipGame demo Shipping folder"
        VERBATIM
    )

    # Keep Shipping in sync even when only assets/shaders change (the executable may not relink,
    # so its POST_BUILD step would not run).
    add_custom_command(
        TARGET AltinaEngineDemoSpaceshipGameStageShaders
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging SpaceshipGame demo Shipping folder (shader changes)"
        VERBATIM
    )
endif()

find_program(AE_DOTNET_HOST dotnet)
if(AE_DOTNET_HOST)
    set(AE_DEMO_MANAGED_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Source/Managed/AltinaEngine.Demo.SpaceshipGame)
    set(AE_DEMO_MANAGED_PROJECT ${AE_DEMO_MANAGED_ROOT}/AltinaEngine.Demo.SpaceshipGame.csproj)
    set(AE_DEMO_MANAGED_OUT_DIR ${CMAKE_BINARY_DIR}/Demo/SpaceshipGame/Managed)
    set(AE_DEMO_MANAGED_STAMP ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.build.stamp)

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
            ${AE_DEMO_MANAGED_ROOT}/FpsCameraController.cs
            ${AE_DEMO_MANAGED_ROOT}/ShipFreeMove.cs
            ${AE_DEMO_MANAGED_ROOT}/SpaceshipMath.cs
            ${CMAKE_SOURCE_DIR}/Source/Managed/AltinaEngine.Managed/AltinaEngine.Managed.csproj
            ${CMAKE_SOURCE_DIR}/Source/Managed/AltinaEngine.Managed/ManagedLog.cs
        BYPRODUCTS
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.dll
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.runtimeconfig.json
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.deps.json
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.pdb
        COMMENT "Building SpaceshipGame demo managed scripting assembly"
        VERBATIM
    )

    add_custom_target(AltinaEngineDemoSpaceshipGameManagedBuild
        DEPENDS ${AE_DEMO_MANAGED_STAMP}
    )
    set_property(TARGET AltinaEngineDemoSpaceshipGameManagedBuild PROPERTY BUILD_ALWAYS TRUE)
    add_dependencies(${TargetName} AltinaEngineDemoSpaceshipGameManagedBuild)
    if(TARGET AltinaEngineManagedBuild)
        add_dependencies(AltinaEngineDemoSpaceshipGameManagedBuild AltinaEngineManagedBuild)
    endif()

    add_custom_command(
        TARGET AltinaEngineDemoSpaceshipGameManagedBuild
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.dll
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.runtimeconfig.json
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.deps.json
            ${DemoBinaryDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AE_DEMO_MANAGED_OUT_DIR}/AltinaEngine.Demo.SpaceshipGame.pdb
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
        COMMENT "Staging SpaceshipGame demo managed scripting assembly"
    )
else()
    message(STATUS "dotnet not found; skipping SpaceshipGame demo managed build.")
endif()

add_custom_command(
    OUTPUT ${DemoAssetCookOutput}
    COMMAND $<TARGET_FILE:AltinaEngineAssetTool> cook
        --root ${CMAKE_SOURCE_DIR}
        --platform Win64
        --demo SpaceshipGame
        --build-root ${CMAKE_BINARY_DIR}
        --cook-root ${DemoCookedRoot}
    DEPENDS
        AltinaEngineAssetTool
        ${DemoAssetSources}
    COMMENT "Cooking SpaceshipGame demo assets"
    VERBATIM
)

add_custom_target(AltinaEngineDemoSpaceshipGameCookAssets
    DEPENDS ${DemoAssetCookOutput}
)

if(AE_DEMO_ENABLE_SHIPPING)
    add_custom_command(
        TARGET AltinaEngineDemoSpaceshipGameCookAssets
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_BIN_DIR:PATH=${DemoBinaryDir}
            -DDEMO_SHIP_DIR:PATH=${DemoShippingDir}
            -P ${AE_DEMO_STAGE_SHIPPING_SCRIPT}
        COMMENT "Staging SpaceshipGame demo Shipping folder (asset changes)"
        VERBATIM
    )
endif()

add_dependencies(${TargetName} AltinaEngineDemoSpaceshipGameCookAssets)
